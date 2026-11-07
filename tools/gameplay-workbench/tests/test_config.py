from pathlib import Path
from shutil import copyfile

from pytest import MonkeyPatch

from mir2x_workbench.config import load_settings

CONFIG_FILE = Path(__file__).parents[1] / "config" / "config.yaml"


def test_settings_loads_dotenv_with_environment_override(
    monkeypatch: MonkeyPatch,
    tmp_path: Path,
) -> None:
    env_file = tmp_path / ".env.prod"
    config_dir = tmp_path / "config"
    config_dir.mkdir()
    copyfile(CONFIG_FILE, config_dir / "config.yaml")
    env_file.write_text(
        "MIR2X_WORKBENCH_GREETING__PREFIX=Bonjour\nMIR2X_WORKBENCH_STATIC__DIRECTORY=./assets\n",
        encoding="utf-8",
    )
    monkeypatch.setenv("MIR2X_WORKBENCH_GREETING__PREFIX", "Hi")
    monkeypatch.setenv("MIR2X_WORKBENCH_PROFILE", "prod")

    settings = load_settings(tmp_path)

    assert settings.greeting.prefix == "Hi"
    assert settings.static.directory == Path("assets")


def test_settings_loads_desktop_environment_overrides(monkeypatch: MonkeyPatch) -> None:
    monkeypatch.setenv("MIR2X_WORKBENCH_DESKTOP__WIDTH", "1400")

    assert load_settings().desktop.width == 1400


def test_load_settings_caches_one_settings_instance(monkeypatch: MonkeyPatch) -> None:
    monkeypatch.setenv("MIR2X_WORKBENCH_GREETING__PREFIX", "Hi")
    initial_settings = load_settings()
    monkeypatch.setenv("MIR2X_WORKBENCH_GREETING__PREFIX", "Bonjour")

    assert load_settings() is initial_settings
    assert initial_settings.greeting.prefix == "Hi"


def test_dev_dotenv_can_add_a_nested_gameplay_branch(
    monkeypatch: MonkeyPatch, tmp_path: Path
) -> None:
    config_dir = tmp_path / "config"
    config_dir.mkdir()
    copyfile(CONFIG_FILE, config_dir / "config.yaml")
    (tmp_path / ".env.dev").write_text(
        "MIR2X_WORKBENCH_GAMEPLAY__ROOT=../../server\n", encoding="utf-8"
    )
    monkeypatch.delenv("MIR2X_WORKBENCH_PROFILE", raising=False)
    monkeypatch.delenv("MIR2X_WORKBENCH_GAMEPLAY__ROOT", raising=False)
    assert load_settings(tmp_path).gameplay.root == (tmp_path / "../../server").resolve()


def test_environment_overrides_new_nested_gameplay_branch(
    monkeypatch: MonkeyPatch, tmp_path: Path
) -> None:
    config_dir = tmp_path / "config"
    config_dir.mkdir()
    copyfile(CONFIG_FILE, config_dir / "config.yaml")
    (tmp_path / ".env.dev").write_text(
        "MIR2X_WORKBENCH_GAMEPLAY__ROOT=../../server\n", encoding="utf-8"
    )
    monkeypatch.setenv("MIR2X_WORKBENCH_PROFILE", "dev")
    monkeypatch.setenv("MIR2X_WORKBENCH_GAMEPLAY__ROOT", "./custom-server")
    assert load_settings(tmp_path).gameplay.root == (tmp_path / "custom-server").resolve()


def test_bundled_defaults_load_before_selected_profile_overrides(
    monkeypatch: MonkeyPatch, tmp_path: Path
) -> None:
    bundled_config = tmp_path / "bundled-config.yaml"
    copyfile(CONFIG_FILE, bundled_config)
    (tmp_path / ".env.prod").write_text(
        "MIR2X_WORKBENCH_GREETING__PREFIX=Production\n", encoding="utf-8"
    )
    monkeypatch.setenv("MIR2X_WORKBENCH_PROFILE", "prod")
    monkeypatch.delenv("MIR2X_WORKBENCH_GREETING__PREFIX", raising=False)
    settings = load_settings(tmp_path, default_config=bundled_config)
    assert settings.profile == "prod"
    assert settings.greeting.prefix == "Production"
    assert settings.server.port == 8000
    load_settings.cache_clear()
    monkeypatch.setenv("MIR2X_WORKBENCH_PROFILE", "dev")
    settings = load_settings(tmp_path, default_config=bundled_config)
    assert settings.profile == "dev"
    assert settings.greeting.prefix == "Hello"
