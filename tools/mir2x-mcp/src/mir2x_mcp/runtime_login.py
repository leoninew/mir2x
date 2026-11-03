"""Load optional local login credentials without interpolation or value diagnostics."""

import re
from dataclasses import dataclass, field
from pathlib import Path

from .runtime_config import RuntimeFailure

MAX_ENV_BYTES = 65536
MAX_LOGIN_BYTES = 64  # CMLogin uses StaticBuffer<SYS_IDSIZE/SYS_PWDSIZE>.


@dataclass(frozen=True)
class ClientLogin:
    username: str = field(default="", repr=False)
    password: str = field(default="", repr=False)


def _value(text: str) -> str:
    text = text.strip()
    if text.startswith(("'", '"')):
        end = text.find(text[0], 1)
        if end < 0 or (text[end + 1 :].strip() and not text[end + 1 :].lstrip().startswith("#")):
            raise RuntimeFailure("登录 .env 引号格式错误；值不支持转义或变量展开")
        return text[1:end]
    return re.split(r"\s+#", text, maxsplit=1)[0].rstrip()


def load_login(path: Path, *, enabled: bool = True) -> ClientLogin:
    if not enabled:
        return ClientLogin()
    try:
        with path.open("rb") as stream:
            data = stream.read(MAX_ENV_BYTES + 1)
    except FileNotFoundError:
        return ClientLogin()
    except OSError:
        raise RuntimeFailure("无法读取登录 .env；未启动客户端") from None
    if len(data) > MAX_ENV_BYTES:
        raise RuntimeFailure("登录 .env 超过 64 KiB；未启动客户端")
    try:
        text = data.decode("utf-8-sig")
    except UnicodeDecodeError:
        raise RuntimeFailure("登录 .env 必须为 UTF-8；未启动客户端") from None
    values = {}
    for line in text.split("\n"):
        line = line.strip()
        if line.startswith("export "):
            line = line[7:].lstrip()
        key, separator, value = line.partition("=")
        key = key.strip()
        if key not in ("username", "password"):
            continue
        if not separator or key in values:
            raise RuntimeFailure("登录 .env 的 username/password 必须各声明一次，使用 key=value")
        values[key] = _value(value)
    for key, value in values.items():
        if any(ord(char) < 32 or ord(char) == 127 for char in value):
            raise RuntimeFailure("登录 .env 凭据不能包含控制字符")
        if len(value.encode("utf-8")) > MAX_LOGIN_BYTES:
            raise RuntimeFailure("登录 .env 每项凭据最多 64 个 UTF-8 字节")
        if key == "username" and ":" in value:
            raise RuntimeFailure("登录 .env 的 username 不能包含冒号")
    if not values.get("username") or not values.get("password"):
        return ClientLogin()
    return ClientLogin(values["username"], values["password"])
