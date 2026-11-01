import asyncio
import copy
import os
import struct

import pytest
from conftest import FIXTURES, FakeBridge, arguments, secure

from mir2x_mcp import bridge_client
from mir2x_mcp.bridge_client import BridgeClient
from mir2x_mcp.config import Config
from mir2x_mcp.errors import BridgeError
from mir2x_mcp.instances import read_instances


@pytest.fixture
async def client(fake):
    bridge = BridgeClient(Config(fake.directory))
    try:
        yield bridge
    finally:
        await bridge.close()


def move_args():
    return arguments(next(f for f in FIXTURES if f["method"] == "game_move_to"))


async def test_no_instance_and_invalid_config(tmp_path):
    with pytest.raises(ValueError):
        Config(tmp_path, "../../arbitrary")
    client = BridgeClient(Config(tmp_path / "missing"))
    with pytest.raises(BridgeError, match="unavailable"):
        await client.call("game_get_session", {})
    await client.close()


async def test_fragmentation_and_outstanding_queries(fake, client):
    fake.fragment = True
    result = await client.call("game_get_session", {})
    assert result["data"]["instance_id"] == fake.instance_id
    results = await asyncio.gather(*(client.call("game_get_state", {}) for _ in range(12)))
    assert all(result["ok"] for result in results)
    assert len({result["id"] for result in results}) == 12
    assert fake.auths == 1


async def test_heartbeat_and_eof(fake, client, monkeypatch):
    monkeypatch.setattr(bridge_client, "HEARTBEAT_INTERVAL", 0.02)
    await client.call("game_get_session", {})
    await asyncio.wait_for(fake.heartbeat.wait(), 2)
    await client.close()
    await asyncio.wait_for(fake.disconnected.wait(), 1)


async def test_multiple_instances_require_explicit_selection(fake):
    other = await FakeBridge(fake.directory, "1" * 32).start()
    client = BridgeClient(Config(fake.directory))
    try:
        with pytest.raises(BridgeError, match="ambiguous_instance"):
            await client.call("game_get_session", {})
        assert client.instance_id is None
        await client.close()
        selected = BridgeClient(Config(fake.directory, other.instance_id))
        try:
            result = await selected.call("game_get_session", {})
            assert result["data"]["instance_id"] == other.instance_id
        finally:
            await selected.close()
    finally:
        await client.close()
        await other.close()


async def test_pinned_instance_never_switches(fake, client):
    await client.call("game_get_session", {})
    await client.link.close()
    await fake.close()
    other = await FakeBridge(fake.directory, "1" * 32).start()
    try:
        with pytest.raises(BridgeError, match="unavailable"):
            await client.call("game_get_session", {})
        assert other.auths == 0
        assert client.instance_id == fake.instance_id
    finally:
        await other.close()


async def test_bad_token_and_no_secret_in_error(fake, client):
    fake.token = "f" * 64
    with pytest.raises(BridgeError) as error:
        await client.call("game_get_session", {})
    assert error.value.code == "unavailable"
    assert fake.record["token"] not in str(error.value.payload())


async def test_wrong_protocol_not_treated_as_live_session(fake, client):
    fake.protocol_version = 2
    with pytest.raises(BridgeError, match="unavailable"):
        await client.call("game_get_session", {})
    assert client.last_context is None


async def test_stale_credentials_do_not_hide_unique_live_instance(fake):
    stale = await FakeBridge(fake.directory, "1" * 32).start()
    await stale.close()
    client = BridgeClient(Config(fake.directory))
    try:
        assert (await client.call("game_get_session", {}))["data"][
            "instance_id"
        ] == fake.instance_id
    finally:
        await client.close()


async def test_action_requires_session_and_role_remains_bound(fake, client):
    with pytest.raises(BridgeError, match="paused"):
        await client.call("game_move_to", move_args())
    await client.call("game_get_session", {})
    assert (await client.call("game_move_to", move_args()))["ok"]
    fake.character_uid = "2"
    await client.call("game_get_session", {})
    with pytest.raises(BridgeError, match="stale_context"):
        await client.call("game_move_to", move_args())


async def test_lost_write_response_unknown_without_replay(fake, client):
    await client.call("game_get_session", {})
    fake.drop_method = "game_move_to"
    result = await client.tool("game_move_to", move_args())
    assert result.is_error
    error = result.structured_content["error"]
    assert error["code"] == "transport_lost"
    assert error["details"] == {"retryable": False, "side_effects_possible": True}
    assert sum(call["method"] == "game_move_to" for call in fake.calls) == 1
    with pytest.raises(BridgeError, match="paused"):
        await client.call("game_move_to", move_args())
    fake.context["control_epoch"] = "1"
    await client.call("game_get_session", {})
    with pytest.raises(BridgeError, match="stale_context"):
        await client.call("game_move_to", move_args())
    assert sum(call["method"] == "game_move_to" for call in fake.calls) == 1


async def test_cancel_inflight_action_disconnects_and_busy(fake, client):
    await client.call("game_get_session", {})
    fake.hold_method = "game_move_to"
    fake.received.clear()
    task = asyncio.create_task(client.call("game_move_to", move_args()))
    await asyncio.wait_for(fake.received.wait(), 1)
    with pytest.raises(BridgeError, match="busy"):
        await client.call("game_move_to", move_args())
    task.cancel()
    with pytest.raises(asyncio.CancelledError):
        await task
    await asyncio.wait_for(fake.disconnected.wait(), 1)
    assert client.link.closed


async def test_timeout_and_priority_stop(fake, client, monkeypatch):
    monkeypatch.setattr(bridge_client, "IO_TIMEOUT", 0.15)
    await client.call("game_get_session", {})
    fake.hold_method = "game_move_to"
    fake.received.clear()
    task = asyncio.create_task(client.call("game_move_to", move_args()))
    await asyncio.wait_for(fake.received.wait(), 1)
    assert (await client.call("game_stop", {}))["ok"]
    with pytest.raises(BridgeError) as error:
        await task
    assert error.value.side_effects_possible


@pytest.mark.parametrize(
    "reply",
    [
        struct.pack("!I", 1048577),
        struct.pack("!I", 0),
        struct.pack("!I", 2) + b"{}",
        struct.pack("!I", 12) + b'{"id":"bad"}',
    ],
)
async def test_invalid_response_closes_connection(fake, client, reply):
    await client.call("game_get_session", {})
    fake.raw_reply = reply
    with pytest.raises(BridgeError, match="transport_lost"):
        await client.call("game_get_state", {})
    assert client.link.closed


async def test_unknown_result_stays_normal_data(fake, client):
    from conftest import OPERATIONS

    fake.responses["game_get_operation"]["data"] = copy.deepcopy(
        next(op for op in OPERATIONS if op["state"] == "unknown")
    )
    result = await client.tool("game_get_operation", {"operation_id": "operation-1"})
    assert not result.is_error
    assert result.structured_content["data"]["state"] == "unknown"


async def test_unsafe_credentials_and_hardlink_rejected(fake, tmp_path):
    assert len(read_instances(Config(fake.directory), None)) == 1
    link = tmp_path / "credential-link.json"
    link.hardlink_to(fake.path)
    with pytest.raises(BridgeError, match="unavailable"):
        read_instances(Config(fake.directory), None)
    link.unlink()
    fake.path.write_text("{}")
    secure(fake.path)
    with pytest.raises(BridgeError, match="unavailable"):
        read_instances(Config(fake.directory), None)


async def test_world_readable_credentials_rejected(fake):
    if os.name == "nt":
        import win32security

        descriptor = win32security.ConvertStringSecurityDescriptorToSecurityDescriptor(
            "D:P(A;;FA;;;WD)", 1
        )
        win32security.SetFileSecurity(
            str(fake.path),
            win32security.DACL_SECURITY_INFORMATION
            | win32security.PROTECTED_DACL_SECURITY_INFORMATION,
            descriptor,
        )
    else:
        fake.path.chmod(0o644)
    with pytest.raises(BridgeError, match="unavailable"):
        read_instances(Config(fake.directory), None)
    secure(fake.path)
