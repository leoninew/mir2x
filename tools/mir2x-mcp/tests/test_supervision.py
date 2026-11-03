import asyncio
import contextlib
import copy
import json
import os
import sys
import time
from itertools import pairwise
from types import SimpleNamespace

import pytest
from conftest import FIXTURES, arguments
from mcp.client.session import ClientSession
from mcp.client.stdio import StdioServerParameters, stdio_client

from mir2x_mcp.runtime_cli import main
from mir2x_mcp.supervision import Binding, run_with_session

BINDING = Binding("0" * 32, "1")
CONTEXT = {
    "instance_id": BINDING.instance_id,
    "session_id": "1" * 32,
    "world_epoch": "1",
    "control_epoch": "1",
}


class FakeSession:
    def __init__(self):
        self.calls = []
        self.context = copy.deepcopy(CONTEXT)
        self.uid = "1"
        self.control = "active"
        self.hp = 300
        self.hold = {}
        self.release = asyncio.Event()

    async def call_tool(self, name, arguments):
        self.calls.append((name, time.monotonic(), arguments))
        if name in self.hold:
            self.hold[name].set()
            await self.release.wait()
        if name == "game_get_session":
            data = {
                "instance_id": BINDING.instance_id,
                "character_uid": self.uid,
                "context": copy.deepcopy(self.context),
                "phase": "run",
                "control_state": self.control,
                "pause_reason": "death" if self.hp == 0 else None,
            }
        elif name == "game_get_state":
            data = {
                "character_uid": self.uid,
                "hp": self.hp,
                "max_hp": 300,
                "known_states": ["alive" if self.hp else "dead"],
                "position": {"x": 1, "y": 1},
                "action": "idle",
                "map_uid": "2",
            }
        else:
            data = {"items": [], "next_cursor": None}
        return SimpleNamespace(
            is_error=False,
            structured_content={"ok": True, "data": data, "context": copy.deepcopy(self.context)},
        )


class EventLog(list):
    def __init__(self):
        super().__init__()
        self.changed = asyncio.Event()

    def append(self, event):
        super().append(event)
        self.changed.set()


async def wait_for(events, predicate):
    async with asyncio.timeout(3):
        while not any(predicate(event) for event in events):
            events.changed.clear()
            await events.changed.wait()


async def queued_commands(queue):
    while True:
        yield await queue.get()


async def test_idle_input_and_pending_read_do_not_suspend_health_monitor():
    session, events, stop, queue = FakeSession(), EventLog(), asyncio.Event(), asyncio.Queue()
    session.hold["game_get_inventory"] = asyncio.Event()
    task = asyncio.create_task(
        run_with_session(
            session, binding=BINDING, emit=events.append, commands=queued_commands(queue), stop=stop
        )
    )
    try:
        await wait_for(events, lambda event: event["event"] == "health")
        await queue.put({"command": "call", "name": "game_get_inventory", "arguments": {}})
        await asyncio.wait_for(session.hold["game_get_inventory"].wait(), 1)
        session.hp = 230
        await wait_for(events, lambda event: event.get("reason") == "low_hp")
        session.hp = 140
        await wait_for(events, lambda event: event.get("reason") == "critical_hp")
        assert not session.release.is_set()
        states = [stamp for name, stamp, _ in session.calls if name == "game_get_state"]
        assert len(states) >= 3
        assert max(b - a for a, b in pairwise(states)) <= 0.5
        assert any(event.get("reason") == "rapid_hp_loss" for event in events)
        assert all(not event["write_enabled"] for event in events)
    finally:
        session.release.set()
        stop.set()
        assert await task == 0


async def test_stalled_monitor_alerts_without_cancelling_query_or_queuing_more():
    session, events, stop = FakeSession(), EventLog(), asyncio.Event()
    session.hold["game_get_state"] = asyncio.Event()
    task = asyncio.create_task(
        run_with_session(session, binding=BINDING, emit=events.append, stop=stop)
    )
    try:
        await asyncio.wait_for(session.hold["game_get_state"].wait(), 1)
        await wait_for(events, lambda event: event.get("reason") == "monitor_stale")
        stale = next(event for event in events if event.get("reason") == "monitor_stale")
        started = next(stamp for name, stamp, _ in session.calls if name == "game_get_state")
        assert 0.45 <= stale["monotonic"] - started < 0.75
        assert not task.done()
        assert sum(name == "game_get_state" for name, _, _ in session.calls) == 1
    finally:
        session.release.set()
        stop.set()
        assert await task == 0


@pytest.mark.parametrize("control,hp", [("paused", 0), ("disabled", 300), ("paused", 300)])
async def test_paused_dead_or_disabled_stays_read_only(control, hp):
    session, events, stop, queue = FakeSession(), EventLog(), asyncio.Event(), asyncio.Queue()
    session.control, session.hp = control, hp
    task = asyncio.create_task(
        run_with_session(
            session, binding=BINDING, emit=events.append, commands=queued_commands(queue), stop=stop
        )
    )
    try:
        await wait_for(events, lambda event: event["event"] == "health")
        await queue.put({"command": "call", "name": "game_stop", "arguments": {}})
        await queue.put({"command": "call", "name": "game_attack", "arguments": {}})
        await queue.put({"command": "arm"})
        await queue.put({"command": "call", "name": []})
        await queue.put({"command": "observe"})
        await wait_for(events, lambda event: event["event"] == "status")
        assert any(event.get("reason") == "write_or_unknown_command_rejected" for event in events)
        assert {name for name, _, _ in session.calls} <= {
            "game_get_session",
            "game_get_state",
            "game_list_nearby",
        }
        assert session.control == control
    finally:
        stop.set()
        assert await task == 0


async def test_context_change_is_reported_and_different_character_stops_monitor():
    session, events, stop = FakeSession(), EventLog(), asyncio.Event()
    task = asyncio.create_task(
        run_with_session(session, binding=BINDING, emit=events.append, stop=stop)
    )
    try:
        await wait_for(events, lambda event: event["event"] == "health")
        session.context["control_epoch"] = "2"
        await wait_for(events, lambda event: event.get("reason") == "context_changed")
        session.uid = "2"
        assert await asyncio.wait_for(task, 2) == 2
        assert any(event.get("reason") == "character_mismatch" for event in events)
    finally:
        stop.set()
        await task


async def test_command_flood_preserves_sampling_and_rolling_request_budget():
    session, events, stop, queue = FakeSession(), EventLog(), asyncio.Event(), asyncio.Queue()
    for _ in range(25):
        queue.put_nowait({"command": "call", "name": "game_get_inventory", "arguments": {}})
    task = asyncio.create_task(
        run_with_session(
            session, binding=BINDING, emit=events.append, commands=queued_commands(queue), stop=stop
        )
    )
    try:
        await wait_for(
            events,
            lambda event: (
                event["event"] == "health" and event["monotonic"] > session.calls[0][1] + 1.1
            ),
        )
        starts = [stamp for _, stamp, _ in session.calls]
        assert all(sum(start <= stamp < start + 1 for stamp in starts) <= 16 for start in starts)
        states = [stamp for name, stamp, _ in session.calls if name == "game_get_state"]
        assert len(states) >= 5
        assert max(b - a for a, b in pairwise(states)) <= 0.5
    finally:
        stop.set()
        assert await task == 0


async def test_official_sdk_monitor_uses_one_auth_and_stops_after_eof(fake, tmp_path):
    for name in ("game_get_state", "game_list_nearby"):
        fake.responses[name]["context"] = copy.deepcopy(fake.context)
    fake.responses["game_get_state"]["data"].update(
        {"character_uid": "1", "hp": 300, "max_hp": 300, "known_states": ["alive"]}
    )
    parameters = StdioServerParameters(
        command=sys.executable,
        args=[
            "-m",
            "mir2x_mcp",
            "--bridge-dir",
            str(fake.directory),
            "--instance-id",
            fake.instance_id,
            "--no-reconnect",
        ],
        env={"PYTHONUTF8": "1"},
    )
    events = EventLog()
    with (tmp_path / "observe-stderr.log").open("w", encoding="utf-8") as errors:
        async with stdio_client(parameters, errlog=errors) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=5) as session:
                await session.initialize()
                task = asyncio.create_task(
                    run_with_session(session, binding=BINDING, emit=events.append)
                )
                await wait_for(events, lambda event: event["event"] == "health")
                fake.drop_method = "game_get_state"
                assert await asyncio.wait_for(task, 4) == 2
                fake.drop_method = None
                result = await session.call_tool("game_get_session", {})
                assert result.is_error
                assert result.structured_content["error"]["code"] == "transport_lost"
                assert fake.auths == 1
    assert fake.token not in (tmp_path / "observe-stderr.log").read_text("utf-8")


async def test_official_sdk_action_wait_does_not_block_observations(fake, tmp_path):
    for name in ("game_get_state", "game_list_nearby"):
        fake.responses[name]["context"] = copy.deepcopy(fake.context)
    fake.responses["game_get_state"]["data"].update(
        {"character_uid": "1", "hp": 300, "max_hp": 300, "known_states": ["alive"]}
    )
    parameters = StdioServerParameters(
        command=sys.executable,
        args=[
            "-m",
            "mir2x_mcp",
            "--bridge-dir",
            str(fake.directory),
            "--instance-id",
            fake.instance_id,
            "--no-reconnect",
        ],
        env={"PYTHONUTF8": "1"},
    )
    events, stop = EventLog(), asyncio.Event()
    with (tmp_path / "pending-stderr.log").open("w", encoding="utf-8") as errors:
        async with stdio_client(parameters, errlog=errors) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=5) as session:
                await session.initialize()
                monitor = asyncio.create_task(
                    run_with_session(session, binding=BINDING, emit=events.append, stop=stop)
                )
                await wait_for(events, lambda event: event["event"] == "monitor_ready")
                fake.hold_method = "game_move_to"
                fake.received.clear()
                move = arguments(
                    next(item for item in FIXTURES if item["method"] == "game_move_to")
                )
                move["context"] = fake.context
                pending = asyncio.create_task(session.call_tool("game_move_to", move))
                try:
                    async with asyncio.timeout(2):
                        while not any(call["method"] == "game_move_to" for call in fake.calls):
                            fake.received.clear()
                            await fake.received.wait()
                    accepted_at = time.monotonic()
                    await wait_for(
                        events,
                        lambda event: (
                            event["event"] == "health" and event["monotonic"] >= accepted_at + 0.5
                        ),
                    )
                    assert not pending.done()
                    assert fake.auths == 1
                    assert sum(call["method"] == "game_move_to" for call in fake.calls) == 1
                finally:
                    stop.set()
                    await monitor
                    pending.cancel()
                    with contextlib.suppress(asyncio.CancelledError):
                        await pending


def test_short_entry_selects_same_instance_and_forces_no_reconnect(tmp_path, monkeypatch):
    from mir2x_mcp import supervision

    calls = []

    async def observe(command, binding, report):
        calls.append((command, binding, report))
        return 0

    monkeypatch.setattr(supervision, "observe_stdio", observe)
    report = tmp_path / "observe.jsonl"
    with pytest.raises(SystemExit) as result:
        main(
            [
                "--root",
                str(tmp_path),
                "mcp",
                "observe",
                "--instance-id",
                BINDING.instance_id,
                "--character-uid",
                "1",
                "--report",
                str(report),
            ]
        )
    assert result.value.code == 0
    command, binding, actual_report = calls[0]
    assert command[-5:] == ["mcp", "serve", "--instance-id", BINDING.instance_id, "--no-reconnect"]
    assert binding == BINDING and actual_report == report
    assert not (tmp_path / ".runtime").exists()


async def test_observe_process_exits_after_disconnect_with_stdin_still_open(fake, tmp_path):
    for name in ("game_get_state", "game_list_nearby"):
        fake.responses[name]["context"] = copy.deepcopy(fake.context)
    fake.responses["game_get_state"]["data"].update(
        {"character_uid": "1", "hp": 300, "max_hp": 300, "known_states": ["alive"]}
    )
    root = tmp_path / "observe root"
    root.mkdir()
    (root / "mir2x.local.toml").write_text(
        "[launcher]\nbridge_dir=" + json.dumps(str(fake.directory)) + "\n", "utf-8"
    )
    command = [sys.executable, "-m", "mir2x_mcp.runtime_cli"]
    if executable := os.environ.get("MIR2X_FROZEN_EXE"):
        command = [executable]
    report = root / "observe.jsonl"
    with (root / "console-stderr.log").open("wb") as errors:
        process = await asyncio.create_subprocess_exec(
            *command,
            "--root",
            str(root),
            "mcp",
            "observe",
            "--instance-id",
            fake.instance_id,
            "--character-uid",
            "1",
            "--report",
            str(report),
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=errors,
            creationflags=0x08000000 if os.name == "nt" else 0,
        )
        try:
            async with asyncio.timeout(10):
                while True:
                    line = await process.stdout.readline()
                    assert line, report.read_text("utf-8") if report.exists() else "host exited"
                    if json.loads(line).get("event") == "monitor_ready":
                        break
            assert not process.stdin.is_closing()
            fake.drop_method = "game_get_state"
            assert await asyncio.wait_for(process.wait(), 5) == 2
            assert fake.auths == 1
            assert all(
                call["method"]
                in {"game_get_session", "game_get_state", "game_list_nearby", "heartbeat"}
                for call in fake.calls
            )
            events = [json.loads(line) for line in report.read_text("utf-8").splitlines()]
            assert any(event.get("reason") == "connection_unavailable" for event in events)
            assert events[-1]["event"] == "monitor_stopped"
        finally:
            if process.returncode is None:
                process.kill()
                await process.wait()
