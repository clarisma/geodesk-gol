#!/usr/bin/env python3
"""Prepare update fixtures in the sibling test.geodesk.com repository.

Usage: python prepare_osc.py basic/day
Paths are relative to this script, regardless of the working directory.
"""

import argparse
from datetime import datetime, timezone
import gzip
from pathlib import Path
import re
import shutil


def prepare(name: Path) -> None:
    """Compress a fixture's OSC files and write its replication state."""
    if name.is_absolute() or not name.parts or ".." in name.parts:
        raise ValueError("NAME must be a relative path without '..'")

    repo = Path(__file__).resolve().parents[2]
    source = repo / "test" / "update" / "data" / name
    target = repo.parent / "test.geodesk.com" / "update" / name
    if not source.is_dir():
        raise ValueError(f"Source directory does not exist: {source}")

    files = []
    sequences = set()
    for path in source.glob("*.osc"):
        if not path.is_file():
            continue
        if not re.fullmatch(r"[0-9]+", path.stem):
            raise ValueError(f"OSC filename must be numeric: {path.name}")
        sequence = int(path.stem)
        if sequence > 999_999_999:
            raise ValueError(f"Sequence exceeds nine digits: {path.name}")
        if sequence in sequences:
            raise ValueError(f"Duplicate sequence number: {sequence}")
        sequences.add(sequence)
        files.append((sequence, path))

    if not files:
        raise ValueError(f"No .osc files found in {source}")

    for sequence, path in sorted(files):
        digits = f"{sequence:09d}"
        destination = (
            target / digits[:3] / digits[3:6] / f"{digits[6:]}.osc.gz"
        )
        destination.parent.mkdir(parents=True, exist_ok=True)
        with path.open("rb") as src, gzip.open(destination, "wb") as dst:
            shutil.copyfileobj(src, dst)

    timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    timestamp = timestamp.replace(":", r"\:")
    state = f"timestamp={timestamp}\nsequenceNumber={max(sequences)}\n"
    with (target / "state.txt").open("w", encoding="utf-8", newline="\n") as dst:
        dst.write(state)


def main() -> None:
    """Parse the fixture name and report preparation errors."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name", type=Path, metavar="NAME")
    args = parser.parse_args()
    try:
        prepare(args.name)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"error: {exc}\n")


if __name__ == "__main__":
    main()
