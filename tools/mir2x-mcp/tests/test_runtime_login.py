import hashlib
import json
import runpy
import sys
from pathlib import Path

import pytest

from mir2x_mcp.runtime_config import RuntimeFailure, discover_root, load_settings
from mir2x_mcp.runtime_login import ClientLogin, load_login


@pytest.mark.parametrize(
    "text",
    ["", "# empty\n", "username=", "username=only", "password=only", "username=a\npassword="],
)
def test_missing_credentials_preserve_manual_login(tmp_path, text):
    path = tmp_path / ".env"
    assert load_login(path) == ClientLogin()
    path.write_text(text, "utf-8")
    assert load_login(path) == ClientLogin()


@pytest.mark.parametrize(
    ("text", "username", "password"),
    [
        ("username=existing\r\npassword=plain:with:colon\r\n", "existing", "plain:with:colon"),
        (
            "export username=\"用户\" # note\npassword=' pass # literal '",
            "用户",
            " pass # literal ",
        ),
        ("other='unclosed\nusername=a # note\npassword=a#b", "a", "a#b"),
        ('username=a\npassword="${NO_EXPANSION}\\n"', "a", "${NO_EXPANSION}\\n"),
        ("username=" + "a" * 64 + "\npassword=" + "b" * 64, "a" * 64, "b" * 64),
    ],
)
def test_login_values_are_literal_bounded_and_not_in_repr(tmp_path, text, username, password):
    path = tmp_path / ".env"
    path.write_text(text, "utf-8-sig")
    login = load_login(path)
    assert login.username == username and login.password == password
    assert repr(login) == "ClientLogin()"


@pytest.mark.parametrize(
    "data",
    [
        b"username=a\nusername=b\npassword=secret",
        b"username\npassword=secret",
        b'username=a\npassword="secret',
        b"username=a\npassword='secret' garbage",
        b"username=a:b\npassword=secret",
        b"username=a\npassword=secret\x00",
        b"username=a\npassword=secret\tvalue",
        b"username=a\npassword=secret\x7f",
        b"username=" + b"a" * 65 + b"\npassword=secret",
        b"username=a\npassword=secret" + "汉".encode() * 22,
        b"username=a\npassword=secret\xff",
        b"#" * 65537,
    ],
)
def test_invalid_credentials_fail_with_value_free_errors(tmp_path, data):
    path = tmp_path / ".env"
    path.write_bytes(data)
    with pytest.raises(RuntimeFailure) as error:
        load_login(path)
    assert "secret" not in str(error.value)
    assert "secret" not in repr(error.value)
    assert load_login(path, enabled=False) == ClientLogin()


def test_unreadable_login_is_static_and_disabled_does_not_read(tmp_path):
    path = tmp_path / ".env"
    path.mkdir()
    with pytest.raises(RuntimeFailure, match="无法读取"):
        load_login(path)
    assert load_login(path, enabled=False) == ClientLogin()


def test_login_paths_follow_source_release_and_explicit_relative_config(tmp_path):
    assert load_settings(tmp_path).login_env == tmp_path / ".env"
    (tmp_path / "mir2x.defaults.toml").write_text("", "utf-8")
    assert load_settings(tmp_path).login_env == tmp_path / ".env"
    (tmp_path / "mir2x.local.toml").write_text(
        '[login]\nenabled=false\nenv_file="private/login.env"', "utf-8"
    )
    settings = load_settings(tmp_path)
    assert not settings.login_enabled
    assert settings.login_env == tmp_path / "private/login.env"


@pytest.mark.parametrize(
    "text",
    [
        '[login]\nenabled="yes"',
        '[login]\nenv_file=""',
        '[login]\npassword="secret"',
        '[client]\nargs=["--auto-login=secret"]',
        '[client]\nargs=["--auto-login", "secret"]',
    ],
)
def test_login_configuration_is_central_and_does_not_accept_secrets(tmp_path, text):
    (tmp_path / "mir2x.local.toml").write_text(text, "utf-8")
    with pytest.raises(RuntimeFailure) as error:
        load_settings(tmp_path)
    assert "secret" not in str(error.value)


def test_login_reader_never_searches_parent_or_current_directory(tmp_path, monkeypatch):
    (tmp_path / ".env").write_text("username=other\npassword=other", "utf-8")
    monkeypatch.chdir(tmp_path)
    assert load_login(tmp_path / "game/.env") == ClientLogin()
    assert isinstance(load_settings(tmp_path).login_env, Path)


def test_stage_allowlist_includes_empty_example_and_never_local_env(tmp_path):
    root = Path(__file__).resolve().parents[3]
    stage = runpy.run_path(str(root / "tools/mir2x-mcp/scripts/stage_runtime.py"))["stage"]
    binary = tmp_path / "build/mir2x.exe"
    binary.parent.mkdir()
    binary.write_bytes(b"synthetic binary")
    (binary.parent / ".env").write_text("username=synthetic\npassword=do-not-copy", "utf-8")
    output = tmp_path / "stage"
    stage(binary, output)
    manifest = json.loads((output / "runtime-manifest.json").read_text("utf-8"))
    names = {item["path"] for item in manifest["files"]}
    assert "login.env.example" in names and ".env" not in names
    assert {path.name for path in output.rglob(".env")} == set()
    assert "username=\npassword=\n" in (output / "login.env.example").read_text("utf-8")
    for item in manifest["files"]:
        data = (output / item["path"]).read_bytes()
        assert hashlib.sha256(data).hexdigest() == item["sha256"]
        assert b"do-not-copy" not in data


def test_implicit_source_uses_installed_root_and_explicit_root_stays_literal(tmp_path, monkeypatch):
    source = tmp_path / "source"
    project = source / "tools/mir2x-mcp"
    project.mkdir(parents=True)
    (project / "pyproject.toml").touch()
    monkeypatch.delenv("MIR2X_HOME", raising=False)
    monkeypatch.chdir(source)
    with pytest.raises(RuntimeFailure, match="install"):
        discover_root()
    installed = source / "install"
    installed.mkdir()
    assert discover_root() == installed
    with pytest.raises(RuntimeFailure, match="发行配置"):
        load_settings(discover_root())
    (installed / "mir2x.defaults.toml").touch()
    settings = load_settings(discover_root())
    assert settings.root == settings.game == installed
    assert settings.state == installed / ".runtime/manager"
    assert settings.bridge == installed / ".runtime/ai-bridge"
    assert settings.login_env == installed / ".env"
    assert settings.client.workdir == installed / "client"
    # Explicit roots never append install or inherit another directory's local config.
    assert discover_root(source) == source
    explicit = load_settings(discover_root(source))
    assert explicit.game == source and explicit.login_env == source / ".env"
    assert explicit.state == source / ".runtime/manager"


def test_environment_and_explicit_roots_have_same_precedence_for_frozen_and_source(
    tmp_path, monkeypatch
):
    selected = tmp_path / "selected"
    selected.mkdir()
    monkeypatch.setenv("MIR2X_HOME", str(selected))
    for frozen in (False, True):
        monkeypatch.setattr(sys, "frozen", frozen, raising=False)
        monkeypatch.setattr(sys, "executable", str(tmp_path / "portable/mir2x.exe"))
        assert discover_root() == selected
        assert discover_root(tmp_path) == tmp_path
    monkeypatch.delenv("MIR2X_HOME")
    assert discover_root() == tmp_path / "portable"


def test_explicit_configuration_paths_resolve_values_from_selected_root(tmp_path):
    selected = tmp_path / "selected"
    selected.mkdir()
    config = tmp_path / "custom.toml"
    config.write_text('[login]\nenv_file="local/login.env"\n[launcher]\nstate_dir="state"', "utf-8")
    settings = load_settings(selected, config)
    assert settings.login_env == selected / "local/login.env"
    assert settings.state == selected / "state"
    with pytest.raises(RuntimeFailure, match="运行目录不存在"):
        load_settings(tmp_path / "missing", config)
