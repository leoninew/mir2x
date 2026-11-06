from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import stat
import tempfile
from contextlib import suppress
from datetime import UTC, datetime
from pathlib import Path
from typing import cast
from uuid import uuid4

from pydantic import JsonValue, ValidationError

from mir2x_workbench.gameplay_models import (
    BackupMetadata,
    MigrationNotes,
    load_json,
    validate_bundle,
)
from mir2x_workbench.gameplay_service import GameplayError, WorkspaceService

_BACKUP_ID = re.compile(r"^[a-f0-9]{32}$")


class GameplayBackupService:
    """Central snapshots for the currently selected gameplay workspace."""

    def __init__(self, workspace: WorkspaceService, backup_root: Path | None) -> None:
        self._workspace = workspace
        self._backup_root = backup_root.resolve() if backup_root is not None else None
        self._lock = workspace._lock

    def list_backups(self, workspace_id: str) -> dict[str, JsonValue]:
        with self._lock:
            self._workspace._context(workspace_id)
            root = self._root()
            records: list[JsonValue] = []
            if not root.exists():
                return {"workspace_id": workspace_id, "backups": records}

            for entry in root.iterdir():
                if not _BACKUP_ID.fullmatch(entry.name) or entry.is_symlink():
                    continue
                if not entry.is_dir():
                    continue
                try:
                    metadata = self._metadata(entry)
                    if metadata.backup_id != entry.name:
                        raise ValueError("Backup identity does not match its directory")
                    record = self._record(metadata)
                    record["source_matches"] = self._same_source(
                        metadata.source_server_root, self._data_root().parent.parent
                    )
                    records.append(record)
                except (OSError, ValueError):
                    records.append(
                        {
                            "backup_id": entry.name,
                            "name": "备份信息损坏",
                            "created_at": "",
                            "description": "",
                            "available": False,
                            "source_server_root": "",
                            "source_matches": False,
                        }
                    )
            records.sort(
                key=lambda record: cast(
                    str, cast(dict[str, JsonValue], record).get("created_at", "")
                ),
                reverse=True,
            )
            return {"workspace_id": workspace_id, "backups": records}

    def create(self, workspace_id: str, description: str) -> dict[str, JsonValue]:
        with self._lock:
            self._workspace._context(workspace_id)
            source = self._data_root()
            self._validate_workspace(source)
            return self._create_locked(source, description)

    def restore(self, workspace_id: str, backup_id: str) -> dict[str, JsonValue]:
        with self._lock:
            self._workspace._context(workspace_id)
            if not _BACKUP_ID.fullmatch(backup_id):
                raise GameplayError(404, "Backup not found")

            source = self._data_root()
            backup = self._root() / backup_id
            if not backup.is_dir() or backup.is_symlink():
                raise GameplayError(404, "Backup not found")
            try:
                metadata = self._metadata(backup)
                if not self._same_source(metadata.source_server_root, source.parent.parent):
                    raise GameplayError(409, "Backup belongs to a different server directory")
                metadata = self._verify_backup(backup, source)
            except FileNotFoundError as error:
                raise GameplayError(404, "Backup not found") from error
            except GameplayError:
                raise
            except (OSError, ValueError, ValidationError) as error:
                raise GameplayError(422, f"Backup is invalid: {error}") from error

            parent = source.parent
            staged = parent / f".gameplay-restore-{uuid4().hex}"
            previous = parent / f".gameplay-previous-{uuid4().hex}"
            try:
                shutil.copytree(backup / "data", staged, symlinks=True)
                self._validate_workspace(staged)
                # The safety snapshot is published before the active directory changes.
                safety = self._create_locked(source, "还原前自动备份")
                source.rename(previous)
                try:
                    staged.rename(source)
                except OSError as swap_error:
                    try:
                        previous.rename(source)
                    except OSError as rollback_error:
                        raise GameplayError(
                            500,
                            f"Restore failed; original data remains at {previous}: "
                            f"{rollback_error}",
                        ) from swap_error
                    raise GameplayError(
                        500, f"Unable to activate backup; original data restored: {swap_error}"
                    ) from swap_error
                self._workspace._id = uuid4().hex
                with suppress(OSError):
                    shutil.rmtree(previous)
            except GameplayError:
                raise
            except (OSError, ValueError) as error:
                raise GameplayError(
                    500, f"Unable to restore backup; current data retained: {error}"
                ) from error
            finally:
                if staged.exists():
                    shutil.rmtree(staged, ignore_errors=True)

            return {
                "workspace": self._workspace.workspace(),
                "backup": self._record(metadata),
                "safety_backup": safety,
            }

    def _create_locked(self, source: Path, description: str) -> dict[str, JsonValue]:
        backup_id = uuid4().hex
        temporary: Path | None = None
        temporary_created = False
        try:
            root = self._root(create=True)
            temporary = root / f".tmp-{backup_id}"
            published = root / backup_id
            data = temporary / "data"
            self._assert_regular_tree(source)
            temporary.mkdir()
            temporary_created = True
            shutil.copytree(source, data, symlinks=True)
            files = self._file_hashes(data)
            self._validate_workspace(data)
            now = datetime.now().astimezone()
            metadata = BackupMetadata(
                schema_version=1,
                backup_id=backup_id,
                name=now.strftime("%Y-%m-%d %H:%M:%S"),
                created_at=now.astimezone(UTC).isoformat(timespec="seconds"),
                description=description,
                source_server_root=str(source.parent.parent),
                directories=self._directories(data),
                files=files,
            )
            self._write_metadata(temporary / "metadata.json", metadata)
            temporary.rename(published)
            return self._record(metadata)
        except (OSError, ValueError, ValidationError) as error:
            if temporary_created and temporary is not None and temporary.exists():
                shutil.rmtree(temporary, ignore_errors=True)
            raise GameplayError(500, f"Unable to create backup: {error}") from error

    def _verify_backup(self, backup: Path, active_root: Path) -> BackupMetadata:
        metadata = self._metadata(backup)
        if metadata.backup_id != backup.name:
            raise ValueError("Backup identity does not match its directory")
        if {path.name for path in backup.iterdir()} != {"metadata.json", "data"}:
            raise ValueError("Backup contains unexpected entries")
        data = backup / "data"
        self._assert_regular_tree(data)
        if self._directories(data) != metadata.directories:
            raise ValueError("Backup directories do not match the recorded inventory")
        actual = self._file_hashes(data)
        if actual != metadata.files:
            raise ValueError("Backup files do not match the recorded checksums")
        self._validate_workspace(data)
        self._assert_separate(data, active_root)
        return metadata

    def _metadata(self, backup: Path) -> BackupMetadata:
        if backup.is_symlink() or not backup.is_dir():
            raise ValueError("Backup directory is unavailable")
        metadata_path = backup / "metadata.json"
        if metadata_path.is_symlink() or not metadata_path.is_file():
            raise ValueError("Backup metadata is unavailable")
        return BackupMetadata.model_validate(load_json(metadata_path.read_bytes()))

    def _data_root(self) -> Path:
        root = self._workspace._root
        if root is None:
            raise GameplayError(409, "Choose a server directory first")
        return root

    def _root(self, *, create: bool = False) -> Path:
        if self._backup_root is None:
            raise GameplayError(503, "Central backup storage is unavailable")
        root = self._backup_root
        if root.is_symlink():
            raise GameplayError(422, "Central backup directory must not be a symbolic link")
        active = self._data_root()
        try:
            self._assert_separate(root, active)
        except ValueError as error:
            raise GameplayError(422, str(error)) from error
        if create:
            root.mkdir(parents=True, exist_ok=True)
            try:
                self._assert_separate(root, active)
            except ValueError as error:
                raise GameplayError(422, str(error)) from error
        return root

    @staticmethod
    def _assert_separate(left: Path, right: Path) -> None:
        left_resolved = left.resolve()
        right_resolved = right.resolve()
        if (
            left_resolved == right_resolved
            or left_resolved.is_relative_to(right_resolved)
            or right_resolved.is_relative_to(left_resolved)
        ):
            raise ValueError("Central backup directory and gameplay data must be separate")

    @staticmethod
    def _assert_regular_tree(root: Path) -> None:
        if root.is_symlink() or not root.is_dir():
            raise ValueError("Gameplay directory is unavailable")
        for path in root.rglob("*"):
            info = path.lstat()
            if stat.S_ISLNK(info.st_mode):
                raise ValueError(f"Symbolic links are not supported: {path.relative_to(root)}")
            if not (stat.S_ISDIR(info.st_mode) or stat.S_ISREG(info.st_mode)):
                raise ValueError(f"Unsupported file type: {path.relative_to(root)}")

    @classmethod
    def _file_hashes(cls, root: Path) -> dict[str, str]:
        cls._assert_regular_tree(root)
        result: dict[str, str] = {}
        for path in root.rglob("*"):
            if path.is_file():
                relative = path.relative_to(root).as_posix()
                result[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
        return dict(sorted(result.items()))

    @staticmethod
    def _directories(root: Path) -> list[str]:
        return sorted(
            path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_dir()
        )

    @staticmethod
    def _validate_workspace(root: Path) -> None:
        validate_bundle(root)
        MigrationNotes.model_validate(load_json((root / "migration-notes.json").read_bytes()))

    @staticmethod
    def _write_metadata(path: Path, metadata: BackupMetadata) -> None:
        raw = (
            json.dumps(metadata.model_dump(mode="json"), ensure_ascii=False, indent=2) + "\n"
        ).encode("utf-8")
        descriptor, temporary = tempfile.mkstemp(
            prefix=".metadata-", suffix=".tmp", dir=path.parent
        )
        try:
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(raw)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, path)
        finally:
            Path(temporary).unlink(missing_ok=True)

    @staticmethod
    def _record(metadata: BackupMetadata) -> dict[str, JsonValue]:
        return {
            "backup_id": metadata.backup_id,
            "name": metadata.name,
            "created_at": metadata.created_at,
            "description": metadata.description,
            "available": True,
            "source_server_root": metadata.source_server_root,
            "source_matches": True,
        }

    @staticmethod
    def _same_source(stored: str, current: Path) -> bool:
        return os.path.normcase(stored) == os.path.normcase(str(current.resolve()))
