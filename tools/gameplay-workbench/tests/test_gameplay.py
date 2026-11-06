import copy
import json
import logging
import os
import shutil
import subprocess
from datetime import datetime
from pathlib import Path
from typing import Any, cast

import pytest
from fastapi import FastAPI
from fastapi.testclient import TestClient

from mir2x_workbench.api import create_app
from mir2x_workbench.config import load_settings
from mir2x_workbench.gameplay_models import load_json, validate_bundle
from mir2x_workbench.gameplay_service import WorkspaceService

ROOT = Path(__file__).resolve().parents[3]


@pytest.fixture
def server_root(tmp_path: Path) -> Path:
    root = tmp_path / "server with spaces"
    shutil.copytree(ROOT / "server/config/gameplay", root / "config/gameplay")
    return root


@pytest.fixture
def client(server_root: Path) -> TestClient:
    workspace = WorkspaceService(server_root)
    app = create_app(
        logging.getLogger("tests.gameplay"),
        settings=load_settings(Path(__file__).parents[1]),
        workspace=workspace,
    )
    return TestClient(app)


def test_desktop_loads_workspace_from_game_launch_json(server_root: Path, tmp_path: Path) -> None:
    from mir2x_workbench.desktop import LocalApiServer
    from mir2x_workbench.game_launcher import GameLauncher

    application_root = tmp_path / "workbench"
    application_root.mkdir()
    launcher = GameLauncher(application_root)
    launcher.save(
        "../client/client.exe", "../server/server.exe", "missing", "../server with spaces"
    )
    settings = load_settings(Path(__file__).parents[1])
    api = LocalApiServer(settings, tmp_path / "static", application_root)
    assert api.workspace.workspace()["server_root"] == str(server_root.resolve())


def request_data(
    client: TestClient, domain: str = "monsters", object_id: int | None = None
) -> dict[str, Any]:
    workspace_id = client.get("/api/gameplay/workspace").json()["workspace_id"]
    params: dict[str, str | int] = {"workspace_id": workspace_id, "domain": domain}
    if object_id is not None:
        params["object_id"] = object_id
    response = client.get("/api/gameplay/config", params=params)
    assert response.status_code == 200, response.text
    return response.json()  # type: ignore[no-any-return]


def test_real_api_save_validate_read_and_backup(client: TestClient, server_root: Path) -> None:
    payload = request_data(client)
    original = (server_root / "config/gameplay/monsters.json").read_bytes()
    payload["data"]["monsters"][0]["exp"] += 123
    validation = {key: value for key, value in payload.items() if key != "revision"}
    assert client.post("/api/gameplay/config/validate", json=validation).json() == {
        "valid": True,
        "issues": [],
    }
    saved = client.put("/api/gameplay/config", json=payload)
    assert saved.status_code == 200, saved.text
    assert saved.json()["revision"] != payload["revision"]
    assert request_data(client)["data"] == payload["data"]
    backups = list((server_root / "config/gameplay/.backups").rglob("*.json"))
    assert len(backups) == 1 and backups[0].read_bytes() == original


def test_gameplay_backup_create_list_and_restore(server_root: Path, tmp_path: Path) -> None:
    backup_root = tmp_path / "workbench" / "backups" / "gameplay"
    app = create_app(
        logging.getLogger("tests.gameplay.backups"),
        settings=load_settings(Path(__file__).parents[1]),
        workspace=WorkspaceService(server_root),
        backup_root=backup_root,
    )
    with TestClient(app) as test_client:
        workspace = test_client.get("/api/gameplay/workspace").json()
        original = (server_root / "config/gameplay/monsters.json").read_bytes()
        created = test_client.post(
            "/api/gameplay/backups",
            json={"workspace_id": workspace["workspace_id"], "description": "改动前"},
        )
        assert created.status_code == 200, created.text
        record = created.json()
        datetime.strptime(record["name"], "%Y-%m-%d %H:%M:%S")
        assert record["description"] == "改动前"
        assert record["source_matches"] is True

        listed = test_client.get(
            "/api/gameplay/backups", params={"workspace_id": workspace["workspace_id"]}
        )
        assert listed.json()["backups"][0]["backup_id"] == record["backup_id"]

        changed = json.loads(original)
        changed["monsters"][0]["exp"] += 1
        (server_root / "config/gameplay/monsters.json").write_text(
            json.dumps(changed), encoding="utf-8"
        )
        restored = test_client.post(
            f"/api/gameplay/backups/{record['backup_id']}/restore",
            json={"workspace_id": workspace["workspace_id"]},
        )
        assert restored.status_code == 200, restored.text
        result = restored.json()
        assert result["workspace"]["workspace_id"] != workspace["workspace_id"]
        assert (server_root / "config/gameplay/monsters.json").read_bytes() == original
        assert result["safety_backup"]["description"] == "还原前自动备份"
        assert len(list(backup_root.iterdir())) == 2


def test_strict_field_errors_do_not_write(client: TestClient, server_root: Path) -> None:
    payload = request_data(client)
    original = (server_root / "config/gameplay/monsters.json").read_bytes()
    payload["data"]["monsters"][0]["exp"] = True
    validation = {key: value for key, value in payload.items() if key != "revision"}
    result = client.post("/api/gameplay/config/validate", json=validation)
    assert result.status_code == 200
    assert result.json()["valid"] is False
    assert result.json()["issues"][0]["path"] == "monsters.0.exp"
    assert client.put("/api/gameplay/config", json=payload).status_code == 422
    assert (server_root / "config/gameplay/monsters.json").read_bytes() == original


def test_external_edit_and_catalog_change_preserve_draft(
    client: TestClient, server_root: Path
) -> None:
    payload = request_data(client)
    path = server_root / "config/gameplay/monsters.json"
    path.write_bytes(path.read_bytes() + b"\n")
    changed = path.read_bytes()
    assert client.put("/api/gameplay/config", json=payload).status_code == 409
    assert path.read_bytes() == changed
    payload = request_data(client)
    catalog = server_root / "config/gameplay/catalog.json"
    catalog.write_bytes(catalog.read_bytes() + b"\n")
    response = client.put("/api/gameplay/config", json=payload)
    assert response.status_code == 409
    assert set(response.json()) == {"code", "error", "requestId"}


def test_workspace_switch_blocks_an_old_request(client: TestClient, server_root: Path) -> None:
    payload = request_data(client)
    before = (server_root / "config/gameplay/monsters.json").read_bytes()
    cast(FastAPI, client.app).state.gameplay_workspace.bind(server_root)
    assert client.put("/api/gameplay/config", json=payload).status_code == 409
    assert (server_root / "config/gameplay/monsters.json").read_bytes() == before


def test_external_invalid_spawn_edit_is_a_conflict(client: TestClient, server_root: Path) -> None:
    manifest = json.loads((server_root / "config/gameplay/manifest.json").read_bytes())
    map_id = manifest["spawn_maps"][0]
    payload = request_data(client, "spawns", map_id)
    path = server_root / f"config/gameplay/spawns/{map_id}.json"
    path.write_bytes(b"external unfinished edit")
    assert client.put("/api/gameplay/config", json=payload).status_code == 409
    assert path.read_bytes() == b"external unfinished edit"


def test_replace_failure_keeps_original(
    client: TestClient, server_root: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    payload = request_data(client)
    payload["data"]["monsters"][0]["exp"] += 1
    path = server_root / "config/gameplay/monsters.json"
    before = path.read_bytes()

    def fail_replace(source: str, destination: Path) -> None:
        raise OSError("injected")

    monkeypatch.setattr(os, "replace", fail_replace)
    assert client.put("/api/gameplay/config", json=payload).status_code == 500
    assert path.read_bytes() == before
    assert not list(path.parent.glob(".gameplay-*.tmp"))


def test_drops_keep_array_order_and_cratio_is_read_only(client: TestClient) -> None:
    workspace = client.get("/api/gameplay/workspace").json()
    catalog = client.get(
        "/api/gameplay/catalog", params={"workspace_id": workspace["workspace_id"]}
    ).json()
    mid = catalog["manifest"]["drop_monsters"][0]
    payload = request_data(client, "drops", mid)
    before = copy.deepcopy(payload["data"]["items"])
    payload["data"]["items"][0]["count"] += 1
    assert client.put("/api/gameplay/config", json=payload).status_code == 200
    after = request_data(client, "drops", mid)["data"]["items"]
    assert after[1:] == before[1:]
    for map_id in catalog["manifest"]["spawn_maps"]:
        payload = request_data(client, "spawns", map_id)
        region = next(
            (r for g in payload["data"]["groups"] for r in g["loc"] if "cratio" in r), None
        )
        if region is not None:
            region["cratio"] += 1
            assert client.put("/api/gameplay/config", json=payload).status_code == 422
            break
    else:
        pytest.fail("Expected a real preserved cratio")


def test_raw_duplicate_keys_and_arbitrary_objects_are_rejected(client: TestClient) -> None:
    payload = request_data(client)
    raw = json.dumps(payload)
    raw = raw[:-1] + ',"revision":"duplicate"}'
    assert (
        client.put(
            "/api/gameplay/config", content=raw, headers={"content-type": "application/json"}
        ).status_code
        == 422
    )
    payload["object_id"] = 4294967295
    payload["domain"] = "drops"
    assert client.put("/api/gameplay/config", json=payload).status_code == 404


def test_shared_native_contract_samples(server_root: Path) -> None:
    root = server_root / "config/gameplay"
    fixtures = load_json(
        (ROOT / "server/test/data/gameplay/contract/expected-results.json").read_bytes()
    )
    cases = fixtures["cases"]
    assert isinstance(cases, list)
    for case in cases:
        assert isinstance(case, dict)
        path = root / str(case.get("file", "monsters.json"))
        original = path.read_bytes()
        try:
            if "file" in case:
                path.write_text(
                    str(case["raw"]) if "raw" in case else json.dumps(case["data"]),
                    encoding="utf-8",
                )
            if case["valid"]:
                validate_bundle(root)
            else:
                with pytest.raises((ValueError, OSError)):
                    validate_bundle(root)
        finally:
            path.write_bytes(original)


@pytest.fixture
def native_tool() -> Path:
    value = os.environ.get("MIR2X_GAMEPLAY_TOOL")
    if not value:
        pytest.skip("Set MIR2X_GAMEPLAY_TOOL to the built gameplay-config executable")
    path = Path(value).resolve(strict=True)
    return path


def native(native_tool: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(native_tool), *arguments], capture_output=True, text=True, encoding="utf-8", timeout=30
    )


def test_native_catalog_and_shared_contract(native_tool: Path, server_root: Path) -> None:
    root = server_root / "config/gameplay"
    exported = native(native_tool, "catalog")
    assert exported.returncode == 0, exported.stderr
    assert json.loads(exported.stdout) == load_json((root / "catalog.json").read_bytes())
    cases = json.loads(
        (ROOT / "server/test/data/gameplay/contract/expected-results.json").read_bytes()
    )["cases"]
    for case in cases:
        path = root / case.get("file", "monsters.json")
        original = path.read_bytes()
        try:
            if "file" in case:
                path.write_text(case.get("raw", json.dumps(case.get("data"))), encoding="utf-8")
            result = native(native_tool, "validate", str(root))
            assert (result.returncode == 0) == case["valid"], (case["name"], result.stderr)
        finally:
            path.write_bytes(original)


def test_api_saves_all_domains_then_native_reads_unicode_directory(
    native_tool: Path, tmp_path: Path
) -> None:
    server = tmp_path / "配置管理 with spaces" / "server"
    root = server / "config/gameplay"
    shutil.copytree(ROOT / "server/config/gameplay", root)
    app = create_app(
        logging.getLogger("tests.native"),
        settings=load_settings(Path(__file__).parents[1]),
        workspace=WorkspaceService(server),
    )
    with TestClient(app) as client:
        manifest = json.loads((root / "manifest.json").read_bytes())
        monster_id = manifest["drop_monsters"][0]
        map_id = manifest["spawn_maps"][0]
        exp = request_data(client)
        row = next(row for row in exp["data"]["monsters"] if row["monster_id"] == monster_id)
        row["exp"] += 17
        expected_exp = row["exp"]
        assert client.put("/api/gameplay/config", json=exp).status_code == 200
        drops = request_data(client, "drops", monster_id)
        drops["data"]["items"][0]["count"] += 3
        assert client.put("/api/gameplay/config", json=drops).status_code == 200
        spawns = request_data(client, "spawns", map_id)
        spawns["data"]["groups"][0]["loc"][0]["count"] += 2
        assert client.put("/api/gameplay/config", json=spawns).status_code == 200
    inspected = native(native_tool, "inspect", str(root), str(monster_id), str(map_id))
    assert inspected.returncode == 0, inspected.stderr
    result = json.loads(inspected.stdout)
    assert result["exp"] == expected_exp
    expected_drops = [
        {key: row[key] for key in ("group", "item_id", "prob_recip", "count")}
        for row in sorted(drops["data"]["items"], key=lambda row: row["group"])
        for _ in range(row["repeat"])
    ]
    assert result["drops"] == expected_drops
    assert result["spawns"] == spawns["data"]["groups"]
