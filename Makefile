SHELL := /usr/bin/env bash

ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
DEPS_DIR ?= $(ROOT_DIR)/.deps
LINUX_BUILD_DIR ?= $(ROOT_DIR)/.build/linux-l4d2
WINDOWS_BUILD_DIR ?= $(ROOT_DIR)/.build/windows-l4d2
POWERSHELL ?= $(shell if command -v pwsh >/dev/null 2>&1; then printf '%s' pwsh; elif command -v powershell.exe >/dev/null 2>&1; then printf '%s' powershell.exe; else printf '%s' pwsh; fi)

.PHONY: help deps-linux deps-windows build-linux build-windows clean-linux clean-windows

help:
	@printf '%s\n' \
		'Available targets:' \
		'  make help                 Show this help message' \
		'  make deps-linux           Fetch Linux build dependencies into .deps/' \
		'  make deps-windows         Fetch Windows build dependencies into .deps/' \
		'  make build-linux          Build the Linux L4D2 extension package' \
		'  make build-windows        Build the Windows L4D2 extension package' \
		'  make clean-linux          Remove Linux build outputs' \
		'  make clean-windows        Remove Windows build outputs'

deps-linux:
	bash ./scripts/fetch-linux-deps.sh

deps-windows:
	$(POWERSHELL) -File ./scripts/fetch-windows-deps.ps1

build-linux:
	bash ./scripts/build-linux-l4d2.sh

build-windows:
	$(POWERSHELL) -File ./scripts/build-windows-l4d2.ps1

clean-linux:
	rm -rf "$(LINUX_BUILD_DIR)"

clean-windows:
	rm -rf "$(WINDOWS_BUILD_DIR)"
