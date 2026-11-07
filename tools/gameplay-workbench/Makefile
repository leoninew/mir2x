fix ?= 0
profile ?= dev
export MIR2X_WORKBENCH_PROFILE := $(profile)

RUFF_CHECK_ARGS := $(if $(filter 1,$(fix)),--fix,)
RUFF_FORMAT_ARGS := $(if $(filter 1,$(fix)),.,--check .)

ifeq ($(OS),Windows_NT)
PROJECT_ROOT := $(shell cygpath -w "$(CURDIR)")
else
PROJECT_ROOT := $(CURDIR)
endif
NATIVE_BUILD_DIR ?= $(PROJECT_ROOT)/../../build

.DEFAULT_GOAL := help

.PHONY: help deps check test binary web webapi

help:
	@printf "Available commands (profile=dev by default):\n"
	@printf "  make deps                  Install locked dependencies\n"
	@printf "  make check                 Check types, lint, and format\n"
	@printf "  make test                  Run backend and frontend unit tests\n"
	@printf "  make web                   Start the frontend\n"
	@printf "  make webapi                Start the backend\n"
	@printf "  make binary                Build and install the desktop package\n"
	@printf "  make binary profile=prod   Override the packaged profile\n"

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
	uv run --locked --extra desktop python build_desktop.py --build-dir "$(NATIVE_BUILD_DIR)"

web:
	cd web && corepack yarn dev --mode $(profile)

webapi:
	uv run --locked --extra desktop python -m mir2x_workbench.server
