"""Stage only launcher artifacts; never read/copy game state or credentials."""

import argparse
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
ASSETS = ROOT / "tools/mir2x-mcp/src/mir2x_mcp/runtime_assets"


def stage(binary: Path, output: Path):
    output.mkdir(parents=True, exist_ok=True)
    files = {
        binary: "mir2x.exe",
        ROOT / "mir2x.cmd": "mir2x.cmd",
        ASSETS / "mir2x.defaults.toml": "mir2x.defaults.toml",
        ASSETS / "QUICKSTART.md": "QUICKSTART.md",
        ASSETS / "login.env.example": "login.env.example",
        ROOT / ".agents/skills/mir2x-runtime/SKILL.md": ".agents/skills/mir2x-runtime/SKILL.md",
    }
    for name in ("standard", "readonly", "movement"):
        files[ASSETS / f"{name}.json"] = f"config/policies/{name}.json"
    for package in (binary.parent / "python").glob("mir2x_mcp-*"):
        if package.suffix == ".whl" or package.name.endswith(".tar.gz"):
            files[package] = f"python/{package.name}"
    manifest = []
    for source, name in files.items():
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        manifest.append({"path": name, "sha256": hashlib.sha256(target.read_bytes()).hexdigest()})
    (output / "runtime-manifest.json").write_text(
        json.dumps({"files": manifest}, ensure_ascii=False, indent=2) + "\n",
        "utf-8",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    stage(arguments.binary.resolve(), arguments.output.resolve())
