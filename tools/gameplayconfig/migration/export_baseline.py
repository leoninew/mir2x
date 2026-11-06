"""One-time export from the pre-migration sources, using their real C++ identity lookup.

The probe compiles only the identity records and the unchanged production drop-table
builder. It does not link or start a client/server. Keep its output for migration review;
the game never reads this module or its evidence.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")


class LuaLiteral:
    token = re.compile(r"\s+|--[^\n]*|'[^'\\]*'|\"[^\"\\]*\"|-?\d+|[A-Za-z_]\w*|[{}=,;]", re.UNICODE)

    def __init__(self, source: str, start: int):
        self.source = source
        self.pos = start

    def next(self) -> str:
        while (match := self.token.match(self.source, self.pos)) is not None:
            self.pos = match.end()
            value = match.group()
            if value.isspace() or value.startswith("--"):
                continue
            return value
        raise ValueError(f"Unsupported Lua literal at offset {self.pos}")

    def peek(self) -> str:
        pos = self.pos
        value = self.next()
        self.pos = pos
        return value

    def value(self) -> object:
        token = self.next()
        if token == "{":
            array, fields = [], {}
            while self.peek() != "}":
                if re.fullmatch(r"[A-Za-z_]\w*", self.peek()):
                    key = self.next()
                    assert self.next() == "="
                    assert key not in fields
                    fields[key] = self.value()
                else:
                    array.append(self.value())
                if self.peek() in (",", ";"):
                    self.next()
                elif self.peek() != "}":
                    raise ValueError(f"Expected table separator at {self.pos}")
            self.next()
            if array and fields:
                raise ValueError("Mixed Lua table is not configuration data")
            return fields if fields else array
        if token.startswith(("'", '"')):
            return token[1:-1]
        if re.fullmatch(r"-?\d+", token):
            return int(token)
        raise ValueError(f"Unsupported Lua value: {token}")


def spawn_literal(source: str) -> tuple[int, int, list[dict]] | None:
    match = re.search(r"\blocal\s+addMonCo\s*=\s*addmon\.monGener\s*\(", source)
    if match is None:
        return None
    parser = LuaLiteral(source, match.end())
    # Preserve the comment before the table when only replacing the data argument.
    while source[parser.pos].isspace() or source.startswith("--", parser.pos):
        if source.startswith("--", parser.pos):
            parser.pos = source.index("\n", parser.pos)
        else:
            parser.pos += 1
    start = parser.pos
    groups = parser.value()
    assert isinstance(groups, list)
    return start, parser.pos, groups


def make_probe(root: Path) -> str:
    drop = (root / "server/src/dropitemconfig.cpp").read_text(encoding="utf-8")
    body = drop[drop.index("struct InnDropItemConfig final"):drop.index("std::vector<SDItem> getMonsterDropItemList")]
    drop_type = (root / "server/src/dropitemconfig.hpp").read_text(encoding="utf-8")
    drop_type = drop_type[drop_type.index("struct DropItemConfig"):drop_type.index("std::vector<SDItem>")]
    return '''#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>
#include "dbcomid.hpp"
''' + drop_type + body + r'''
int main()
{
    for(uint32_t id = 1; id < DBCOM_MONSTERENDID(); ++id){
        const auto &r = DBCOM_MONSTERRECORD(id);
        if(r){ std::cout << "M\t" << id << '\t' << reinterpret_cast<const char *>(r.name) << '\t' << r.exp << '\n'; }
    }
    for(uint32_t id = 1; id < DBCOM_ITEMENDID(); ++id){
        const auto &r = DBCOM_ITEMRECORD(id);
        if(r){ std::cout << "I\t" << id << '\t' << reinterpret_cast<const char *>(r.name) << '\n'; }
    }
    for(uint32_t id = 1; id < DBCOM_MAPENDID(); ++id){
        const auto &r = DBCOM_MAPRECORD(id);
        if(r){ std::cout << "P\t" << id << '\t' << reinterpret_cast<const char *>(r.name) << '\n'; }
    }
    const InnDropItemConfig rows[] {
        #include "dropitemconfig.inc"
    };
    for(const auto &r: rows){
        std::cout << "R\t" << reinterpret_cast<const char *>(r.monsterName) << '\t' << reinterpret_cast<const char *>(r.itemName)
                  << '\t' << r.group << '\t' << r.probRecip << '\t' << r.repeat << '\t' << r.count
                  << '\t' << DBCOM_MONSTERID(r.monsterName) << '\t' << DBCOM_ITEMID(r.itemName) << '\t' << bool(r) << '\n';
    }
    for(uint32_t id = 1; id < DBCOM_MONSTERENDID(); ++id){
        for(const auto &[group, items]: getMonsterDropItemConfigList(id)){
            for(const auto &item: items){
                std::cout << "D\t" << id << '\t' << group << '\t' << item.itemID << '\t' << item.probRecip << '\t' << item.count << '\n';
            }
        }
    }
}
'''


def export(root: Path, scratch: Path, compiler: str, includes: list[str], probe_output: Path | None = None) -> None:
    scratch.mkdir(parents=True, exist_ok=True)
    probe = scratch / "old-production-probe.cpp"
    probe.write_text(make_probe(root), encoding="utf-8")
    exe = scratch / "old-production-probe.exe"
    command = [compiler, "-std=c++23", "-O0", "-fconstexpr-ops-limit=1000000000", str(probe), "-o", str(exe),
               f"-I{root / 'common/src'}", f"-I{root / 'server/src'}", *[f"-I{x}" for x in includes]]
    if probe_output is None:
        result = subprocess.run(command, capture_output=True, encoding="utf-8", errors="replace")
        write_json(scratch / "probe-command.json", {"command": command, "returncode": result.returncode, "stderr": result.stderr})
        result.check_returncode()
        raw = subprocess.check_output([str(exe)]).decode("utf-8")
    else:
        raw = probe_output.read_text(encoding="utf-8")
    (scratch / "probe.tsv").write_text(raw, encoding="utf-8", newline="\n")
    catalog = {"schema_version": 1, "monsters": [], "items": [], "maps": []}
    monsters, rows, effective = [], [], []
    for line in raw.splitlines():
        if not line:
            continue
        fields = line.split("\t")
        kind = fields.pop(0)
        if kind in ("M", "I", "P"):
            identity = {"id": int(fields[0]), "name": fields[1]}
            catalog[{"M": "monsters", "I": "items", "P": "maps"}[kind]].append(identity)
            if kind == "M":
                monsters.append({"monster_id": identity["id"], "monster_name": identity["name"], "exp": int(fields[2])})
        elif kind == "R":
            rows.append(dict(zip(("monster_name", "item_name", "group", "prob_recip", "repeat", "count", "monster_id", "item_id", "active"), [*fields[:2], *map(int, fields[2:])], strict=True)))
        elif kind == "D":
            effective.append(list(map(int, fields)))
        else:
            raise ValueError(kind)
    identities = {key: {r["name"]: r["id"] for r in reversed(catalog[key])} for key in ("monsters", "items", "maps")}
    output = root / "server/config/gameplay"
    write_json(output / "catalog.json", catalog)
    write_json(output / "monsters.json", {"schema_version": 1, "monsters": monsters})
    source_revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    notes, drops, reconstructed = [], {}, []
    drop_source = root / "server/src/dropitemconfig.inc"
    line_numbers = [n for n, line in enumerate(drop_source.read_text(encoding="utf-8").splitlines(), 1) if re.match(r"\s*\{u8", line)]
    assert len(line_numbers) == len(rows)
    for row, line_number in zip(rows, line_numbers, strict=True):
        if not row["active"]:
            reason = "Production InnDropItemConfig rejects " + ", ".join(k for k in ("monster_id", "item_id") if not row[k])
            if row["group"] < 0 or any(row[k] < 1 for k in ("prob_recip", "repeat", "count")):
                reason += "; disabled numeric value"
            notes.append({"domain": "drops", "source_path": "server/src/dropitemconfig.inc", "source_position": line_number, "raw": row, "reason": reason})
            continue
        mid = row["monster_id"]
        document = drops.setdefault(mid, {"schema_version": 1, "monster_id": mid, "monster_name": row["monster_name"], "items": []})
        document["items"].append({k: row[k] for k in ("item_id", "item_name", "group", "prob_recip", "repeat", "count")})
    for mid, document in sorted(drops.items()):
        write_json(output / f"drops/{mid}.json", document)
        for group in sorted({r["group"] for r in document["items"]}):
            for row in document["items"]:
                if row["group"] == group:
                    reconstructed.extend([[mid, group, row["item_id"], row["prob_recip"], row["count"]]] * row["repeat"])
    assert reconstructed == effective, "JSON does not reconstruct the unchanged production table"
    spawns, source_files = [], [root / "common/src" / f for f in ("dbcomid.hpp", "monsterrecord.hpp", "monsterrecord.inc", "itemrecord.inc", "maprecord.inc")]
    source_files.extend((drop_source, root / "server/src/dropitemconfig.cpp", root / "server/src/dropitemconfig.hpp"))
    raw_groups = inactive_groups = regions = cratios = 0
    for path in sorted((root / "server/script/map").glob("*.lua")):
        source = path.read_text(encoding="utf-8")
        parsed = spawn_literal(source)
        if parsed is None:
            continue
        source_files.append(path)
        mid = identities["maps"][path.stem]
        groups = []
        for index, group in enumerate(parsed[2]):
            raw_groups += 1
            assert set(group) == {"name", "loc"}
            monster_id = identities["monsters"].get(group["name"], 0)
            if not monster_id:
                inactive_groups += 1
                notes.append({"domain": "spawns", "source_path": path.relative_to(root).as_posix(), "source_position": index + 1, "raw": group, "reason": "DBCOM_MONSTERID returns zero; original monGener removes this group"})
                continue
            for location in group["loc"]:
                assert {"x", "y", "w", "h", "count", "time"} <= set(location) <= {"x", "y", "w", "h", "count", "time", "cratio"}
                regions += 1
                cratios += "cratio" in location
            groups.append({"monster_id": monster_id, "monster_name": group["name"], "loc": group["loc"]})
        spawns.append(mid)
        write_json(output / f"spawns/{mid}.json", {"schema_version": 1, "map_id": mid, "map_name": path.stem, "groups": groups})
    write_json(output / "manifest.json", {"schema_version": 1, "drop_monsters": sorted(drops), "spawn_maps": sorted(spawns)})
    write_json(output / "migration-notes.json", {"schema_version": 1, "source_revision": source_revision, "records": notes})
    evidence = {"source_revision": source_revision,
                "source_hashes": {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files},
                "catalog": catalog, "monster_experience": monsters, "raw_drops": rows, "expanded_production_drops": effective,
                "counts": {"monsters": len(monsters), "raw_drops": len(rows), "active_drops": sum(r["active"] for r in rows), "inactive_drops": len(rows) - sum(r["active"] for r in rows), "expanded_drops": len(effective), "spawn_maps": len(spawns), "raw_spawn_groups": raw_groups, "inactive_spawn_groups": inactive_groups, "active_regions": regions, "active_cratio": cratios}}
    write_json(root / "tools/gameplayconfig/migration/baseline.json", evidence)
    print(json.dumps(evidence["counts"], indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--scratch", type=Path, required=True)
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--include", action="append", default=[])
    parser.add_argument("--probe-output", type=Path, help="Reuse the already captured probe from the same frozen inputs")
    args = parser.parse_args()
    export(args.root.resolve(), args.scratch.resolve(), args.compiler, args.include, args.probe_output)
