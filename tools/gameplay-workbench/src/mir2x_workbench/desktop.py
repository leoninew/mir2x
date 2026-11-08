from __future__ import annotations

import argparse
import ctypes
import errno
import logging
import os
import platform
import shutil
import socket
import subprocess
import sys
import threading
import time
from contextlib import suppress
from datetime import datetime
from pathlib import Path
from typing import Protocol, cast
from urllib.parse import urlsplit

import psutil
import requests
import uvicorn
import webview

from mir2x_workbench.api import build_app
from mir2x_workbench.config import DesktopSettings, Settings, load_settings
from mir2x_workbench.game_launcher import GameLauncher, LaunchError
from mir2x_workbench.gameplay_models import issues_for
from mir2x_workbench.gameplay_service import GameplayError, WorkspaceService
from mir2x_workbench.logging_setup import configure_logging

logger = logging.getLogger("mir2x_workbench.desktop")


class WindowHandle(Protocol):
    title: str
    resizable: bool

    def resize(self, width: int, height: int) -> None: ...

    def create_confirmation_dialog(self, title: str, message: str) -> bool: ...

    def get_current_url(self) -> str | None: ...


class LocalApiPortInUseError(RuntimeError):
    def __init__(self, host: str, port: int) -> None:
        self.host = host
        self.port = port
        super().__init__(f"{host}:{port}")


class DesktopApi:
    def __init__(
        self, workspace: WorkspaceService | None = None, *, application_root: Path | None = None
    ) -> None:
        self._window: WindowHandle | None = None
        self._trusted_url: str | None = None
        self._workspace = workspace if workspace is not None else WorkspaceService()
        self._launcher = GameLauncher(application_root or Path.cwd())

    def get_game_launch_status(self) -> dict[str, object]:
        if not self._is_trusted_page():
            return {"success": False, "message": "当前页面未获桌面桥接授权"}
        try:
            return {"success": True, "data": self._launcher.status()}
        except (LaunchError, OSError) as error:
            return {"success": False, "message": str(error)}

    def save_game_launch_paths(
        self, client_path: str, server_path: str, revision: str, server_config_path: str
    ) -> dict[str, object]:
        if not self._is_trusted_page():
            return {"success": False, "message": "当前页面未获桌面桥接授权"}
        try:
            return {
                "success": True,
                "data": self._launcher.save(client_path, server_path, revision, server_config_path),
            }
        except ValueError as error:
            return {"success": False, "message": "请修正启动路径", "issues": issues_for(error)}
        except (LaunchError, OSError) as error:
            return {"success": False, "message": str(error)}

    def launch_game(self, kind: str, revision: str) -> dict[str, object]:
        if not self._is_trusted_page():
            return {"success": False, "message": "当前页面未获桌面桥接授权"}
        try:
            return {"success": True, **self._launcher.launch(kind, revision)}
        except (LaunchError, OSError) as error:
            return {"success": False, "message": str(error)}

    def stop_game(self, kind: str, pid: int) -> dict[str, object]:
        if not self._is_trusted_page():
            return {"success": False, "message": "当前页面未获桌面桥接授权"}
        try:
            return {"success": True, **self._launcher.stop(kind, pid)}
        except (LaunchError, OSError) as error:
            return {"success": False, "message": str(error)}

    def get_game_log(self, kind: str) -> dict[str, object]:
        if not self._is_trusted_page():
            return {"success": False, "message": "当前页面未获桌面桥接授权"}
        try:
            return {"success": True, "data": self._launcher.log_tail(kind)}
        except LaunchError as error:
            return {"success": False, "message": str(error)}

    def select_gameplay_workspace(self) -> dict[str, object]:
        if not self._is_trusted_page() or self._window is None:
            return {"success": False, "message": "当前页面未获桌面桥接授权"}
        try:
            selected = cast(webview.Window, self._window).create_file_dialog(
                webview.FileDialog.FOLDER
            )
            if not selected:
                return {"success": False, "message": "未选择目录"}
            workspace = self._workspace.bind(Path(selected[0]))
            return {"success": True, "workspace": workspace}
        except (OSError, ValueError, GameplayError) as error:
            return {"success": False, "message": str(error)}

    def _bind_window(self, window: WindowHandle, trusted_url: str) -> None:
        self._window = window
        self._trusted_url = trusted_url

    def _is_trusted_page(self) -> bool:
        if self._window is None or self._trusted_url is None:
            return False
        current_url = self._window.get_current_url()
        if current_url is None:
            return False

        current = urlsplit(current_url)
        trusted = urlsplit(self._trusted_url)
        if (current.scheme, current.netloc) != (trusted.scheme, trusted.netloc):
            return False
        trusted_path = trusted.path.rstrip("/")
        return (
            not trusted_path
            or current.path == trusted_path
            or current.path.startswith(f"{trusted_path}/")
        )

    def say_hello(self, name: str) -> dict[str, object]:
        now = datetime.now()
        return {
            "success": True,
            "message": f"你好，{name}！当前时间：{now.strftime('%Y-%m-%d %H:%M:%S')}",
            "timestamp": now.isoformat(),
        }

    def get_system_info(self) -> dict[str, object]:
        if not self._is_trusted_page():
            return {"success": False, "message": "当前页面未获桌面桥接授权"}
        return {
            "success": True,
            "data": {
                "system": platform.system(),
                "release": platform.release(),
                "version": platform.version(),
                "machine": platform.machine(),
                "processor": platform.processor(),
                "python_version": platform.python_version(),
            },
        }

    def resize_window(self, width: int, height: int) -> dict[str, object]:
        if not self._is_trusted_page() or self._window is None:
            return {"success": False, "message": "当前页面未获桌面桥接授权"}
        if width <= 0 or height <= 0:
            return {"success": False, "message": "窗口大小超出允许范围"}

        try:
            self._window.resize(width, height)
            return {"success": True, "message": f"窗口大小已调整为 {width}x{height}"}
        except Exception as exc:
            logger.exception("Failed to resize the desktop window")
            return {"success": False, "message": f"调整窗口失败: {exc}"}

    def get_window_info(self) -> dict[str, object]:
        if not self._is_trusted_page() or self._window is None:
            return {"success": False, "message": "当前页面未获桌面桥接授权"}
        return {
            "success": True,
            "title": self._window.title,
            "resizable": self._window.resizable,
        }

    def test_notification(self, title: str, message: str) -> dict[str, object]:
        if not self._is_trusted_page() or self._window is None:
            return {"success": False, "message": "当前页面未获桌面桥接授权"}
        try:
            self._window.create_confirmation_dialog(title, message)
            return {"success": True, "message": "提示框已显示"}
        except Exception as exc:
            logger.exception("Failed to show the desktop notification dialog")
            return {"success": False, "message": f"显示通知失败: {exc}"}

    def run_shell_command(self, command: str) -> dict[str, object]:
        if not self._is_trusted_page() or self._window is None:
            return {"success": False, "message": "当前页面未获桌面桥接授权"}
        if not self._window.create_confirmation_dialog("确认执行 Shell 命令", command):
            return {"success": False, "message": "命令已取消"}
        try:
            result = subprocess.run(
                command,
                shell=True,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=30,
                check=False,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
            )
            return {
                "success": result.returncode == 0,
                "stdout": result.stdout,
                "stderr": result.stderr,
                "returncode": result.returncode,
            }
        except subprocess.TimeoutExpired:
            return {"success": False, "message": "命令执行超时（30 秒）"}
        except OSError as exc:
            logger.exception("Failed to start a desktop shell command")
            return {"success": False, "message": f"命令执行失败: {exc}"}


class LocalApiServer:
    def __init__(
        self,
        settings: Settings,
        static_directory: Path,
        application_root: Path | None = None,
    ) -> None:
        self.settings = settings
        self.static_directory = static_directory
        self.backup_root = (
            application_root / "backups" / "gameplay" if application_root is not None else None
        )
        self.url = ""
        self._socket: socket.socket | None = None
        self._server: uvicorn.Server | None = None
        self._thread: threading.Thread | None = None
        self._error: BaseException | None = None
        self.workspace = WorkspaceService()
        if application_root is not None:
            launcher = GameLauncher(application_root)
            try:
                launcher.initialize(settings.gameplay.root)
                self.workspace.bind(launcher.server_config_root())
            except (LaunchError, OSError, ValueError, GameplayError):
                logger.warning("Unable to load configured gameplay directory", exc_info=True)
        elif settings.gameplay.root is not None:
            self.workspace.bind(settings.gameplay.root)

    def start(self) -> str:
        server_settings = self.settings.server.model_copy(
            update={"host": "127.0.0.1", "reload": False}
        )
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket = listener
        try:
            try:
                listener.bind((server_settings.host, server_settings.port))
            except OSError as error:
                if error.errno == errno.EADDRINUSE or getattr(error, "winerror", None) == 10048:
                    raise LocalApiPortInUseError(
                        server_settings.host, server_settings.port
                    ) from error
                raise
            listener.listen(2048)
            listener.setblocking(False)
            self.url = f"http://{server_settings.host}:{listener.getsockname()[1]}"
            self._server = uvicorn.Server(
                uvicorn.Config(
                    build_app(
                        settings=self.settings,
                        static_directory=self.static_directory,
                        workspace=self.workspace,
                        backup_root=self.backup_root,
                    ),
                    host=server_settings.host,
                    port=server_settings.port,
                    log_config=None,
                    access_log=False,
                    reload=False,
                )
            )
            self._thread = threading.Thread(
                target=self._serve,
                args=(self._server, listener),
                name="mir2x-gameplay-workbench",
                daemon=True,
            )
            self._thread.start()
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                if self._error is not None:
                    raise RuntimeError("FastAPI failed to start") from self._error
                if self._server.started:
                    return self.url
                time.sleep(0.05)
            raise TimeoutError("FastAPI did not become ready within 15 seconds")
        except BaseException:
            self.stop()
            raise

    def _serve(self, server: uvicorn.Server, listener: socket.socket) -> None:
        try:
            server.run(sockets=[listener])
        except BaseException as exc:
            self._error = exc

    def stop(self) -> None:
        if self._server is not None:
            self._server.should_exit = True
        if self._thread is not None:
            self._thread.join(timeout=10)
            if self._thread.is_alive() and self._server is not None:
                self._server.force_exit = True
                self._thread.join(timeout=2)
        if self._socket is not None:
            self._socket.close()
            self._socket = None

    def __enter__(self) -> LocalApiServer:
        self.start()
        return self

    def __exit__(self, *_: object) -> None:
        self.stop()


class DevelopmentFrontend:
    def __init__(self, project_root: Path, url: str, api_url: str, *, profile: str) -> None:
        self.project_root = project_root
        self.url = url
        self.api_url = api_url
        self.profile = profile
        self._process: subprocess.Popen[bytes] | None = None

    def start(self) -> str:
        if wait_for_frontend(self.url, timeout=0):
            logger.info("Reusing the Vite server at %s", self.url)
            return self.url

        frontend_directory = self.project_root / "web"
        if not (frontend_directory / "package.json").is_file():
            raise FileNotFoundError(f"Vite project not found: {frontend_directory}")

        corepack = shutil.which("corepack")
        if corepack is None:
            raise FileNotFoundError("Corepack is required to start the Yarn 4 development server")

        log_directory = self.project_root / "logs"
        log_directory.mkdir(parents=True, exist_ok=True)
        environment = os.environ.copy()
        environment["VITE_API_PROXY_TARGET"] = self.api_url
        frontend_url = urlsplit(self.url)
        log_file = (log_directory / "vite-dev.log").open("ab")
        try:
            self._process = subprocess.Popen(
                [
                    corepack,
                    "yarn",
                    "dev",
                    "--mode",
                    self.profile,
                    "--host",
                    str(frontend_url.hostname),
                    "--port",
                    str(frontend_url.port or (443 if frontend_url.scheme == "https" else 80)),
                ],
                cwd=frontend_directory,
                env=environment,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
                shell=os.name == "nt",
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
            )
        finally:
            log_file.close()

        if wait_for_frontend(self.url, timeout=30, process=self._process):
            logger.info("Vite development server is ready at %s", self.url)
            return self.url
        self.stop()
        raise RuntimeError("Vite did not become ready; check logs/vite-dev.log")

    def stop(self) -> None:
        if self._process is None:
            return
        try:
            parent = psutil.Process(self._process.pid)
            children = parent.children(recursive=True)
            for process in [*children, parent]:
                with suppress(psutil.NoSuchProcess):
                    process.terminate()
            _, running = psutil.wait_procs([*children, parent], timeout=3)
            for process in running:
                process.kill()
            self._process.wait(timeout=5)
        except (psutil.NoSuchProcess, subprocess.TimeoutExpired):
            pass
        self._process = None

    def __enter__(self) -> str:
        return self.start()

    def __exit__(self, *_: object) -> None:
        self.stop()


def wait_for_frontend(
    url: str,
    *,
    timeout: float,
    process: subprocess.Popen[bytes] | None = None,
) -> bool:
    deadline = time.monotonic() + timeout
    with requests.Session() as session:
        session.trust_env = False
        while True:
            if process is not None and process.poll() is not None:
                return False
            try:
                response = session.get(url, timeout=0.5, allow_redirects=False)
                if response.status_code == 200:
                    return True
            except requests.RequestException:
                pass
            if time.monotonic() >= deadline:
                return False
            time.sleep(0.1)


def application_paths(project_root: Path, *, frozen: bool) -> tuple[Path, Path]:
    if frozen:
        resource_root = Path(vars(sys)["_MEIPASS"])
        return (
            resource_root / "mir2x_workbench" / "config.yaml",
            resource_root / "static",
        )
    installed_static = Path(__file__).with_name("static")
    return (
        Path(__file__).with_name("config.yaml"),
        installed_static if installed_static.is_dir() else project_root / "web" / "dist",
    )


def create_window(settings: DesktopSettings, url: str, api: DesktopApi) -> webview.Window:
    window = webview.create_window(
        title=settings.title,
        url=url,
        width=settings.width,
        height=settings.height,
        min_size=(settings.min_width, settings.min_height),
        resizable=True,
        text_select=True,
        js_api=api,
    )
    if window is None:
        raise RuntimeError("PyWebView did not create the desktop window")

    api._bind_window(window, url)
    return window


def run_desktop(*, development: bool = False, project_root: Path | None = None) -> None:
    frozen = bool(getattr(sys, "frozen", False))
    root = (project_root or (Path(sys.executable).parent if frozen else Path.cwd())).resolve()
    configure_logging(root)
    default_config, static_directory = application_paths(root, frozen=frozen)
    settings = load_settings(root, default_config=default_config)
    if development and frozen:
        raise ValueError("The Vite development mode is not available in a packaged application")
    if not development and not (static_directory / "index.html").is_file():
        raise FileNotFoundError(f"Built web app not found: {static_directory / 'index.html'}")

    logger.info("Starting the desktop application")
    with LocalApiServer(settings, static_directory, root) as api_server:
        frontend = (
            DevelopmentFrontend(
                root, settings.desktop.frontend_url, api_server.url, profile=settings.profile
            )
            if development
            else None
        )
        try:
            if frontend is not None:
                frontend.start()
            window_url = (
                settings.desktop.frontend_url if development else f"{api_server.url}/static/"
            )
            create_window(
                settings.desktop,
                window_url,
                DesktopApi(api_server.workspace, application_root=root),
            )
            webview.start(debug=settings.desktop.debug)
            logger.info("Desktop window closed")
        finally:
            if frontend is not None:
                frontend.stop()


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the FastAPI desktop application.")
    parser.add_argument(
        "--dev",
        action="store_true",
        help="Start or reuse the Vite development server instead of built web assets.",
    )
    parser.add_argument(
        "--check-workspace",
        type=Path,
        help="Validate a selected server directory without opening a desktop window.",
    )
    arguments = parser.parse_args()
    try:
        if arguments.check_workspace is not None:
            root = Path(sys.executable).parent if getattr(sys, "frozen", False) else Path.cwd()
            default_config, static_directory = application_paths(
                root, frozen=bool(getattr(sys, "frozen", False))
            )
            load_settings(root, default_config=default_config)
            if not (static_directory / "index.html").is_file():
                raise FileNotFoundError("Built frontend is missing")
            WorkspaceService(arguments.check_workspace)
            return
        run_desktop(development=arguments.dev)
    except Exception as error:
        logger.exception("Desktop application startup failed")
        root = Path(sys.executable).parent if getattr(sys, "frozen", False) else Path.cwd()
        log_path = root / "logs" / "mir2x-gameplay-workbench.log"
        if isinstance(error, LocalApiPortInUseError):
            detail = (
                f"本地服务端口 {error.host}:{error.port} 已被占用。\n"
                "请关闭占用该端口的开发服务或其他程序后重试；"
                "也可以修改 Workbench 的 config/config.yaml 中的 server.port。"
            )
        else:
            detail = f"{type(error).__name__}: {error}"
        message = f"Workbench 启动失败。\n\n{detail}\n\n详细日志：\n{log_path}"
        if os.name == "nt" and getattr(sys, "frozen", False):
            ctypes.windll.user32.MessageBoxW(None, message, "Mir2x Workbench 启动失败", 0x10)
        elif sys.stderr is not None:
            print(message, file=sys.stderr)
        raise SystemExit(1) from None


if __name__ == "__main__":
    main()
