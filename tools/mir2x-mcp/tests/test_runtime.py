import dataclasses
import hashlib
import json
import os
import socket
import subprocess
import sys
import tomllib
from pathlib import Path

import pytest
from mcp.client.session import ClientSession
from mcp.client.stdio import StdioServerParameters, stdio_client

from mir2x_mcp.contract import METHODS, validate
from mir2x_mcp.runtime_cli import host_command, host_config, main
from mir2x_mcp.runtime_config import POLICIES, RuntimeFailure, load_settings
from mir2x_mcp.runtime_process import Manager, management_lock, owned_process


def free_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


@pytest.fixture
def runtime(tmp_path):
    root = tmp_path / "portable space 中文"
    root.mkdir()
    script = Path(__file__).with_name("runtime_child.py")
    port, peer = free_port(), free_port()
    while peer == port:
        peer = free_port()
    (root / "mir2x.local.toml").write_text(
        f"[network]\nclient_port={port}\npeer_port={peer}\n"
        "[launcher]\nstart_timeout=5\nstop_timeout=2\n"
        + "\n".join(
            f"[{role}]\nexecutable={json.dumps(sys._base_executable)}\n"
            f'workdir="."\nargs=["-u", {json.dumps(str(script))}]'
            for role in ("server", "client")
        ),
        "utf-8",
    )
    settings = load_settings(root)
    manager = Manager(settings)
    yield manager
    for role in ("client", "server"):
        manager.stop(role, force=True)


def test_default_policy_ready_without_local_preparation(tmp_path):
    settings = load_settings(tmp_path)
    assert settings.policy == "standard" and settings.bridge_enabled
    for name in POLICIES:
        validate(json.loads(settings.policy_path(name).read_text("utf-8")), "policy")
    policy = json.loads(settings.policy_path().read_text("utf-8"))
    assert "game_move_to" in policy["allowed_actions"]
    assert policy["allow_pvp"] is False
    assert int(policy["max_gold_per_operation"]) > 0
    assert not settings.bridge.exists()


@pytest.mark.parametrize(
    "text",
    [
        "[network]\nclient_prt=7",
        "[network]\nclient_port=true",
        '[launcher]\nbridge="true"',
        '[launcher]\npolicy="unsafe"',
        '[client]\nargs=["--disable-version-check"]',
        "[client]\nargs=[1]",
        "[network]\nclient_port=7000\npeer_port=7000",
        '[launcher]\nstate_dir=".runtime/ai-bridge/x"',
    ],
)
def test_bad_configuration_fails_before_creating_process(tmp_path, text):
    (tmp_path / "mir2x.local.toml").write_text(text, "utf-8")
    with pytest.raises(RuntimeFailure):
        load_settings(tmp_path)
    assert not (tmp_path / ".runtime").exists()


def test_real_managed_start_status_restart_and_normal_close(runtime):
    for role in ("server", "client"):
        first = runtime.start(role)
        assert first["state"] == "running"
        assert runtime.start(role)["pid"] == first["pid"]
        record = runtime.read(role)
        assert owned_process(record)
        assert not owned_process(record | {"created": record["created"] + 100})
        assert not owned_process(record | {"executable": str(Path("unrelated.exe").resolve())})
        if role == "client":
            with pytest.raises(RuntimeFailure, match="更换策略"):
                runtime.start(role, "readonly")
        runtime.stop(role)
        assert runtime.status(role)["state"] == "stopped"
        second = runtime.start(role)
        assert second["pid"] != first["pid"]
        assert runtime.stop(role, force=True)["forced"] is False
    assert not runtime.settings.bridge.exists()


def test_external_port_owner_is_preserved(runtime):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", runtime.settings.client_port))
        listener.listen()
        with pytest.raises(RuntimeFailure, match="端口"):
            runtime.start("server")
        assert listener.fileno() != -1
        assert runtime.read("server") is None


def test_management_lock_excludes_other_process(runtime):
    code = (
        "from pathlib import Path; from mir2x_mcp.runtime_process import management_lock; "
        f"lock=management_lock(Path({str(runtime.settings.state)!r})); lock.__enter__()"
    )
    with management_lock(runtime.settings.state):
        child = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True)
        assert child.returncode != 0 and "另一条管理命令" in child.stderr
    with management_lock(runtime.settings.state):
        pass


def test_stale_record_does_not_touch_an_unrelated_process(runtime):
    runtime.settings.state.mkdir(parents=True)
    record = {"pid": os.getpid(), "created": 0, "executable": sys.executable, "workdir": "."}
    runtime.record_path("client").write_text(json.dumps(record), "utf-8")
    assert runtime.status("client")["stale_record"]
    assert runtime.stop("client")["already_stopped"]
    assert os.getpid() == record["pid"]


def test_damaged_records_fail_closed(runtime):
    runtime.settings.state.mkdir(parents=True)
    runtime.record_path("client").write_text('{"pid":true}', "utf-8")
    with pytest.raises(RuntimeFailure, match="损坏"):
        runtime.stop("client", force=True)
    runtime.record_path("client").unlink()


def test_distribution_move_and_host_config_use_current_location(tmp_path):
    for name in ("first", "moved 中文"):
        root = tmp_path / name
        (root / "client").mkdir(parents=True)
        (root / "client" / ("client.exe" if os.name == "nt" else "client")).touch()
        settings = load_settings(root)
        assert settings.game == root
        command = host_command(settings)
        config = json.loads(host_config(command, "json"))["mcpServers"]["mir2x"]
        toml = tomllib.loads(host_config(command, "toml"))["mcp_servers"]["mir2x"]
        assert config == toml
        assert config["args"][-4:] == ["--root", str(root), "mcp", "serve"]
        assert str(settings.bridge) in Manager(settings).command("client")[-2]


def test_cli_up_status_and_stop_use_same_records(runtime, capsys):
    root = str(runtime.settings.root)
    for command in (["up"], ["status"], ["stop"]):
        with pytest.raises(SystemExit) as result:
            main(["--root", root, "--json", *command])
        assert result.value.code == 0
        assert len(json.loads(capsys.readouterr().out)) == 2


@pytest.mark.parametrize("packaged", [False, True], ids=["source", "frozen"])
async def test_runtime_entry_real_stdio_sdk_without_game(fake, tmp_path, packaged):
    (tmp_path / "mir2x.local.toml").write_text(
        f"[launcher]\nbridge_dir={json.dumps(str(fake.directory))}",
        "utf-8",
    )
    command = sys.executable
    prefix = ["-m", "mir2x_mcp.runtime_cli"]
    if packaged:
        command = os.environ.get("MIR2X_FROZEN_EXE")
        if not command:
            pytest.skip("Set MIR2X_FROZEN_EXE after building the portable executable")
        prefix = []
    parameters = StdioServerParameters(
        command=command,
        args=[*prefix, "--root", str(tmp_path), "mcp", "serve"],
        env={"PYTHONUTF8": "1"},
    )
    async with stdio_client(parameters) as (read, write):
        async with ClientSession(read, write, read_timeout_seconds=15) as session:
            assert (await session.initialize()).server_info.name == "mir2x"
            assert {tool.name for tool in (await session.list_tools()).tools} == set(METHODS)
            assert len((await session.list_resources()).resources) == 6
            result = await session.call_tool("game_get_session", {})
            assert not result.is_error
    assert fake.auths == 1


def test_overridden_policy_is_validated_and_used(tmp_path):
    (tmp_path / "config/policies").mkdir(parents=True)
    path = tmp_path / "config/policies/standard.json"
    path.write_text("{}", "utf-8")
    assert load_settings(tmp_path).policy_path() == path
    before = hashlib.sha256(path.read_bytes()).hexdigest()
    load_settings(tmp_path)
    assert hashlib.sha256(path.read_bytes()).hexdigest() == before
    path.write_text('{"allow_pvp":"yes"}', "utf-8")
    with pytest.raises(RuntimeFailure, match="策略"):
        load_settings(tmp_path)


def test_stop_timeout_keeps_record_until_explicit_force(runtime):
    sleeper = dataclasses.replace(
        runtime.settings.client, args=("-c", "import time;time.sleep(60)")
    )
    manager = Manager(dataclasses.replace(runtime.settings, client=sleeper, stop_timeout=1))
    if os.name != "nt":
        pytest.skip("Windows no-window close behavior")
    manager.start("client")
    with pytest.raises(RuntimeFailure, match="正常关闭未完成"):
        manager.stop("client")
    assert manager.status("client")["state"] == "running"
    stopped = manager.stop("client", force=True)
    assert stopped["state"] == "stopped" and stopped["forced"] is True
