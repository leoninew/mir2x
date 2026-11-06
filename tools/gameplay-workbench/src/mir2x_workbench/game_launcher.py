"""Native-only game process launching and relative-path preferences."""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
import tempfile
from contextlib import suppress
from pathlib import Path, PurePosixPath, PureWindowsPath
from threading import RLock
from typing import Annotated, Literal
from uuid import uuid4

from pydantic import Field, StrictInt, field_validator

from mir2x_workbench.gameplay_models import Model, load_json

type GameKind = Literal["client", "server"]
KINDS: tuple[GameKind, ...] = ("client", "server")


class LaunchError(Exception):
    pass


class LaunchPaths(Model):
    schema_version: Annotated[StrictInt, Field(ge=1, le=1)] = 1
    client_path: str = "../client/client.exe"
    server_path: str = "../server/server.exe"

    @field_validator("client_path", "server_path")
    @classmethod
    def relative_executable(cls, value: str) -> str:
        if not value.strip() or value != value.strip() or any(ord(c) < 32 for c in value):
            raise ValueError("请输入不含空白前后缀的相对程序路径")
        windows = PureWindowsPath(value)
        if windows.drive or windows.root or PurePosixPath(value).is_absolute():
            raise ValueError("请使用相对于工作台目录的路径，不要填写绝对路径")
        if PurePosixPath(value.replace("\\", "/")).suffix.lower() != ".exe":
            raise ValueError("请选择 .exe 程序，不支持脚本或命令")
        return value


class GameLauncher:
    def __init__(self, application_root: Path):
        self.root = application_root.resolve()
        self.preferences = self.root / "game-launch.json"
        self._lock = RLock()
        self._children: dict[GameKind, subprocess.Popen[bytes]] = {}
        self._launched_paths: dict[GameKind, str] = {}

    def _read(self) -> tuple[LaunchPaths, str, str | None, bytes | None]:
        try:
            raw = self.preferences.read_bytes()
        except FileNotFoundError:
            return LaunchPaths(), "missing", None, None
        except OSError as error:
            raise LaunchError(f"无法读取启动配置：{error}") from error
        revision = hashlib.sha256(raw).hexdigest()
        try:
            return LaunchPaths.model_validate(load_json(raw)), revision, None, raw
        except ValueError:
            return LaunchPaths(), revision, "启动配置格式错误，请重新保存路径", raw

    def _executable(self, paths: LaunchPaths, kind: GameKind) -> Path:
        relative = paths.client_path if kind == "client" else paths.server_path
        # Accept Windows separators in a preference file copied between platforms.
        return (self.root / relative.replace("\\", "/")).resolve()

    def status(self) -> dict[str, object]:
        with self._lock:
            paths, revision, error, _ = self._read()
            targets: dict[str, object] = {}
            for kind in KINDS:
                executable = self._executable(paths, kind)
                child = self._children.get(kind)
                exit_code = child.poll() if child is not None else None
                warning = ""
                if kind == "client" and not (executable.parent / "res").is_dir():
                    warning = "缺少 res 资源目录"
                if kind == "server":
                    missing = [
                        name
                        for name in ("config/gameplay/manifest.json", "map/mapbin.zsdb", "script")
                        if not (executable.parent / name).exists()
                    ]
                    if missing:
                        warning = "缺少 " + "、".join(missing)
                targets[kind] = {
                    "executable": str(executable),
                    "working_directory": str(executable.parent),
                    "exists": executable.is_file(),
                    "running": child is not None and exit_code is None,
                    "pid": child.pid if child is not None else None,
                    "exit_code": exit_code,
                    "launched_executable": self._launched_paths.get(kind),
                    "warning": warning,
                }
            return {
                "base_directory": str(self.root),
                "settings": paths.model_dump(),
                "revision": revision,
                "settings_error": error,
                "targets": targets,
            }

    def save(self, client_path: str, server_path: str, revision: str) -> dict[str, object]:
        paths = LaunchPaths(client_path=client_path, server_path=server_path)
        with self._lock:
            _, current, error, old_raw = self._read()
            if revision != current:
                raise LaunchError("启动配置已被外部修改，请刷新后重新编辑")
            content = (json.dumps(paths.model_dump(), ensure_ascii=False, indent=2) + "\n").encode(
                "utf-8"
            )
            temporary: str | None = None
            try:
                if error and old_raw is not None:
                    backup = self.root / f"game-launch-invalid-{uuid4().hex}.json.bak"
                    with backup.open("xb") as stream:
                        stream.write(old_raw)
                        stream.flush()
                        os.fsync(stream.fileno())
                descriptor, temporary = tempfile.mkstemp(prefix=".game-launch-", dir=self.root)
                with os.fdopen(descriptor, "wb") as stream:
                    stream.write(content)
                    stream.flush()
                    os.fsync(stream.fileno())
                os.replace(temporary, self.preferences)
                temporary = None
            except OSError as save_error:
                raise LaunchError("无法保存启动路径，原配置保持不变") from save_error
            finally:
                if temporary is not None:
                    with suppress(OSError):
                        Path(temporary).unlink(missing_ok=True)
            return self.status()

    def launch(self, kind: str, revision: str) -> dict[str, object]:
        if kind not in KINDS:
            raise LaunchError("仅支持 client/server")
        game: GameKind = "client" if kind == "client" else "server"
        with self._lock:
            paths, current, error, _ = self._read()
            if revision != current:
                raise LaunchError("启动路径已变化，请刷新后再启动")
            if error:
                raise LaunchError(error)
            child = self._children.get(game)
            if child is not None and child.poll() is None:
                raise LaunchError(f"本工作台启动的 {game} 仍在运行，PID {child.pid}")
            executable = self._executable(paths, game)
            if not executable.is_file():
                raise LaunchError(f"程序不存在：{executable}；请修改相对路径并保存")
            try:
                child = subprocess.Popen(
                    [str(executable)],
                    cwd=executable.parent,
                    shell=False,
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    close_fds=True,
                    creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
                )
            except OSError as launch_error:
                raise LaunchError(f"无法创建 {game} 进程：{launch_error}") from launch_error
            self._children[game] = child
            self._launched_paths[game] = str(executable)
            if (exit_code := child.poll()) is not None:
                raise LaunchError(f"{game} 已退出，退出码 {exit_code}；请检查游戏日志")
            return {
                "pid": child.pid,
                "message": f"进程已创建，PID {child.pid}",
            }
