"""Native-only game process management and relative-path preferences."""

from __future__ import annotations

import hashlib
import json
import math
import os
import subprocess
import tempfile
from contextlib import suppress
from datetime import datetime
from pathlib import Path, PurePosixPath, PureWindowsPath
from threading import RLock
from typing import Annotated, Literal
from uuid import uuid4

import psutil
from pydantic import Field, StrictInt, field_validator

from mir2x_workbench.gameplay_models import Model, load_json

type GameKind = Literal["client", "server"]
KINDS: tuple[GameKind, ...] = ("client", "server")
type ManagedProcess = subprocess.Popen[bytes] | psutil.Process
PSUTIL_PROCESS_TYPE = psutil.Process
SERVER_PORT = 7000
LOG_TAIL_BYTES = 64 * 1024
PROCESS_STATE_SCHEMA = 1


class LaunchError(Exception):
    pass


class LaunchPaths(Model):
    schema_version: Annotated[StrictInt, Field(ge=1, le=1)] = 1
    client_path: str = "../client/client.exe"
    server_path: str = "../server/server.exe"
    server_config_path: str = "../server"

    @field_validator("client_path", "server_path", "server_config_path")
    @classmethod
    def relative_path(cls, value: str) -> str:
        if not value.strip() or value != value.strip() or any(ord(c) < 32 for c in value):
            raise ValueError("请输入不含空白前后缀的相对路径")
        windows = PureWindowsPath(value)
        if windows.drive or windows.root or PurePosixPath(value).is_absolute():
            raise ValueError("请使用相对于工作台目录的路径，不要填写绝对路径")
        return value

    @field_validator("client_path", "server_path")
    @classmethod
    def executable(cls, value: str) -> str:
        if PurePosixPath(value.replace("\\", "/")).suffix.lower() != ".exe":
            raise ValueError("请选择 .exe 程序，不支持脚本或命令")
        return value


class ManagedProcessRecord(Model):
    pid: Annotated[StrictInt, Field(gt=0)]
    executable: str
    create_time: float = Field(gt=0, allow_inf_nan=False)


class ManagedProcessRegistry(Model):
    schema_version: Annotated[StrictInt, Field(ge=1, le=PROCESS_STATE_SCHEMA)] = (
        PROCESS_STATE_SCHEMA
    )
    processes: dict[GameKind, ManagedProcessRecord] = Field(default_factory=dict)


class GameLauncher:
    def __init__(self, application_root: Path):
        self.root = application_root.resolve()
        self.preferences = self.root / "game-launch.json"
        self.process_state = self.root / "game-processes.json"
        self._lock = RLock()
        self._children: dict[GameKind, subprocess.Popen[bytes]] = {}
        self._process_records: dict[GameKind, ManagedProcessRecord] = {}
        self._process_state_error: str | None = None
        self._launched_paths: dict[GameKind, str] = {}
        self._restore_processes()

    @staticmethod
    def _normalize_executable(path: str) -> str:
        return os.path.normcase(os.path.normpath(path))

    def _inspect_process(
        self, record: ManagedProcessRecord
    ) -> tuple[psutil.Process | None, str, str | None]:
        process: psutil.Process | None = None
        try:
            process = psutil.Process(record.pid)
            if not process.is_running():
                return process, "stale", None
            if not math.isclose(
                process.create_time(), record.create_time, rel_tol=0, abs_tol=0.001
            ):
                return process, "stale", None
            if self._normalize_executable(process.exe()) != self._normalize_executable(
                record.executable
            ):
                return process, "stale", None
            return process, "managed", None
        except (psutil.NoSuchProcess, psutil.ZombieProcess):
            return process, "stale", None
        except (psutil.AccessDenied, psutil.Error, OSError) as error:
            return process, "unknown", f"无法验证 PID {record.pid} 的进程身份：{error}"

    def _restore_processes(self) -> None:
        try:
            raw = self.process_state.read_bytes()
        except FileNotFoundError:
            return
        except OSError as error:
            self._process_state_error = f"无法读取进程记录 {self.process_state}：{error}"
            return
        try:
            registry = ManagedProcessRegistry.model_validate(load_json(raw))
        except ValueError as error:
            self._process_state_error = f"进程记录格式错误：{error}"
            return

        self._process_records = registry.processes
        stale: list[GameKind] = []
        for kind, record in self._process_records.items():
            _, ownership, _ = self._inspect_process(record)
            if ownership == "stale":
                stale.append(kind)
            else:
                self._launched_paths[kind] = record.executable
        for kind in stale:
            self._process_records.pop(kind)
        if stale:
            try:
                self._write_process_registry()
            except LaunchError as error:
                self._process_state_error = str(error)

    def _write_process_registry(self) -> None:
        content = (
            ManagedProcessRegistry(processes=self._process_records).model_dump_json(indent=2) + "\n"
        ).encode("utf-8")
        temporary: str | None = None
        try:
            descriptor, temporary = tempfile.mkstemp(prefix=".game-process-state-", dir=self.root)
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(content)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, self.process_state)
            temporary = None
            self._process_state_error = None
        except (OSError, ValueError) as error:
            raise LaunchError(f"无法保存进程记录 {self.process_state}：{error}") from error
        finally:
            if temporary is not None:
                with suppress(OSError):
                    Path(temporary).unlink(missing_ok=True)

    def _capture_process(self, pid: int) -> ManagedProcessRecord:
        try:
            process = psutil.Process(pid)
            return ManagedProcessRecord(
                pid=pid,
                executable=self._normalize_executable(process.exe()),
                create_time=process.create_time(),
            )
        except (psutil.Error, OSError) as error:
            raise LaunchError(f"无法记录 PID {pid} 的进程身份：{error}") from error

    @staticmethod
    def _is_running(process: ManagedProcess) -> bool:
        if not isinstance(process, PSUTIL_PROCESS_TYPE):
            return process.poll() is None
        try:
            return process.is_running()
        except (psutil.NoSuchProcess, psutil.ZombieProcess):
            return False
        except (psutil.Error, OSError):
            return True

    @classmethod
    def _stop_process(cls, process: ManagedProcess, kind: str) -> None:
        if not cls._is_running(process):
            return
        try:
            process.terminate()
            try:
                process.wait(timeout=5)
            except (subprocess.TimeoutExpired, psutil.TimeoutExpired):
                process.kill()
                process.wait(timeout=5)
        except (OSError, psutil.Error, subprocess.TimeoutExpired) as error:
            if cls._is_running(process):
                raise LaunchError(f"无法停止 {kind} 进程（PID {process.pid}）：{error}") from error

    def _clear_process_record(self, kind: GameKind) -> None:
        self._process_records.pop(kind, None)
        self._write_process_registry()

    def initialize(self, legacy_server_root: Path | None = None) -> None:
        """Upgrade local preferences once, preserving existing executable paths."""
        with self._lock:
            paths, revision, error, _ = self._read()
            if error:
                raise LaunchError(error)
            if "server_config_path" not in paths.model_fields_set:
                relative = paths.server_config_path
                if legacy_server_root is not None:
                    relative = os.path.relpath(legacy_server_root, self.root).replace("\\", "/")
                self.save(paths.client_path, paths.server_path, revision, relative)

    def server_config_root(self) -> Path:
        paths, _, error, _ = self._read()
        if error:
            raise LaunchError(error)
        return (self.root / paths.server_config_path.replace("\\", "/")).resolve()

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

    def log_tail(self, kind: str) -> str:
        if kind not in KINDS:
            raise LaunchError("仅支持 client/server")
        path = self.root / "logs" / f"game-{kind}.log"
        try:
            with path.open("rb") as stream:
                stream.seek(0, os.SEEK_END)
                stream.seek(max(0, stream.tell() - LOG_TAIL_BYTES))
                return stream.read().decode("utf-8", errors="replace")
        except FileNotFoundError:
            return ""
        except OSError as error:
            raise LaunchError(f"无法读取游戏日志：{error}") from error

    @staticmethod
    def _server_listening(process: ManagedProcess) -> bool | None:
        try:
            if isinstance(process, PSUTIL_PROCESS_TYPE):
                if not process.is_running():
                    return None
            elif process.poll() is not None:
                return None
            connections = psutil.Process(process.pid).net_connections(kind="tcp")
            listening = any(
                connection.status == psutil.CONN_LISTEN
                and connection.laddr
                and connection.laddr.port == SERVER_PORT
                for connection in connections
            )
            return listening if GameLauncher._is_running(process) else None
        except (psutil.Error, OSError):
            return None

    def status(self) -> dict[str, object]:
        with self._lock:
            paths, revision, error, _ = self._read()
            targets: dict[str, object] = {}
            state_changed = False
            for kind in KINDS:
                executable = self._executable(paths, kind)
                child = self._children.get(kind)
                record = self._process_records.get(kind)
                process: ManagedProcess | None = child
                running = False
                ownership: str | None = None
                ownership_warning: str | None = None
                exit_code = child.poll() if child is not None else None
                if child is not None and exit_code is None:
                    running = True
                    ownership = "managed"
                elif record is not None and child is None:
                    process, process_state, ownership_warning = self._inspect_process(record)
                    if process_state == "stale":
                        self._process_records.pop(kind)
                        state_changed = True
                        process = None
                        ownership_warning = None
                    elif process_state == "unknown":
                        ownership = "unknown"
                    else:
                        running = True
                        ownership = "managed"
                if child is not None and exit_code is not None and record is not None:
                    self._process_records.pop(kind)
                    state_changed = True
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
                    "running": running,
                    "pid": (
                        record.pid
                        if record is not None and ownership == "unknown"
                        else process.pid
                        if process is not None and running
                        else child.pid
                        if child is not None
                        else None
                    ),
                    "exit_code": exit_code,
                    "launched_executable": self._launched_paths.get(kind),
                    "warning": warning,
                    "ownership": ownership,
                    "ownership_warning": ownership_warning,
                    "listening": (
                        self._server_listening(process)
                        if kind == "server" and running and process is not None
                        else None
                    ),
                }
            if state_changed:
                try:
                    self._write_process_registry()
                except LaunchError as state_error:
                    self._process_state_error = str(state_error)
            return {
                "base_directory": str(self.root),
                "settings": paths.model_dump(),
                "revision": revision,
                "settings_error": error,
                "process_state_error": self._process_state_error,
                "targets": targets,
            }

    def save(
        self,
        client_path: str,
        server_path: str,
        revision: str,
        server_config_path: str | None = None,
    ) -> dict[str, object]:
        with self._lock:
            previous, current, error, old_raw = self._read()
            if revision != current:
                raise LaunchError("启动配置已被外部修改，请刷新后重新编辑")
            paths = LaunchPaths(
                client_path=client_path,
                server_path=server_path,
                server_config_path=(
                    previous.server_config_path
                    if server_config_path is None
                    else server_config_path
                ),
            )
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
            if self._process_state_error:
                raise LaunchError(self._process_state_error)
            paths, current, error, _ = self._read()
            if revision != current:
                raise LaunchError("启动路径已变化，请刷新后再启动")
            if error:
                raise LaunchError(error)
            child = self._children.get(game)
            if child is not None and child.poll() is None:
                raise LaunchError(f"本工作台启动的 {game} 仍在运行，PID {child.pid}")
            if child is not None:
                self._children.pop(game)
            record = self._process_records.get(game)
            if record is not None:
                _, ownership, warning = self._inspect_process(record)
                if ownership != "stale":
                    if ownership == "unknown":
                        raise LaunchError(warning or "无法验证进程身份")
                    raise LaunchError(f"本工作台启动的 {game} 仍在运行，PID {record.pid}")
                self._clear_process_record(game)
            executable = self._executable(paths, game)
            if not executable.is_file():
                raise LaunchError(f"程序不存在：{executable}；请修改相对路径并保存")
            try:
                log_dir = self.root / "logs"
                log_dir.mkdir(parents=True, exist_ok=True)
                with (log_dir / f"game-{game}.log").open("ab") as output:
                    output.write(
                        f"\n--- {datetime.now().isoformat(timespec='seconds')} ---\n".encode()
                    )
                    output.flush()
                    child = subprocess.Popen(
                        [str(executable), *(["--auto-launch"] if game == "server" else [])],
                        cwd=executable.parent,
                        shell=False,
                        stdin=subprocess.DEVNULL,
                        stdout=output,
                        stderr=subprocess.STDOUT,
                        close_fds=True,
                        creationflags=(
                            subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NO_WINDOW
                            if os.name == "nt"
                            else 0
                        ),
                    )
            except OSError as launch_error:
                raise LaunchError(f"无法创建 {game} 进程：{launch_error}") from launch_error
            self._children[game] = child
            self._launched_paths[game] = str(executable)
            if (exit_code := child.poll()) is not None:
                self._children.pop(game)
                raise LaunchError(f"{game} 已退出，退出码 {exit_code}；请检查游戏日志")
            try:
                record = self._capture_process(child.pid)
            except LaunchError as identity_error:
                try:
                    self._stop_process(child, game)
                    self._children.pop(game)
                except LaunchError as stop_error:
                    self._process_state_error = str(stop_error)
                    raise LaunchError(
                        f"无法持久记录 {game} 的进程身份，且无法停止 PID {child.pid}：{stop_error}"
                    ) from identity_error
                raise identity_error
            self._process_records[game] = record
            try:
                self._write_process_registry()
            except LaunchError as state_error:
                try:
                    self._stop_process(child, game)
                    self._children.pop(game)
                    self._process_records.pop(game)
                except LaunchError as stop_error:
                    self._process_state_error = str(stop_error)
                    raise LaunchError(
                        f"无法持久记录 {game}，且无法停止 PID {child.pid}：{stop_error}"
                    ) from state_error
                raise
            return {
                "pid": child.pid,
                "message": f"进程已创建，PID {child.pid}",
            }

    def stop(self, kind: str, pid: int) -> dict[str, object]:
        if kind not in KINDS:
            raise LaunchError("仅支持 client/server")
        game: GameKind = "client" if kind == "client" else "server"
        with self._lock:
            child = self._children.get(game)
            record = self._process_records.get(game)
            process: ManagedProcess | None = child
            if child is not None:
                if child.pid != pid:
                    raise LaunchError(f"{game} 进程已变化，请刷新后重试")
            elif record is not None:
                if record.pid != pid:
                    raise LaunchError(f"{game} 进程已变化，请刷新后重试")
                process, ownership, warning = self._inspect_process(record)
                if ownership == "stale":
                    self._clear_process_record(game)
                    raise LaunchError(f"{game} 进程已退出或身份已变化，请刷新状态")
                if ownership == "unknown":
                    raise LaunchError(warning or "无法验证进程身份")
            else:
                raise LaunchError(f"没有本工作台启动的 {game} 进程")
            if process is None:
                raise LaunchError(f"没有本工作台启动的 {game} 进程")
            self._stop_process(process, game)
            self._children.pop(game, None)
            self._process_records.pop(game, None)
            try:
                self._write_process_registry()
            except LaunchError as state_error:
                self._process_state_error = str(state_error)
            return {"pid": process.pid, "message": f"进程已停止，PID {process.pid}"}
