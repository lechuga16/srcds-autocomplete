from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--basename", required=True)
    parser.add_argument("--artifact-dir")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    artifact_dir = (
        Path(args.artifact_dir).resolve()
        if args.artifact_dir
        else root / "dist" / "sourcemod" / "artifact"
    )
    release_dir = root / "dist" / "release"
    release_dir.mkdir(parents=True, exist_ok=True)

    archive_base = release_dir / args.basename
    for suffix in (".zip", ".tar", ".tar.gz"):
        candidate = release_dir / f"{args.basename}{suffix}"
        if candidate.exists():
            candidate.unlink()

    shutil.make_archive(str(archive_base), "zip", root_dir=artifact_dir)
    print(f"Release archive generated in: {archive_base}.zip")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
