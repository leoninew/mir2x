.DEFAULT_GOAL := help

fix ?= 0
dev ?= 0
profile ?= dev

RUFF_CHECK_ARGS := $(if $(filter 1,$(fix)),--fix,)
RUFF_FORMAT_ARGS := $(if $(filter 1,$(fix)),.,--check .)

ifeq ($(OS),Windows_NT)
PYINSTALLER_DATA_SEPARATOR := ;
PROJECT_ROOT := $(shell cygpath -w "$(CURDIR)")
else
PYINSTALLER_DATA_SEPARATOR := :
PROJECT_ROOT := $(CURDIR)
endif

.PHONY: help deps check test binary desktop web webapi

help:
	@printf "Available targets:\n"
	@printf "  make deps             Install locked backend and frontend dependencies\n"
	@printf "  make check            Check backend and frontend types, lint, and format\n"
	@printf "  make check fix=1      Apply backend and frontend lint and format fixes\n"
	@printf "  make desktop          Open the native window with built web assets\n"
	@printf "  make desktop dev=1    Start FastAPI and Vite in desktop development mode\n"
	@printf "  make binary           Build the current platform PyInstaller package\n"
	@printf "  make web              Start the Vite development server on port 8081\n"
	@printf "  make webapi           Start the FastAPI development server on port 8000\n"
	@printf "  profile=dev           Select .env.dev (default); use profile=qa for .env.qa\n"

deps:
	uv sync --extra desktop --all-groups --locked
	cd web && corepack yarn install --immutable

check:
	uv run --locked --extra desktop mypy src tests
	uv run --locked --extra desktop ruff check . $(RUFF_CHECK_ARGS)
	uv run --locked --extra desktop ruff format $(RUFF_FORMAT_ARGS)
	$(MAKE) -C web check fix=$(fix)

test:
	uv run --locked --extra desktop pytest
	$(MAKE) -C web test

binary:
	uv sync --extra desktop --all-groups --locked
	cd web && corepack yarn install --immutable && corepack yarn build
	uv run --locked --no-sync pyinstaller --noconfirm --clean --windowed --onedir --name Mir2xWorkbench --paths src --collect-all webview --hidden-import webview.platforms.winforms --add-data "$(PROJECT_ROOT)/web/dist$(PYINSTALLER_DATA_SEPARATOR)static" --add-data "$(PROJECT_ROOT)/src/mir2x_workbench/logging.yaml$(PYINSTALLER_DATA_SEPARATOR)mir2x_workbench" --add-data "$(PROJECT_ROOT)/config/config.yaml$(PYINSTALLER_DATA_SEPARATOR)mir2x_workbench" --workpath build/pyinstaller --specpath build/pyinstaller src/mir2x_workbench/desktop.py

desktop: export MIR2X_WORKBENCH_PROFILE = $(profile)
desktop:
	uv run --locked --extra desktop mir2x-workbench $(if $(filter 1,$(dev)),--dev,)

web:
	cd web && corepack yarn dev --mode $(profile)

webapi: export MIR2X_WORKBENCH_PROFILE = $(profile)
webapi:
	uv run --locked --extra desktop python -m mir2x_workbench.server
