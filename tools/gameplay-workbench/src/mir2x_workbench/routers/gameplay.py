from typing import Annotated, cast

from fastapi import APIRouter, Query, Request
from fastapi.responses import JSONResponse
from pydantic import JsonValue

from mir2x_workbench.errors import error_json_response
from mir2x_workbench.gameplay_models import ConfigRequest, Domain, SaveRequest, load_json
from mir2x_workbench.gameplay_service import GameplayConfigService, GameplayError, WorkspaceService

router = APIRouter(prefix="/gameplay", tags=["gameplay"])


def service(request: Request) -> GameplayConfigService:
    return GameplayConfigService(cast(WorkspaceService, request.app.state.gameplay_workspace))


async def body[Payload: ConfigRequest](request: Request, model: type[Payload]) -> Payload:
    try:
        return model.model_validate(load_json(await request.body()))
    except ValueError as error:
        raise GameplayError(422, f"Invalid request: {error}") from error


async def gameplay_error(request: Request, error: Exception) -> JSONResponse:
    assert isinstance(error, GameplayError)
    return error_json_response(request, error.status, str(error))


@router.get("/workspace")
def workspace(request: Request) -> dict[str, JsonValue]:
    return service(request).workspace.workspace()


@router.get("/catalog")
def catalog(request: Request, workspace_id: str) -> dict[str, JsonValue]:
    return service(request).catalog(workspace_id)


@router.get("/config")
def config(
    request: Request,
    workspace_id: str,
    domain: Domain,
    object_id: Annotated[int | None, Query(ge=1, le=2**32 - 1)] = None,
) -> dict[str, JsonValue]:
    return service(request).get(workspace_id, domain, object_id)


@router.post("/config/validate")
async def validate(request: Request) -> dict[str, JsonValue]:
    return service(request).validate(await body(request, ConfigRequest))


@router.put("/config")
async def save(request: Request) -> dict[str, JsonValue]:
    return service(request).save(await body(request, SaveRequest))
