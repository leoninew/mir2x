"""Read-only supervision using one explicitly bound official MCP SDK session."""

import asyncio
import contextlib
import json
import re
import sys
import threading
import time
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

    def event(self, event, **fields):
        self.emit(
            {
                "time": datetime.now(UTC).isoformat(),
                "monotonic": time.monotonic(),
                "event": event,
                "instance_id": self.binding.instance_id,
                "character_uid": self.binding.character_uid,
                "context": self.context,
                "mode": "observe",
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
            await wait_until(self.stop, now + 0.025)

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
                    self.stop.set()
                    return
            finally:
                for task in (incoming, stopped):
                    if not task.done():
                        task.cancel()
                await asyncio.gather(incoming, stopped, return_exceptions=True)
            if not isinstance(command, dict):
                self.alert("invalid_command")
                continue
            if command.get("command") == "quit":
                self.stop.set()
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
                    continue
                await self.read(name, arguments, monitor=False)
            else:
                self.alert("write_or_unknown_command_rejected")


async def run_with_session(
    session: ClientSession,
    *,
    binding: Binding,
    emit: Callable[[dict], None],
    commands: AsyncIterator[dict] | None = None,
    stop: asyncio.Event | None = None,
) -> int:
    """Use a caller-owned session whose bridge was started with --no-reconnect.

    No connection is created, closed or replaced here. Set stop for graceful shutdown;
    stale reads are never cancelled just to satisfy the observation freshness target.
    """
    observer = Observer(session, binding, emit, stop if stop is not None else asyncio.Event())
    observer.event("monitor_started", period_ms=250, max_age_ms=500)
    tasks = [asyncio.create_task(observer.poll()), asyncio.create_task(observer.watchdog())]
    if commands is not None:
        tasks.append(asyncio.create_task(observer.commands(commands)))
    try:
        await asyncio.gather(*tasks)
    finally:
        observer.stop.set()
        await asyncio.gather(*tasks, return_exceptions=True)
        observer.event("monitor_stopped", failed=observer.failed)
    return 2 if observer.failed else 0


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


async def observe_stdio(command: list[str], binding: Binding, report: Path) -> int:
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
                print("mir2x observe alert: " + event["reason"], file=sys.stderr, flush=True)

        try:
            async with stdio_client(parameters, errlog=errors) as (read, write):
                async with ClientSession(read, write, read_timeout_seconds=10) as session:
                    await session.initialize()
                    return await run_with_session(
                        session, binding=binding, emit=emit, commands=stdin_commands()
                    )
        except Exception as exc:
            emit(
                {
                    "time": datetime.now(UTC).isoformat(),
                    "event": "alert",
                    "reason": "host_unavailable",
                    "instance_id": binding.instance_id,
                    "character_uid": binding.character_uid,
                    "mode": "observe",
                    "write_enabled": False,
                    "exception_type": type(exc).__name__,
                }
            )
            return 2
