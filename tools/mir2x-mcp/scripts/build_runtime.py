"""Build wheel/sdist and the Windows portable launcher using the locked uv project."""

import argparse
import os
import subprocess
from pathlib import Path

from stage_runtime import stage

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "tools/mir2x-mcp"


def build(uv: str, output: Path):
    if os.name != "nt":
        raise SystemExit("Portable mir2x.exe requires a native Windows build; wheel supports POSIX")
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    environment = os.environ | {
        "UV_CACHE_DIR": str(ROOT / ".runtime/uv-cache"),
        "UV_PYTHON_INSTALL_DIR": str(ROOT / ".runtime/uv-python"),
        "PYTHONUTF8": "1",
    }

    def run(*args):
        subprocess.run([uv, *args], cwd=ROOT, env=environment, check=True)

    run("sync", "--project", str(PROJECT), "--all-groups", "--locked")
    run("build", "--project", str(PROJECT), "--out-dir", str(output / "python"))
    run(
        "run",
        "--project",
        str(PROJECT),
        "--locked",
        "--no-sync",
        "python",
        "-m",
        "PyInstaller",
        "--noconfirm",
        "--onefile",
        "--name",
        "mir2x",
        "--paths",
        str(PROJECT / "src"),
        "--distpath",
        str(output),
        "--workpath",
        str(output / "pyinstaller"),
        "--specpath",
        str(output / "pyinstaller"),
        "--collect-data",
        "mir2x_mcp",
        "--copy-metadata",
        "mcp",
        "--hidden-import",
        "win32timezone",
        str(PROJECT / "scripts/runtime_entry.py"),
    )
    executable = output / "mir2x.exe"
    subprocess.run([str(executable), "--help"], check=True, env=environment)
    stage(executable, output / "stage")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uv", default="uv")
    parser.add_argument("--output", type=Path, default=ROOT / "build/runtime-launcher")
    arguments = parser.parse_args()
    build(arguments.uv, arguments.output)
