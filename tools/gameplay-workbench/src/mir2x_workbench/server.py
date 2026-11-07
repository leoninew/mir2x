import argparse

import uvicorn
from fastapi import FastAPI

from mir2x_workbench.api import build_app
from mir2x_workbench.config import ServerSettings, load_settings
from mir2x_workbench.logging_setup import configure_logging


def create_server_app() -> FastAPI:
    configure_logging()
    return build_app()


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the example FastAPI service.")
    parser.add_argument("--host")
    parser.add_argument("--port", type=int)
    parser.add_argument("--reload", action="store_true", default=None)
    args = parser.parse_args()
    configure_logging(include_files=False)
    settings = load_settings()
    server = ServerSettings.model_validate(
        {
            **settings.server.model_dump(),
            **{name: value for name, value in vars(args).items() if value is not None},
        }
    )
    uvicorn.run(
        "mir2x_workbench.server:create_server_app",
        host=server.host,
        port=server.port,
        reload=server.reload,
        reload_includes=[
            "config/config.yaml",
            f".env.{settings.profile}",
            "logging.yaml",
        ]
        if server.reload
        else None,
        factory=True,
        access_log=False,
        log_config=None,
    )


if __name__ == "__main__":
    main()
