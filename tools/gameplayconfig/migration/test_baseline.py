"""Targeted migration checks against the frozen production export."""

import json
import hashlib
from pathlib import Path
import unittest

from export_baseline import spawn_literal

ROOT = Path(__file__).resolve().parents[3]
CONFIG = ROOT / "server/config/gameplay"


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class MigrationTest(unittest.TestCase):
    def test_production_drop_table_and_accounting(self):
        baseline = read(Path(__file__).with_name("baseline.json"))
        manifest = read(CONFIG / "manifest.json")
        expanded = []
        rows = 0
        for monster_id in manifest["drop_monsters"]:
            data = read(CONFIG / f"drops/{monster_id}.json")
            rows += len(data["items"])
            for group in sorted({row["group"] for row in data["items"]}):
                for row in data["items"]:
                    if row["group"] == group:
                        expanded.extend([[monster_id, group, row["item_id"], row["prob_recip"], row["count"]]] * row["repeat"])
        self.assertEqual(expanded, baseline["expanded_production_drops"])
        notes = read(CONFIG / "migration-notes.json")["records"]
        self.assertEqual(rows + sum(r["domain"] == "drops" for r in notes), len(baseline["raw_drops"]))

    def test_identity_and_experience(self):
        baseline = read(Path(__file__).with_name("baseline.json"))
        self.assertEqual(read(CONFIG / "catalog.json"), baseline["catalog"])
        self.assertEqual(read(CONFIG / "monsters.json")["monsters"], baseline["monster_experience"])

    def test_all_map_groups_keep_order_and_inactive_sources(self):
        catalog = read(CONFIG / "catalog.json")
        names = {r["name"]: r["id"] for r in reversed(catalog["monsters"])}
        maps = {r["name"]: r["id"] for r in catalog["maps"]}
        notes = read(CONFIG / "migration-notes.json")["records"]
        baseline = read(Path(__file__).with_name("baseline.json"))
        for relative, frozen in baseline["raw_spawns"].items():
            path = ROOT / relative
            source = path.read_text(encoding="utf-8")
            parsed = spawn_literal(source) if "getMapSpawnConfig()" not in source else None
            if parsed is not None:
                self.assertEqual(parsed[2], frozen["groups"])
            else:
                start = source.index("getMapSpawnConfig()")
                end = start + len("getMapSpawnConfig()")
                self.assertEqual(hashlib.sha256(source[:start].encode()).hexdigest(), frozen["prefix_sha256"])
                self.assertEqual(hashlib.sha256(source[end:].encode()).hexdigest(), frozen["suffix_sha256"])
            expected = [{"monster_id": names[g["name"]], "monster_name": g["name"], "loc": g["loc"]} for g in frozen["groups"] if g["name"] in names]
            self.assertEqual(read(CONFIG / f"spawns/{maps[path.stem]}.json")["groups"], expected)
            inactive = [g for g in frozen["groups"] if g["name"] not in names]
            self.assertEqual(inactive, [r["raw"] for r in notes if r["domain"] == "spawns" and r["source_path"] == path.relative_to(ROOT).as_posix()])
        self.assertEqual(len(baseline["raw_spawns"]), 251)


if __name__ == "__main__":
    unittest.main()
