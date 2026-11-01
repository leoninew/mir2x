"""Manage only processes started and identified by this distribution."""

import contextlib
import ctypes
import hashlib
import json
import os
import signal
import socket
import subprocess
import time
from pathlib import Path

import psutil

from .runtime_config import RuntimeFailure, Settings


@contextlib.contextmanager
def management_lock(directory: Path):
    directory.mkdir(parents=True, exist_ok=True)
    with (directory / "manager.lock").open("a+b") as stream:
        if os.fstat(stream.fileno()).st_size == 0:
            stream.write(b"0")
            stream.flush()
        stream.seek(0)
        try:
            if os.name == "nt":
                import msvcrt

                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl

                fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as exc:
            raise RuntimeFailure("另一条管理命令正在运行，请稍后重试") from exc
        try:
            yield
        finally:
            if os.name == "nt":
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(stream, fcntl.LOCK_UN)


def identity(process: psutil.Process) -> dict:
    return {
        "pid": process.pid,
        "created": process.create_time(),
        "executable": str(Path(process.exe()).resolve()),
    }


def owned_process(record: dict) -> psutil.Process | None:
    try:
        process = psutil.Process(record["pid"])
        actual = identity(process)
        if (
            actual["created"] == record["created"]
            and os.path.normcase(actual["executable"]) == os.path.normcase(record["executable"])
            and process.status() != psutil.STATUS_ZOMBIE
        ):
            return process
    except (KeyError, TypeError, ValueError, psutil.NoSuchProcess):
        return None
    except psutil.AccessDenied as exc:
        raise RuntimeFailure("无法核对进程身份，未执行停止或替换") from exc
    return None


def port_open(host: str, port: int) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.2):
            return True
    except OSError:
        return False


def _free_port(port: int):
    try:
        with socket.socket() as listener:
            if os.name == "nt":
                listener.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            listener.bind(("0.0.0.0", port))
    except OSError as exc:
        raise RuntimeFailure(f"端口 {port} 已被占用；不会停止外部进程") from exc


def _close_windows(pid: int):
    from ctypes import wintypes

    user32 = ctypes.WinDLL("user32", use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user32.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    sent = 0

    @callback_type
    def visit(window, _):
        nonlocal sent
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(window, ctypes.byref(owner))
        if owner.value == pid:
            sent += bool(user32.PostMessageW(window, 0x0010, 0, 0))
        return True

    user32.EnumWindows(visit, 0)
    return sent


class Manager:
    def __init__(self, settings: Settings):
        self.settings = settings

    def record_path(self, role: str) -> Path:
        if role not in ("server", "client"):
            raise RuntimeFailure("未知进程类别")
        return self.settings.state / f"{role}.json"

    def read(self, role: str) -> dict | None:
        path = self.record_path(role)
        if not path.is_file():
            return None
        try:
            record = json.loads(path.read_text("utf-8"))
            if (
                not isinstance(record, dict)
                or type(record.get("pid")) is not int
                or not isinstance(record.get("created"), (int, float))
                or not isinstance(record.get("executable"), str)
                or not isinstance(record.get("workdir"), str)
            ):
                raise ValueError
            return record
        except (ValueError, OSError) as exc:
            raise RuntimeFailure(f"{role} 管理记录损坏；未执行进程操作") from exc

    def status(self, role: str) -> dict:
        record = self.read(role)
        process = owned_process(record) if record else None
        result = {"role": role, "state": "running" if process else "stopped"}
        if process:
            result.update(pid=process.pid, workdir=record["workdir"], policy=record.get("policy"))
        elif record:
            result["stale_record"] = True
        return result

    def command(self, role: str, policy: str | None = None) -> list[str]:
        settings = self.settings
        program = getattr(settings, role)
        args = [str(program.executable), *program.args]
        if role == "server":
            args += [
                "--auto-launch",
                f"--client-port={settings.client_port}",
                f"--peer-port={settings.peer_port}",
            ]
        else:
            args += [f"--server-ip={settings.host}", f"--server-port={settings.client_port}"]
            if settings.bridge_enabled:
                args += [
                    "--enable-ai-bridge",
                    f"--ai-bridge-dir={settings.bridge}",
                    f"--ai-policy={self.policy_copy(policy)[0]}",
                ]
        return args

    def policy_copy(self, policy: str | None) -> tuple[Path, bytes]:
        content = self.settings.policy_path(policy).read_bytes()
        name = hashlib.sha256(content).hexdigest()[:24] + ".json"
        return self.settings.state / "policies" / name, content

    def start(self, role: str, policy: str | None = None) -> dict:
        current = self.status(role)
        if current["state"] == "running":
            current["already_running"] = True
            if role == "client" and policy and policy != current.get("policy"):
                raise RuntimeFailure("客户端已运行；更换策略请使用 client restart --policy")
            return current
        program = getattr(self.settings, role)
        if not program.executable.is_file() or not program.workdir.is_dir():
            raise RuntimeFailure(f"{role} 程序或工作目录不存在，请先构建并安装游戏")
        if role == "server":
            _free_port(self.settings.client_port)
            _free_port(self.settings.peer_port)
        elif self.settings.bridge_enabled:
            # Frozen resources vanish when the CLI exits; the client needs a durable policy.
            path, content = self.policy_copy(policy)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        logdir = self.settings.state / "logs"
        logdir.mkdir(parents=True, exist_ok=True)
        options = {}
        if os.name == "nt":
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0 if role == "server" else 1
            options.update(startupinfo=startup, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
        else:
            options["start_new_session"] = True
        logfile = logdir / f"{role}.log"
        if logfile.is_file() and logfile.stat().st_size > 5 * 1024 * 1024:
            logfile.replace(logfile.with_suffix(".previous.log"))
        with logfile.open("ab") as output:
            child = subprocess.Popen(
                self.command(role, policy),
                cwd=program.workdir,
                stdin=subprocess.DEVNULL,
                stdout=output,
                stderr=output,
                **options,
            )
        try:
            record = identity(psutil.Process(child.pid))
        except psutil.NoSuchProcess as exc:
            raise RuntimeFailure(f"{role} 启动后立即退出，查看 logs {role}") from exc
        record.update(workdir=str(program.workdir), policy=policy or self.settings.policy)
        path = self.record_path(role)
        temporary = path.with_suffix(".tmp")
        temporary.write_text(json.dumps(record), "utf-8")
        temporary.replace(path)
        # Popen is deliberately detached from the CLI lifetime; keep no inherited pipes open.
        if role == "server":
            self.wait_server()
        else:
            time.sleep(0.15)
            if child.poll() is not None:
                raise RuntimeFailure("client 启动后立即退出，查看 logs client")
        return self.status(role)

    def wait_server(self):
        deadline = time.monotonic() + self.settings.start_timeout
        while time.monotonic() < deadline:
            if self.status("server")["state"] != "running":
                raise RuntimeFailure("server 已退出，查看 logs server 和游戏日志")
            process = owned_process(self.read("server"))
            if process and any(
                entry.status == psutil.CONN_LISTEN and entry.laddr.port == self.settings.client_port
                for entry in process.net_connections(kind="tcp")
            ):
                return
            time.sleep(0.1)
        raise RuntimeFailure("服务器尚未就绪；进程保留可诊断，查看 status 或 logs server")

    def stop(self, role: str, force: bool = False) -> dict:
        record = self.read(role)
        process = owned_process(record) if record else None
        if not process:
            self.record_path(role).unlink(missing_ok=True)
            return {"role": role, "state": "stopped", "already_stopped": True}
        forced = False
        try:
            if os.name == "nt":
                deadline = time.monotonic() + self.settings.stop_timeout
                while owned_process(record) and not _close_windows(process.pid):
                    if time.monotonic() >= deadline:
                        break
                    time.sleep(0.05)
                remaining = max(0, deadline - time.monotonic())
            else:
                process.send_signal(signal.SIGTERM)
                remaining = self.settings.stop_timeout
            try:
                process.wait(remaining)
            except psutil.TimeoutExpired as exc:
                if not force:
                    raise RuntimeFailure(
                        "正常关闭未完成；处理窗口提示后重试，或显式 --force"
                    ) from exc
                # Recheck the exact identity before a destructive fallback.
                process = owned_process(record)
                if process:
                    process.kill()
                    process.wait(self.settings.stop_timeout)
                    forced = True
        except psutil.NoSuchProcess:
            pass
        self.record_path(role).unlink(missing_ok=True)
        return {"role": role, "state": "stopped", "forced": forced}
