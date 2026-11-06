"""HTTP route modules."""

from fastapi import APIRouter

from mir2x_workbench.routers.diagnostics import router as diagnostics_router
from mir2x_workbench.routers.gameplay import router as gameplay_router
from mir2x_workbench.routers.greetings import router as greetings_router

api_router = APIRouter(prefix="/api")
api_router.include_router(greetings_router)
api_router.include_router(diagnostics_router)
api_router.include_router(gameplay_router)
