import asyncio
import contextlib
import copy
import json
import os
import struct
import uuid
from pathlib import Path

import pytest

from mir2x_mcp.bridge_client import read_frame
from mir2x_mcp.contract import METHODS, dumps

ROOT = Path(__file__).resolve().parents[3]
FIXTURES = json.loads((ROOT / "contracts/ai-bridge/v1/fixtures.json").read_text("utf-8"))
OPERATIONS = json.loads(
    (ROOT / "contracts/ai-bridge/v1/operation-fixtures.json").read_text("utf-8")
)


def pytest_configure(config):
    if config.option.basetemp is None:
        config.option.basetemp = str(ROOT / ".runtime/mcp-python-tests")


def secure(path: Path, directory=False):
    if os.name != "nt":
        path.chmod(0o700 if directory else 0o600)
        return
    import win32api
    import win32con
    import win32security

    token = win32security.OpenProcessToken(win32api.GetCurrentProcess(), win32con.TOKEN_QUERY)
    try:
        sid = win32security.GetTokenInformation(token, win32security.TokenUser)[0]
        user = win32security.ConvertSidToStringSid(sid)
    finally:
        token.Close()
    inherit = "OICI" if directory else ""
    sd = win32security.ConvertStringSecurityDescriptorToSecurityDescriptor(
        f"O:{user}D:P(A;{inherit};FA;;;{user})(A;{inherit};FA;;;SY)", 1
    )
    win32security.SetFileSecurity(
        str(path),
        win32security.DACL_SECURITY_INFORMATION | win32security.PROTECTED_DACL_SECURITY_INFORMATION,
        sd,
    )


def arguments(fixture):
    request = fixture["request"]
    return copy.deepcopy(
        request["params"]
        | {
            key: value
            for key, value in request.items()
            if key in {"context", "request_id", "timeout_ms"}
        }
    )


class FakeBridge:
    def __init__(self, directory, instance_id="0" * 32):
        self.directory = directory
        self.instance_id = instance_id
        self.token = uuid.uuid4().hex + uuid.uuid4().hex
        self.context = {
            "instance_id": instance_id,
            "session_id": "0" * 32,
            "world_epoch": "0",
            "control_epoch": "0",
        }
        self.responses = {item["method"]: copy.deepcopy(item["success"]) for item in FIXTURES}
        self.calls = []
        self.clients = set()
        self.tasks = set()
        self.auths = 0
        self.disconnected = asyncio.Event()
        self.received = asyncio.Event()
        self.heartbeat = asyncio.Event()
        self.drop_method = None
        self.hold_method = None
        self.raw_reply = None
        self.fragment = False
        self.protocol_version = 1
        self.character_uid = "1"

    async def start(self):
        self.directory.mkdir(parents=True, exist_ok=True)
        secure(self.directory, True)
        self.server = await asyncio.start_server(self.handle, "127.0.0.1", 0)
        self.port = self.server.sockets[0].getsockname()[1]
        self.path = self.directory / f"{self.instance_id}.json"
        self.record = {
            "v": 1,
            "instance_id": self.instance_id,
            "token": self.token,
            "pid": str(os.getpid()),
            "started_at": "0",
            "port": self.port,
            "build": "fake-test",
        }
        self.path.write_bytes(dumps(self.record))
        secure(self.path)
        return self

    async def close(self):
        self.server.close()
        await self.server.wait_closed()
        for writer in list(self.clients):
            writer.close()
        for task in list(self.tasks):
            task.cancel()
        await asyncio.gather(*self.tasks, return_exceptions=True)

    async def handle(self, reader, writer):
        self.clients.add(writer)
        self.tasks.add(asyncio.current_task())
        try:
            hello = await read_frame(reader)
            if hello["params"] != {"instance_id": self.instance_id, "token": self.token}:
                return
            self.auths += 1
            await self.send(
                writer,
                {
                    "v": 1,
                    "id": hello["id"],
                    "ok": True,
                    "data": {
                        "instance_id": self.instance_id,
                        "protocol_version": self.protocol_version,
                    },
                    "context": None,
                },
            )
            while True:
                request = await read_frame(reader)
                self.calls.append(request)
                method = request["method"]
                self.received.set()
                if method == self.drop_method:
                    return
                if method == self.hold_method:
                    continue
                if self.raw_reply is not None:
                    writer.write(self.raw_reply)
                    await writer.drain()
                    continue
                if method == "heartbeat":
                    self.heartbeat.set()
                    response = {"v": 1, "ok": True, "data": {"alive": True}, "context": None}
                else:
                    response = copy.deepcopy(self.responses[method])
                    if method == "game_get_session" and response["ok"]:
                        response["data"].update(
                            {
                                "instance_id": self.instance_id,
                                "phase": "run",
                                "control_state": "paused",
                                "context": self.context,
                                "character_uid": self.character_uid,
                            }
                        )
                        response["data"]["capabilities"]["tools"] = list(METHODS)
                        response["context"] = self.context
                response["id"] = request["id"]
                await self.send(writer, response)
        except (OSError, asyncio.IncompleteReadError):
            pass
        finally:
            writer.close()
            with contextlib.suppress(OSError):
                await writer.wait_closed()
            self.clients.discard(writer)
            self.tasks.discard(asyncio.current_task())
            self.disconnected.set()

    async def send(self, writer, response):
        data = dumps(response)
        packet = struct.pack("!I", len(data)) + data
        if self.fragment:
            for start in range(0, len(packet), 7):
                writer.write(packet[start : start + 7])
                await writer.drain()
                await asyncio.sleep(0)
        else:
            writer.write(packet)
            await writer.drain()


@pytest.fixture
async def fake(tmp_path):
    bridge = await FakeBridge(tmp_path / "bridge").start()
    try:
        yield bridge
    finally:
        await bridge.close()
