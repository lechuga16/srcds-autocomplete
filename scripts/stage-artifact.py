from __future__ import annotations

import argparse
import shutil
from pathlib import Path


ROOT_FILES = ("README.md", "LICENSE", "plugin-package-map.json")
ROOT_DIRS = ("docs",)


def copy_tree(src: Path, dst: Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root")
    parser.add_argument("build_dir")
    parser.add_argument("output_dir", nargs="?")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    build_dir = Path(args.build_dir).resolve()
    output_dir = (
        Path(args.output_dir).resolve()
        if args.output_dir
        else root / "dist" / "sourcemod" / "artifact"
    )

    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    addons_src = build_dir / "addons"
    if not addons_src.is_dir():
        raise SystemExit(f"Missing addons directory in build output: {addons_src}")

    copy_tree(addons_src, output_dir / "addons")

    for file_name in ROOT_FILES:
        src = root / file_name
        if src.exists():
            shutil.copy2(src, output_dir / file_name)

    for dir_name in ROOT_DIRS:
        src = root / dir_name
        if src.is_dir():
            copy_tree(src, output_dir / dir_name)

    print(f"srcds-autocomplete artifacts generated in {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
