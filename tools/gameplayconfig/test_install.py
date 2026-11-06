"""Check the generated gameplay-config install component in disposable directories."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def snapshot(root: Path) -> dict[str, str]:
    return {
        str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in root.rglob("*")
        if path.is_file()
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mir2x-gameplay-install-") as directory:
        prefix = Path(directory) / "配置 with spaces"
        root = prefix / "server/config/gameplay"

        def install(success: bool) -> None:
            result = subprocess.run(
                ["cmake", "--install", str(args.build.resolve()), "--prefix", str(prefix),
                 "--component", "gameplay-config"],
                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60,
            )
            assert (result.returncode == 0) == success, result.stdout + result.stderr

        install(True)
        original = snapshot(root)
        assert len(original) > 250
        monsters = root / "monsters.json"
        values = json.loads(monsters.read_bytes())
        values["monsters"][0]["exp"] += 127
        monsters.write_text(json.dumps(values, ensure_ascii=False), encoding="utf-8")
        modified = snapshot(root)
        install(True)
        assert snapshot(root) == modified
        manifest = root / "manifest.json"
        manifest_bytes = manifest.read_bytes()
        manifest.unlink()
        incomplete = snapshot(root)
        install(False)
        assert snapshot(root) == incomplete
        manifest.write_bytes(manifest_bytes)
        values["monsters"][0]["monster_name"] = "incompatible identity"
        monsters.write_text(json.dumps(values), encoding="utf-8")
        incompatible = snapshot(root)
        install(False)
        assert snapshot(root) == incompatible
        print("4 install scenarios passed: new, preserved edits, incomplete, incompatible")


if __name__ == "__main__":
    main()
