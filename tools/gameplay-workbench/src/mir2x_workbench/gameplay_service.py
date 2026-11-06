from __future__ import annotations

import hashlib
import json
import os
import tempfile
from pathlib import Path
from threading import RLock
from uuid import uuid4

from pydantic import JsonValue

from mir2x_workbench.gameplay_models import (
    Catalog,
    ConfigRequest,
    Domain,
    Manifest,
    MigrationNotes,
    SaveRequest,
    Spawns,
    identity_map,
    issues_for,
    load_json,
    validate_bundle,
    validate_document,
)


class GameplayError(Exception):
    def __init__(self, status: int, message: str):
        self.status = status
        super().__init__(message)


def digest(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


class WorkspaceService:
    """One selected server directory and one serialized file writer per application."""

    def __init__(self, server_root: Path | None = None) -> None:
        self._lock = RLock()
        self._root: Path | None = None
        self._id: str | None = None
        if server_root is not None:
            self.bind(server_root)

    def bind(self, server_root: Path) -> dict[str, JsonValue]:
        server = server_root.resolve(strict=True)
        root = (server / "config/gameplay").resolve(strict=True)
        if not root.is_relative_to(server):
            raise GameplayError(422, "Gameplay directory escapes selected server directory")
        with self._lock:
            try:
                # Do not allow a symlinked document to redirect a managed file elsewhere.
                for path in root.rglob("*.json"):
                    if not path.resolve().is_relative_to(root):
                        raise ValueError("Configuration contains an external symlink")
                validate_bundle(root)
                MigrationNotes.model_validate(
                    load_json((root / "migration-notes.json").read_bytes())
                )
            except (ValueError, OSError) as error:
                raise GameplayError(422, f"Invalid gameplay workspace: {error}") from error
            self._root = root
            self._id = uuid4().hex
            return self.workspace()

    def workspace(self) -> dict[str, JsonValue]:
        with self._lock:
            return {
                "bound": self._root is not None,
                "workspace_id": self._id,
                "server_root": str(self._root.parent.parent) if self._root else None,
            }

    def _path(self, relative: str) -> Path:
        if self._root is None:
            raise GameplayError(409, "Choose a server directory first")
        path = self._root / relative
        if not path.resolve().is_relative_to(self._root):
            raise GameplayError(422, "Configuration path escapes workspace")
        return path

    def _read(self, relative: str) -> bytes:
        try:
            return self._path(relative).read_bytes()
        except OSError as error:
            raise GameplayError(422, f"Unable to read {relative}: {error}") from error

    def _context(self, workspace_id: str) -> tuple[Catalog, Manifest, str]:
        if self._id is None or workspace_id != self._id:
            raise GameplayError(409, "Workspace changed; keep your draft and reload")
        try:
            catalog_raw = self._read("catalog.json")
            manifest_raw = self._read("manifest.json")
            catalog = Catalog.model_validate(load_json(catalog_raw))
            manifest = Manifest.model_validate(load_json(manifest_raw))
            for ids, category in (
                (manifest.drop_monsters, "monsters"),
                (manifest.spawn_maps, "maps"),
            ):
                if len(set(ids)) != len(ids) or not set(ids) <= set(
                    identity_map(catalog, category)
                ):
                    raise ValueError("manifest: unknown or duplicate identity")
            revision = digest(
                catalog_raw + b"\0" + manifest_raw + b"\0" + self._read("migration-notes.json")
            )
            return catalog, manifest, revision
        except ValueError as error:
            raise GameplayError(422, f"Invalid workspace metadata: {error}") from error


class GameplayConfigService:
    def __init__(self, workspace: WorkspaceService):
        self.workspace = workspace
        self._lock = workspace._lock
        self._context = workspace._context
        self._path = workspace._path
        self._read = workspace._read

    def catalog(self, workspace_id: str) -> dict[str, JsonValue]:
        with self._lock:
            catalog, manifest, revision = self._context(workspace_id)
            return {
                "workspace_id": workspace_id,
                "catalog_revision": revision,
                "catalog": catalog.model_dump(mode="json"),
                "manifest": manifest.model_dump(mode="json"),
                "migration_notes": load_json(self._read("migration-notes.json")),
                "fields": {
                    "exp": "基础经验；保留原伤害分配、最低值与取整",
                    "prob_recip": "概率分母；1/p，不是最终独立爆率",
                    "group": "0组逐项尝试；其他组最多掉落一项，保留条目顺序",
                    "repeat": "尝试次数；按原顺序展开",
                    "count": "掉落数量；刷怪中为原目标数，0仍沿原最少1规则",
                    "time": "刷怪周期，单位秒",
                    "cratio": "历史字段，当前未消费；只读",
                },
            }

    @staticmethod
    def _relative(domain: Domain, object_id: int | None, manifest: Manifest) -> str:
        if domain == "monsters":
            if object_id is not None:
                raise GameplayError(422, "monsters does not use object_id")
            return "monsters.json"
        ids = manifest.drop_monsters if domain == "drops" else manifest.spawn_maps
        if object_id is None or object_id not in ids:
            raise GameplayError(404, "Object is not present in the configuration manifest")
        return f"{domain}/{object_id}.json"

    def get(self, workspace_id: str, domain: Domain, object_id: int | None) -> dict[str, JsonValue]:
        with self._lock:
            catalog, manifest, catalog_revision = self._context(workspace_id)
            relative = self._relative(domain, object_id, manifest)
            raw = self._read(relative)
            try:
                data = load_json(raw)
                validate_document(domain, data, catalog, object_id)
            except ValueError as error:
                raise GameplayError(422, f"Invalid {relative}: {error}") from error
            return {
                "workspace_id": workspace_id,
                "domain": domain,
                "object_id": object_id,
                "catalog_revision": catalog_revision,
                "revision": digest(raw),
                "data": data,
            }

    def _validate(
        self, payload: ConfigRequest, expected_revision: str | None = None
    ) -> tuple[str, bytes]:
        catalog, manifest, revision = self._context(payload.workspace_id)
        if payload.catalog_revision != revision:
            raise GameplayError(409, "Catalog changed; keep your draft and reload")
        relative = self._relative(payload.domain, payload.object_id, manifest)
        old_raw = self._read(relative)
        if expected_revision is not None and digest(old_raw) != expected_revision:
            raise GameplayError(409, "File changed outside the application; keep your draft")
        validate_document(payload.domain, payload.data, catalog, payload.object_id)
        if payload.domain == "spawns":
            old = validate_document("spawns", load_json(old_raw), catalog, payload.object_id)
            new = validate_document("spawns", payload.data, catalog, payload.object_id)
            assert isinstance(old, Spawns) and isinstance(new, Spawns)
            # Comparison includes group/region placement so read-only data cannot migrate silently.
            if [[r.cratio for r in g.loc] for g in old.groups] != [
                [r.cratio for r in g.loc] for g in new.groups
            ]:
                raise ValueError("cratio: historical unused fields are read-only")
        return relative, old_raw

    def validate(self, payload: ConfigRequest) -> dict[str, JsonValue]:
        with self._lock:
            try:
                self._validate(payload)
            except ValueError as error:
                return {"valid": False, "issues": issues_for(error)}
            return {"valid": True, "issues": []}

    def save(self, payload: SaveRequest) -> dict[str, JsonValue]:
        with self._lock:
            try:
                relative, old_raw = self._validate(payload, payload.revision)
            except ValueError as error:
                raise GameplayError(422, str(error)) from error
            path = self._path(relative)
            content = (
                json.dumps(payload.data, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
            ).encode("utf-8")
            backup = self._path(f".backups/{relative.removesuffix('.json')}/{uuid4().hex}.json")
            temporary: str | None = None
            try:
                backup.parent.mkdir(parents=True, exist_ok=True)
                with backup.open("xb") as stream:
                    stream.write(old_raw)
                    stream.flush()
                    os.fsync(stream.fileno())
                descriptor, temporary = tempfile.mkstemp(
                    prefix=".gameplay-", suffix=".tmp", dir=path.parent
                )
                with os.fdopen(descriptor, "wb") as stream:
                    stream.write(content)
                    stream.flush()
                    os.fsync(stream.fileno())
                os.replace(temporary, path)
                temporary = None
            except OSError as error:
                raise GameplayError(
                    500, "Unable to save configuration; original data retained"
                ) from error
            finally:
                if temporary is not None:
                    Path(temporary).unlink(missing_ok=True)
            return self.get(payload.workspace_id, payload.domain, payload.object_id)
