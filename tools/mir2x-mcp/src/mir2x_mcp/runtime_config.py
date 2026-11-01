"""Distribution-relative runtime configuration; no game data is loaded here."""

import copy
import json
import os
import sys
import tomllib
from dataclasses import dataclass
from pathlib import Path

ASSETS = Path(__file__).parent / "runtime_assets"
POLICIES = ("standard", "readonly", "movement")


class RuntimeFailure(Exception):
    pass


def discover_root(explicit: Path | None = None) -> Path:
    if explicit:
        return explicit.resolve()
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    if os.environ.get("MIR2X_HOME"):
        return Path(os.environ["MIR2X_HOME"]).resolve()
    for directory in (Path.cwd(), *Path.cwd().parents):
        if (directory / "mir2x.defaults.toml").is_file() or (
            directory / "tools/mir2x-mcp/pyproject.toml"
        ).is_file():
            return directory
    source_root = Path(__file__).resolve().parents[4]
    if (source_root / "CMakeLists.txt").is_file():
        return source_root
    raise RuntimeFailure("找不到游戏目录；请在制品目录运行，或设置 MIR2X_HOME")


def _merge(target: dict, value: dict):
    for section, fields in value.items():
        if section not in target or not isinstance(fields, dict):
            raise RuntimeFailure(f"未知配置节：{section}")
        for key, item in fields.items():
            if key not in target[section]:
                raise RuntimeFailure(f"未知配置字段：{section}.{key}")
            target[section][key] = item


def _text(value, name: str) -> str:
    if not isinstance(value, str) or not value or "\x00" in value:
        raise RuntimeFailure(f"{name} 必须为非空字符串")
    return value


def _integer(value, low: int, high: int, name: str) -> int:
    if type(value) is not int or not low <= value <= high:
        raise RuntimeFailure(f"{name} 必须在 {low}..{high} 之间")
    return value


@dataclass(frozen=True)
class Program:
    executable: Path
    workdir: Path
    args: tuple[str, ...]


@dataclass(frozen=True)
class Settings:
    root: Path
    game: Path
    state: Path
    bridge: Path
    host: str
    client_port: int
    peer_port: int
    policy: str
    bridge_enabled: bool
    start_timeout: int
    stop_timeout: int
    server: Program
    client: Program

    def policy_path(self, name: str | None = None) -> Path:
        name = name or self.policy
        if name not in POLICIES:
            raise RuntimeFailure(f"未知策略：{name}")
        external = self.root / "config" / "policies" / f"{name}.json"
        return external if external.is_file() else ASSETS / f"{name}.json"


def load_settings(root: Path, override: Path | None = None) -> Settings:
    root = root.resolve()
    defaults = copy.deepcopy(tomllib.loads((ASSETS / "mir2x.defaults.toml").read_text("utf-8")))
    try:
        public = root / "mir2x.defaults.toml"
        if public.is_file():
            _merge(defaults, tomllib.loads(public.read_text("utf-8")))
        local = override or root / "mir2x.local.toml"
        if local.is_file():
            _merge(defaults, tomllib.loads(local.read_text("utf-8")))
        elif override:
            raise RuntimeFailure("指定的配置文件不存在")
    except (OSError, tomllib.TOMLDecodeError) as exc:
        raise RuntimeFailure("无法读取运行配置，请检查 TOML 格式") from exc
    net, launcher = defaults["network"], defaults["launcher"]
    host = _text(net["host"], "network.host")
    port = _integer(net["client_port"], 1024, 65535, "network.client_port")
    peer = _integer(net["peer_port"], 1024, 65535, "network.peer_port")
    if port == peer:
        raise RuntimeFailure("玩家端口和节点端口不能相同")
    policy = _text(launcher["policy"], "launcher.policy")
    if policy not in POLICIES:
        raise RuntimeFailure("launcher.policy 必须为 standard/readonly/movement")
    if type(launcher["bridge"]) is not bool:
        raise RuntimeFailure("launcher.bridge 必须为布尔值")
    game = (
        root
        if public.is_file()
        or (root / "client" / "client.exe").is_file()
        or (root / "client" / "client").is_file()
        else root / "install"
    )

    def relative(value, name):
        path = Path(_text(value, name))
        return (root / path).resolve()

    def program(role):
        fields = defaults[role]
        suffix = ".exe" if os.name == "nt" else ""
        executable = _text(fields["executable"], f"{role}.executable")
        workdir = _text(fields["workdir"], f"{role}.workdir")
        args = fields["args"]
        if not isinstance(args, list) or any(not isinstance(a, str) or "\x00" in a for a in args):
            raise RuntimeFailure(f"{role}.args 必须为字符串数组")
        reserved = (
            "--client-port",
            "--peer-port",
            "--server-port",
            "--server-ip",
            "--enable-ai-bridge",
            "--ai-bridge-dir",
            "--ai-policy",
            "--disable-version-check",
        )
        if any(a.split("=", 1)[0] in reserved for a in args):
            raise RuntimeFailure(f"{role}.args 不接受受管理端口、桥接或禁用版本检查参数")
        return Program(
            game / role / (role + suffix)
            if executable == "auto"
            else relative(executable, f"{role}.executable"),
            game / role if workdir == "auto" else relative(workdir, f"{role}.workdir"),
            tuple(args),
        )

    settings = Settings(
        root,
        game,
        relative(launcher["state_dir"], "launcher.state_dir"),
        relative(launcher["bridge_dir"], "launcher.bridge_dir"),
        host,
        port,
        peer,
        policy,
        launcher["bridge"],
        _integer(launcher["start_timeout"], 1, 300, "start_timeout"),
        _integer(launcher["stop_timeout"], 1, 120, "stop_timeout"),
        program("server"),
        program("client"),
    )
    if settings.state.is_relative_to(settings.bridge):
        raise RuntimeFailure("state_dir 不能位于 bridge_dir 内；凭据目录由客户端安全创建")
    for name in POLICIES:
        try:
            from .contract import validate

            value = json.loads(settings.policy_path(name).read_text("utf-8"))
            validate(value, "policy")
        except Exception as exc:
            raise RuntimeFailure(f"预置策略 {name} 无效") from exc
    return settings
