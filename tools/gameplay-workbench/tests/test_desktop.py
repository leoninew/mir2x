import os
import socket
import subprocess
import sys
from pathlib import Path
from shutil import copyfile
from typing import Any
from unittest.mock import Mock

import pytest

from mir2x_workbench.config import load_settings
from mir2x_workbench.desktop import (
    DesktopApi,
    DevelopmentFrontend,
    LocalApiPortInUseError,
    LocalApiServer,
    application_paths,
)
from mir2x_workbench.game_launcher import LaunchError


class FakeWindow:
    title = "Example Desktop"
    resizable = True
    url = "http://localhost:8081/"

    def __init__(self) -> None:
        self.size: tuple[int, int] | None = None
        self.dialog: tuple[str, str] | None = None

    def resize(self, width: int, height: int) -> None:
        self.size = (width, height)

    def create_confirmation_dialog(self, title: str, message: str) -> bool:
        self.dialog = (title, message)
        return True

    def get_current_url(self) -> str:
        return self.url


def test_desktop_bridge_exposes_native_window_and_system_capabilities() -> None:
    api = DesktopApi()

    assert api.say_hello("Ada")["success"] is True
    assert api.get_system_info()["success"] is False
    assert api.get_window_info()["success"] is False
    assert api.resize_window(1200, 800)["success"] is False

    window = FakeWindow()
    api._bind_window(window, window.url)

    assert api.get_system_info()["success"] is True
    assert api.resize_window(1200, 800)["success"] is True
    assert window.size == (1200, 800)
    assert api.resize_window(0, 800)["success"] is False
    assert api.get_window_info() == {
        "success": True,
        "title": "Example Desktop",
        "resizable": True,
    }
    assert api.test_notification("Notice", "Ready") == {
        "success": True,
        "message": "提示框已显示",
    }
    assert window.dialog == ("Notice", "Ready")


def test_shell_bridge_uses_utf8_and_returns_process_output(monkeypatch: pytest.MonkeyPatch) -> None:
    def run(*args: Any, **kwargs: Any) -> subprocess.CompletedProcess[str]:
        assert args == ("echo 桌面",)
        assert kwargs["encoding"] == "utf-8"
        assert kwargs["errors"] == "replace"
        assert kwargs["timeout"] == 30
        return subprocess.CompletedProcess(args[0], 0, "桌面\n", "")

    monkeypatch.setattr("mir2x_workbench.desktop.subprocess.run", run)
    api = DesktopApi()
    window = FakeWindow()
    api._bind_window(window, window.url)

    assert api.run_shell_command("echo 桌面") == {
        "success": True,
        "stdout": "桌面\n",
        "stderr": "",
        "returncode": 0,
    }
    assert window.dialog == ("确认执行 Shell 命令", "echo 桌面")


def test_shell_bridge_rejects_untrusted_pages(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(
        "mir2x_workbench.desktop.subprocess.run",
        lambda *args, **kwargs: pytest.fail("Untrusted pages must not run shell commands"),
    )
    api = DesktopApi()
    window = FakeWindow()
    window.url = "https://example.com/"
    api._bind_window(window, "http://localhost:8081/")

    assert api.run_shell_command("whoami") == {
        "success": False,
        "message": "当前页面未获桌面桥接授权",
    }


def test_shell_confirmation_displays_the_entire_executed_command(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    command = "echo " + "桌面" * 600
    run = Mock(return_value=subprocess.CompletedProcess(command, 0, "", ""))
    monkeypatch.setattr("mir2x_workbench.desktop.subprocess.run", run)
    api = DesktopApi()
    window = FakeWindow()
    api._bind_window(window, window.url)

    assert api.run_shell_command(command)["success"] is True
    assert window.dialog == ("确认执行 Shell 命令", command)
    assert run.call_args.args == (command,)


def test_game_stop_bridge_requires_a_trusted_page(monkeypatch: pytest.MonkeyPatch) -> None:
    api = DesktopApi()
    stop = Mock(return_value={"pid": 1234, "message": "进程已停止"})
    monkeypatch.setattr(api._launcher, "stop", stop)
    window = FakeWindow()
    api._bind_window(window, window.url)

    assert api.stop_game("client", 1234) == {
        "success": True,
        "pid": 1234,
        "message": "进程已停止",
    }
    stop.assert_called_once_with("client", 1234)

    window.url = "https://example.com/"
    assert api.stop_game("client", 1234) == {
        "success": False,
        "message": "当前页面未获桌面桥接授权",
    }
    stop.assert_called_once()


def test_game_stop_bridge_reports_native_stop_failures(monkeypatch: pytest.MonkeyPatch) -> None:
    api = DesktopApi()
    stop = Mock(side_effect=LaunchError("无法停止 client 进程"))
    monkeypatch.setattr(api._launcher, "stop", stop)
    window = FakeWindow()
    api._bind_window(window, window.url)

    assert api.stop_game("client", 1234) == {
        "success": False,
        "message": "无法停止 client 进程",
    }


def test_development_frontend_stops_its_owned_process(tmp_path: Path) -> None:
    process = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
    frontend = DevelopmentFrontend(
        tmp_path,
        "http://localhost:8081",
        "http://127.0.0.1:8000",
        profile="dev",
    )
    frontend._process = process

    frontend.stop()

    assert process.poll() is not None


@pytest.mark.parametrize(("host", "port"), [("localhost", 8081), ("127.0.0.1", 9123)])
def test_development_frontend_starts_selected_profile(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, host: str, port: int
) -> None:
    (tmp_path / "web").mkdir()
    (tmp_path / "web/package.json").write_text("{}", encoding="utf-8")
    monkeypatch.setattr(
        "mir2x_workbench.desktop.wait_for_frontend", Mock(side_effect=[False, True])
    )
    monkeypatch.setattr("mir2x_workbench.desktop.shutil.which", lambda name: "corepack")
    start_process = Mock()
    monkeypatch.setattr("mir2x_workbench.desktop.subprocess.Popen", start_process)
    url = f"http://{host}:{port}"
    frontend = DevelopmentFrontend(tmp_path, url, "http://127.0.0.1:8000", profile="qa")

    assert frontend.start() == url

    arguments, options = start_process.call_args
    assert arguments == (
        ["corepack", "yarn", "dev", "--mode", "qa", "--host", host, "--port", str(port)],
    )
    assert options["cwd"] == tmp_path / "web"
    assert options["env"]["VITE_API_PROXY_TARGET"] == "http://127.0.0.1:8000"


def test_invalid_desktop_configuration_is_logged_without_a_console(tmp_path: Path) -> None:
    (tmp_path / "config").mkdir()
    copyfile(Path(__file__).parents[1] / "config/config.yaml", tmp_path / "config/config.yaml")
    script = """
import sys
from mir2x_workbench.desktop import main
sys.argv = ['mir2x-workbench']
sys.stdout = None
sys.stderr = None
try:
    main()
except SystemExit as exc:
    assert exc.code == 1
else:
    raise AssertionError('Invalid configuration must fail before starting the GUI')
"""
    subprocess.run(
        [sys.executable, "-c", script],
        cwd=tmp_path,
        env={
            **os.environ,
            "PYTHONPATH": str(Path(__file__).parents[1] / "src"),
            "MIR2X_WORKBENCH_SERVER__PORT": "invalid",
        },
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    file_log = (tmp_path / "logs/mir2x-gameplay-workbench.log").read_text(encoding="utf-8")
    assert "Desktop application startup failed" in file_log
    assert "server.port" in file_log


def test_packaged_paths_use_bundled_config_and_static_assets(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    monkeypatch.setattr(sys, "_MEIPASS", str(tmp_path), raising=False)

    config_path, static_path = application_paths(tmp_path, frozen=True)

    assert config_path == tmp_path / "mir2x_workbench" / "config.yaml"
    assert static_path == tmp_path / "static"


@pytest.mark.parametrize("built_frontend", [True, False])
def test_local_api_server_serves_desktop_and_stops(tmp_path: Path, built_frontend: bool) -> None:
    import requests

    static_directory = tmp_path / "static"
    if built_frontend:
        static_directory.mkdir()
        (static_directory / "index.html").write_text("桌面应用", encoding="utf-8")
    settings = load_settings().model_copy(
        update={
            "server": load_settings().server.model_copy(update={"port": 0}),
        }
    )
    server = LocalApiServer(settings, static_directory)

    with server:
        path = "/static/" if built_frontend else "/api/greet?name=Ada"
        response = requests.get(f"{server.url}{path}", timeout=5)
        assert response.status_code == 200
        assert response.encoding == "utf-8"
        if built_frontend:
            assert response.text == "桌面应用"
        else:
            assert response.json() == {"text": "Hello, Ada!"}
    with pytest.raises(requests.RequestException):
        requests.get(f"{server.url}/static/", timeout=0.5)


def test_local_api_server_reports_an_occupied_port(tmp_path: Path) -> None:
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen()
    port = listener.getsockname()[1]
    settings = load_settings().model_copy(
        update={
            "server": load_settings().server.model_copy(update={"port": port}),
        }
    )
    server = LocalApiServer(settings, tmp_path)

    try:
        with pytest.raises(LocalApiPortInUseError) as error:
            server.start()
    finally:
        listener.close()

    assert error.value.host == "127.0.0.1"
    assert error.value.port == port
