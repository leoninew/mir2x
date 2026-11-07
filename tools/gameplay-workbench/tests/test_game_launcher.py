import json
import os
import subprocess
import sys
from pathlib import Path
from typing import cast
from unittest.mock import Mock

import psutil
import pytest

from mir2x_workbench.game_launcher import GameKind, GameLauncher, LaunchError


def test_relative_paths_are_persisted_and_resolved_from_workbench(tmp_path: Path) -> None:
    launcher = GameLauncher(tmp_path)
    saved = launcher.save(r"..\游戏 空格\客户端.exe", "../server/server.exe", "missing")
    assert GameLauncher(tmp_path).status() == saved
    assert saved["settings"] == {
        "schema_version": 1,
        "client_path": r"..\游戏 空格\客户端.exe",
        "server_path": "../server/server.exe",
        "server_config_path": "../server",
    }
    assert saved["targets"]["client"]["executable"] == str(  # type: ignore[index]
        (tmp_path.parent / "游戏 空格/客户端.exe").resolve()
    )


def test_existing_paths_gain_config_directory_and_keep_user_edits(tmp_path: Path) -> None:
    preferences = tmp_path / "game-launch.json"
    preferences.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "client_path": "custom/client.exe",
                "server_path": "custom/server.exe",
            }
        ),
        encoding="utf-8",
    )
    launcher = GameLauncher(tmp_path)
    launcher.initialize(tmp_path.parent / "previous-server")
    migrated = launcher.status()
    assert migrated["settings"] == {
        "schema_version": 1,
        "client_path": "custom/client.exe",
        "server_path": "custom/server.exe",
        "server_config_path": "../previous-server",
    }
    launcher.save(
        "custom/client.exe", "custom/server.exe", str(migrated["revision"]), "../chosen-server"
    )
    reopened = GameLauncher(tmp_path)
    reopened.initialize(tmp_path.parent / "previous-server")
    assert reopened.server_config_root() == (tmp_path.parent / "chosen-server").resolve()


@pytest.mark.parametrize("kind", ["client", "server"])
def test_normal_launch_uses_saved_executable_and_its_own_working_directory(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, kind: GameKind
) -> None:
    executable = tmp_path / f"{kind}.exe"
    executable.touch()
    launcher = GameLauncher(tmp_path)
    saved = launcher.save("client.exe", "server.exe", "missing")
    child = Mock(pid=1234)
    child.poll.return_value = None
    popen = Mock(return_value=child)
    monkeypatch.setattr("mir2x_workbench.game_launcher.subprocess.Popen", popen)
    process = Mock()
    process.exe.return_value = str(executable.resolve())
    process.create_time.return_value = 1234.5
    process.net_connections.return_value = []
    monkeypatch.setattr("mir2x_workbench.game_launcher.psutil.Process", Mock(return_value=process))
    assert launcher.launch(kind, str(saved["revision"]))["pid"] == 1234
    args, kwargs = popen.call_args
    assert args == ([str(executable.resolve()), *(["--auto-launch"] if kind == "server" else [])],)
    assert kwargs["cwd"] == executable.parent.resolve()
    assert kwargs["shell"] is False
    assert kwargs["stdin"] == subprocess.DEVNULL
    assert kwargs["stdout"].name == str(tmp_path / "logs" / f"game-{kind}.log")
    assert kwargs["stderr"] == subprocess.STDOUT
    assert kwargs["close_fds"] is True
    assert kwargs["creationflags"] == (
        subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    )
    assert launcher.status()["targets"][kind]["running"] is True  # type: ignore[index]
    assert launcher.log_tail(kind).startswith("\n--- ")
    state = json.loads((tmp_path / "game-processes.json").read_text(encoding="utf-8"))
    assert state["processes"][kind] == {
        "pid": 1234,
        "executable": GameLauncher._normalize_executable(str(executable.resolve())),
        "create_time": 1234.5,
    }


def test_server_status_reports_own_listening_port(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    launcher = GameLauncher(tmp_path)
    child = Mock(pid=1234)
    child.poll.return_value = None
    launcher._children["server"] = child
    connection = Mock(status="LISTEN", laddr=Mock(port=7000))
    monkeypatch.setattr(
        "mir2x_workbench.game_launcher.psutil.Process",
        lambda pid: Mock(net_connections=Mock(return_value=[connection])),
    )
    assert launcher.status()["targets"]["server"]["listening"] is True  # type: ignore[index]


@pytest.mark.parametrize("kind", ["client", "server"])
def test_stop_terminates_the_requested_workbench_child(tmp_path: Path, kind: GameKind) -> None:
    launcher = GameLauncher(tmp_path)
    child = Mock(pid=1234)
    child.poll.return_value = None
    other = Mock(pid=5678)
    other.poll.return_value = None

    def wait_for_exit(*, timeout: float) -> int:
        child.poll.return_value = 0
        return 0

    child.wait.side_effect = wait_for_exit
    launcher._children[kind] = child
    launcher._children["server" if kind == "client" else "client"] = other
    launcher.save("changed/client.exe", "changed/server.exe", "missing")

    result = launcher.stop(kind, 1234)

    assert result == {"pid": 1234, "message": "进程已停止，PID 1234"}
    child.terminate.assert_called_once_with()
    child.wait.assert_called_once_with(timeout=5)
    assert launcher.status()["targets"][kind]["running"] is False  # type: ignore[index]
    other.terminate.assert_not_called()
    other.kill.assert_not_called()


def test_stop_ignores_a_child_that_already_exited(tmp_path: Path) -> None:
    launcher = GameLauncher(tmp_path)
    child = Mock(pid=1234)
    child.poll.return_value = 1
    launcher._children["client"] = child

    assert launcher.stop("client", 1234)["pid"] == 1234
    child.terminate.assert_not_called()


def test_stop_rejects_an_unowned_process_or_stale_pid(tmp_path: Path) -> None:
    launcher = GameLauncher(tmp_path)
    child = Mock(pid=1234)
    child.poll.return_value = None
    launcher._children["client"] = child

    with pytest.raises(LaunchError, match="进程已变化"):
        launcher.stop("client", 4321)
    with pytest.raises(LaunchError, match="没有本工作台启动"):
        launcher.stop("server", 1234)
    with pytest.raises(LaunchError, match="仅支持 client/server"):
        launcher.stop("other", 1234)

    child.terminate.assert_not_called()


def test_stop_kills_child_when_termination_times_out(tmp_path: Path) -> None:
    launcher = GameLauncher(tmp_path)
    child = Mock(pid=1234)
    child.poll.return_value = None
    child.wait.side_effect = [
        subprocess.TimeoutExpired(cmd="client", timeout=5),
        0,
    ]
    launcher._children["client"] = child

    assert launcher.stop("client", 1234)["pid"] == 1234
    child.terminate.assert_called_once_with()
    child.kill.assert_called_once_with()
    assert child.wait.call_count == 2


def test_stop_reports_failure_when_child_survives_kill_timeout(tmp_path: Path) -> None:
    launcher = GameLauncher(tmp_path)
    child = Mock(pid=1234)
    child.poll.return_value = None
    child.wait.side_effect = subprocess.TimeoutExpired(cmd="server", timeout=5)
    launcher._children["server"] = child

    with pytest.raises(LaunchError, match="无法停止 server 进程"):
        launcher.stop("server", 1234)

    child.kill.assert_called_once_with()
    assert launcher.status()["targets"]["server"]["running"] is True  # type: ignore[index]


def test_stop_waits_for_a_real_child_process_to_exit(tmp_path: Path) -> None:
    process = subprocess.Popen(
        [sys.executable, "-c", "import time; time.sleep(30)"],
        creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
    )
    launcher = GameLauncher(tmp_path)
    launcher._children["client"] = process

    try:
        assert launcher.stop("client", process.pid)["pid"] == process.pid
        assert process.poll() is not None
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


def test_reopened_launcher_recovers_and_stops_its_own_process(tmp_path: Path) -> None:
    process = subprocess.Popen(
        [sys.executable, "-c", "import time; time.sleep(30)"],
        creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
    )
    first = GameLauncher(tmp_path)
    first._process_records["client"] = first._capture_process(process.pid)
    first._write_process_registry()

    try:
        reopened = GameLauncher(tmp_path)
        status = reopened.status()
        targets = cast(dict[str, object], status["targets"])
        target = cast(dict[str, object], targets["client"])
        assert target["running"] is True
        assert target["ownership"] == "managed"
        assert target["pid"] == process.pid

        assert reopened.stop("client", process.pid)["pid"] == process.pid
        assert process.poll() is not None
        assert (
            json.loads((tmp_path / "game-processes.json").read_text(encoding="utf-8"))["processes"]
            == {}
        )
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


@pytest.mark.parametrize(
    ("create_time", "executable", "running"),
    [
        (2.0, r"c:\games\client.exe", True),
        (1.0, r"c:\other\client.exe", True),
        (1.0, r"c:\games\client.exe", False),
    ],
)
def test_reopened_launcher_clears_stale_identity_without_signaling_processes(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    create_time: float,
    executable: str,
    running: bool,
) -> None:
    state = {
        "schema_version": 1,
        "processes": {
            "client": {"pid": 1234, "executable": r"c:\games\client.exe", "create_time": 1.0}
        },
    }
    (tmp_path / "game-processes.json").write_text(json.dumps(state), encoding="utf-8")
    process = Mock()
    process.pid = 1234
    process.is_running.return_value = running
    process.create_time.return_value = create_time
    process.exe.return_value = executable
    monkeypatch.setattr("mir2x_workbench.game_launcher.psutil.Process", Mock(return_value=process))

    launcher = GameLauncher(tmp_path)

    with pytest.raises(LaunchError, match="没有本工作台启动"):
        launcher.stop("client", 1234)
    process.terminate.assert_not_called()
    assert launcher.status()["targets"]["client"]["ownership"] is None  # type: ignore[index]
    assert (
        json.loads((tmp_path / "game-processes.json").read_text(encoding="utf-8"))["processes"]
        == {}
    )


def test_reopened_launcher_disables_control_when_process_identity_is_unknown(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    state = {
        "schema_version": 1,
        "processes": {
            "client": {"pid": 1234, "executable": r"c:\games\client.exe", "create_time": 1.0}
        },
    }
    (tmp_path / "game-processes.json").write_text(json.dumps(state), encoding="utf-8")
    process = Mock()
    process.pid = 1234
    process.is_running.return_value = True
    process.create_time.return_value = 1.0
    process.exe.side_effect = psutil.AccessDenied(pid=1234)
    monkeypatch.setattr("mir2x_workbench.game_launcher.psutil.Process", Mock(return_value=process))

    launcher = GameLauncher(tmp_path)
    targets = cast(dict[str, object], launcher.status()["targets"])
    target = cast(dict[str, object], targets["client"])

    assert target["ownership"] == "unknown"
    assert target["pid"] == 1234
    assert target["ownership_warning"]
    with pytest.raises(LaunchError, match="无法验证 PID 1234"):
        launcher.stop("client", 1234)
    with pytest.raises(LaunchError, match="无法验证 PID 1234"):
        launcher.launch("client", "missing")
    process.terminate.assert_not_called()


def test_recovered_process_identity_is_checked_again_before_stopping(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    first = GameLauncher(tmp_path)
    first._process_records["client"] = first._capture_process(os.getpid())
    first._write_process_registry()
    reopened = GameLauncher(tmp_path)
    assert reopened.status()["targets"]["client"]["running"] is True  # type: ignore[index]
    replacement = Mock()
    replacement.is_running.return_value = True
    replacement.create_time.return_value = 1.0
    monkeypatch.setattr(
        "mir2x_workbench.game_launcher.psutil.Process", Mock(return_value=replacement)
    )

    with pytest.raises(LaunchError, match="身份已变化"):
        reopened.stop("client", os.getpid())

    replacement.terminate.assert_not_called()
    replacement.kill.assert_not_called()


def test_unknown_identity_preserves_pid_when_process_cannot_be_opened(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    first = GameLauncher(tmp_path)
    first._process_records["client"] = first._capture_process(os.getpid())
    first._write_process_registry()
    monkeypatch.setattr(
        "mir2x_workbench.game_launcher.psutil.Process",
        Mock(side_effect=psutil.AccessDenied(pid=os.getpid())),
    )

    target = GameLauncher(tmp_path).status()["targets"]["client"]  # type: ignore[index]

    assert target["ownership"] == "unknown"
    assert target["pid"] == os.getpid()


def test_invalid_process_registry_blocks_launch_without_overwriting_file(tmp_path: Path) -> None:
    state = tmp_path / "game-processes.json"
    state.write_bytes(b"invalid json")
    launcher = GameLauncher(tmp_path)

    assert launcher.status()["process_state_error"]
    with pytest.raises(LaunchError, match="进程记录格式错误"):
        launcher.launch("client", "missing")

    assert state.read_bytes() == b"invalid json"


def test_registry_write_failure_stops_new_child_and_preserves_previous_state(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    (tmp_path / "client.exe").touch()
    previous = b'{"schema_version": 1, "processes": {}}\n'
    state = tmp_path / "game-processes.json"
    state.write_bytes(previous)
    launcher = GameLauncher(tmp_path)
    saved = launcher.save("client.exe", "server.exe", "missing")
    child = Mock(pid=1234)
    child.poll.return_value = None
    child.wait.side_effect = lambda **kwargs: setattr(child.poll, "return_value", 0)
    monkeypatch.setattr("mir2x_workbench.game_launcher.subprocess.Popen", Mock(return_value=child))
    process = Mock()
    process.exe.return_value = str(tmp_path / "client.exe")
    process.create_time.return_value = 1234.5
    monkeypatch.setattr("mir2x_workbench.game_launcher.psutil.Process", Mock(return_value=process))
    monkeypatch.setattr(
        "mir2x_workbench.game_launcher.os.replace", Mock(side_effect=PermissionError("locked"))
    )

    with pytest.raises(LaunchError, match="无法保存进程记录"):
        launcher.launch("client", str(saved["revision"]))

    child.terminate.assert_called_once_with()
    assert launcher.status()["targets"]["client"]["running"] is False  # type: ignore[index]
    assert state.read_bytes() == previous
    assert list(tmp_path.glob(".game-process-state-*")) == []
