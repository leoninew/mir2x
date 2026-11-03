"""Short runtime commands shared by the source checkout and portable release."""

import argparse
import importlib.util
import json
import logging
import os
import shutil
import subprocess
import sys
from collections import deque
from pathlib import Path

import psutil

from .runtime_config import POLICIES, RuntimeFailure, discover_root, load_settings
from .runtime_process import Manager, management_lock, port_open


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="mir2x 游戏与 MCP 管理入口；.env 仅为 launcher 可选登录输入",
        epilog=(
            "首次有效角色与认证宿主绑定后默认自动开启 AI；右下角可关闭，暂停后仅窗口恢复。"
            "stdio 由宿主启动。"
        ),
    )
    result.add_argument("--root", type=Path, help="运行根；仓库默认 install，显式指定时使用该目录")
    result.add_argument("--config", type=Path, help="可选 TOML 文件；字段内相对路径按运行根解析")
    result.add_argument("--json", action="store_true", help="输出结构化结果")
    commands = result.add_subparsers(dest="command", required=True)
    for name in ("up", "restart"):
        command = commands.add_parser(
            name, help="启动服务器和客户端" if name == "up" else "重启两端"
        )
        command.add_argument("--policy", choices=POLICIES)
        command.add_argument(
            "--manual-login", action="store_true", help="本次跳过 .env，使用原游戏手工登录"
        )
        if name == "restart":
            command.add_argument("--force", action="store_true")
    command = commands.add_parser("stop", help="先正常关闭客户端，再关闭服务器")
    command.add_argument("--force", action="store_true", help="正常关闭超时后强停本工具登记的进程")
    commands.add_parser("status", help="查看本工具管理的进程")
    commands.add_parser("doctor", help="检查游戏制品、资源、端口和 MCP 依赖")
    command = commands.add_parser("logs", help="查看最近的管理日志")
    command.add_argument("role", choices=("server", "client"))
    command.add_argument("--lines", type=int, default=50)
    for role in ("server", "client"):
        command = commands.add_parser(role, help=f"管理 {role}")
        actions = command.add_subparsers(dest="action", required=True)
        for action in ("start", "stop", "restart", "status"):
            item = actions.add_parser(action)
            if action in ("stop", "restart"):
                item.add_argument("--force", action="store_true")
            if role == "client" and action in ("start", "restart"):
                item.add_argument("--policy", choices=POLICIES)
                item.add_argument(
                    "--manual-login", action="store_true", help="本次跳过 .env，使用手工登录"
                )
    command = commands.add_parser("mcp", help="连接 LLM 宿主；游戏窗口不提供聊天框")
    actions = command.add_subparsers(dest="action", required=True)
    item = actions.add_parser("serve", help="stdio MCP 服务，由 LLM 宿主自动启动")
    item.add_argument("--instance-id")
    item.add_argument("--no-reconnect", action="store_true", help="连接丢失后锁闭，不重新认证")
    for name, help_text in (
        ("observe", "唯一官方 SDK 宿主持续只读监督，不执行角色动作"),
        ("supervise", "持续监督；按调用者预算 purchase 单件物品，按显式安全计划 arm 短动作"),
    ):
        item = actions.add_parser(name, help=help_text, description=help_text)
        item.add_argument("--instance-id", required=True)
        item.add_argument("--character-uid", required=True)
        item.add_argument("--report", type=Path, required=True, help="追加 JSONL 观察与告警")
    item = actions.add_parser("config", help="生成当前目录对应的宿主配置")
    item.add_argument("--format", choices=("json", "toml"), default="json")
    item = actions.add_parser("register", help="将当前入口登记到本机 Codex MCP 配置")
    item.add_argument("--name", default="mir2x")
    return result


def host_command(settings, override: Path | None = None) -> list[str]:
    if getattr(sys, "frozen", False):
        result = [str(Path(sys.executable).resolve())]
    else:
        result = [sys.executable, "-m", "mir2x_mcp.runtime_cli"]
    result += ["--root", str(settings.root)]
    if override:
        result += ["--config", str(override.resolve())]
    return [*result, "mcp", "serve"]


def host_config(command: list[str], style: str) -> str:
    if style == "toml":
        return "\n".join(
            (
                "[mcp_servers.mir2x]",
                f"command = {json.dumps(command[0], ensure_ascii=False)}",
                f"args = {json.dumps(command[1:], ensure_ascii=False)}",
                "",
            )
        )
    return json.dumps(
        {
            "mcpServers": {
                "mir2x": {
                    "command": command[0],
                    "args": command[1:],
                }
            }
        },
        ensure_ascii=False,
        indent=2,
    )


def register(command: list[str], name: str) -> dict:
    if not name or any(
        c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-" for c in name
    ):
        raise RuntimeFailure("MCP 名称只接受字母、数字、下划线和短横线")
    executable = shutil.which("codex")
    if not executable:
        raise RuntimeFailure("未找到 codex；请先安装 Codex，或用 mcp config 配置其他宿主")
    invocation = [executable]
    if os.name == "nt" and Path(executable).suffix.lower() in (".cmd", ".ps1"):
        # npm's JS entry avoids cmd.exe reinterpreting a distribution path as shell code.
        script = Path(executable).parent / "node_modules/@openai/codex/bin/codex.js"
        node = shutil.which("node")
        if not node or not script.is_file():
            raise RuntimeFailure("无法定位 Codex 原生/Node 入口；请使用 mcp config 生成配置")
        invocation = [node, str(script)]
    result = subprocess.run(
        [*invocation, "mcp", "add", name, "--", *command],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode:
        raise RuntimeFailure("Codex 注册失败：" + (result.stderr or result.stdout).strip())
    return {"registered": name, "next": "在该目录重新开启 Codex 会话，用 /mcp 检查连接"}


def doctor(manager: Manager) -> dict:
    settings = manager.settings
    checks = []

    def check(name, ok, detail):
        checks.append({"name": name, "ok": bool(ok), "detail": str(detail)})

    for role in ("server", "client"):
        program = getattr(settings, role)
        check(f"{role}.executable", program.executable.is_file(), program.executable)
        check(f"{role}.workdir", program.workdir.is_dir(), program.workdir)
        resources = ("map", "script") if role == "server" else ("res", "script")
        for directory in resources:
            check(
                f"{role}.{directory}",
                (program.workdir / directory).is_dir(),
                program.workdir / directory,
            )
    check("policy", True, settings.policy)
    check("mcp", importlib.util.find_spec("mcp") is not None, "Python MCP SDK")
    for role in ("server", "client"):
        status = manager.status(role)
        check(f"{role}.record", True, status["state"])
    listening = port_open(settings.host, settings.client_port)
    managed = manager.status("server")["state"] == "running"
    check(
        "server.port",
        not listening or managed,
        "已就绪" if listening and managed else "端口空闲" if not listening else "外部服务占用",
    )
    return {"ok": all(item["ok"] for item in checks), "root": str(settings.root), "checks": checks}


def display(value, structured: bool):
    if structured:
        print(json.dumps(value, ensure_ascii=False, indent=2))
    elif isinstance(value, list):
        for item in value:
            display(item, False)
    elif "checks" in value:
        for check in value["checks"]:
            print(f"{'OK' if check['ok'] else 'FAIL'} {check['name']}: {check['detail']}")
    elif "role" in value:
        print(
            f"{value['role']}: {value['state']}"
            + (f" (PID {value['pid']})" if "pid" in value else "")
        )
    else:
        for key, item in value.items():
            print(f"{key}: {item}")


def run(args) -> int:
    settings = load_settings(discover_root(args.root), args.config)
    if args.command == "mcp":
        if args.action == "serve":
            from .config import Config
            from .server import create_server

            logging.basicConfig(level=logging.WARNING, stream=sys.stderr)
            create_server(Config(settings.bridge, args.instance_id, args.no_reconnect)).run(
                transport="stdio"
            )
            return 0
        if args.action in ("observe", "supervise"):
            import asyncio

            from .supervision import Binding, observe_stdio

            command = [
                *host_command(settings, args.config),
                "--instance-id",
                args.instance_id,
                "--no-reconnect",
            ]
            kwargs = {"allow_actions": True} if args.action == "supervise" else {}
            return asyncio.run(
                observe_stdio(
                    command, Binding(args.instance_id, args.character_uid), args.report, **kwargs
                )
            )
        command = host_command(settings, args.config)
        if args.action == "config":
            print(host_config(command, args.format))
        else:
            display(register(command, args.name), args.json)
        return 0
    manager = Manager(settings)
    if args.command == "logs":
        if not 1 <= args.lines <= 10000:
            raise RuntimeFailure("--lines 必须在 1..10000 之间")
        path = settings.state / "logs" / f"{args.role}.log"
        if not path.is_file():
            raise RuntimeFailure("尚无管理日志；游戏自身日志在对应 client/server 工作目录")
        with path.open(encoding="utf-8", errors="replace") as stream:
            print("".join(deque(stream, maxlen=args.lines)), end="")
        return 0
    if args.command == "doctor":
        result = doctor(manager)
        display(result, args.json)
        return 0 if result["ok"] else 1
    if args.command == "status" or getattr(args, "action", None) == "status":
        roles = ("server", "client") if args.command == "status" else (args.command,)
        display([manager.status(role) for role in roles], args.json)
        return 0
    with management_lock(settings.state):
        force = getattr(args, "force", False)
        policy = getattr(args, "policy", None)
        login = None
        starts_client = args.command in ("up", "restart") or (
            args.command == "client" and args.action in ("start", "restart")
        )
        if starts_client and (
            args.command == "restart"
            or getattr(args, "action", None) == "restart"
            or manager.status("client")["state"] != "running"
        ):
            # Validate before stopping either process or starting the local server.
            login = manager.prepare_login(getattr(args, "manual_login", False))
        result = []
        if args.command in ("up", "restart", "stop"):
            if args.command != "up":
                for role in ("client", "server"):
                    result.append(manager.stop(role, force))
            if args.command != "stop":
                result.append(manager.start("server"))
                manager.wait_server()
                result.append(manager.start("client", policy, login=login))
        else:
            if args.action in ("stop", "restart"):
                result.append(manager.stop(args.command, force))
            if args.action in ("start", "restart"):
                result.append(manager.start(args.command, policy, login=login))
        display(result, args.json)
    return 0


def main(argv: list[str] | None = None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    args = parser().parse_args(argv)
    try:
        code = run(args)
    except (RuntimeFailure, OSError, ValueError, psutil.Error) as exc:
        if args.json:
            print(json.dumps({"ok": False, "error": str(exc)}, ensure_ascii=False), file=sys.stderr)
        else:
            print(f"mir2x: {exc}", file=sys.stderr)
        code = 1
    raise SystemExit(code)


if __name__ == "__main__":
    main()
