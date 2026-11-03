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

from mir2x_mcp.contract import validate
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


async def wait_for(events, predicate, seconds=3):
    async with asyncio.timeout(seconds):
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


class SafetySession(FakeSession):
    """Synthetic game state; only the public SDK call boundary is replaced."""

    def __init__(self):
        super().__init__()
        self.position = {"x": 1, "y": 1}
        self.action = "idle"
        self.endpoint = None
        self.potions = 5
        self.reserved = 0
        self.potion_name = "\u91d1\u521b\u836f\uff08\u5c0f\uff09"
        self.blocked = False
        self.target = {
            "target_handle": "target-2",
            "uid": "2",
            "kind": "monster",
            "alive": True,
            "hp": 10,
            "position": {"x": 4, "y": 1},
        }
        self.operations = {}
        self.terminal = "succeeded"
        self.potion_heals = False
        self.busy = False
        self.write_entered = asyncio.Event()
        self.write_release = None
        self.operation_release = None
        self.drop_after_write = False
        self.map_uid = "2"
        self.gold = 1000
        self.shop_pending = 1
        self.shop_delivers = True
        self.quote_overrides = {}
        self.goods = {}
        self.purchase_quotes = {}
        self.current_quote = None
        self.post_purchase_fault = None
        self.spent = 0
        self.policy = {
            "allowed_actions": ["game_shop_buy"],
            "allow_pvp": False,
            "allow_npc_dialogue": True,
            "max_gold_per_operation": "1000",
            "max_gold_per_session": "10000",
            "max_item_count_per_operation": 10,
            "remaining_gold_budget": "10000",
            "economic_uncertain": False,
        }

    async def call_tool(self, name, arguments):
        if name in {"game_get_session", "game_get_state"}:
            result = await super().call_tool(name, arguments)
            if name == "game_get_session":
                capabilities = {
                    "protocol_version": 1,
                    "build": "synthetic-test",
                    "tools": [
                        "game_shop_buy",
                        "game_move_to",
                        "game_attack",
                        "game_use_item",
                        "game_stop",
                    ],
                    "supported_skills": [],
                    "policy": copy.deepcopy(self.policy),
                    "limitations": [],
                }
                result.structured_content["data"]["capabilities"] = capabilities
                capabilities["policy"]["remaining_gold_budget"] = str(
                    max(0, int(self.policy["remaining_gold_budget"]) - self.spent)
                )
                validate(result.structured_content["data"], "session")
            if name == "game_get_state":
                result.structured_content["data"].update(
                    position=dict(self.position),
                    action_endpoint=dict(self.endpoint or self.position),
                    action=self.action,
                    map_uid=self.map_uid,
                    gold=str(self.gold),
                )
                if self.spent and self.post_purchase_fault == "gold_error":
                    return SimpleNamespace(
                        is_error=True,
                        structured_content={
                            "ok": False,
                            "context": copy.deepcopy(self.context),
                            "error": {"code": "unavailable"},
                        },
                    )
            return result
        self.calls.append((name, time.monotonic(), copy.deepcopy(arguments)))
        if name in self.hold:
            self.hold[name].set()
            await self.release.wait()
        if name == "game_list_nearby":
            data = {"items": [copy.deepcopy(self.target)], "next_cursor": None}
        elif name == "game_get_inventory":
            if self.spent and self.post_purchase_fault == "inventory_error":
                return SimpleNamespace(
                    is_error=True,
                    structured_content={
                        "ok": False,
                        "context": copy.deepcopy(self.context),
                        "error": {"code": "unavailable"},
                    },
                )
            data = {
                "items": [
                    {
                        "item_id": 6,
                        "seq_id": 1,
                        "count": self.potions,
                        "reserved_count": self.reserved,
                        "location": "inventory",
                        "name": self.potion_name,
                    }
                ]
                if self.potions
                else [],
                "next_cursor": None,
                "unavailable_fields": [],
            }
            data["items"].extend(
                {
                    "item_id": item,
                    "seq_id": 1,
                    "count": count,
                    "reserved_count": 0,
                    "location": "inventory",
                    "name": "synthetic ordinary item",
                }
                for item, count in self.goods.items()
            )
            if self.spent and self.post_purchase_fault == "incomplete":
                data["next_cursor"] = "more-inventory"
            elif self.spent and self.post_purchase_fault == "unavailable":
                data["unavailable_fields"] = ["inventory"]
        elif name == "game_get_map_region":
            data = {
                "map_uid": self.map_uid,
                "cells": [
                    {"x": x, "y": y, "walkable": True, "dynamic_blocked": self.blocked}
                    for x in range(arguments["x"], arguments["x"] + arguments["width"])
                    for y in range(arguments["y"], arguments["y"] + arguments["height"])
                ],
            }
        elif name == "game_shop_list":
            pending = self.shop_pending > 0
            self.shop_pending -= 1
            data = {
                "items": []
                if pending
                else [
                    {
                        "quote_handle": "fresh-quote",
                        "item_id": arguments["item_id"],
                        "seq_id": 0,
                        "unit_count": 1,
                        "gold": "80",
                        "materials": [],
                        "direction": "buy",
                        "expires_at": str(int(time.time() * 1000) + 30000),
                        **self.quote_overrides,
                    }
                ],
                "next_cursor": None,
                "refresh_pending": pending,
                "last_query_error": None,
                "unavailable_fields": [],
            }
            if data["items"]:
                self.current_quote = copy.deepcopy(data["items"][0])
        elif name in {"game_attack", "game_use_item", "game_move_to", "game_shop_buy"}:
            self.write_entered.set()
            if self.write_release is not None:
                await self.write_release.wait()
            if self.drop_after_write:
                raise ConnectionError("synthetic response loss")
            if self.busy:
                return SimpleNamespace(
                    is_error=True,
                    structured_content={
                        "ok": False,
                        "context": copy.deepcopy(self.context),
                        "error": {"code": "busy"},
                    },
                )
            operation = "op-" + str(len(self.operations) + 1)
            self.operations[operation] = {
                "operation_id": operation,
                "method": name,
                "request_id": arguments["request_id"],
                "context": copy.deepcopy(arguments["context"]),
                "state": self.terminal,
                "side_effects_possible": self.terminal == "unknown",
                "result": {"submitted": True},
                "completion_scope": "observed_postcondition",
                "arguments": arguments,
            }
            data = {"operation_id": operation, "state": "accepted"}
            if name == "game_shop_buy":
                self.purchase_quotes[operation] = copy.deepcopy(self.current_quote)
        elif name == "game_get_operation":
            if self.operation_release is not None:
                await self.operation_release.wait()
            data = copy.deepcopy(self.operations[arguments["operation_id"]])
            args = data.pop("arguments")
            if data["state"] == "succeeded":
                if data["method"] == "game_use_item":
                    data["result"]["item_changes"] = [
                        {
                            "item_id": 6,
                            "seq_id": 1,
                            "before": self.potions,
                            "after": self.potions - 1,
                        }
                    ]
                    self.potions -= 1
                    if self.potion_heals:
                        self.hp += 30
                if data["method"] == "game_move_to":
                    self.position = {"x": args["x"], "y": args["y"]}
                    data["result"]["position"] = dict(self.position)
                if data["method"] == "game_attack":
                    data["completion_scope"] = "client_state"
                if data["method"] == "game_shop_buy":
                    data["completion_scope"] = "server_ack"
                    data["result"]["delivery_pending"] = not self.shop_delivers
                    if self.shop_delivers:
                        quote = self.purchase_quotes[data["operation_id"]]
                        if quote["item_id"] == 6:
                            self.potions += 1
                        else:
                            self.goods[quote["item_id"]] = self.goods.get(quote["item_id"], 0) + 1
                        self.gold -= int(quote["gold"])
                        self.spent += int(quote["gold"])
        elif name == "game_stop":
            self.control = "paused"
            self.context["control_epoch"] = str(int(self.context["control_epoch"]) + 1)
            data = {"stopped": ["attack"]}
        else:
            raise AssertionError(name)
        return SimpleNamespace(
            is_error=False,
            structured_content={
                "ok": True,
                "data": data,
                "context": copy.deepcopy(self.context),
            },
        )


class SafetyHarness:
    async def __aenter__(self):
        self.session, self.events = SafetySession(), EventLog()
        self.stop, self.queue = asyncio.Event(), asyncio.Queue()
        self.task = asyncio.create_task(
            run_with_session(
                self.session,
                binding=BINDING,
                emit=self.events.append,
                commands=queued_commands(self.queue),
                stop=self.stop,
                allow_actions=True,
            )
        )
        await wait_for(self.events, lambda event: event["event"] == "monitor_ready")
        return self

    async def __aexit__(self, *exc):
        self.session.release.set()
        for name in ("write_release", "operation_release"):
            if event := getattr(self.session, name):
                event.set()
        self.stop.set()
        await asyncio.wait_for(self.task, 5)

    async def visit(self, x):
        started = time.monotonic()
        self.session.action = "motion_22"
        self.session.endpoint = {"x": x, "y": 1}
        await wait_for(
            self.events,
            lambda event: (
                event["event"] == "health"
                and event["monotonic"] > started
                and event["action"] == "motion_22"
            ),
        )
        await asyncio.sleep(0.04)
        started = time.monotonic()
        self.session.position = {"x": x, "y": 1}
        self.session.endpoint = None
        self.session.action = "idle"
        await wait_for(
            self.events,
            lambda event: (
                event["event"] == "health"
                and event["monotonic"] > started
                and event["position"]["x"] == x
            ),
        )
        # Let the independent watchdog record this actual observed stopped point.
        await asyncio.sleep(0.04)

    def arm_command(self):
        return {
            "command": "arm",
            "context": copy.deepcopy(self.session.context),
            "map_uid": "2",
            "potions": [{"item_id": 6, "seq_id": 1}],
            "safety": {
                "reserve_doses": 3,
                "low_hp_percent": 80,
                "rapid_loss_percent": 5,
                "rapid_loss_window_ms": 1000,
            },
            "retreat": [{"x": 3, "y": 1}, {"x": 2, "y": 1}, {"x": 1, "y": 1}],
        }

    async def prepare(self):
        await self.visit(2)
        await self.visit(3)

    async def arm(self):
        await self.prepare()
        await self.queue.put(self.arm_command())
        await wait_for(self.events, lambda event: event["event"] == "armed")

    async def attack(self, intent="00000000-0000-4000-8000-000000000010"):
        await self.queue.put(
            {"command": "attack", "target_handle": "target-2", "intent_id": intent}
        )

    def writes(self):
        return [
            (name, args)
            for name, _, args in self.session.calls
            if name
            in {"game_attack", "game_use_item", "game_move_to", "game_shop_buy", "game_stop"}
        ]

    def purchase_command(self, number=1):
        return {
            "command": "purchase",
            "context": copy.deepcopy(self.session.context),
            "shop_handle": "current-shop",
            "item_id": 6,
            "count": 1,
            "max_total_gold": "80",
            "intent_id": f"00000000-0000-4000-8000-{number:012d}",
        }

    async def town(self):
        self.session.target["kind"] = "npc"
        self.session.potions = 0
        await asyncio.sleep(0.3)


async def test_purchase_refreshes_and_buys_three_separate_doses_without_llm_quote_roundtrip():
    async with SafetyHarness() as test:
        await test.town()
        for number in range(1, 4):
            command = test.purchase_command(number)
            await test.queue.put(command)
            await wait_for(
                test.events,
                lambda row, intent=command["intent_id"]: (
                    row["event"] == "purchase_result" and row["intent_id"] == intent
                ),
                seconds=5,
            )
        results = [row for row in test.events if row["event"] == "purchase_result"]
        assert all(
            row["purchase_confirmed"] and row["delivery_observed"] and not row["healing_attributed"]
            for row in results
        )
        assert test.session.potions == 3 and test.session.gold == 760 and test.session.hp == 300
        assert [name for name, _ in test.writes()] == ["game_shop_buy"] * 3
        for index, (name, stamp, args) in enumerate(test.session.calls):
            if name != "game_shop_buy":
                continue
            quote_call = next(
                row for row in reversed(test.session.calls[:index]) if row[0] == "game_shop_list"
            )
            assert stamp - quote_call[1] < 0.5
            assert args["quote_handle"] == "fresh-quote" and args["count"] == 1
            assert args["max_total_gold"] == "80"
        await test.queue.put(test.purchase_command(3))
        await wait_for(
            test.events,
            lambda row: row.get("reason") == "purchase_intent_already_seen_or_limit_reached",
        )
        assert len(test.writes()) == 3
        starts = [stamp for _, stamp, _ in test.session.calls]
        assert all(sum(start <= stamp < start + 1 for stamp in starts) <= 16 for start in starts)
        states = [stamp for name, stamp, _ in test.session.calls if name == "game_get_state"]
        assert max(b - a for a, b in pairwise(states)) <= 0.5


@pytest.mark.parametrize(
    "case",
    [
        "price",
        "expired",
        "materials",
        "sell",
        "bundle",
        "wrong_item",
        "context",
        "count",
        "invalid_budget",
        "monster",
    ],
)
async def test_purchase_rejects_unsafe_or_nonmatching_preparation_without_writes(case):
    async with SafetyHarness() as test:
        await test.town()
        command = test.purchase_command()
        test.session.shop_pending = 0
        overrides = {
            "price": {"gold": "81"},
            "expired": {"expires_at": "1"},
            "materials": {"materials": [{"item_id": 7, "count": 1}]},
            "sell": {"direction": "sell"},
            "bundle": {"unit_count": 2},
            "wrong_item": {"item_id": 7},
        }
        test.session.quote_overrides = overrides.get(case, {})
        if case == "context":
            command["context"]["control_epoch"] = "9"
        elif case == "count":
            command["count"] = 3
        elif case == "invalid_budget":
            command["max_total_gold"] = "18446744073709551616"
        elif case == "monster":
            test.session.target["kind"] = "monster"
        await asyncio.sleep(0.3)
        await test.queue.put(command)
        await wait_for(test.events, lambda row: row["event"] == "command_rejected")
        assert not test.writes()


@pytest.mark.parametrize("case", ["unknown", "lost", "busy", "undelivered"])
async def test_purchase_never_retries_or_infers_delivery_from_accepted_or_ack(case):
    async with SafetyHarness() as test:
        await test.town()
        test.session.shop_pending = 0
        test.session.terminal = "unknown" if case == "unknown" else "succeeded"
        test.session.drop_after_write = case == "lost"
        test.session.busy = case == "busy"
        test.session.shop_delivers = case != "undelivered"
        await test.queue.put(test.purchase_command())
        if case in ("unknown", "lost"):
            await wait_for(
                test.events, lambda row: row.get("reason") == "write_uncertain_no_replay"
            )
            await test.queue.put(test.purchase_command(2))
        else:
            await wait_for(test.events, lambda row: row["event"] == "purchase_result", seconds=6)
            result = next(row for row in test.events if row["event"] == "purchase_result")
            assert not result["purchase_confirmed"] and not result["delivery_observed"]
            if case == "undelivered":
                assert result["operation"]["completion_scope"] == "server_ack"
                await test.queue.put(test.purchase_command(2))
        await asyncio.sleep(0.3)
        assert len(test.writes()) == 1
        assert test.session.potions == 0 and test.session.gold == 1000


async def test_purchase_quote_wait_keeps_monitor_live_and_manual_pause_prevents_buy():
    async with SafetyHarness() as test:
        await test.town()
        test.session.hold["game_shop_list"] = asyncio.Event()
        await test.queue.put(test.purchase_command())
        await asyncio.wait_for(test.session.hold["game_shop_list"].wait(), 3)
        started = time.monotonic()
        await wait_for(
            test.events, lambda row: row["event"] == "health" and row["monotonic"] > started + 0.5
        )
        test.session.control = "paused"
        await asyncio.sleep(0.3)
        test.session.release.set()
        await wait_for(test.events, lambda row: row["event"] == "command_rejected")
        assert not test.writes()


async def test_generic_purchase_accepts_caller_item_budget_and_four_explicit_intents_at_low_hp():
    async with SafetyHarness() as test:
        await test.town()
        test.session.hp = 150
        test.session.shop_pending = 0
        test.session.quote_overrides = {"gold": "1"}
        await asyncio.sleep(0.3)
        for number in range(1, 5):
            command = test.purchase_command(number)
            command.update(item_id=826, max_total_gold="1")
            await test.queue.put(command)
            await wait_for(
                test.events,
                lambda row, intent=command["intent_id"]: (
                    row["event"] == "purchase_result" and row["intent_id"] == intent
                ),
                seconds=5,
            )
        assert test.session.goods[826] == 4 and test.session.gold == 996
        assert len(test.writes()) == 4 and test.session.potions == 0 and test.session.hp == 150
        command = test.purchase_command(5)
        command.update(item_id=7, max_total_gold="175")
        test.session.quote_overrides = {"gold": "150"}
        await test.queue.put(command)
        await wait_for(
            test.events,
            lambda row: (
                row["event"] == "purchase_result" and row["intent_id"] == command["intent_id"]
            ),
            seconds=5,
        )
        assert test.session.goods[7] == 1 and test.session.gold == 846
        assert test.writes()[-1][1]["max_total_gold"] == "175"


@pytest.mark.parametrize(
    "case", ["request_cap", "operation_cap", "remaining", "uncertain", "denied", "count_policy"]
)
async def test_generic_purchase_respects_request_and_native_frozen_policy_bounds(case):
    async with SafetyHarness() as test:
        await test.town()
        test.session.shop_pending = 0
        command = test.purchase_command()
        if case == "request_cap":
            command["max_total_gold"] = "79"
        elif case == "operation_cap":
            test.session.policy["max_gold_per_operation"] = "79"
        elif case == "remaining":
            test.session.policy["remaining_gold_budget"] = "79"
        elif case == "uncertain":
            test.session.policy["economic_uncertain"] = True
        elif case == "denied":
            test.session.policy["allowed_actions"] = []
        else:
            test.session.policy["max_item_count_per_operation"] = 0
        await asyncio.sleep(0.3)
        await test.queue.put(command)
        await wait_for(test.events, lambda row: row["event"] == "command_rejected")
        assert not test.writes()


async def test_generic_purchase_policy_session_budget_limits_successive_explicit_purchases():
    async with SafetyHarness() as test:
        await test.town()
        test.session.policy["remaining_gold_budget"] = "80"
        await asyncio.sleep(0.3)
        await test.queue.put(test.purchase_command())
        await wait_for(test.events, lambda row: row["event"] == "purchase_result", seconds=5)
        await asyncio.sleep(0.3)
        await test.queue.put(test.purchase_command(2))
        await wait_for(
            test.events, lambda row: row.get("reason") == "purchase_quote_terms_not_allowed"
        )
        assert len(test.writes()) == 1 and test.session.spent == 80


@pytest.mark.parametrize("fault", ["inventory_error", "incomplete", "unavailable", "gold_error"])
async def test_unconfirmed_purchase_delivery_survives_read_failure_and_locks_new_intents(fault):
    async with SafetyHarness() as test:
        await test.town()
        test.session.shop_pending = 0
        test.session.post_purchase_fault = fault
        await test.queue.put(test.purchase_command())
        await wait_for(test.events, lambda row: row["event"] == "command_rejected", seconds=5)
        test.session.post_purchase_fault = None
        await asyncio.sleep(0.3)
        await test.queue.put(test.purchase_command(2))
        await asyncio.sleep(1.3)
        assert len(test.writes()) == 1
        assert any(
            row["event"] == "plan_invalidated" and row.get("writes_locked") for row in test.events
        )
        await test.queue.put({"command": "observe"})
        await wait_for(test.events, lambda row: row["event"] == "status")
        status = next(row for row in test.events if row["event"] == "status")
        assert status["unconfirmed_purchase"]["operation"]["state"] == "succeeded"
        assert status["unconfirmed_purchase"]["operation"]["request_id"]


async def test_caller_safety_reserve_and_threshold_replace_test_role_constants():
    async with SafetyHarness() as test:
        await test.prepare()
        test.session.potions = 2
        command = test.arm_command()
        command["safety"].update(reserve_doses=1, low_hp_percent=60, rapid_loss_percent=50)
        await test.queue.put(command)
        await wait_for(test.events, lambda row: row["event"] == "armed")
        test.session.hp = 230
        await asyncio.sleep(0.6)
        assert not any(row["event"] == "hunting_locked" for row in test.events)
        test.session.hp = 170
        await wait_for(test.events, lambda row: row["event"] == "retreat_finished", seconds=9)
        assert test.writes()[0][0] == "game_use_item"


async def test_caller_rapid_loss_window_uses_health_history_beyond_default_observer_second():
    async with SafetyHarness() as test:
        await test.prepare()
        command = test.arm_command()
        command["safety"].update(low_hp_percent=10, rapid_loss_percent=5, rapid_loss_window_ms=2000)
        await test.queue.put(command)
        await wait_for(test.events, lambda row: row["event"] == "armed")
        test.session.hp = 290
        await wait_for(test.events, lambda row: row["event"] == "health" and row["hp"] == 290)
        await asyncio.sleep(1.1)
        assert not any(row["event"] == "hunting_locked" for row in test.events)
        test.session.hp = 280
        await wait_for(test.events, lambda row: row["event"] == "hunting_locked")


@pytest.mark.parametrize("case", ["missing", "reserve", "threshold", "window"])
async def test_arming_rejects_absent_or_invalid_caller_safety_task(case):
    async with SafetyHarness() as test:
        command = test.arm_command()
        if case == "missing":
            command.pop("safety")
        elif case == "reserve":
            command["safety"]["reserve_doses"] = 0
        elif case == "threshold":
            command["safety"]["low_hp_percent"] = 100
        else:
            command["safety"]["rapid_loss_window_ms"] = 10001
        await test.queue.put(command)
        await wait_for(test.events, lambda row: row["event"] == "command_rejected")
        assert not test.writes()


@pytest.mark.parametrize(
    "missing", ["trail", "doses", "reserved", "fake_name", "map", "context", "blocked"]
)
async def test_arm_requires_traversed_clear_route_identity_and_three_real_potions(missing):
    async with SafetyHarness() as test:
        if missing != "trail":
            await test.prepare()
        command = test.arm_command()
        if missing == "doses":
            test.session.potions = 2
        elif missing == "reserved":
            test.session.reserved = 3
        elif missing == "fake_name":
            test.session.potion_name = "a wearable named like a potion"
        elif missing == "map":
            command["map_uid"] = "3"
        elif missing == "context":
            command["context"]["control_epoch"] = "2"
        elif missing == "blocked":
            test.session.blocked = True
        await test.queue.put(command)
        await wait_for(test.events, lambda event: event["event"] == "command_rejected")
        assert not any(event["event"] == "armed" for event in test.events)
        assert not test.writes()


async def test_armed_short_attack_uses_one_scheduler_and_never_replays_an_intent():
    async with SafetyHarness() as test:
        await test.arm()
        await test.attack()
        await wait_for(
            test.events,
            lambda event: (
                event["event"] == "operation" and event["operation"]["state"] == "succeeded"
            ),
        )
        await asyncio.sleep(0.3)
        await test.attack()
        await wait_for(
            test.events,
            lambda event: event.get("reason") == "attack_intent_already_seen_or_limit_reached",
        )
        writes = test.writes()
        assert len(writes) == 1
        name, args = writes[0]
        assert name == "game_attack" and args["mode"] == "once" and args["timeout_ms"] == 2000
        assert args["context"] == CONTEXT


@pytest.mark.parametrize("target_change", ["distant", "unknown_health", "player"])
async def test_armed_attack_refuses_nonadjacent_unknown_or_player(target_change):
    async with SafetyHarness() as test:
        await test.arm()
        if target_change == "distant":
            test.session.target["position"]["x"] = 7
        elif target_change == "unknown_health":
            test.session.target["alive"] = None
            test.session.target["hp"] = None
        else:
            test.session.target["kind"] = "player"
        stamp = time.monotonic()
        await wait_for(
            test.events, lambda event: event["event"] == "nearby" and event["monotonic"] > stamp
        )
        await test.attack()
        await wait_for(test.events, lambda event: event["event"] == "command_rejected")
        assert not test.writes()


@pytest.mark.parametrize("heals", [False, True])
async def test_idle_low_hp_prioritizes_one_potion_then_witnessed_retreat_and_stays_disarmed(heals):
    async with SafetyHarness() as test:
        await test.arm()
        test.session.potion_heals = heals
        test.session.hp = 230
        # No stdin or attack is needed: the independent monitor wakes recovery.
        await wait_for(test.events, lambda event: event["event"] == "retreat_finished", seconds=9)
        writes = test.writes()
        assert [name for name, _ in writes] == ["game_use_item", "game_move_to", "game_move_to"]
        assert writes[0][1]["count"] == 1 and writes[0][1]["seq_id"] == 1
        assert [(args["x"], args["y"]) for name, args in writes if name == "game_move_to"] == [
            (2, 1),
            (1, 1),
        ]
        potion = next(event for event in test.events if event["event"] == "potion_result")
        finish = next(event for event in test.events if event["event"] == "retreat_finished")
        assert potion["consumption_observed"] and not potion["healing_attributed"]
        assert finish["hp_growth_observed"] is heals and not finish["automatic_resume"]
        await test.attack()
        await wait_for(test.events, lambda event: event.get("reason") == "plan_no_longer_valid")
        assert len(test.writes()) == 3
        stamps = [stamp for _, stamp, _ in test.session.calls]
        assert all(sum(start <= stamp < start + 1 for stamp in stamps) <= 16 for start in stamps)


@pytest.mark.parametrize("failure", ["unknown", "lost_response"])
async def test_unknown_or_lost_attack_response_locks_writes_even_at_low_hp(failure):
    async with SafetyHarness() as test:
        await test.arm()
        if failure == "unknown":
            test.session.terminal = "unknown"
        else:
            test.session.drop_after_write = True
        await test.attack()
        await wait_for(
            test.events, lambda event: event.get("reason") == "write_uncertain_no_replay"
        )
        test.session.hp = 150
        await test.queue.put(test.arm_command())
        await test.attack()
        await asyncio.sleep(0.6)
        assert [name for name, _ in test.writes()] == ["game_attack"]
        locked = [
            event
            for event in test.events
            if event["event"] == "plan_invalidated" and event.get("writes_locked")
        ]
        assert locked and locked[0]["operation"]["request_id"]


@pytest.mark.parametrize("change", ["manual", "map", "death", "stale"])
async def test_pause_context_death_or_stale_observation_invalidates_plan_without_writes(change):
    async with SafetyHarness() as test:
        await test.arm()
        if change == "manual":
            test.session.control = "paused"
            test.session.context["control_epoch"] = "2"
        elif change == "map":
            test.session.map_uid = "3"
        elif change == "death":
            test.session.hp = 0
        else:
            test.session.hold["game_get_state"] = asyncio.Event()
        await wait_for(
            test.events,
            lambda event: (
                event["event"] == "plan_invalidated" and event["reason"] != "explicit_rearm"
            ),
        )
        await test.attack()
        await asyncio.sleep(0.3)
        assert not test.writes()


async def test_pending_action_does_not_starve_monitor_and_stop_is_global_without_retreat():
    async with SafetyHarness() as test:
        await test.arm()
        test.session.operation_release = asyncio.Event()
        await test.attack()
        await asyncio.wait_for(test.session.write_entered.wait(), 2)
        started = time.monotonic()
        test.session.hp = 230
        await wait_for(test.events, lambda event: event["event"] == "hunting_locked")
        await wait_for(
            test.events,
            lambda event: event["event"] == "health" and event["monotonic"] > started + 0.5,
        )
        assert [name for name, _ in test.writes()] == ["game_attack"]
        await test.queue.put({"command": "observe"})
        await wait_for(test.events, lambda event: event["event"] == "status")
        status = next(event for event in test.events if event["event"] == "status")
        assert status["pending_actions"] == 1
        assert status["pending_operation"]["operation_id"] == "op-1"
        assert status["hunting_enabled"] is False
        await test.queue.put({"command": "stop"})
        test.session.operation_release.set()
        await wait_for(test.events, lambda event: event["event"] == "global_stop")
        assert [name for name, _ in test.writes()] == ["game_attack", "game_stop"]
        await asyncio.sleep(0.4)
        assert test.session.control == "paused" and len(test.writes()) == 2


async def test_adjacent_idle_positions_without_native_walk_do_not_qualify_as_traversal():
    async with SafetyHarness() as test:
        for x in (2, 3):
            test.session.position = {"x": x, "y": 1}
            await asyncio.sleep(0.3)
        await test.queue.put(test.arm_command())
        await wait_for(
            test.events, lambda event: event.get("reason") == "retreat_edge_not_observed_traversed"
        )
        assert not test.writes()


async def test_low_hp_during_unsent_attack_preflight_preserves_recovery_priority():
    async with SafetyHarness() as test:
        await test.arm()
        test.session.hold["game_get_inventory"] = asyncio.Event()
        await test.attack()
        await asyncio.wait_for(test.session.hold["game_get_inventory"].wait(), 2)
        test.session.hp = 230
        await wait_for(test.events, lambda event: event["event"] == "hunting_locked")
        test.session.release.set()
        await wait_for(test.events, lambda event: event["event"] == "retreat_finished", seconds=9)
        assert [name for name, _ in test.writes()] == [
            "game_use_item",
            "game_move_to",
            "game_move_to",
        ]


async def test_rapid_loss_above_low_hp_threshold_also_latches_hunting_off():
    async with SafetyHarness() as test:
        await test.arm()
        test.session.hp = 285
        await wait_for(test.events, lambda event: event["event"] == "hunting_locked")
        assert test.session.hp > 300 * 0.8
        await wait_for(test.events, lambda event: event["event"] == "retreat_finished", seconds=9)
        assert test.writes()[0][0] == "game_use_item"


async def test_known_busy_is_bounded_to_zero_automatic_retries():
    async with SafetyHarness() as test:
        await test.arm()
        test.session.busy = True
        await test.attack()
        await wait_for(test.events, lambda event: event["event"] == "action_not_admitted")
        test.session.action = "motion_16"
        await asyncio.sleep(0.6)
        assert len(test.writes()) == 1
        assert not test.session.operations


async def test_unresolved_admission_deadline_locks_writes_without_cancelling_the_sdk_call():
    async with SafetyHarness() as test:
        await test.arm()
        test.session.write_release = asyncio.Event()
        await test.attack()
        await asyncio.wait_for(test.session.write_entered.wait(), 2)
        await wait_for(
            test.events,
            lambda event: event.get("writes_locked") == "operation_deadline_unresolved",
            seconds=4,
        )
        assert not test.session.write_release.is_set()
        assert not test.task.done()
        assert len(test.writes()) == 1


async def test_explicit_scout_registers_one_real_public_move_before_arm_can_accept_its_retreat():
    async with SafetyHarness() as test:
        command = test.arm_command()
        command.update(command="scout", destination={"x": 2, "y": 1})
        command.pop("retreat")
        await test.queue.put(command)
        await wait_for(
            test.events,
            lambda event: event.get("reason") == "scout_finished_hunting_still_disabled",
        )
        assert [name for name, _ in test.writes()] == ["game_move_to"]
        command = test.arm_command()
        command["retreat"] = [{"x": 2, "y": 1}, {"x": 1, "y": 1}]
        await test.queue.put(command)
        await wait_for(test.events, lambda event: event["event"] == "armed")
        assert len(test.writes()) == 1


async def test_confirmed_attack_allows_only_a_new_explicit_intent_after_fresh_target_observation():
    async with SafetyHarness() as test:
        await test.arm()
        await test.attack()
        await wait_for(
            test.events,
            lambda event: (
                event["event"] == "operation" and event["operation"]["state"] == "succeeded"
            ),
        )
        completed = time.monotonic()
        await wait_for(
            test.events, lambda event: event["event"] == "nearby" and event["monotonic"] > completed
        )
        await test.attack("00000000-0000-4000-8000-000000000011")
        await wait_for(
            test.events,
            lambda event: (
                event["event"] == "operation" and event["operation"]["operation_id"] == "op-2"
            ),
        )
        assert [name for name, _ in test.writes()] == ["game_attack", "game_attack"]
        assert test.writes()[0][1]["request_id"] != test.writes()[1][1]["request_id"]


async def test_scout_cannot_move_without_three_verified_doses():
    async with SafetyHarness() as test:
        test.session.potions = 2
        command = test.arm_command()
        command.update(command="scout", destination={"x": 2, "y": 1})
        await test.queue.put(command)
        await wait_for(
            test.events,
            lambda event: event.get("reason") == "task_potion_reserve_not_met",
        )
        assert not test.writes()


async def test_retreat_rechecks_changed_occupancy_after_one_potion_without_repeating_it():
    async with SafetyHarness() as test:
        await test.arm()
        test.session.blocked = True
        test.session.hp = 230
        await wait_for(
            test.events, lambda event: event.get("reason") == "retreat_cell_not_clear", seconds=5
        )
        assert [name for name, _ in test.writes()] == ["game_use_item"]
        assert any(event.get("reason") == "no_safe_action" for event in test.events)


async def test_eof_invalidates_armed_plan_and_retains_caller_owned_session():
    session, events = SafetySession(), EventLog()
    helper = SafetyHarness()
    helper.session, helper.events = session, events

    async def commands():
        await wait_for(events, lambda event: event["event"] == "monitor_ready")
        scout = helper.arm_command()
        scout.update(command="scout", destination={"x": 2, "y": 1})
        yield scout
        await wait_for(
            events, lambda event: event.get("reason") == "scout_finished_hunting_still_disabled"
        )
        arm = helper.arm_command()
        arm["retreat"] = [{"x": 2, "y": 1}, {"x": 1, "y": 1}]
        yield arm
        await wait_for(events, lambda event: event["event"] == "armed")

    assert (
        await run_with_session(
            session, binding=BINDING, emit=events.append, commands=commands(), allow_actions=True
        )
        == 0
    )
    assert any(event.get("reason") == "input_closed" for event in events)
    assert session.control == "active"
    assert [name for name, _ in helper.writes()] == ["game_move_to"]


async def test_supervise_process_official_sdk_arms_attacks_once_and_locks_after_unknown(
    fake, tmp_path
):
    model = SafetySession()
    model.context = copy.deepcopy(fake.context)
    model.target["kind"] = "npc"
    model.potions = 0
    send_original = fake.send

    async def send_synthetic(writer, response):
        request = next((row for row in reversed(fake.calls) if row["id"] == response["id"]), None)
        if request is not None and request["method"] != "heartbeat":
            args = dict(request["params"])
            args.update(
                {
                    key: request[key]
                    for key in ("context", "request_id", "timeout_ms")
                    if key in request
                }
            )
            result = await model.call_tool(request["method"], args)
            payload = result.structured_content
            response = copy.deepcopy(response)
            response["context"] = copy.deepcopy(model.context)
            response["data"].update(payload["data"])
            if request["method"] == "game_list_nearby":
                response["data"]["items"][0].update(
                    name="synthetic chicken", max_hp=10, ground_handle=None, items=[]
                )
            if request["method"] == "game_get_inventory" and response["data"]["items"]:
                response["data"]["items"][0]["slot"] = None
            if request["method"] == "game_get_map_region":
                response["data"].update(request["params"])
            if request["method"] == "game_get_operation":
                if response["data"]["state"] == "succeeded":
                    response["data"]["evidence"] = [
                        {
                            "source": "client",
                            "kind": "position",
                            "observed_at": "1",
                            "snapshot_seq": "1",
                            "server_code": None,
                            "description": "synthetic stopped arrival",
                        }
                    ]
                else:
                    response["data"]["completion_scope"] = None
        await send_original(writer, response)

    fake.send = send_synthetic
    root = tmp_path / "armed root"
    root.mkdir()
    (root / "mir2x.local.toml").write_text(
        "[launcher]\nbridge_dir=" + json.dumps(str(fake.directory)) + "\n", "utf-8"
    )
    command = [sys.executable, "-m", "mir2x_mcp.runtime_cli"]
    if executable := os.environ.get("MIR2X_FROZEN_EXE"):
        command = [executable]
    report = root / "safety.jsonl"
    events = EventLog()
    with (root / "stderr.log").open("wb") as errors:
        process = await asyncio.create_subprocess_exec(
            *command,
            "--root",
            str(root),
            "mcp",
            "supervise",
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

        async def drain():
            while line := await process.stdout.readline():
                events.append(json.loads(line))

        reader = asyncio.create_task(drain())
        try:
            await wait_for(events, lambda event: event["event"] == "monitor_ready", seconds=10)
            test = SafetyHarness()
            test.session, test.events = model, events

            async def submit(command):
                process.stdin.write((json.dumps(command) + "\n").encode())
                await process.stdin.drain()

            for number in range(1, 4):
                purchase = test.purchase_command(number)
                await submit(purchase)
                await wait_for(
                    events,
                    lambda row, intent=purchase["intent_id"]: (
                        row["event"] == "purchase_result" and row["intent_id"] == intent
                    ),
                    seconds=8,
                )
            assert model.potions == 3 and model.gold == 760
            assert all(
                row["purchase_confirmed"] for row in events if row["event"] == "purchase_result"
            )
            model.target["kind"] = "monster"
            scout = test.arm_command()
            scout.update(command="scout", destination={"x": 2, "y": 1})
            scout.pop("retreat")
            await submit(scout)
            await wait_for(
                events, lambda event: event.get("reason") == "scout_finished_hunting_still_disabled"
            )
            model.target["position"]["x"] = 3
            model.terminal = "unknown"
            arm = test.arm_command()
            arm["retreat"] = [{"x": 2, "y": 1}, {"x": 1, "y": 1}]
            await submit(arm)
            await wait_for(events, lambda event: event["event"] == "armed")
            await submit(
                {
                    "command": "attack",
                    "target_handle": "target-2",
                    "intent_id": "00000000-0000-4000-8000-000000000010",
                }
            )
            await wait_for(
                events, lambda event: event.get("reason") == "write_uncertain_no_replay", seconds=5
            )
            await submit(test.arm_command())
            await asyncio.sleep(0.3)
            fake.drop_method = "game_get_state"
            assert await asyncio.wait_for(process.wait(), 5) == 2
            await reader
            assert fake.auths == 1
            writes = [
                row
                for row in fake.calls
                if row["method"]
                in {"game_attack", "game_move_to", "game_use_item", "game_shop_buy", "game_stop"}
            ]
            assert [row["method"] for row in writes] == ["game_shop_buy"] * 3 + [
                "game_move_to",
                "game_attack",
            ]
            assert writes[-1]["params"]["mode"] == "once" and writes[-1]["timeout_ms"] == 2000
            assert any(event.get("writes_locked") == "operation_unknown" for event in events)
            assert fake.token not in report.read_text("utf-8")
            if sample := os.environ.get("MIR2X_SUPERVISION_SAMPLE"):
                from pathlib import Path

                await asyncio.to_thread(Path(sample).write_text, report.read_text("utf-8"), "utf-8")
        finally:
            if process.returncode is None:
                process.kill()
                await process.wait()
            await reader
