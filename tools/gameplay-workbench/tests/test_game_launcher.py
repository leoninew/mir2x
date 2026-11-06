import os
import subprocess
from pathlib import Path
from unittest.mock import Mock

import pytest

from mir2x_workbench.game_launcher import GameLauncher


def test_relative_paths_are_persisted_and_resolved_from_workbench(tmp_path: Path) -> None:
    launcher = GameLauncher(tmp_path)
    saved = launcher.save(r"..\游戏 空格\客户端.exe", "../server/server.exe", "missing")
    assert GameLauncher(tmp_path).status() == saved
    assert saved["settings"] == {
        "schema_version": 1,
        "client_path": r"..\游戏 空格\客户端.exe",
        "server_path": "../server/server.exe",
    }
    assert saved["targets"]["client"]["executable"] == str(  # type: ignore[index]
        (tmp_path.parent / "游戏 空格/客户端.exe").resolve()
    )


def test_normal_launch_uses_saved_executable_and_its_own_working_directory(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    executable = tmp_path / "client.exe"
    executable.touch()
    launcher = GameLauncher(tmp_path)
    saved = launcher.save("client.exe", "server.exe", "missing")
    child = Mock(pid=1234)
    child.poll.return_value = None
    popen = Mock(return_value=child)
    monkeypatch.setattr("mir2x_workbench.game_launcher.subprocess.Popen", popen)
    assert launcher.launch("client", str(saved["revision"]))["pid"] == 1234
    args, kwargs = popen.call_args
    assert args == ([str(executable.resolve())],)
    assert kwargs == {
        "cwd": executable.parent.resolve(),
        "shell": False,
        "stdin": subprocess.DEVNULL,
        "stdout": subprocess.DEVNULL,
        "stderr": subprocess.DEVNULL,
        "close_fds": True,
        "creationflags": subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
    }
    assert launcher.status()["targets"]["client"]["running"] is True  # type: ignore[index]
