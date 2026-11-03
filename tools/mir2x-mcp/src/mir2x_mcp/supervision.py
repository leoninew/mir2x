"""Continuous supervision using one explicitly bound official MCP SDK session."""

import asyncio
import contextlib
import json
import re
import sys
import threading
import time
import uuid
from collections import deque
from collections.abc import AsyncIterator, Callable
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path

from mcp.client.session import ClientSession
from mcp.client.stdio import StdioServerParameters, stdio_client

from .contract import METHODS, ContractError, validate_arguments

READ_ONLY = frozenset(name for name, spec in METHODS.items() if not spec["action"]) - {"game_stop"}
PERIOD = 0.25
MAX_AGE = 0.5


@dataclass(frozen=True)
class Binding:
    instance_id: str
    character_uid: str

    def __post_init__(self):
        if not re.fullmatch(r"[0-9a-f]{32}", self.instance_id):
            raise ValueError("instance_id must be 32 lowercase hexadecimal characters")
        if not re.fullmatch(r"[1-9][0-9]{0,19}", self.character_uid):
            raise ValueError("character_uid must be a nonzero unsigned decimal UID")
        if int(self.character_uid) > 2**64 - 1:
            raise ValueError("character_uid exceeds uint64")


class ReadBudget:
    """Reserve monitor capacity while limiting optional commands to four per second."""

    def __init__(self):
        self.calls = deque()
        self.commands = deque()

    async def acquire(self, stop: asyncio.Event, *, monitor: bool):
        while not stop.is_set():
            now = time.monotonic()
            for entries in (self.calls, self.commands):
                while entries and entries[0] <= now - 1:
                    entries.popleft()
            if len(self.calls) < 16 and (monitor or len(self.commands) < 4):
                self.calls.append(now)
                if not monitor:
                    self.commands.append(now)
                return True
            await wait_until(stop, now + 0.025)
        return False

    async def room(self, stop: asyncio.Event, count: int):
        """Start a short preflight only when its optional-call allowance is free."""
        while not stop.is_set():
            now = time.monotonic()
            for entries in (self.calls, self.commands):
                while entries and entries[0] <= now - 1:
                    entries.popleft()
            if len(self.calls) <= 16 - count and len(self.commands) <= 4 - count:
                return
            await wait_until(stop, now + 0.025)
        raise ValueError("stopping")


async def wait_until(stop: asyncio.Event, deadline: float):
    with contextlib.suppress(TimeoutError):
        await asyncio.wait_for(stop.wait(), max(0, deadline - time.monotonic()))


class Observer:
    def __init__(self, session, binding: Binding, emit: Callable[[dict], None], stop):
        self.session = session
        self.binding = binding
        self.emit = emit
        self.stop = stop
        self.budget = ReadBudget()
        self.context = None
        self.failed = False
        self.alerts = set()
        self.started = time.monotonic()
        self.last = dict.fromkeys(("session", "state", "nearby"))
        self.health = deque()
        self.control = None
        self.position = None
        self.snapshots = {}
        self.mode = "observe"

    def event(self, event, **fields):
        self.emit(
            {
                "time": datetime.now(UTC).isoformat(),
                "monotonic": time.monotonic(),
                "event": event,
                "instance_id": self.binding.instance_id,
                "character_uid": self.binding.character_uid,
                "context": self.context,
                "mode": self.mode,
                "write_enabled": False,
                **fields,
            }
        )

    def alert(self, reason, **fields):
        if reason not in self.alerts:
            self.alerts.add(reason)
            self.event("alert", reason=reason, **fields)

    def fail(self, reason):
        self.failed = True
        self.alert(reason)
        self.stop.set()

    async def read(self, name, arguments, *, monitor):
        if not await self.budget.acquire(self.stop, monitor=monitor):
            return None
        started = time.monotonic()
        try:
            result = await self.session.call_tool(name, arguments)
            payload = result.structured_content
            if not isinstance(payload, dict):
                self.fail("invalid_tool_response")
                return None
            self.event("response", tool=name, response=payload, started_monotonic=started)
            if result.is_error or not payload.get("ok"):
                code = payload.get("error", {}).get("code", "unavailable")
                if code == "transport_lost" or name == "game_get_session":
                    self.fail("connection_unavailable")
                else:
                    self.alert("query_unavailable", tool=name, code=code)
                return None
            self.snapshots[name] = (payload, started)
            return payload, started
        except Exception as exc:
            self.event("query_exception", tool=name, exception_type=type(exc).__name__)
            self.fail("connection_unavailable")
            return None

    def same_context(self, payload):
        if payload.get("context") != self.context:
            self.alert("inconsistent_observation")
            return False
        return True

    async def poll(self):
        next_tick = time.monotonic()
        try:
            while not self.stop.is_set():
                result = await self.read("game_get_session", {}, monitor=True)
                if result is None:
                    if not self.stop.is_set():
                        self.fail("session_unavailable")
                    return
                payload, started = result
                data = payload["data"]
                if data.get("instance_id") != self.binding.instance_id:
                    self.fail("instance_mismatch")
                    return
                uid = data.get("character_uid")
                if uid is not None and uid != self.binding.character_uid:
                    self.fail("character_mismatch")
                    return
                current = data.get("context")
                if current != payload.get("context"):
                    self.fail("inconsistent_session")
                    return
                if self.context is not None and current != self.context:
                    self.alert("context_changed", previous_context=self.context)
                    self.health.clear()
                self.context = current
                self.last["session"] = started
                control = (data.get("control_state"), data.get("pause_reason"), data.get("phase"))
                if control != self.control:
                    self.control = control
                    self.event("control", control_state=control[0], pause_reason=control[1])
                if control[0] != "active":
                    self.alert(
                        "control_not_active", control_state=control[0], pause_reason=control[1]
                    )
                if data.get("phase") == "run" and current is not None and uid is not None:
                    await self.observe_world()
                else:
                    self.alert("not_in_game")
                next_tick = max(next_tick + PERIOD, time.monotonic())
                await wait_until(self.stop, next_tick)
        except Exception as exc:
            self.event("monitor_exception", exception_type=type(exc).__name__)
            self.fail("monitor_failed")

    async def observe_world(self):
        observations = await asyncio.gather(
            self.read("game_get_state", {}, monitor=True),
            self.read("game_list_nearby", {"kind": "all", "radius": 15}, monitor=True),
        )
        state = observations[0]
        if state is not None and self.same_context(state[0]):
            data = state[0]["data"]
            if data.get("character_uid") != self.binding.character_uid:
                self.fail("character_mismatch")
                return
            self.last["state"] = state[1]
            self.position = data.get("position")
            hp, maximum = data.get("hp"), data.get("max_hp")
            known = data.get("known_states", [])
            if "dead" in known or hp == 0:
                self.alert("death")
            if (
                type(hp) is not int
                or type(maximum) is not int
                or maximum <= 0
                or "alive" not in known
            ):
                self.alert("health_unknown")
            else:
                now = time.monotonic()
                self.health.append((now, hp))
                while self.health and self.health[0][0] < now - 1:
                    self.health.popleft()
                if hp <= maximum * 0.8:
                    self.alert("low_hp", hp=hp, max_hp=maximum, urgent=hp <= maximum * 0.5)
                if hp <= maximum * 0.5:
                    self.alert("critical_hp", hp=hp, max_hp=maximum)
                if max(value for _, value in self.health) - hp >= maximum * 0.05:
                    self.alert("rapid_hp_loss", hp=hp, max_hp=maximum)
            self.event(
                "health",
                hp=hp,
                max_hp=maximum,
                known_states=known,
                position=data.get("position"),
                action=data.get("action"),
                map_uid=data.get("map_uid"),
            )
        if observations[1] is not None and self.same_context(observations[1][0]):
            payload, started = observations[1]
            self.last["nearby"] = started
            data = payload["data"]
            if data.get("next_cursor") is not None:
                self.alert("nearby_coverage_incomplete")
            if self.position is not None:
                close = [
                    row
                    for row in data.get("items", [])
                    if row.get("kind") == "monster"
                    and row.get("alive") is not False
                    and max(
                        abs(row["position"]["x"] - self.position["x"]),
                        abs(row["position"]["y"] - self.position["y"]),
                    )
                    <= 3
                ]
                if len(close) >= 2:
                    self.alert("multiple_nearby_monsters", count=len(close), aggression_known=False)
            self.event("nearby", observation=data, aggression_known=False)

    async def watchdog(self):
        stale = False
        ready = False
        while not self.stop.is_set():
            now = time.monotonic()
            ages = {
                name: now - (observed if observed is not None else self.started)
                for name, observed in self.last.items()
            }
            expired = any(age > MAX_AGE for age in ages.values())
            if not ready and not expired and all(value is not None for value in self.last.values()):
                ready = True
                self.event("monitor_ready", ages_seconds=ages)
            if expired and not stale:
                self.event("alert", reason="monitor_stale", ages_seconds=ages)
            elif stale and not expired:
                self.event("monitor_fresh", requires_review=True)
            stale = expired
            self.check_safety()
            await wait_until(self.stop, now + 0.025)

    def check_safety(self):
        pass

    def input_closed(self):
        self.stop.set()

    async def handle_command(self, command):
        if command.get("command") == "quit":
            self.input_closed()
        elif command.get("command") == "observe":
            self.event("status", pending_actions=0)
        elif (
            command.get("command") == "call"
            and isinstance(command.get("name"), str)
            and command["name"] in READ_ONLY
        ):
            name = command["name"]
            arguments = command.get("arguments", {})
            try:
                validate_arguments(name, arguments)
            except ContractError:
                self.alert("invalid_command")
                return
            await self.read(name, arguments, monitor=False)
        else:
            self.alert("write_or_unknown_command_rejected")

    async def commands(self, commands):
        iterator = aiter(commands)
        while not self.stop.is_set():
            incoming = asyncio.create_task(anext(iterator))
            stopped = asyncio.create_task(self.stop.wait())
            try:
                done, _ = await asyncio.wait(
                    (incoming, stopped), return_when=asyncio.FIRST_COMPLETED
                )
                if stopped in done:
                    return
                try:
                    command = incoming.result()
                except StopAsyncIteration:
                    self.input_closed()
                    return
            finally:
                for task in (incoming, stopped):
                    if not task.done():
                        task.cancel()
                await asyncio.gather(incoming, stopped, return_exceptions=True)
            if not isinstance(command, dict):
                self.alert("invalid_command")
                continue
            await self.handle_command(command)


# Native itemrecord.inc entries 6..9 are the four HP recovery potions. A name
# alone is untrusted; require both the canonical ID/name and available inventory.
HP_POTIONS = {
    6: "\u91d1\u521b\u836f\uff08\u5c0f\uff09",
    7: "\u91d1\u521b\u836f\uff08\u4e2d\uff09",
    8: "\u91d1\u521b\u836f\uff08\u5927\uff09",
    9: "\u91d1\u521b\u836f\uff08\u7279\uff09",
}
TRAIL_AGE = 120


def grid(value):
    if not isinstance(value, dict) or set(value) != {"x", "y"}:
        raise ValueError("invalid_grid")
    point = value["x"], value["y"]
    if any(type(v) is not int or not 0 <= v <= 32767 for v in point):
        raise ValueError("invalid_grid")
    return point


def distance(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


class ArmedSupervisor(Observer):
    """The only writer; the inherited poller/watchdog never wait on this worker."""

    def __init__(self, *args):
        super().__init__(*args)
        self.mode = "supervise"
        self.plan = None
        self.version = 0
        self.locked = None
        self.hunting = False
        self.recover = False
        self.pending = None
        self.pending_since = None
        self.halt = False
        self.exiting = False
        self.work = asyncio.Queue(maxsize=1)
        self.wake = asyncio.Event()
        self.attack_intents = set()
        self.purchase_intents = set()
        self.purchase_submissions = 0
        self.last_attack = {}
        self.trail_scope = None
        self.visited = {}
        self.edges = {}
        self.previous_point = None
        self.previous_sample = None
        self.walk_edge = None

    def invalidate(self, reason, *, lock=False):
        had_plan = self.plan is not None
        self.plan = None
        self.hunting = self.recover = False
        self.version += 1
        if lock and self.locked is None:
            self.locked = reason
        while not self.work.empty():
            self.work.get_nowait()
        if had_plan or lock:
            self.event(
                "plan_invalidated", reason=reason, writes_locked=self.locked, operation=self.pending
            )
        self.wake.set()

    def world(self):
        now = time.monotonic()
        for name in ("game_get_session", "game_get_state", "game_list_nearby"):
            response, started = self.snapshots.get(name, ({}, 0))
            if now - started > MAX_AGE or response.get("context") != self.context:
                raise ValueError("observations_not_fresh")
        session = self.snapshots["game_get_session"][0]["data"]
        state = self.snapshots["game_get_state"][0]["data"]
        nearby = self.snapshots["game_list_nearby"][0]["data"]
        if (
            self.stop.is_set()
            or self.failed
            or self.exiting
            or self.locked
            or session.get("instance_id") != self.binding.instance_id
            or session.get("character_uid") != self.binding.character_uid
            or state.get("character_uid") != self.binding.character_uid
            or session.get("context") != self.context
            or not self.context
            or session.get("phase") != "run"
            or session.get("control_state") != "active"
            or session.get("pause_reason") is not None
        ):
            raise ValueError("control_or_identity_invalid")
        if (
            type(state.get("hp")) is not int
            or state["hp"] <= 0
            or type(state.get("max_hp")) is not int
            or state["max_hp"] <= 0
            or "alive" not in state.get("known_states", [])
            or "dead" in state.get("known_states", [])
        ):
            raise ValueError("life_unknown_or_dead")
        if nearby.get("next_cursor") is not None or "items" not in nearby:
            raise ValueError("nearby_coverage_incomplete")
        grid(state.get("position"))
        return state, nearby

    def guard(self, version):
        state, nearby = self.world()
        if (
            self.plan is None
            or version != self.version
            or self.plan["context"] != self.context
            or self.plan["map_uid"] != state["map_uid"]
        ):
            raise ValueError("plan_no_longer_valid")
        return state, nearby

    def danger(self, state):
        hp, maximum = state["hp"], state["max_hp"]
        return hp <= maximum * 0.8 or any(
            stamp >= time.monotonic() - 1 and value - hp >= maximum * 0.05
            for stamp, value in self.health
        )

    def record_trail(self):
        response, started = self.snapshots.get("game_get_state", ({}, 0))
        if time.monotonic() - started > MAX_AGE or started == self.previous_sample:
            return
        self.previous_sample = started
        state = response.get("data", {})
        if "alive" not in state.get("known_states", []) or state.get("hp", 0) <= 0:
            self.visited.clear()
            self.edges.clear()
            self.previous_point = None
            self.walk_edge = None
            return
        context = response.get("context") or {}
        scope = (context.get("session_id"), context.get("world_epoch"), state.get("map_uid"))
        if scope != self.trail_scope:
            self.visited.clear()
            self.edges.clear()
            self.previous_point = None
            self.walk_edge = None
            self.trail_scope = scope
        point = grid(state.get("position"))
        endpoint = grid(state.get("action_endpoint"))
        # Public motion_22 / motion_31 are native walk / mounted walk (motion.hpp).
        # Adjacent idle samples alone may be corrections; they are not a traversed edge.
        if state.get("action") in ("motion_22", "motion_31"):
            if (
                self.previous_point is not None
                and distance(self.previous_point, endpoint) == 1
                and point in (self.previous_point, endpoint)
            ):
                self.walk_edge = (self.previous_point, endpoint, started)
            return
        if state.get("action") != "idle":
            self.walk_edge = None
            return
        if endpoint != point:
            return
        if (
            self.walk_edge is not None
            and self.walk_edge[:2] == (self.previous_point, point)
            and started - self.walk_edge[2] <= MAX_AGE
        ):
            self.edges[frozenset((self.previous_point, point))] = started
            self.event(
                "traversed_edge",
                start=list(self.previous_point),
                end=list(point),
                source="observed_native_walk_and_stopped_arrival",
            )
        self.walk_edge = None
        self.previous_point = point
        self.visited[point] = started
        cutoff = time.monotonic() - TRAIL_AGE
        self.visited = {p: t for p, t in self.visited.items() if t >= cutoff}
        self.edges = {p: t for p, t in self.edges.items() if t >= cutoff}

    def check_safety(self):
        try:
            self.record_trail()
            if self.pending_since is not None and time.monotonic() - self.pending_since > 2.5:
                if not self.locked:
                    self.invalidate("operation_deadline_unresolved", lock=True)
            if self.plan is None:
                return
            state, _ = self.guard(self.version)
            if self.plan["phase"] == "scout" and self.danger(state):
                self.invalidate("scout_health_changed")
                self.event("alert", reason="no_safe_action", detail="scout_aborted_no_hunting")
                return
            if self.hunting and self.danger(state):
                self.hunting = False
                self.recover = True
                self.event(
                    "hunting_locked",
                    reason="low_or_falling_hp",
                    hp=state["hp"],
                    pending_operation=self.pending,
                    automatic_resume=False,
                )
                self.wake.set()
        except (KeyError, TypeError, ValueError) as exc:
            if self.plan is not None:
                self.invalidate(str(exc))

    def input_closed(self):
        self.invalidate("input_closed")
        self.exiting = True
        self.halt = self.pending_since is not None
        self.wake.set()

    async def handle_command(self, command):
        kind = command.get("command")
        if kind == "observe":
            self.event(
                "status",
                pending_actions=int(self.pending_since is not None),
                pending_operation=self.pending,
                hunting_enabled=self.hunting,
                plan_loaded=self.plan is not None,
                writes_locked=self.locked,
            )
        elif kind in ("quit", "disarm", "stop"):
            self.invalidate(kind)
            self.halt = self.pending_since is not None or kind == "stop"
            self.exiting |= kind == "quit"
            self.wake.set()
        elif kind == "trail":
            self.event(
                "trail",
                scope=self.trail_scope,
                max_age_seconds=TRAIL_AGE,
                points=[{"x": p[0], "y": p[1]} for p in self.visited],
                edges=[[list(p) for p in edge] for edge in self.edges],
            )
        elif kind in ("arm", "attack", "scout", "purchase"):
            if self.work.full() or self.pending_since is not None or self.recover:
                self.event("command_rejected", reason="scheduler_busy")
                return
            self.work.put_nowait((command, self.version))
            self.wake.set()
        elif kind == "call" and (self.plan is not None or self.pending_since is not None):
            self.event("command_rejected", reason="optional_queries_wait_until_disarmed")
        else:
            await super().handle_command(command)

    async def query(self, name, arguments, version):
        result = await self.read(name, arguments, monitor=False)
        self.guard(version)
        if result is None or result[0].get("context") != self.context:
            raise ValueError("preflight_query_unavailable")
        if time.monotonic() - result[1] > MAX_AGE:
            raise ValueError("preflight_query_stale")
        return result[0]["data"]

    def available_potions(self, data, keys):
        if data.get("next_cursor") is not None or "inventory" in data.get("unavailable_fields", []):
            raise ValueError("inventory_coverage_incomplete")
        doses = []
        for key in keys:
            rows = [
                row
                for row in data.get("items", [])
                if (row.get("item_id"), row.get("seq_id")) == key
                and row.get("location") == "inventory"
            ]
            if len(rows) != 1:
                raise ValueError("potion_instance_missing_or_ambiguous")
            row = rows[0]
            if row.get("name") != HP_POTIONS.get(key[0]) or key[0] not in HP_POTIONS:
                raise ValueError("not_a_verified_hp_potion")
            count, reserved = row.get("count"), row.get("reserved_count")
            if type(count) is not int or type(reserved) is not int or not 0 <= reserved <= count:
                raise ValueError("potion_count_unknown")
            doses.append((key, count - reserved))
        return doses

    async def check_route(self, route, version, *, witnessed=True):
        state, nearby = self.guard(version)
        if grid(state["position"]) != route[0]:
            raise ValueError("retreat_origin_changed")
        cutoff = time.monotonic() - TRAIL_AGE
        for a, b in zip(route, route[1:], strict=False):
            if distance(a, b) != 1 or (witnessed and self.edges.get(frozenset((a, b)), 0) < cutoff):
                raise ValueError("retreat_edge_not_observed_traversed")
        x, y = min(p[0] for p in route), min(p[1] for p in route)
        region = await self.query(
            "game_get_map_region",
            {
                "x": x,
                "y": y,
                "width": max(p[0] for p in route) - x + 1,
                "height": max(p[1] for p in route) - y + 1,
            },
            version,
        )
        if region.get("map_uid") != self.plan["map_uid"]:
            raise ValueError("retreat_map_changed")
        cells = {(row["x"], row["y"]): row for row in region.get("cells", [])}
        required = set(route[1:])
        for a, b in zip(route, route[1:], strict=False):
            if a[0] != b[0] and a[1] != b[1]:
                required.update(((a[0], b[1]), (b[0], a[1])))
        for point in required:
            cell = cells.get(point, {})
            if cell.get("walkable") is not True or cell.get("dynamic_blocked") is not False:
                raise ValueError("retreat_cell_not_clear")
        _, nearby = self.guard(version)
        for row in nearby["items"]:
            if row.get("kind") in ("monster", "player", "npc") and row.get("alive") is not False:
                if any(distance(grid(row["position"]), point) <= 1 for point in route[1:]):
                    raise ValueError("retreat_near_known_creature")

    async def arm(self, command):
        self.invalidate("explicit_rearm")
        state, _ = self.world()
        scouting = command["command"] == "scout"
        if command.get("context") != self.context or command.get("map_uid") != state["map_uid"]:
            raise ValueError("arm_context_or_map_mismatch")
        if self.danger(state) or state.get("action") != "idle":
            raise ValueError("arm_requires_stopped_healthy_state")
        route = (
            (grid(state["position"]), grid(command.get("destination")))
            if scouting
            else tuple(grid(point) for point in command.get("retreat", []))
        )
        if not 2 <= len(route) <= 5 or len(set(route)) != len(route):
            raise ValueError("retreat_requires_one_to_four_traversed_steps")
        keys = tuple((row["item_id"], row["seq_id"]) for row in command.get("potions", []))
        if not 1 <= len(keys) <= 8 or len(set(keys)) != len(keys):
            raise ValueError("invalid_potion_selection")
        if any(type(v) is not int or not 0 <= v <= 2**32 - 1 for key in keys for v in key):
            raise ValueError("invalid_potion_selection")
        self.plan = {
            "context": dict(self.context),
            "map_uid": state["map_uid"],
            "route": route,
            "potions": keys,
            "phase": "scout" if scouting else "arming",
        }
        version = self.version
        await self.budget.room(self.stop, 3 if scouting else 2)
        data = await self.query("game_get_inventory", {}, version)
        doses = sum(count for _, count in self.available_potions(data, keys))
        if doses < 3:
            raise ValueError("at_least_three_verified_doses_required")
        await self.check_route(route, version, witnessed=not scouting)
        state, _ = self.guard(version)
        if self.danger(state):
            raise ValueError("health_changed_during_arm")
        if scouting:
            operation = await self.action(
                "game_move_to", {"x": route[1][0], "y": route[1][1], "tolerance": 0}, version
            )
            await self.arrival(operation, route[1], version)
            self.edges[frozenset(route)] = time.monotonic()
            self.event(
                "traversed_edge",
                start=list(route[0]),
                end=list(route[1]),
                source="submitted_public_move_and_fresh_stopped_arrival",
            )
            self.invalidate("scout_finished_hunting_still_disabled")
            return
        self.plan["phase"] = "armed"
        self.hunting = True
        self.event(
            "armed",
            write_enabled=True,
            doses=doses,
            retreat=[list(p) for p in route],
            attack_mode="once",
            attack_timeout_ms=2000,
            automatic_resume=False,
        )

    async def global_stop(self):
        self.halt = False
        # This is a global pause, never a cancel-only prelude to potion/retreat.
        if self.locked or self.failed:
            self.event("alert", reason="stop_unavailable_writes_locked", operation=self.pending)
            return
        expected = self.pending["context"] if self.pending is not None else self.context
        self.invalidate("global_stop", lock=True)
        if not await self.budget.acquire(self.stop, monitor=False):
            return
        response, started = self.snapshots.get("game_get_session", ({}, 0))
        if (
            time.monotonic() - started > MAX_AGE
            or response.get("context") != expected
            or response.get("data", {}).get("control_state") != "active"
        ):
            self.event("alert", reason="stop_context_not_fresh_no_write")
            return
        try:
            response = await self.session.call_tool("game_stop", {})
            self.event("global_stop", response=response.structured_content, retreat_allowed=False)
        except Exception as exc:
            self.event("alert", reason="stop_response_unknown", exception_type=type(exc).__name__)

    async def action(self, name, params, version):
        if not await self.budget.acquire(self.stop, monitor=False):
            raise ValueError("stopping")
        state, _ = self.guard(version)
        if name == "game_attack" and (not self.hunting or self.danger(state)):
            raise ValueError("hunting_locked")
        for prerequisite in (
            ("game_get_inventory", "game_shop_list")
            if name == "game_shop_buy"
            else ("game_get_inventory", "game_get_map_region")
            if name == "game_attack"
            else ("game_get_inventory",)
            if name == "game_use_item"
            else ("game_get_inventory", "game_get_map_region")
            if self.plan["phase"] == "scout"
            else ("game_get_map_region",)
        ):
            snapshot, started = self.snapshots.get(prerequisite, ({}, 0))
            if time.monotonic() - started > MAX_AGE or snapshot.get("context") != self.context:
                raise ValueError("write_preflight_expired")
        if name == "game_shop_buy":
            self.purchase_guard(version)
            if self.purchase_submissions >= 3:
                raise ValueError("purchase_three_single_attempt_limit")
            if int(self.plan["quote"]["expires_at"]) <= time.time() * 1000 + 2000:
                raise ValueError("purchase_quote_expiring")
        if name == "game_attack":
            target = next(
                (
                    row
                    for row in self.guard(version)[1]["items"]
                    if row.get("target_handle") == params["target_handle"]
                ),
                {},
            )
            if (
                target.get("kind") != "monster"
                or target.get("alive") is not True
                or type(target.get("hp")) is not int
                or target["hp"] <= 0
                or distance(grid(target.get("position")), grid(state["position"])) != 1
            ):
                raise ValueError("target_changed_before_send")
        arguments = {
            **params,
            "context": dict(self.context),
            "request_id": str(uuid.uuid4()),
            "timeout_ms": 2000,
        }
        validate_arguments(name, arguments)
        if name == "game_shop_buy":
            self.purchase_submissions += 1
        self.pending_since = time.monotonic()
        self.pending = {
            "method": name,
            "request_id": arguments["request_id"],
            "context": dict(self.context),
            "operation_id": None,
        }
        self.event("write_submitting", tool=name, arguments=arguments, write_enabled=True)
        try:
            result = await self.session.call_tool(name, arguments)
            payload = result.structured_content
            self.event("write_response", tool=name, response=payload)
            if not isinstance(payload, dict):
                raise ValueError("invalid_write_response")
            if result.is_error or not payload.get("ok"):
                code = payload.get("error", {}).get("code")
                if payload.get("error", {}).get("details", {}).get("side_effects_possible"):
                    raise ValueError("write_error_side_effects_unresolved")
                if code in (
                    "busy",
                    "paused",
                    "stale_context",
                    "unavailable",
                    "policy_denied",
                    "invalid_argument",
                    "target_lost",
                    "need_move",
                    "stale_target",
                    "budget_exceeded",
                ):
                    self.event("action_not_admitted", tool=name, code=code, automatic_retries=0)
                    return None
                raise ValueError("write_response_uncertain")
            operation = payload.get("data", {}).get("operation_id")
            if (
                not isinstance(operation, str)
                or not operation
                or payload.get("context") != arguments["context"]
            ):
                raise ValueError("operation_identity_invalid")
            self.pending["operation_id"] = operation
            self.event("operation_accepted", operation=dict(self.pending))
            while not self.stop.is_set():
                if self.halt:
                    await self.global_stop()
                    return None
                result = await self.read(
                    "game_get_operation", {"operation_id": operation}, monitor=False
                )
                if result is None:
                    raise ValueError("operation_observation_unavailable")
                payload = result[0]
                data = payload["data"]
                if (
                    data.get("operation_id") != operation
                    or data.get("request_id") != arguments["request_id"]
                    or data.get("method") != name
                    or data.get("context") != arguments["context"]
                ):
                    raise ValueError("operation_identity_invalid")
                self.event("operation", operation=data)
                status = data.get("state")
                if status == "unknown":
                    raise ValueError("operation_unknown")
                if status in ("succeeded", "failed", "cancelled"):
                    if status != "succeeded" and data.get("side_effects_possible"):
                        raise ValueError("terminal_side_effects_unresolved")
                    return data
                if status not in ("accepted", "running") or self.locked:
                    raise ValueError("operation_deadline_unresolved")
                await wait_until(self.stop, time.monotonic() + 0.15)
            raise ValueError("operation_unresolved_at_shutdown")
        except Exception as exc:
            self.invalidate(str(exc), lock=True)
            self.event("alert", reason="write_uncertain_no_replay", operation=self.pending)
            return None
        finally:
            self.pending = self.pending_since = None

    def purchase_guard(self, version):
        state, nearby = self.guard(version)
        if self.plan["phase"] != "purchase" or state.get("action") != "idle":
            raise ValueError("purchase_requires_stopped_preparation")
        if self.danger(state) or any(row.get("kind") == "monster" for row in nearby["items"]):
            raise ValueError("purchase_requires_safe_surroundings")
        gold = state.get("gold")
        if (
            not isinstance(gold, str)
            or not re.fullmatch(r"0|[1-9][0-9]{0,19}", gold)
            or "gold" in state.get("unavailable_fields", [])
        ):
            raise ValueError("purchase_gold_unknown")
        return state

    @staticmethod
    def purchased_count(data, item_id):
        if data.get("next_cursor") is not None or data.get("unavailable_fields"):
            raise ValueError("purchase_inventory_incomplete")
        count = 0
        for row in data["items"]:
            if row.get("item_id") == item_id and row.get("location") == "inventory":
                if type(row.get("count")) is not int or row["count"] < 0:
                    raise ValueError("purchase_inventory_count_unknown")
                count += row["count"]
        return count

    async def purchase(self, command, version):
        if self.plan is not None or version != self.version:
            raise ValueError("purchase_requires_disarmed_preparation")
        required = {
            "command",
            "context",
            "shop_handle",
            "item_id",
            "count",
            "max_total_gold",
            "intent_id",
        }
        if set(command) != required:
            raise ValueError("invalid_purchase_fields")
        if (
            type(command["item_id"]) is not int
            or command["item_id"] != 6
            or type(command["count"]) is not int
            or command["count"] != 1
            or not isinstance(command["max_total_gold"], str)
            or not re.fullmatch(r"[1-9][0-9]?", command["max_total_gold"])
            or int(command["max_total_gold"]) > 80
            or not isinstance(command["shop_handle"], str)
            or not 1 <= len(command["shop_handle"]) <= 128
        ):
            raise ValueError("purchase_only_item6_count1_up_to80gold")
        intent = command["intent_id"]
        if not isinstance(intent, str) or not re.fullmatch(
            r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}", intent
        ):
            raise ValueError("explicit_purchase_intent_id_required")
        if intent in self.purchase_intents or len(self.purchase_intents) >= 4096:
            raise ValueError("purchase_intent_already_seen_or_limit_reached")
        if self.purchase_submissions >= 3:
            raise ValueError("purchase_three_single_attempt_limit")
        state, _ = self.world()
        if command["context"] != self.context:
            raise ValueError("purchase_context_mismatch")
        self.purchase_intents.add(intent)
        self.plan = {
            "context": dict(self.context),
            "map_uid": state["map_uid"],
            "phase": "purchase",
        }
        self.purchase_guard(version)
        deadline = time.monotonic() + 8
        while True:
            # Keep inventory, exact quote and buy in one scheduler execution.
            # Only read refreshes repeat; a purchase write is never retried.
            await self.budget.room(self.stop, 3)
            self.purchase_guard(version)
            inventory = await self.query("game_get_inventory", {}, version)
            before_count = self.purchased_count(inventory, command["item_id"])
            quotes = await self.query(
                "game_shop_list",
                {"shop_handle": command["shop_handle"], "item_id": command["item_id"]},
                version,
            )
            state = self.purchase_guard(version)
            if time.monotonic() > deadline or quotes.get("last_query_error") is not None:
                raise ValueError("purchase_quote_refresh_unavailable")
            if quotes.get("refresh_pending") is True:
                await wait_until(self.stop, time.monotonic() + 0.15)
                continue
            rows = [
                row for row in quotes.get("items", []) if row.get("item_id") == command["item_id"]
            ]
            if (
                quotes.get("refresh_pending") is not False
                or quotes.get("next_cursor") is not None
                or quotes.get("unavailable_fields")
                or len(rows) != 1
            ):
                raise ValueError("purchase_quote_missing_or_ambiguous")
            quote = rows[0]
            if (
                quote.get("direction") != "buy"
                or quote.get("unit_count") != 1
                or quote.get("materials") != []
                or int(quote["gold"]) > min(int(command["max_total_gold"]), int(state["gold"]))
                or int(quote["gold"]) < 0
                or int(quote["expires_at"]) <= time.time() * 1000 + 2000
            ):
                raise ValueError("purchase_quote_terms_not_allowed")
            self.plan["quote"] = quote
            gold_before = int(state["gold"])
            self.event(
                "purchase_quote_selected",
                intent_id=intent,
                quote=quote,
                shop_handle=command["shop_handle"],
            )
            operation = await self.action(
                "game_shop_buy",
                {
                    "quote_handle": quote["quote_handle"],
                    "count": 1,
                    "max_total_gold": command["max_total_gold"],
                },
                version,
            )
            break
        if self.locked or self.plan is None:
            return
        # Server acknowledgement and delivered inventory/gold are separate facts.
        deadline = time.monotonic() + 2
        while True:
            inventory = await self.query("game_get_inventory", {}, version)
            state = await self.query("game_get_state", {}, version)
            self.purchase_guard(version)
            after_count = self.purchased_count(inventory, command["item_id"])
            gold_after = int(state["gold"])
            delivered = after_count - before_count == 1 and gold_before - gold_after == int(
                quote["gold"]
            )
            confirmed = (operation or {}).get("state") == "succeeded" and delivered
            if (
                confirmed
                or (operation or {}).get("state") != "succeeded"
                or time.monotonic() >= deadline
            ):
                break
            await wait_until(self.stop, time.monotonic() + 0.15)
        self.event(
            "purchase_result",
            intent_id=intent,
            item_id=command["item_id"],
            count=1,
            operation=operation,
            inventory_before=before_count,
            inventory_after=after_count,
            gold_before=str(gold_before),
            gold_after=str(gold_after),
            delivery_observed=delivered,
            purchase_confirmed=confirmed,
            healing_attributed=False,
            automatic_retries=0,
        )
        self.invalidate(
            "purchase_finished" if confirmed else "purchase_not_confirmed",
            lock=bool(operation and not confirmed),
        )

    async def attack(self, command, version):
        state, nearby = self.guard(version)
        if not self.hunting or self.danger(state) or state.get("action") != "idle":
            raise ValueError("hunting_not_ready")
        rows = [
            row
            for row in nearby["items"]
            if row.get("target_handle") == command.get("target_handle")
        ]
        if len(rows) != 1:
            raise ValueError("fresh_target_missing")
        target = rows[0]
        if (
            target.get("kind") != "monster"
            or target.get("alive") is not True
            or type(target.get("hp")) is not int
            or target["hp"] <= 0
            or not target.get("uid")
            or distance(grid(target["position"]), grid(state["position"])) != 1
        ):
            raise ValueError("target_requires_known_alive_adjacent_monster")
        intent = command.get("intent_id")
        if not isinstance(intent, str) or not re.fullmatch(
            r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}", intent
        ):
            raise ValueError("explicit_attack_intent_id_required")
        if intent in self.attack_intents or len(self.attack_intents) >= 4096:
            raise ValueError("attack_intent_already_seen_or_limit_reached")
        if self.snapshots["game_list_nearby"][1] <= self.last_attack.get(target["uid"], 0):
            raise ValueError("new_target_observation_required_after_attack")
        self.attack_intents.add(intent)
        # Recheck reserves and the witnessed retreat before every single attack.
        await self.budget.room(self.stop, 3)
        data = await self.query("game_get_inventory", {}, version)
        if sum(count for _, count in self.available_potions(data, self.plan["potions"])) < 3:
            raise ValueError("potion_reserve_below_three")
        await self.check_route(self.plan["route"], version)
        state, nearby = self.guard(version)
        current = next((row for row in nearby["items"] if row.get("uid") == target["uid"]), {})
        if (
            current.get("target_handle") != target["target_handle"]
            or current.get("alive") is not True
            or distance(grid(current.get("position")), grid(state["position"])) != 1
        ):
            raise ValueError("target_changed_during_preflight")
        self.event("attack_intent", intent_id=intent, target_uid=target["uid"])
        await self.action(
            "game_attack", {"target_handle": target["target_handle"], "mode": "once"}, version
        )
        self.last_attack[target["uid"]] = time.monotonic()

    async def recovery(self, version):
        self.recover = False
        state, _ = self.guard(version)
        await self.budget.room(self.stop, 2)
        data = await self.query("game_get_inventory", {}, version)
        doses = self.available_potions(data, self.plan["potions"])
        key = next((key for key, count in doses if count > 0), None)
        if key is None:
            raise ValueError("no_verified_potion_for_recovery")
        hp_before = self.guard(version)[0]["hp"]
        operation = await self.action(
            "game_use_item", {"item_id": key[0], "seq_id": key[1], "count": 1}, version
        )
        self.guard(version)
        consumed = any(
            row.get("item_id") == key[0]
            and row.get("seq_id") == key[1]
            and row.get("before", 0) - row.get("after", 0) == 1
            for row in ((operation or {}).get("result") or {}).get("item_changes", [])
        )
        self.event(
            "potion_result",
            item_id=key[0],
            seq_id=key[1],
            consumption_observed=consumed,
            completion_scope=(operation or {}).get("completion_scope"),
            hp_before=hp_before,
            hp_now=self.guard(version)[0]["hp"],
            healing_attributed=False,
        )
        if operation is not None and operation.get("state") == "succeeded" and not consumed:
            raise ValueError("potion_consumption_not_observed")
        route = self.plan["route"]
        for index in range(len(route) - 1):
            await self.budget.room(self.stop, 2)
            await self.check_route(route[index:], version)
            point = route[index + 1]
            operation = await self.action(
                "game_move_to", {"x": point[0], "y": point[1], "tolerance": 0}, version
            )
            state = await self.arrival(operation, point, version)
        self.event(
            "retreat_finished",
            hp_before=hp_before,
            hp_now=state["hp"],
            hp_growth_observed=state["hp"] > hp_before,
            healing_attributed=False,
            hunting_enabled=False,
            automatic_resume=False,
        )
        self.invalidate("recovery_finished_explicit_rearm_required")

    async def arrival(self, operation, point, version):
        if (
            operation is None
            or operation.get("state") != "succeeded"
            or operation.get("completion_scope") != "observed_postcondition"
            or (operation.get("result") or {}).get("submitted") is not True
            or grid((operation.get("result") or {}).get("position")) != point
        ):
            raise ValueError("move_segment_not_confirmed")
        deadline = time.monotonic() + MAX_AGE
        while True:
            state, _ = self.guard(version)
            if grid(state["position"]) == point and state.get("action") == "idle":
                return state
            if time.monotonic() > deadline:
                raise ValueError("move_arrival_not_freshly_observed")
            await wait_until(self.stop, time.monotonic() + 0.025)

    async def scheduler(self):
        while not self.stop.is_set():
            try:
                if self.halt:
                    await self.global_stop()
                if self.exiting:
                    self.stop.set()
                    return
                if self.recover and self.plan is not None:
                    await self.recovery(self.version)
                    continue
                if not self.work.empty():
                    command, version = self.work.get_nowait()
                    if command["command"] in ("arm", "scout"):
                        await self.arm(command)
                    elif command["command"] == "purchase":
                        await self.purchase(command, version)
                    else:
                        await self.attack(command, version)
                    continue
            except (KeyError, TypeError, ValueError) as exc:
                self.event("command_rejected", reason=str(exc), automatic_retries=0)
                if self.recover and self.plan is not None:
                    # A low-HP observation can arrive during an attack preflight.
                    # Abandon that unsent attack while retaining the recovery plan.
                    self.event("attack_aborted_for_recovery", reason=str(exc))
                    continue
                self.invalidate(str(exc))
                self.event("alert", reason="no_safe_action", detail=str(exc))
            self.wake.clear()
            await wait_until(self.stop, time.monotonic() + 0.025)


async def run_with_session(
    session: ClientSession,
    *,
    binding: Binding,
    emit: Callable[[dict], None],
    commands: AsyncIterator[dict] | None = None,
    stop: asyncio.Event | None = None,
    allow_actions: bool = False,
) -> int:
    """Use a caller-owned session whose bridge was started with --no-reconnect.

    No connection is created, closed or replaced here. Set stop for graceful shutdown;
    stale reads are never cancelled just to satisfy the observation freshness target.
    """
    observer_class = ArmedSupervisor if allow_actions else Observer
    observer = observer_class(session, binding, emit, stop if stop is not None else asyncio.Event())
    observer.event("monitor_started", period_ms=250, max_age_ms=500, allow_actions=allow_actions)
    tasks = [asyncio.create_task(observer.poll()), asyncio.create_task(observer.watchdog())]
    if allow_actions:
        tasks.append(asyncio.create_task(observer.scheduler()))
    if commands is not None:
        tasks.append(asyncio.create_task(observer.commands(commands)))
    try:
        await asyncio.gather(*tasks)
    finally:
        observer.stop.set()
        await asyncio.gather(*tasks, return_exceptions=True)
        failed = observer.failed or (allow_actions and observer.locked not in (None, "global_stop"))
        observer.event("monitor_stopped", failed=failed)
    return 2 if failed else 0


async def stdin_commands() -> AsyncIterator[dict]:
    loop = asyncio.get_running_loop()
    lines = asyncio.Queue(maxsize=16)
    closed = threading.Event()

    def read_lines():
        # A daemon reader lets transport failure exit without waiting for stdin EOF.
        while not closed.is_set():
            line = sys.stdin.readline(65537)
            if closed.is_set():
                return
            future = asyncio.run_coroutine_threadsafe(lines.put(line), loop)
            try:
                future.result()
            except Exception:
                return
            if not line or len(line) > 65536:
                return

    threading.Thread(target=read_lines, name="mir2x-observe-stdin", daemon=True).start()
    try:
        while True:
            line = await lines.get()
            if not line:
                return
            if len(line) > 65536:
                yield {"command": "invalid"}
                return
            try:
                yield json.loads(line)
            except (ValueError, RecursionError):
                yield {"command": "invalid"}
    finally:
        closed.set()


async def observe_stdio(
    command: list[str], binding: Binding, report: Path, *, allow_actions=False
) -> int:
    """Start one explicitly selected stdio host; never run against a guessed instance."""
    if command[-3:] != ["--instance-id", binding.instance_id, "--no-reconnect"]:
        raise ValueError("observe requires an explicit instance and --no-reconnect")
    parameters = StdioServerParameters(
        command=command[0], args=command[1:], env={"PYTHONUTF8": "1"}
    )
    with (
        report.open("a", encoding="utf-8") as output,
        report.with_suffix(".stderr.log").open("a", encoding="utf-8") as errors,
    ):

        def emit(event):
            line = json.dumps(event, ensure_ascii=False, allow_nan=False)
            output.write(line + "\n")
            output.flush()
            print(line, flush=True)
            if event["event"] == "alert":
                label = "supervise" if allow_actions else "observe"
                print("mir2x " + label + " alert: " + event["reason"], file=sys.stderr, flush=True)

        try:
            async with stdio_client(parameters, errlog=errors) as (read, write):
                async with ClientSession(read, write, read_timeout_seconds=10) as session:
                    await session.initialize()
                    return await run_with_session(
                        session,
                        binding=binding,
                        emit=emit,
                        commands=stdin_commands(),
                        allow_actions=allow_actions,
                    )
        except Exception as exc:
            emit(
                {
                    "time": datetime.now(UTC).isoformat(),
                    "event": "alert",
                    "reason": "host_unavailable",
                    "instance_id": binding.instance_id,
                    "character_uid": binding.character_uid,
                    "mode": "supervise" if allow_actions else "observe",
                    "write_enabled": False,
                    "exception_type": type(exc).__name__,
                }
            )
            return 2
