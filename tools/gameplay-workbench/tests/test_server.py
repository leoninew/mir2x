from pathlib import Path
from unittest.mock import Mock

import pytest
from uvicorn import Config
from uvicorn.supervisors.watchfilesreload import FileFilter

from mir2x_workbench import server
from mir2x_workbench.config import load_settings


@pytest.mark.parametrize("reload_enabled", [True, False])
def test_server_reloads_configuration_with_the_active_profile(
    monkeypatch: pytest.MonkeyPatch, reload_enabled: bool
) -> None:
    base = load_settings()
    settings = base.model_copy(
        update={
            "profile": "qa",
            "server": base.server.model_copy(update={"reload": reload_enabled}),
        }
    )
    run = Mock()
    monkeypatch.setattr(server, "load_settings", lambda: settings)
    monkeypatch.setattr(server, "configure_logging", Mock())
    monkeypatch.setattr("uvicorn.run", run)
    monkeypatch.setattr("sys.argv", ["workbench-server"])

    server.main()

    options = run.call_args.kwargs
    assert options["reload"] is reload_enabled
    if not reload_enabled:
        assert options["reload_includes"] is None
        return

    config = Config(*run.call_args.args, **options)
    watched = FileFilter(config)
    assert watched(Path.cwd() / "config" / "config.yaml")
    assert watched(Path.cwd() / ".env.qa")
    assert watched(Path(server.__file__).with_name("logging.yaml"))
    assert watched(Path(server.__file__))
    assert not watched(Path.cwd() / ".env.prod")
