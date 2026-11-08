"""Export the repository's static map connections and teleport NPCs for the guide."""

from __future__ import annotations

import json
import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "tools/gameplay-workbench/web/src/domains/map-guide/map-guide.json"
MAP_RECORD = ROOT / "common/src/maprecord.inc"
CATALOG = ROOT / "server/config/gameplay/catalog.json"
NPC_DIR = ROOT / "server/script/npc"

MAP_NAME = re.compile(r'^\{\s*\.name\s*=\s*u8"([^"]+)"')
SWITCH = re.compile(
    r"\.x\s*=\s*(-?\d+).*?\.y\s*=\s*(-?\d+).*?\.w\s*=\s*(\d+)"
    r'.*?\.h\s*=\s*(\d+).*?\.endName\s*=\s*u8"([^"]+)"'
    r".*?\.endX\s*=\s*(-?\d+).*?\.endY\s*=\s*(-?\d+)"
)
NPC_NAME = re.compile(r"^(.+)\.(.+)\.GLOC_(\d+)_(\d+)_\d+\.LOOK_\d+\.lua$")
MENU_ENTRY = re.compile(r"\{([^{}]*\bmap\s*=\s*'[^']+'[^{}]*)\}")
DIRECT_ENTRY = re.compile(r"tp\.uidReqSpaceMove\(uid,\s*'([^']+)',\s*(\d+),\s*(\d+)\s*\)")

# Browsing categories follow names and resource-code families, not a world-map parent relation.
MAP_GROUPS = [
    (r"0$", ("城镇与野外", "比奇县")),
    (r"01$", ("城镇与野外", "边境城市")),
    (r"02$", ("城镇与野外", "银杏山谷")),
    (r"1$", ("城镇与野外", "道馆")),
    (r"12\d*$", ("城镇与野外", "灌木林")),
    (r"2$", ("城镇与野外", "毒蛇山谷")),
    (r"3$", ("城镇与野外", "沙巴克城")),
    (r"31$", ("洞穴与地宫", "祖玛神殿")),
    (r"4$|40\d$", ("城镇与野外", "绿洲")),
    (r"41$", ("城镇与野外", "诺玛村庄")),
    (r"4[234]$|6$|7[123678]$", ("城镇与野外", "沙漠")),
    (r"5$", ("城镇与野外", "沙漠土城")),
    (r"74$", ("城镇与野外", "盟重县")),
    (r"75$", ("城镇与野外", "石阁庙")),
    (r"8$", ("城镇与野外", "潘夜岛")),
    (r"81$", ("城镇与野外", "流放岛")),
    (r"9$", ("城镇与野外", "失乐园")),
    (r"D00", ("洞穴与地宫", "半兽洞穴")),
    (r"D01", ("洞穴与地宫", "天然洞穴")),
    (r"D0[2345]", ("洞穴与地宫", "沃玛神殿")),
    (r"D10", ("洞穴与地宫", "赤月山谷")),
    (r"D11", ("洞穴与地宫", "潘夜神殿")),
    (r"D120", ("洞穴与地宫", "罪孽洞穴")),
    (r"D121", ("洞穴与地宫", "潘夜石窟")),
    (r"D130", ("矿区", "银杏废矿", "东部")),
    (r"D131", ("矿区", "银杏废矿", "北部")),
    (r"D132", ("矿区", "银杏废矿", "西部")),
    (r"D140", ("矿区", "石矿", "东部")),
    (r"D141", ("矿区", "石矿", "北部")),
    (r"D142", ("矿区", "石矿", "西部")),
    (r"D150", ("洞穴与地宫", "真天宫")),
    (r"D151", ("洞穴与地宫", "黑度宫")),
    (r"D16", ("洞穴与地宫", "诺玛遗址")),
    (r"D2", ("未命名与其他", "D2 系列")),
    (r"D4[01]", ("矿区", "废矿矿山")),
    (r"D42", ("矿区", "矿山")),
    (r"D4[34]", ("矿区", "北部矿山")),
    (r"D45", ("矿区", "南部矿山")),
    (r"D5", ("洞穴与地宫", "祖玛神殿")),
    (r"D600", ("洞穴与地宫", "绝命谷")),
    (r"D601", ("洞穴与地宫", "万年谷")),
    (r"D602", ("洞穴与地宫", "虫峡谷")),
    (r"D60", ("洞穴与地宫", "石屋与通路")),
    (r"D7", ("洞穴与地宫", "石阁")),
    (r"D80", ("洞穴与地宫", "蚂蚁洞穴", "南部")),
    (r"D81", ("洞穴与地宫", "蚂蚁洞穴", "北部")),
    (r"D82", ("洞穴与地宫", "蚂蚁洞穴", "东部")),
    (r"D9", ("洞穴与地宫", "神舰")),
    (r"DBEVENT", ("特殊场景", "活动地图")),
    (r"DM", ("洞穴与地宫", "洞穴附属场景")),
    (r"E00", ("洞穴与地宫", "半兽天然洞穴")),
    (r"E40", ("洞穴与地宫", "连接通路")),
    (r"E60", ("矿区", "废矿秘密通路")),
    (r"B", ("未命名与其他", "B 系列")),
    (r"ISLAND", ("特殊场景", "生存游戏场")),
    (r"KT", ("未命名与其他", "KT 系列")),
    (r"NPCROOM", ("特殊场景", "NPC 场景")),
    (r"OXMAP", ("未命名与其他", "OXMAP 系列")),
    (r"Q", ("未命名与其他", "Q 系列")),
    (r"RUSH", ("特殊场景", "试练屋")),
    (r"SNOW", ("特殊场景", "打雪仗")),
    (r"Z", ("特殊场景", "慈善屋与 Z 系列")),
]


def group_path(name: str) -> tuple[str, ...]:
    # These names share resource codes with maps from a different series.
    for prefix, path in (
        ("沙巴克", ("城镇与野外", "沙巴克城")),
        ("祖玛", ("洞穴与地宫", "祖玛神殿")),
        ("行会大战", ("特殊场景", "行会大战")),
        ("竞技场", ("特殊场景", "竞技场")),
    ):
        if name.startswith(prefix):
            return path
    code = name.split("_", 1)[1].split("_", 1)[0]
    for pattern, path in MAP_GROUPS:
        if re.match(pattern, code):
            return path
    return ("未命名与其他", "其他地图")


def string_field(entry: str, field: str) -> str | None:
    match = re.search(rf"\b{field}\s*=\s*'([^']+)'", entry)
    return match.group(1) if match else None


def number_field(entry: str, field: str, default: int = 0) -> int:
    match = re.search(rf"\b{field}\s*=\s*(\d+)", entry)
    if not match and re.search(rf"\b{field}\s*=", entry):
        raise ValueError(f"Unsupported {field} in teleport entry: {entry}")
    return int(match.group(1)) if match else default


def export() -> dict:
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    maps = [
        {
            "id": row["id"],
            "name": row["name"],
            "groupPath": group_path(row["name"]),
            "exits": [],
            "teleports": [],
        }
        for row in catalog["maps"]
    ]
    by_name = {row["name"]: row for row in maps}
    if len(by_name) != len(maps):
        raise ValueError("Duplicate map names in catalog")

    # Map loading applies valid switch records in order, so later records own overlapping cells.
    grids: dict[str, dict[tuple[int, int], tuple[str, int, int]]] = defaultdict(dict)
    current = ""
    for line in MAP_RECORD.read_text(encoding="utf-8").splitlines():
        if match := MAP_NAME.match(line):
            current = match.group(1)
            if current not in by_name:
                raise ValueError(f"Map missing from catalog: {current}")
        if ".endName" not in line:
            continue
        match = SWITCH.search(line)
        if not match or not current:
            raise ValueError(f"Unsupported map switch: {line}")
        x, y, width, height = map(int, match.group(1, 2, 3, 4))
        target = match.group(5)
        landing = (target, int(match.group(6)), int(match.group(7)))
        if target not in by_name:
            continue
        for dx in range(width):
            for dy in range(height):
                grids[current][x + dx, y + dy] = landing

    for name, cells in grids.items():
        destinations: dict[str, dict[tuple[int, int], list[list[int]]]] = {}
        for (x, y), (target, tx, ty) in cells.items():
            destinations.setdefault(target, {}).setdefault((tx, ty), []).append([x, y])
        for target, landings in destinations.items():
            by_name[name]["exits"].append(
                {
                    "mapId": by_name[target]["id"],
                    "mapName": target,
                    "gates": [
                        {"entrances": coordinates, "arrival": [tx, ty]}
                        for (tx, ty), coordinates in landings.items()
                    ],
                }
            )

    for path in sorted(NPC_DIR.glob("*.lua")):
        text = path.read_text(encoding="utf-8")
        if "require('npc.include.teleport')" not in text:
            continue
        match = NPC_NAME.match(path.name)
        if not match:
            raise ValueError(f"Unsupported NPC filename: {path.name}")
        map_name, npc_name, x, y = match.groups()
        if map_name not in by_name:
            raise ValueError(f"NPC map missing from catalog: {map_name}")
        entries = MENU_ENTRY.findall(text)
        if len(entries) != len(re.findall(r"\bmap\s*=", text)):
            raise ValueError(f"Unsupported teleport menu: {path.name}")
        destinations = []
        for entry in entries:
            target = string_field(entry, "map")
            if target not in by_name:
                raise ValueError(f"Unknown teleport target {target}: {path.name}")
            destinations.append(
                {
                    "mapId": by_name[target]["id"],
                    "mapName": target,
                    "label": string_field(entry, "name") or target.split("_")[0],
                    "arrival": [number_field(entry, "x", -1), number_field(entry, "y", -1)],
                    "gold": number_field(entry, "gold"),
                    "level": number_field(entry, "level"),
                }
            )
        direct = DIRECT_ENTRY.findall(text)
        if len(direct) != text.count("tp.uidReqSpaceMove("):
            raise ValueError(f"Unsupported direct teleport: {path.name}")
        for target, tx, ty in direct:
            if target not in by_name:
                raise ValueError(f"Unknown teleport target {target}: {path.name}")
            destinations.append(
                {
                    "mapId": by_name[target]["id"],
                    "mapName": target,
                    "label": target.split("_")[0],
                    "arrival": [int(tx), int(ty)],
                    "gold": 0,
                    "level": 0,
                }
            )
        if not destinations or any(-1 in item["arrival"] for item in destinations):
            raise ValueError(f"Invalid teleport destinations: {path.name}")
        by_name[map_name]["teleports"].append(
            {"name": npc_name, "location": [int(x), int(y)], "destinations": destinations}
        )

    return {"schemaVersion": 1, "maps": maps}


def main() -> None:
    data = export()
    OUTPUT.write_text(
        json.dumps(data, ensure_ascii=False, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )
    print(f"Exported {len(data['maps'])} maps to {OUTPUT}")


if __name__ == "__main__":
    main()
