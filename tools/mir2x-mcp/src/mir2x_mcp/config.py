import argparse
import re
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Config:
    bridge_dir: Path
    instance_id: str | None = None

    def __post_init__(self):
        if not self.bridge_dir.is_absolute():
            raise ValueError("--bridge-dir 必须为绝对路径")
        if self.instance_id is not None and not re.fullmatch("[0-9a-f]{32}", self.instance_id):
            raise ValueError("--instance-id 必须为32位小写十六进制")


def parse_args() -> Config:
    parser = argparse.ArgumentParser(description="通过本机桥接访问 mir2x 当前窗口角色")
    parser.add_argument(
        "--bridge-dir", type=Path, required=True, help="客户端凭据发现目录（绝对路径）"
    )
    parser.add_argument("--instance-id", help="明确指定客户端实例；绑定后不会自动切换")
    args = parser.parse_args()
    try:
        return Config(args.bridge_dir, args.instance_id)
    except ValueError as exc:
        parser.error(str(exc))
