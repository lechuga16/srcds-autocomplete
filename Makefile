SHELL := /usr/bin/env bash

ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
ifeq ($(OS),Windows_NT)
PYTHON ?= python
else
PYTHON ?= $(shell command -v python3 >/dev/null 2>&1 && echo python3 || echo python)
endif
LINUX_DEPS_DIR ?= $(ROOT_DIR)/.deps/exts-linux
WINDOWS_DEPS_DIR ?= $(ROOT_DIR)/.deps/exts-windows
LINUX_BUILD_DIR ?= $(ROOT_DIR)/.build/linux-l4d2
WINDOWS_BUILD_DIR ?= $(ROOT_DIR)/.build/windows-l4d2
LINUX_PACKAGE_DIR ?= $(ROOT_DIR)/.build/package-exts-linux
WINDOWS_PACKAGE_DIR ?= $(ROOT_DIR)/.build/package-exts-windows
LINUX_ARTIFACT_DIR ?= $(ROOT_DIR)/dist/linux/artifact
WINDOWS_ARTIFACT_DIR ?= $(ROOT_DIR)/dist/windows/artifact
LINUX_RELEASE_BASENAME ?= srcds-autocomplete-local-linux
WINDOWS_RELEASE_BASENAME ?= srcds-autocomplete-local-windows

.PHONY: help deps-exts-linux deps-exts-windows build-exts-linux build-exts-windows package-exts-linux package-exts-windows release-linux release-windows clean-linux clean-windows clean-all

help:
	@printf '%s\n' \
		'Available targets:' \
		'  make help                 Show this help message' \
		'  make deps-exts-linux      Fetch Linux extension dependencies into .deps/exts-linux/' \
		'  make deps-exts-windows    Fetch Windows extension dependencies into .deps/exts-windows/' \
		'  make build-exts-linux     Build the Linux extension package' \
		'  make build-exts-windows   Build the Windows extension package' \
		'  make package-exts-linux   Stage the Linux package tree' \
		'  make package-exts-windows Stage the Windows package tree' \
		'  make release-linux        Create the Linux release zip' \
		'  make release-windows      Create the Windows release zip' \
		'  make clean-linux          Remove Linux build outputs' \
		'  make clean-windows        Remove Windows build outputs' \
		'  make clean-all            Remove all build outputs and dependencies'

deps-exts-linux:
	DEPS_DIR="$(LINUX_DEPS_DIR)" bash ./scripts/fetch-linux-deps.sh

deps-exts-windows:
	DEPS_DIR="$(WINDOWS_DEPS_DIR)" pwsh -File ./scripts/fetch-windows-deps.ps1

build-exts-linux:
	DEPS_DIR="$(LINUX_DEPS_DIR)" BUILD_DIR="$(LINUX_BUILD_DIR)" bash ./scripts/build-linux-l4d2.sh

build-exts-windows:
	DEPS_DIR="$(WINDOWS_DEPS_DIR)" BUILD_DIR="$(WINDOWS_BUILD_DIR)" pwsh -File ./scripts/build-windows-l4d2.ps1

package-exts-linux:
	$(PYTHON) ./scripts/stage-artifact.py . "$(LINUX_BUILD_DIR)/package" "$(LINUX_PACKAGE_DIR)"

package-exts-windows:
	$(PYTHON) ./scripts/stage-artifact.py . "$(WINDOWS_BUILD_DIR)/package" "$(WINDOWS_PACKAGE_DIR)"

release-linux:
	$(PYTHON) ./scripts/stage-artifact.py . "$(LINUX_PACKAGE_DIR)" "$(LINUX_ARTIFACT_DIR)"
	$(PYTHON) ./scripts/package-release.py --root . --artifact-dir "$(LINUX_ARTIFACT_DIR)" --basename "$(or $(RELEASE_BASENAME),$(LINUX_RELEASE_BASENAME))"

release-windows:
	$(PYTHON) ./scripts/stage-artifact.py . "$(WINDOWS_PACKAGE_DIR)" "$(WINDOWS_ARTIFACT_DIR)"
	$(PYTHON) ./scripts/package-release.py --root . --artifact-dir "$(WINDOWS_ARTIFACT_DIR)" --basename "$(or $(RELEASE_BASENAME),$(WINDOWS_RELEASE_BASENAME))"

clean-linux:
	rm -rf "$(LINUX_BUILD_DIR)" "$(LINUX_PACKAGE_DIR)" "$(ROOT_DIR)/dist/linux"

clean-windows:
	rm -rf "$(WINDOWS_BUILD_DIR)" "$(WINDOWS_PACKAGE_DIR)" "$(ROOT_DIR)/dist/windows"

clean-all:
	rm -rf "$(ROOT_DIR)/.build" "$(ROOT_DIR)/dist" "$(ROOT_DIR)/.deps"
