"""Strict interchange models shared by file loading and the gameplay API."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Annotated, Literal

from pydantic import (
    BaseModel,
    ConfigDict,
    Field,
    JsonValue,
    StrictInt,
    ValidationError,
    field_validator,
)

type Int32 = Annotated[StrictInt, Field(ge=-(2**31), le=2**31 - 1)]
type Nonnegative = Annotated[StrictInt, Field(ge=0, le=2**31 - 1)]
type Positive = Annotated[StrictInt, Field(ge=1, le=2**31 - 1)]
type Identity = Annotated[StrictInt, Field(ge=1, le=2**32 - 1)]
type Name = Annotated[str, Field(min_length=1)]
type Domain = Literal["monsters", "drops", "spawns"]
type BackupId = Annotated[str, Field(pattern=r"^[a-f0-9]{32}$")]
type Sha256 = Annotated[str, Field(pattern=r"^[a-f0-9]{64}$")]


class Model(BaseModel):
    model_config = ConfigDict(strict=True, extra="forbid")


class Document(Model):
    schema_version: Annotated[StrictInt, Field(ge=1, le=1)]


class IdentityRecord(Model):
    id: Identity
    name: Name


class Catalog(Document):
    monsters: list[IdentityRecord]
    items: list[IdentityRecord]
    maps: list[IdentityRecord]


class Manifest(Document):
    drop_monsters: list[Identity]
    spawn_maps: list[Identity]


class Monster(Model):
    monster_id: Identity
    monster_name: Name
    exp: Nonnegative


class Monsters(Document):
    monsters: list[Monster]


class DropItem(Model):
    item_id: Identity
    item_name: Name
    group: Nonnegative
    prob_recip: Positive
    repeat: Positive
    count: Positive


class Drops(Document):
    monster_id: Identity
    monster_name: Name
    items: list[DropItem]


class Region(Model):
    x: Int32
    y: Int32
    w: Positive
    h: Positive
    count: Nonnegative
    time: Positive
    cratio: Int32 | None = None

    @field_validator("cratio", mode="before")
    @classmethod
    def explicit_cratio_is_integer(cls, value: object) -> object:
        if value is None:
            raise ValueError("cratio: expected integer, not null")
        return value


class SpawnGroup(Model):
    monster_id: Identity
    monster_name: Name
    loc: list[Region]


class Spawns(Document):
    map_id: Identity
    map_name: Name
    groups: list[SpawnGroup]


class MigrationRecord(Model):
    domain: Literal["drops", "spawns"]
    source_path: Name
    source_position: Positive
    raw: dict[str, JsonValue]
    reason: Name


class MigrationNotes(Document):
    source_revision: Name
    records: list[MigrationRecord]


class ConfigRequest(Model):
    workspace_id: Name
    domain: Domain
    object_id: Identity | None = None
    catalog_revision: Name
    data: dict[str, JsonValue]


class SaveRequest(ConfigRequest):
    revision: Name


class BackupCreateRequest(Model):
    workspace_id: Name
    description: Annotated[str, Field(max_length=1000)] = ""


class BackupRestoreRequest(Model):
    workspace_id: Name


class BackupMetadata(Model):
    schema_version: Annotated[StrictInt, Field(ge=1, le=1)]
    backup_id: BackupId
    name: Name
    created_at: Name
    description: Annotated[str, Field(max_length=1000)]
    source_server_root: Name
    directories: list[Name]
    files: dict[Name, Sha256]


def load_json(raw: bytes) -> dict[str, JsonValue]:
    def pairs(values: list[tuple[str, JsonValue]]) -> dict[str, JsonValue]:
        result: dict[str, JsonValue] = {}
        for key, value in values:
            if key in result:
                raise ValueError(f"Duplicate object key: {key}")
            result[key] = value
        return result

    def constant(value: str) -> JsonValue:
        raise ValueError(f"Not a JSON number: {value}")

    value: JsonValue = json.loads(
        raw.decode("utf-8"), object_pairs_hook=pairs, parse_constant=constant
    )
    if not isinstance(value, dict):
        raise ValueError("Expected a JSON object")
    return value


def identity_map(catalog: Catalog, category: str) -> dict[int, str]:
    records: list[IdentityRecord] = getattr(catalog, category)
    result = {row.id: row.name for row in records}
    if len(result) != len(records):
        raise ValueError(f"catalog.{category}: duplicate identity")
    return result


def check_reference(values: dict[int, str], identity: int, name: str, path: str) -> None:
    if values.get(identity) != name:
        raise ValueError(f"{path}: identity/name does not match catalog")


def validate_document(
    domain: Domain, data: dict[str, JsonValue], catalog: Catalog, object_id: int | None = None
) -> Monsters | Drops | Spawns:
    monsters = identity_map(catalog, "monsters")
    if domain == "monsters":
        if object_id is not None:
            raise ValueError("monsters: object_id must be omitted")
        document = Monsters.model_validate(data)
        ids = [row.monster_id for row in document.monsters]
        if len(set(ids)) != len(ids) or set(ids) != set(monsters):
            raise ValueError("monsters: identities must cover the catalog exactly once")
        for row in document.monsters:
            check_reference(monsters, row.monster_id, row.monster_name, "monsters.monster_name")
        return document
    if domain == "drops":
        drops = Drops.model_validate(data)
        if drops.monster_id != object_id:
            raise ValueError("monster_id: file identity differs from manifest")
        check_reference(monsters, drops.monster_id, drops.monster_name, "monster_name")
        items = identity_map(catalog, "items")
        for index, item in enumerate(drops.items):
            check_reference(items, item.item_id, item.item_name, f"items.{index}.item_name")
        return drops
    spawns = Spawns.model_validate(data)
    if spawns.map_id != object_id:
        raise ValueError("map_id: file identity differs from manifest")
    check_reference(identity_map(catalog, "maps"), spawns.map_id, spawns.map_name, "map_name")
    for index, group in enumerate(spawns.groups):
        check_reference(
            monsters, group.monster_id, group.monster_name, f"groups.{index}.monster_name"
        )
    return spawns


def issues_for(error: ValueError) -> list[JsonValue]:
    if isinstance(error, ValidationError):
        return [
            {"path": ".".join(map(str, entry["loc"])), "message": entry["msg"]}
            for entry in error.errors(include_url=False, include_input=False)
        ]
    path, _, message = str(error).partition(": ")
    return [{"path": path if message else "", "message": message or str(error)}]


def validate_bundle(root: Path) -> tuple[Catalog, Manifest]:
    catalog = Catalog.model_validate(load_json((root / "catalog.json").read_bytes()))
    for category in ("monsters", "items", "maps"):
        identity_map(catalog, category)
    manifest = Manifest.model_validate(load_json((root / "manifest.json").read_bytes()))
    for ids in (manifest.drop_monsters, manifest.spawn_maps):
        if len(set(ids)) != len(ids):
            raise ValueError("manifest: duplicate identity")
    validate_document("monsters", load_json((root / "monsters.json").read_bytes()), catalog)
    indexed: tuple[tuple[Domain, list[int]], ...] = (
        ("drops", manifest.drop_monsters),
        ("spawns", manifest.spawn_maps),
    )
    for domain, ids in indexed:
        for identity in ids:
            validate_document(
                domain,
                load_json((root / domain / f"{identity}.json").read_bytes()),
                catalog,
                identity,
            )
    return catalog, manifest
