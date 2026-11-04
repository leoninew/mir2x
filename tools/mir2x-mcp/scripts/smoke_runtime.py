"""Check the actual packaged stdio entry with the official MCP client SDK."""

import argparse
import asyncio
import json
from pathlib import Path

from mcp.client.session import ClientSession
from mcp.client.stdio import StdioServerParameters, stdio_client


async def smoke(executable: Path, root: Path):
    parameters = StdioServerParameters(
        command=str(executable),
        args=[
            "--root",
            str(root),
            "mcp",
            "serve",
        ],
        env={"PYTHONUTF8": "1"},
    )
    async with stdio_client(parameters) as (read, write):
        async with ClientSession(read, write, read_timeout_seconds=30) as session:
            initialized = await session.initialize()
            tools = (await session.list_tools()).tools
            resources = (await session.list_resources()).resources
            assert initialized.server_info.name == "mir2x"
            assert len(tools) == 37 and len({tool.name for tool in tools}) == 37
            assert len(resources) == 6
            result = await session.call_tool("game_get_session", {})
            # This check deliberately uses an empty, isolated bridge directory.
            assert result.is_error and result.structured_content["error"]["code"] == "unavailable"
    print(
        json.dumps(
            {
                "initialize": "mir2x",
                "tools": len(tools),
                "resources": len(resources),
                "empty_bridge": "unavailable",
                "stdio_eof": "normal",
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    arguments = parser.parse_args()
    asyncio.run(smoke(arguments.executable.resolve(), arguments.root.resolve()))
