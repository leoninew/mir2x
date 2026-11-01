import asyncio
import json
import os
import sys

import pytest
from conftest import FIXTURES, OPERATIONS, arguments
from mcp.client.session import ClientSession
from mcp.client.stdio import StdioServerParameters, stdio_client

from mir2x_mcp.contract import METHODS, StrictValidator, input_schema, output_schema


async def test_real_sdk_stdio_all_tools_resources_and_eof(fake, tmp_path):
    parameters = StdioServerParameters(
        command=sys.executable,
        args=["-m", "mir2x_mcp", "--bridge-dir", str(fake.directory)],
        env={"PYTHONUTF8": "1", "PYTHONDONTWRITEBYTECODE": "1"},
    )
    with (tmp_path / "stdio-stderr.log").open("w", encoding="utf-8") as errors:
        async with stdio_client(parameters, errlog=errors) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=10) as session:
                initialized = await session.initialize()
                assert initialized.server_info.name == "mir2x"
                tools = (await session.list_tools()).tools
                assert {tool.name for tool in tools} == set(METHODS)
                for tool in tools:
                    assert tool.input_schema == input_schema(tool.name)
                    assert tool.output_schema == output_schema(tool.name)
                    write_tool = METHODS[tool.name]["action"] or tool.name == "game_stop"
                    assert tool.annotations.read_only_hint is not write_tool
                    assert tool.annotations.destructive_hint is write_tool
                resources = (await session.list_resources()).resources
                assert {str(resource.uri) for resource in resources} == {
                    f"mir2x://{name}"
                    for name in (
                        "session",
                        "character",
                        "nearby",
                        "inventory",
                        "interaction",
                        "capabilities",
                    )
                }
                for fixture in FIXTURES:
                    result = await session.call_tool(fixture["method"], arguments(fixture))
                    assert not result.is_error, (fixture["method"], result)
                    StrictValidator(output_schema(fixture["method"])).validate(
                        result.structured_content
                    )
                    invalid = await session.call_tool(
                        fixture["method"], arguments(fixture) | {"extra": "rejected"}
                    )
                    assert invalid.is_error
                    assert invalid.structured_content["error"]["code"] == "invalid_argument"
                for resource in resources:
                    result = await session.read_resource(resource.uri)
                    payload = json.loads(result.contents[0].text)
                    assert payload["ok"]
                for literal in ("null", "[]", "{}"):
                    result = await session.call_tool("game_get_inventory", {"cursor": literal})
                    assert not result.is_error
                    assert fake.calls[-1]["params"]["cursor"] == literal
                    cast_args = arguments(next(f for f in FIXTURES if f["method"] == "game_cast"))
                    cast_args.pop("x", None)
                    cast_args.pop("y", None)
                    result = await session.call_tool(
                        "game_cast", cast_args | {"target_handle": literal}
                    )
                    assert not result.is_error
                    assert fake.calls[-1]["params"]["target_handle"] == literal
                for fixture in FIXTURES:
                    fake.responses[fixture["method"]] = fixture["failure"]
                    result = await session.call_tool(fixture["method"], arguments(fixture))
                    assert result.is_error
                    assert result.structured_content["error"]["code"] == "not_in_game"
                    fake.responses[fixture["method"]] = fixture["success"]
                invalid = arguments(next(f for f in FIXTURES if f["method"] == "game_move_to"))
                for value in (True, 1.0, -1, "3"):
                    result = await session.call_tool("game_move_to", invalid | {"x": value})
                    assert result.is_error
                    assert result.structured_content["error"]["code"] == "invalid_argument"
                result = await session.call_tool("game_get_session", {"unknown": "rejected"})
                assert result.is_error
                for operation in OPERATIONS:
                    fake.responses["game_get_operation"]["data"] = operation
                    result = await session.call_tool("game_get_operation", {"operation_id": "op"})
                    assert not result.is_error
                    assert result.structured_content["data"]["state"] == operation["state"]
        await asyncio.wait_for(fake.disconnected.wait(), 3)
    stderr = (tmp_path / "stdio-stderr.log").read_text("utf-8")
    assert fake.token not in stderr
    assert "Traceback" not in stderr
    assert fake.auths == 1


async def test_sdk_cancellation_pauses_inflight_action(fake, tmp_path):
    parameters = StdioServerParameters(
        command=sys.executable,
        args=["-m", "mir2x_mcp", "--bridge-dir", str(fake.directory)],
        env={"PYTHONUTF8": "1"},
    )
    with (tmp_path / "cancel-stderr.log").open("w", encoding="utf-8") as errors:
        async with stdio_client(parameters, errlog=errors) as (read, write):
            async with ClientSession(read, write) as session:
                await session.initialize()
                await session.call_tool("game_get_session", {})
                fake.hold_method = "game_move_to"
                fake.received.clear()
                args = arguments(next(f for f in FIXTURES if f["method"] == "game_move_to"))
                task = asyncio.create_task(session.call_tool("game_move_to", args))
                await asyncio.wait_for(fake.received.wait(), 2)
                task.cancel()
                with pytest.raises(asyncio.CancelledError):
                    await task
                await asyncio.wait_for(fake.disconnected.wait(), 2)
    assert sum(call["method"] == "game_move_to" for call in fake.calls) == 1


async def test_stdio_eof_exits_without_forced_kill(fake, tmp_path):
    with (tmp_path / "eof-stderr.log").open("wb") as errors:
        process = await asyncio.create_subprocess_exec(
            sys.executable,
            "-m",
            "mir2x_mcp",
            "--bridge-dir",
            str(fake.directory),
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=errors,
            creationflags=0x08000000 if os.name == "nt" else 0,
        )
        try:

            async def send(message):
                process.stdin.write(json.dumps(message).encode() + b"\n")
                await process.stdin.drain()

            async def receive(request_id):
                async with asyncio.timeout(3):
                    while True:
                        line = await process.stdout.readline()
                        assert line, "server exited before response"
                        message = json.loads(line)
                        if message.get("id") == request_id:
                            return message

            await send(
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "initialize",
                    "params": {
                        "protocolVersion": "2025-11-25",
                        "capabilities": {},
                        "clientInfo": {"name": "eof-test", "version": "1"},
                    },
                }
            )
            assert "result" in await receive(1)
            await send({"jsonrpc": "2.0", "method": "notifications/initialized"})
            await send(
                {
                    "jsonrpc": "2.0",
                    "id": 2,
                    "method": "tools/call",
                    "params": {"name": "game_get_session", "arguments": {}},
                }
            )
            assert not (await receive(2))["result"].get("isError", False)
            process.stdin.close()
            assert await asyncio.wait_for(process.wait(), 3) == 0
            await asyncio.wait_for(fake.disconnected.wait(), 1)
        finally:
            if process.returncode is None:
                process.kill()
                await process.wait()
