import dataclasses
import hashlib
import json
import os
import shutil
import socket
import subprocess
import sys
import time
import tomllib
from pathlib import Path

import pytest
from mcp.client.session import ClientSession
from mcp.client.stdio import StdioServerParameters, stdio_client

from mir2x_mcp.contract import METHODS, validate
from mir2x_mcp.runtime_cli import host_command, host_config, main
from mir2x_mcp.runtime_config import POLICIES, RuntimeFailure, load_settings
from mir2x_mcp.runtime_login import ClientLogin
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


def test_managed_bridge_optout_overrides_native_automatic_default(tmp_path):
    (tmp_path / "mir2x.local.toml").write_text("[launcher]\nbridge=false\n", "utf-8")
    manager = Manager(load_settings(tmp_path))
    args = manager.command("client")
    assert "--enable-ai-bridge=0" in args
    assert not any(arg.startswith("--ai-policy=") for arg in args)
    assert not any(arg.startswith("--ai-bridge-dir=") for arg in args)
    # Startup opt-out must not create credentials or require a policy copy.
    assert not manager.settings.bridge.exists()
    assert not manager.settings.state.exists()


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


def test_invalid_login_is_validated_before_restart_or_up(runtime, capsys):
    path = runtime.settings.login_env
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('username=a\npassword="sensitive-unclosed', "utf-8")
    root = str(runtime.settings.root)
    with pytest.raises(SystemExit) as result:
        main(["--root", root, "up"])
    assert result.value.code == 1
    assert runtime.status("server")["state"] == "stopped"
    original = runtime.start("client", login=ClientLogin())
    for command in (["client", "restart"], ["restart"]):
        with pytest.raises(SystemExit) as result:
            main(["--root", root, *command])
        assert result.value.code == 1
        assert runtime.status("client")["pid"] == original["pid"]
    assert "sensitive-unclosed" not in capsys.readouterr().err
    with pytest.raises(SystemExit) as result:
        main(["--root", root, "client", "restart", "--manual-login"])
    assert result.value.code == 0
    assert runtime.status("client")["pid"] != original["pid"]


def test_spawn_error_never_echoes_login_argv(runtime, monkeypatch, capsys):
    secret = "synthetic-spawn-secret"
    path = runtime.settings.login_env
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f"username=synthetic\npassword={secret}", "utf-8")

    def fail(args, **kwargs):
        assert args[-1] == f"--auto-login=synthetic:{secret}"
        raise OSError(f"spawn failed: {args}")

    monkeypatch.setattr(subprocess, "Popen", fail)
    with pytest.raises(SystemExit) as result:
        main(["--root", str(runtime.settings.root), "--json", "client", "start"])
    assert result.value.code == 1
    output = capsys.readouterr()
    assert secret not in output.err + output.out
    assert "无法创建进程" in json.loads(output.err)["error"]
    assert runtime.read("client") is None
    assert secret not in repr(runtime.settings)
    assert secret not in repr(runtime.command("client"))


@pytest.mark.parametrize("packaged", [False, True], ids=["source", "frozen"])
@pytest.mark.parametrize("mode", ["auto", "manual", "missing", "disabled"])
def test_entry_passes_login_only_to_private_spawn_and_keeps_records_clean(runtime, packaged, mode):
    secret = "synthetic-test-password:literal $HOME & (literal)"
    login = f"synthetic-user:{secret}" if mode == "auto" else ""
    expected = hashlib.sha256(login.encode()).hexdigest()
    config = runtime.settings.root / "mir2x.local.toml"
    text = config.read_text("utf-8")
    # Append the synthetic child's assertion to client args, after the Python script.
    settings = runtime.settings
    script_args = [*settings.client.args, f"--expect-login-sha256={expected}"]
    prefix, _ = text.rsplit("args=", 1)
    text = prefix + "args=" + json.dumps(script_args)
    if mode == "disabled":
        text += "\n[login]\nenabled=false\n"
    config.write_text(text, "utf-8")
    path = settings.login_env
    path.parent.mkdir(parents=True, exist_ok=True)
    if mode != "missing":
        path.write_text(f"username=synthetic-user\npassword={secret}", "utf-8")
    command = [sys.executable, "-m", "mir2x_mcp.runtime_cli"]
    if packaged:
        executable = os.environ.get("MIR2X_FROZEN_EXE")
        if not executable:
            pytest.skip("Set MIR2X_FROZEN_EXE after building the portable executable")
        command = [executable]
    command += ["--root", str(settings.root), "--json", "client", "start"]
    if mode == "manual":
        command += ["--manual-login"]
    result = subprocess.run(
        command,
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=30,
    )
    assert result.returncode == 0, "synthetic launcher failed"
    assert json.loads(result.stdout)[0]["state"] == "running"
    log = settings.state / "logs/client.log"
    deadline = time.monotonic() + 5
    while "synthetic login argument verified" not in log.read_text("utf-8"):
        assert time.monotonic() < deadline
        time.sleep(0.05)
    for file in settings.state.rglob("*"):
        if file.is_file():
            assert secret.encode() not in file.read_bytes()
            assert b"synthetic-user" not in file.read_bytes()
    assert secret not in result.stdout + result.stderr


@pytest.mark.parametrize("with_login", [False, True], ids=["manual-fallback", "local-login"])
def test_actual_source_and_release_short_entries_share_processes_and_runtime_root(
    runtime, with_login, monkeypatch, capsys
):
    executable = os.environ.get("MIR2X_FROZEN_EXE")
    if os.name != "nt" or not executable:
        pytest.skip("Windows short entries require the freshly built MIR2X_FROZEN_EXE")
    repo = Path(__file__).resolve().parents[3]
    source = runtime.settings.root / "source space 中文"
    project = source / "tools/mir2x-mcp"
    project.mkdir(parents=True)
    installed = source / "install"
    installed.mkdir()
    for name in ("pyproject.toml", "uv.lock"):
        shutil.copy2(repo / "tools/mir2x-mcp" / name, project / name)
    for directory in (source, installed):
        shutil.copy2(repo / "mir2x.cmd", directory / "mir2x.cmd")
    shutil.copy2(executable, installed / "mir2x.exe")
    shutil.copy2(
        repo / "tools/mir2x-mcp/src/mir2x_mcp/runtime_assets/mir2x.defaults.toml",
        installed / "mir2x.defaults.toml",
    )
    # A stale source-root override/login file must not affect the default install namespace.
    (source / "mir2x.local.toml").write_text("[invalid]\nvalue=true", "utf-8")
    (source / ".env").write_text('username=wrong\npassword="invalid', "utf-8")
    config = (runtime.settings.root / "mir2x.local.toml").read_text("utf-8")
    login = "synthetic-user:synthetic-short-entry" if with_login else ""
    expected = hashlib.sha256(login.encode()).hexdigest()
    args = [
        *runtime.settings.client.args,
        f"--expect-login-sha256={expected}",
        f"--expect-bridge-dir={installed / '.runtime/ai-bridge'}",
    ]
    prefix, _ = config.rsplit("args=", 1)
    (installed / "mir2x.local.toml").write_text(prefix + "args=" + json.dumps(args), "utf-8")
    if with_login:
        (installed / ".env").write_text(
            "username=synthetic-user\npassword=synthetic-short-entry", "utf-8"
        )
    environment = os.environ.copy()
    environment.pop("MIR2X_HOME", None)
    environment.update(
        UV_PROJECT_ENVIRONMENT=sys.prefix,
        UV_NO_SYNC="1",  # Exercise the real wrapper/uv without modifying the locked environment.
        PYTHONUTF8="1",
    )

    def short(directory, *arguments, ok=True):
        # All arguments here are fixed test commands or controlled paths, never credentials.
        invocation = f'""{directory / "mir2x.cmd"}" {subprocess.list2cmdline(arguments)}"'
        result = subprocess.run(
            subprocess.list2cmdline([os.environ.get("COMSPEC", "cmd.exe"), "/d", "/s", "/c"])
            + " "
            + invocation,
            cwd=runtime.settings.root,
            env=environment,
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=45,
        )
        assert result.returncode == (0 if ok else 1), result.stderr
        return json.loads(result.stdout) if ok else result

    manager = Manager(load_settings(installed))
    try:
        first = short(source, "--json", "up")
        second = short(installed, "--json", "up")
        assert [item["pid"] for item in first] == [item["pid"] for item in second]
        assert all(item["already_running"] for item in second)
        assert short(source, "--json", "status") == short(installed, "--json", "status")
        configs = [short(directory, "mcp", "config") for directory in (source, installed)]
        for config in configs:
            assert config["mcpServers"]["mir2x"]["args"][-4:] == [
                "--root",
                str(installed),
                "mcp",
                "serve",
            ]
        assert manager.settings.bridge == installed / ".runtime/ai-bridge"
        assert not (source / ".runtime/manager").exists()
        assert not (source / ".runtime/ai-bridge").exists()
        logfile = manager.settings.state / "logs/client.log"
        assert "synthetic login argument verified" in logfile.read_text("utf-8")
        assert "synthetic bridge argument verified" in logfile.read_text("utf-8")
        for file in manager.settings.state.rglob("*"):
            if file.is_file():
                assert b"synthetic-short-entry" not in file.read_bytes()
        # Register shares host_command; intercept only the external global-config mutation.
        registered = []
        monkeypatch.setattr(
            "mir2x_mcp.runtime_cli.register",
            lambda command, name: registered.append(command) or {"registered": name},
        )
        with pytest.raises(SystemExit) as result:
            main(["--root", str(installed), "mcp", "register"])
        assert result.value.code == 0
        capsys.readouterr()
        assert registered[0][-4:] == ["--root", str(installed), "mcp", "serve"]
        # Explicit roots override the source wrapper's default and frozen executable directory.
        for directory in (source, installed):
            other = short(directory, "--root", str(runtime.settings.root), "--json", "status")
            assert all(item["state"] == "stopped" for item in other)
        environment["MIR2X_HOME"] = str(runtime.settings.root)
        for directory in (source, installed):
            assert all(item["state"] == "stopped" for item in short(directory, "--json", "status"))
        environment.pop("MIR2X_HOME")
        short(source, "--json", "stop")
        assert all(item["state"] == "stopped" for item in short(installed, "--json", "status"))
        empty_source = runtime.settings.root / "empty source"
        empty_project = empty_source / "tools/mir2x-mcp"
        empty_project.mkdir(parents=True)
        for name in ("pyproject.toml", "uv.lock"):
            shutil.copy2(project / name, empty_project / name)
        shutil.copy2(repo / "mir2x.cmd", empty_source / "mir2x.cmd")
        missing = short(empty_source, "--json", "status", ok=False)
        assert "运行目录不存在" in json.loads(missing.stderr)["error"]
        (empty_source / "install").mkdir()
        missing = short(empty_source, "--json", "status", ok=False)
        assert "发行配置" in json.loads(missing.stderr)["error"]
        assert not (empty_source / "install/.runtime").exists()
    finally:
        for role in ("client", "server"):
            manager.stop(role, force=True)
