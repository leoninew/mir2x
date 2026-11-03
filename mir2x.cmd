@echo off
setlocal
set PYTHONUTF8=1
if exist "%~dp0mir2x.exe" (
    "%~dp0mir2x.exe" %*
) else (
    set "UV_CACHE_DIR=%~dp0.runtime\uv-cache"
    set "UV_PYTHON_INSTALL_DIR=%~dp0.runtime\uv-python"
    if not defined MIR2X_HOME set "MIR2X_HOME=%~dp0install"
    uv run --project "%~dp0tools\mir2x-mcp" --locked mir2x %*
)
exit /b %errorlevel%
