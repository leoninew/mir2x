import logging
from pathlib import Path

from fastapi import FastAPI

from mir2x_workbench.config import Settings, load_settings
from mir2x_workbench.exception_handlers import (
    register_exception_handlers,
    unexpected_exception_handler,
)
from mir2x_workbench.gameplay_backups import GameplayBackupService
from mir2x_workbench.gameplay_service import GameplayError, WorkspaceService
from mir2x_workbench.middleware import RequestLoggingMiddleware
from mir2x_workbench.routers import api_router
from mir2x_workbench.routers.gameplay import gameplay_error
from mir2x_workbench.static_files import mount_static_files


def create_app(
    logger: logging.Logger,
    *,
    settings: Settings | None = None,
    static_directory: Path | None = None,
    workspace: WorkspaceService | None = None,
    backup_root: Path | None = None,
) -> FastAPI:
    app = FastAPI(title="Mir2x Gameplay Workbench")
    configure_application_state(app, logger, settings)
    app.state.gameplay_workspace = (
        workspace if workspace is not None else WorkspaceService(app.state.settings.gameplay.root)
    )
    app.state.gameplay_backups = GameplayBackupService(app.state.gameplay_workspace, backup_root)
    app.add_exception_handler(GameplayError, gameplay_error)
    app.add_middleware(RequestLoggingMiddleware, exception_handler=unexpected_exception_handler)
    register_exception_handlers(app)
    app.include_router(api_router)
    mount_static_files(app, directory=static_directory)
    return app


def configure_application_state(
    app: FastAPI,
    logger: logging.Logger,
    settings: Settings | None,
) -> None:
    app.state.logger = logger
    app.state.settings = settings if settings is not None else load_settings()


def build_app(
    *,
    settings: Settings | None = None,
    static_directory: Path | None = None,
    workspace: WorkspaceService | None = None,
    backup_root: Path | None = None,
) -> FastAPI:
    if backup_root is None:
        backup_root = Path.cwd() / "backups" / "gameplay"
    return create_app(
        logging.getLogger("mir2x_workbench.http"),
        settings=settings,
        static_directory=static_directory,
        workspace=workspace,
        backup_root=backup_root,
    )
