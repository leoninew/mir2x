"""Build the desktop package in the native CMake build tree."""

import argparse
import os
import shutil
import subprocess
from pathlib import Path

PROJECT = Path(__file__).resolve().parent


def run(executable: str, *arguments: str, cwd: Path = PROJECT) -> None:
    tool = shutil.which(executable)
    if tool is None:
        raise SystemExit(f"Required build tool not found: {executable}")
    command = [tool, *arguments]
    if os.name == "nt" and Path(tool).suffix.lower() in {".cmd", ".bat"}:
        command = [os.environ.get("COMSPEC", "cmd.exe"), "/d", "/c", *command]
    subprocess.run(command, cwd=cwd, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=PROJECT.parents[1] / "build")
    parser.add_argument("--no-install", action="store_true")
    arguments = parser.parse_args()
    build = arguments.build_dir.resolve()
    if not (build / "CMakeCache.txt").is_file():
        parser.error("Configure the main mir2x CMake build tree first")
    run("uv", "sync", "--locked", "--extra", "desktop", "--all-groups")
    run("corepack", "yarn", "install", "--immutable", cwd=PROJECT / "web")
    run("corepack", "yarn", "build", cwd=PROJECT / "web")
    run(
        "uv",
        "run",
        "--locked",
        "--no-sync",
        "pyinstaller",
        "--noconfirm",
        "--clean",
        "--windowed",
        "--onedir",
        "--name",
        "Mir2xWorkbench",
        "--paths",
        "src",
        "--collect-all",
        "webview",
        "--hidden-import",
        "webview.platforms.winforms",
        "--add-data",
        f"{PROJECT / 'web/dist'}{os.pathsep}static",
        "--add-data",
        f"{PROJECT / 'src/mir2x_workbench/logging.yaml'}{os.pathsep}mir2x_workbench",
        "--add-data",
        f"{PROJECT / 'config/config.yaml'}{os.pathsep}mir2x_workbench",
        "--distpath",
        str(build / "workbench"),
        "--workpath",
        "build/pyinstaller",
        "--specpath",
        "build/pyinstaller",
        "src/mir2x_workbench/desktop.py",
    )
    if not arguments.no_install:
        run("cmake", "--install", str(build), "--component", "workbench")


if __name__ == "__main__":
    main()
