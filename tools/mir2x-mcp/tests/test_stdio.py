import asyncio
import json
import os
import sys
from importlib.resources import files

import pytest
from conftest import FIXTURES, OPERATIONS, arguments
from mcp.client.session import ClientSession
from mcp.client.stdio import StdioServerParameters, stdio_client

from mir2x_mcp.contract import METHODS, StrictValidator, input_schema, output_schema

KNOWLEDGE = {
    "mir2x://knowledge/leveling": "leveling.md",
    "mir2x://knowledge/leveling/maps": "leveling-maps.md",
    "mir2x://knowledge/leveling/equipment": "leveling-equipment.md",
    "mir2x://knowledge/leveling/skills": "leveling-skills.md",
}


@pytest.mark.parametrize("frozen", [False, True], ids=["source", "frozen"])
async def test_official_sdk_reads_offline_leveling_knowledge_without_bridge(tmp_path, frozen):
    executable = os.environ.get("MIR2X_FROZEN_EXE")
    if frozen and not executable:
        pytest.skip("MIR2X_FROZEN_EXE is not set")
    root = tmp_path / "offline knowledge root"
    root.mkdir()
    bridge = root / "nonexistent bridge"
    (root / "mir2x.local.toml").write_text(
        "[launcher]\nbridge_dir=" + json.dumps(str(bridge)) + "\n", "utf-8"
    )
    parameters = StdioServerParameters(
        command=executable if frozen else sys.executable,
        args=([] if frozen else ["-m", "mir2x_mcp.runtime_cli"])
        + [
            "--root",
            str(root),
            "mcp",
            "serve",
            "--instance-id",
            "f" * 32,
            "--no-reconnect",
        ],
        env={"PYTHONUTF8": "1", "PYTHONDONTWRITEBYTECODE": "1"},
    )
    assert not bridge.exists()
    async with stdio_client(parameters) as (read, write):
        async with ClientSession(read, write, read_timeout_seconds=10) as session:
            initialized = await session.initialize()
            assert "mir2x://knowledge/leveling" in initialized.instructions
            resources = {
                str(resource.uri): resource
                for resource in (await session.list_resources()).resources
            }
            for uri, filename in KNOWLEDGE.items():
                assert resources[uri].mime_type == "text/markdown"
                result = await session.read_resource(uri)
                assert len(result.contents) == 1
                content = result.contents[0]
                assert content.mime_type == "text/markdown"
                expected = (
                    files("mir2x_mcp").joinpath("runtime_assets", filename).read_text("utf-8")
                )
                assert content.text == expected
            index = (await session.read_resource("mir2x://knowledge/leveling")).contents[0].text
            for uri in KNOWLEDGE.keys() - {"mir2x://knowledge/leveling"}:
                assert uri in index
    assert not bridge.exists()


async def test_official_sdk_targeted_interruption_sends_exact_scoped_params_once(fake, tmp_path):
    executable = os.environ.get("MIR2X_FROZEN_EXE")
    root = tmp_path / "scoped SDK root"
    root.mkdir()
    (root / "mir2x.local.toml").write_text(
        "[launcher]\nbridge_dir=" + json.dumps(str(fake.directory)) + "\n", "utf-8"
    )
    parameters = StdioServerParameters(
        command=executable or sys.executable,
        args=([] if executable else ["-m", "mir2x_mcp.runtime_cli"])
        + [
            "--root",
            str(root),
            "mcp",
            "serve",
            "--instance-id",
            fake.instance_id,
            "--no-reconnect",
        ],
        env={"PYTHONUTF8": "1"},
    )
    async with stdio_client(parameters) as (read, write):
        async with ClientSession(read, write, read_timeout_seconds=10) as session:
            await session.initialize()
            args = {
                "interrupt_only": True,
                "operation_id": "owned-operation",
                "context": fake.context,
            }
            result = await session.call_tool("game_stop", args)
            assert not result.is_error
            calls = [row for row in fake.calls if row["method"] == "game_stop"]
            assert len(calls) == 1 and calls[0]["params"] == args
            invalid = await session.call_tool("game_stop", {"interrupt_only": True})
            assert invalid.is_error
            assert len([row for row in fake.calls if row["method"] == "game_stop"]) == 1
            assert fake.auths == 1


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
                    write_tool = METHODS[tool.name]["write"]
                    assert tool.annotations.read_only_hint is not write_tool
                    assert tool.annotations.destructive_hint is write_tool
                resources = (await session.list_resources()).resources
                assert {str(resource.uri) for resource in resources} == set(KNOWLEDGE) | {
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
                    if fixture["method"] == "game_grant_control":
                        assert not (await session.call_tool("game_get_session", {})).is_error
                for resource in resources:
                    result = await session.read_resource(resource.uri)
                    if resource.mime_type == "application/json":
                        payload = json.loads(result.contents[0].text)
                        assert payload["ok"]
                    else:
                        assert resource.mime_type == "text/markdown"
                        assert str(resource.uri) in KNOWLEDGE
                        assert result.contents[0].text.startswith("# ")
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
