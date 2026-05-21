#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -n "${EXTS_ARTIFACT_DIR:-}" ]]; then
  ARTIFACT_DIR="$EXTS_ARTIFACT_DIR"
elif [[ -d "$ROOT_DIR/dist/windows/artifact" ]]; then
  ARTIFACT_DIR="$ROOT_DIR/dist/windows/artifact"
elif [[ -d "$ROOT_DIR/dist/linux/artifact" ]]; then
  ARTIFACT_DIR="$ROOT_DIR/dist/linux/artifact"
else
  ARTIFACT_DIR="$ROOT_DIR/dist/sourcemod/artifact"
fi
PACKAGE_MAP_PATH="$ROOT_DIR/plugin-package-map.json"
if [[ -n "${EXTS_PLATFORM:-}" ]]; then
  PLATFORM="$EXTS_PLATFORM"
elif [[ "$ARTIFACT_DIR" == *"/dist/windows/artifact" ]] || [[ "$ARTIFACT_DIR" == *"\\dist\\windows\\artifact" ]]; then
  PLATFORM="windows"
else
  PLATFORM="linux"
fi

if [[ ! -d "$ARTIFACT_DIR" ]]; then
  echo "Extension artifact directory not found at $ARTIFACT_DIR" >&2
  exit 1
fi

PYTHON_BIN="$(command -v python3 >/dev/null 2>&1 && echo python3 || echo python)"

"$PYTHON_BIN" - "$ARTIFACT_DIR" "$PACKAGE_MAP_PATH" "$PLATFORM" <<'PY'
import json
import os
import sys


def validate_manifest_tree(artifact_root: str, manifest: dict) -> None:
    if manifest.get("all", False):
        if not os.path.isdir(artifact_root):
            raise SystemExit(f"Missing artifact directory: {artifact_root}")

    for relative_file in manifest.get("files", []):
        artifact_path = os.path.join(artifact_root, relative_file)
        if not os.path.isfile(artifact_path):
            raise SystemExit(f"Missing packaged artifact file: {artifact_path}")

    for relative_dir in manifest.get("dirs", []):
        artifact_path = os.path.join(artifact_root, relative_dir)
        if not os.path.isdir(artifact_path):
            raise SystemExit(f"Missing packaged artifact directory: {artifact_path}")

    for key, value in manifest.items():
        if key in {"all", "files", "dirs"}:
            continue
        if isinstance(value, dict):
            validate_manifest_tree(os.path.join(artifact_root, key), value)


artifact_dir, package_map_path, platform = sys.argv[1], sys.argv[2], sys.argv[3]

with open(package_map_path, "r", encoding="utf-8") as fh:
    package_map = json.load(fh)

extensions_dir = os.path.join(artifact_dir, "addons", "sourcemod", "extensions")
expected_name = "autocomplete.ext.2.l4d2.dll" if platform == "windows" else "autocomplete.ext.2.l4d2.so"

for bucket, extensions in package_map.get("build", {}).get("extensions", {}).items():
    for extension in extensions:
        extension_path = (
            os.path.join(extensions_dir, expected_name)
            if bucket == "root"
            else os.path.join(extensions_dir, bucket, expected_name)
        )
        if not os.path.isfile(extension_path):
            raise SystemExit(f"Missing compiled extension: {extension_path}")

artifact_manifest = package_map.get("artifact", {})
validate_manifest_tree(artifact_dir, artifact_manifest)

for rel_path in ("README.md", "LICENSE", "plugin-package-map.json"):
    artifact_path = os.path.join(artifact_dir, rel_path)
    if not os.path.exists(artifact_path):
        raise SystemExit(f"Missing packaged project asset: {artifact_path}")

print("ARTIFACT_VALIDATION_OK")
PY
