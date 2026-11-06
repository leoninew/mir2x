"""Write the version-one interchange schemas and shared contract test mutations."""

import json
from pathlib import Path

from export_baseline import write_json


def obj(properties: dict, *, optional: tuple[str, ...] = ()) -> dict:
    return {"type": "object", "additionalProperties": False, "required": [key for key in properties if key not in optional], "properties": properties}


def array(items: dict) -> dict:
    return {"type": "array", "items": items}


def document(properties: dict) -> dict:
    return {"$schema": "https://json-schema.org/draft/2020-12/schema", **obj({"schema_version": {"const": 1, "type": "integer"}, **properties})}


def main() -> None:
    root = Path(__file__).resolve().parents[3]
    config = root / "server/config/gameplay"
    identity = {"type": "integer", "minimum": 1, "maximum": 4294967295}
    integer = {"type": "integer", "minimum": -2147483648, "maximum": 2147483647}
    positive = {**integer, "minimum": 1}
    nonnegative = {**integer, "minimum": 0}
    name = {"type": "string", "minLength": 1}
    schemas = {
        "manifest": document({"drop_monsters": {**array(identity), "uniqueItems": True}, "spawn_maps": {**array(identity), "uniqueItems": True}}),
        "monsters": document({"monsters": array(obj({"monster_id": identity, "monster_name": name, "exp": nonnegative}))}),
        "drops": document({"monster_id": identity, "monster_name": name, "items": array(obj({"item_id": identity, "item_name": name, "group": nonnegative, "prob_recip": positive, "repeat": positive, "count": positive}))}),
        "spawns": document({"map_id": identity, "map_name": name, "groups": array(obj({"monster_id": identity, "monster_name": name, "loc": array(obj({"x": integer, "y": integer, "w": positive, "h": positive, "count": nonnegative, "time": positive, "cratio": integer}, optional=("cratio",)))}))}),
        "catalog": document({key: array(obj({"id": identity, "name": name})) for key in ("monsters", "items", "maps")}),
        "migration-notes": document({"source_revision": name, "records": array(obj({"domain": {"enum": ["drops", "spawns"]}, "source_path": name, "source_position": positive, "raw": {"type": "object"}, "reason": name}))}),
    }
    for key, schema in schemas.items():
        write_json(config / f"schema/{key}.schema.json", schema)
    original = json.loads((config / "monsters.json").read_text(encoding="utf-8"))
    cases = [{"name": "valid", "valid": True}]
    for label, value in (("negative", -1), ("boolean", True), ("float", 1.0), ("string", "1"), ("overflow", 2147483648)):
        data = json.loads(json.dumps(original))
        data["monsters"][0]["exp"] = value
        cases.append({"name": f"exp-{label}", "valid": False, "file": "monsters.json", "data": data})
    for label, change in (
        ("duplicate-id", lambda d: d["monsters"].append(d["monsters"][0].copy())),
        ("unknown-id", lambda d: d["monsters"][0].update(monster_id=4294967295)),
        ("wrong-name", lambda d: d["monsters"][0].update(monster_name="wrong")),
        ("missing-id", lambda d: d["monsters"].pop()),
        ("unknown-field", lambda d: d.update(tuning=1)),
    ):
        data = json.loads(json.dumps(original))
        change(data)
        cases.append({"name": label, "valid": False, "file": "monsters.json", "data": data})
    cases.extend([
        {"name": "duplicate-object-key", "valid": False, "file": "manifest.json", "raw": '{"schema_version":1,"schema_version":1,"drop_monsters":[],"spawn_maps":[]}'},
        {"name": "missing-indexed-file", "valid": False, "file": "manifest.json", "data": {"schema_version": 1, "drop_monsters": [4294967295], "spawn_maps": []}},
        {"name": "nan", "valid": False, "file": "manifest.json", "raw": '{"schema_version":NaN,"drop_monsters":[],"spawn_maps":[]}'},
    ])
    manifest = json.loads((config / "manifest.json").read_text(encoding="utf-8"))
    drop_file = f"drops/{manifest['drop_monsters'][0]}.json"
    drops = json.loads((config / drop_file).read_text(encoding="utf-8"))
    for label, key, value in (
        ("group-negative", "group", -1), ("probability-zero", "prob_recip", 0),
        ("repeat-zero", "repeat", 0), ("count-zero", "count", 0),
        ("unknown-item", "item_id", 4294967295),
    ):
        data = json.loads(json.dumps(drops))
        data["items"][0][key] = value
        cases.append({"name": f"drops-{label}", "valid": False, "file": drop_file, "data": data})
    data = json.loads(json.dumps(drops))
    data["monster_id"] = 4294967295
    cases.append({"name": "drops-file-identity", "valid": False, "file": drop_file, "data": data})
    spawn_file = f"spawns/{manifest['spawn_maps'][0]}.json"
    spawns = json.loads((config / spawn_file).read_text(encoding="utf-8"))
    for label, key, value in (
        ("time-zero", "time", 0), ("count-negative", "count", -1),
        ("width-zero", "w", 0), ("coordinate-overflow", "x", -2147483649),
        ("cratio-null", "cratio", None),
    ):
        data = json.loads(json.dumps(spawns))
        data["groups"][0]["loc"][0][key] = value
        cases.append({"name": f"spawns-{label}", "valid": False, "file": spawn_file, "data": data})
    data = json.loads(json.dumps(spawns))
    data["groups"][0]["monster_id"] = 4294967295
    cases.append({"name": "spawns-unknown-monster", "valid": False, "file": spawn_file, "data": data})
    write_json(root / "server/test/data/gameplay/contract/expected-results.json", {"schema_version": 1, "base": "server/config/gameplay", "cases": cases})


if __name__ == "__main__":
    main()
