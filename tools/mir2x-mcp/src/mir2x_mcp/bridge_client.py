import asyncio
import contextlib
import struct
import uuid

from mcp_types import CallToolResult, TextContent
from pydantic import BaseModel

from .config import Config
from .contract import METHODS, ContractError, dumps, loads, validate, validate_arguments
from .errors import BridgeError
from .instances import read_instances

REQUEST_LIMIT = 65536
RESPONSE_LIMIT = 1048576
IO_TIMEOUT = 3.0
HEARTBEAT_INTERVAL = 2.0


async def read_frame(reader: asyncio.StreamReader) -> dict:
    header = await reader.readexactly(4)
    size = struct.unpack("!I", header)[0]
    if not 0 < size <= RESPONSE_LIMIT:
        raise ContractError("frame_size")
    value = loads(await reader.readexactly(size))
    if not isinstance(value, dict):
        raise ContractError("frame_object")
    return value


def frame(message: dict) -> bytes:
    data = dumps(message)
    if not 0 < len(data) <= REQUEST_LIMIT:
        raise ContractError("request_size")
    return struct.pack("!I", len(data)) + data


class Link:
    def __init__(self, reader, writer):
        self.reader = reader
        self.writer = writer
        self.pending = {}
        self.write_lock = asyncio.Lock()
        self.closed = False
        self.reader_task = None
        self.heartbeat_task = None

    def start(self):
        self.reader_task = asyncio.create_task(self._read_loop())
        self.heartbeat_task = asyncio.create_task(self._heartbeat())

    def abort(self):
        if self.closed:
            return
        self.closed = True
        self.writer.close()
        for future, _method in self.pending.values():
            if not future.done():
                future.set_exception(BridgeError("transport_lost", "桥接连接已断开"))
        self.pending.clear()
        for task in (self.reader_task, self.heartbeat_task):
            if task is not None and task is not asyncio.current_task():
                task.cancel()

    async def close(self):
        self.abort()
        tasks = [
            task
            for task in (self.reader_task, self.heartbeat_task)
            if task is not None and task is not asyncio.current_task()
        ]
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)
        with contextlib.suppress(OSError, TimeoutError):
            async with asyncio.timeout(1):
                await self.writer.wait_closed()

    async def _read_loop(self):
        try:
            while not self.closed:
                async with asyncio.timeout(6):
                    response = await read_frame(self.reader)
                request_id = response.get("id")
                if not isinstance(request_id, str) or request_id not in self.pending:
                    raise ContractError("unexpected_response")
                future, method = self.pending[request_id]
                validate(response, method + "Response")
                del self.pending[request_id]
                if not future.done():
                    future.set_result(response)
        except (OSError, EOFError, asyncio.IncompleteReadError, TimeoutError, ContractError):
            self.abort()

    async def _heartbeat(self):
        try:
            while not self.closed:
                await asyncio.sleep(HEARTBEAT_INTERVAL)
                await self.exchange(
                    {"v": 1, "id": uuid.uuid4().hex, "method": "heartbeat", "params": {}}
                )
        except BridgeError:
            self.abort()

    async def exchange(self, request: dict) -> dict:
        if self.closed:
            raise BridgeError("transport_lost", "桥接连接已断开")
        if len(self.pending) >= 32 and request["method"] != "game_stop":
            raise BridgeError("busy", "待响应请求过多")
        data = frame(request)
        future = asyncio.get_running_loop().create_future()
        request_id = request["id"]
        sent = False
        self.pending[request_id] = (future, request["method"])
        try:
            async with asyncio.timeout(IO_TIMEOUT):
                async with self.write_lock:
                    if self.closed:
                        raise BridgeError("transport_lost", "桥接连接已断开")
                    sent = True
                    self.writer.write(data)
                    await self.writer.drain()
                return await future
        except asyncio.CancelledError:
            # Disconnect makes C++ pause even if acceptance was sent but its ID was lost.
            self.abort()
            raise
        except (OSError, TimeoutError, BridgeError):
            self.abort()
            action = METHODS.get(request["method"], {}).get("write", False)
            raise BridgeError(
                "transport_lost",
                "桥接响应丢失；动作结果未知，禁止自动重发" if action and sent else "桥接连接不可用",
                side_effects_possible=action and sent,
            ) from None
        finally:
            self.pending.pop(request_id, None)
            if future.done() and not future.cancelled():
                future.exception()
            elif not future.done():
                future.cancel()


class BridgeClient:
    def __init__(self, config: Config):
        self.config = config
        self.instance_id = config.instance_id
        self.link = None
        self.connect_lock = asyncio.Lock()
        self.action_lock = asyncio.Lock()
        self.last_context = None
        self.character_uid = None
        self.current_character_uid = None
        self.closed = False
        self.connection_attempted = False

    async def _authenticate(self, record):
        link = None
        try:
            async with asyncio.timeout(IO_TIMEOUT):
                reader, writer = await asyncio.open_connection(
                    "127.0.0.1", record["port"], limit=65536
                )
                link = Link(reader, writer)
                request_id = uuid.uuid4().hex
                writer.write(
                    frame(
                        {
                            "v": 1,
                            "id": request_id,
                            "method": "hello",
                            "params": {
                                "instance_id": record["instance_id"],
                                "token": record["token"],
                            },
                        }
                    )
                )
                await writer.drain()
                response = await read_frame(reader)
                validate(response, "helloResponse")
                if (
                    response["id"] != request_id
                    or response["data"]["instance_id"] != record["instance_id"]
                ):
                    raise ContractError("instance_mismatch")
                link.start()
                return link
        except BaseException:
            if link:
                await link.close()
            raise

    async def connect(self):
        async with self.connect_lock:
            if self.closed:
                raise BridgeError("unavailable", "MCP已关闭")
            if self.link and not self.link.closed:
                return self.link
            if self.config.no_reconnect and self.connection_attempted:
                raise BridgeError("transport_lost", "监督连接已锁闭；不会自动重新认证")
            self.connection_attempted = True
            if self.link:
                await self.link.close()
                self.link = None
            self.last_context = None
            records = await asyncio.to_thread(read_instances, self.config, self.instance_id)
            candidates = []
            uncertain = 0
            try:
                for record in records:
                    try:
                        link = await self._authenticate(record)
                        candidates.append((record["instance_id"], link))
                    except ConnectionRefusedError:
                        pass
                    except (OSError, TimeoutError, asyncio.IncompleteReadError, ContractError):
                        uncertain += 1
                if len(candidates) + uncertain > 1:
                    raise BridgeError(
                        "ambiguous_instance", "存在多个可能存活的实例，请配置明确的实例ID"
                    )
                if uncertain or not candidates:
                    raise BridgeError(
                        "unavailable", "没有可认证实例；请检查客户端桥接、版本及占用连接"
                    )
                self.instance_id, self.link = candidates.pop()
                return self.link
            finally:
                for _, link in candidates:
                    link.abort()
                if candidates:
                    await asyncio.gather(*(link.close() for _, link in candidates))

    async def close(self):
        self.closed = True
        if self.link:
            await self.link.close()

    async def call(self, method: str, arguments: dict) -> dict:
        validate_arguments(method, arguments)
        action = METHODS[method]["action"] or method == "game_grant_control"
        if action and self.action_lock.locked():
            raise BridgeError("busy", "另一个动作正在等待桥接接收")
        if action:
            async with self.action_lock:
                if not self.link or self.link.closed or self.last_context is None:
                    raise BridgeError("paused", "请先在已有绑定连接读取会话；授权写入不自动连接")
                if (
                    arguments["context"] != self.last_context
                    or self.character_uid != self.current_character_uid
                ):
                    raise BridgeError(
                        "stale_context", "上下文或绑定角色已变化，请读取会话并检查游戏窗口"
                    )
                result = await self._call(self.link, method, arguments)
                if method == "game_grant_control" and result["ok"]:
                    self.last_context = None
                return result
        return await self._call(await self.connect(), method, arguments)

    async def _call(self, link: Link, method: str, arguments: dict) -> dict:
        params = dict(arguments)
        request = {"v": 1, "id": uuid.uuid4().hex, "method": method, "params": params}
        if METHODS[method]["action"]:
            for field in ("request_id", "context", "timeout_ms"):
                request[field] = params.pop(field)
        validate(request, method + "Request")
        response = await link.exchange(request)
        if method == "game_get_session" and response["ok"]:
            session = response["data"]
            if session["instance_id"] != self.instance_id:
                link.abort()
                raise BridgeError("transport_lost", "实例身份不一致")
            self.last_context = session["context"]
            self.current_character_uid = session["character_uid"]
            if self.character_uid is None:
                self.character_uid = self.current_character_uid
        return response

    async def tool(self, method: str, arguments: dict) -> CallToolResult:
        def plain(value):
            if isinstance(value, BaseModel):
                return value.model_dump()
            if isinstance(value, list):
                return [plain(item) for item in value]
            return value

        try:
            response = await self.call(
                method, {k: plain(v) for k, v in arguments.items() if v is not None}
            )
            return tool_result(response, is_error=not response["ok"])
        except BridgeError as exc:
            return tool_result(exc.payload(), is_error=True)
        except ContractError:
            return tool_result(
                BridgeError("invalid_argument", "参数不符合桥接契约").payload(), is_error=True
            )


def tool_result(payload, *, is_error=False):
    return CallToolResult(
        content=[TextContent(type="text", text=dumps(payload).decode("utf-8"))],
        structured_content=payload,
        is_error=is_error,
    )
