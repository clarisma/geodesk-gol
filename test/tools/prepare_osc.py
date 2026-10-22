#!/usr/bin/env python3
"""Prepare update fixtures; both repositories must be sibling directories."""

import argparse
from datetime import datetime, timezone
import gzip
from pathlib import Path
import shutil


def prepare(name: str) -> None:
    """Compress numbered OSC files and write the latest replication state."""
    relative_name = Path(name)
    if (not name or relative_name.is_absolute()
            or ".." in relative_name.parts or relative_name == Path(".")):
        raise ValueError("NAME must be a relative test path, e.g. basic/day")

    repository = Path(__file__).resolve().parents[2]
    source = repository / "test" / "update" / relative_name
    target = repository.parent / "test.geodesk.com" / "update" / relative_name
    if not source.is_dir():
        raise ValueError(f"Source directory does not exist: {source}")

    files = {}
    for path in source.glob("*.osc"):
        if not path.is_file():
            continue
        if not path.stem.isascii() or not path.stem.isdecimal():
            raise ValueError(f"Expected a numeric OSC filename: {path.name}")
        sequence = int(path.stem)
        if sequence > 999_999_999:
            raise ValueError(f"Sequence exceeds nine digits: {path.name}")
        if sequence in files:
            raise ValueError(f"Duplicate sequence number: {sequence}")
        files[sequence] = path

    if not files:
        raise ValueError(f"No .osc files found in {source}")

    for sequence, path in sorted(files.items()):
        digits = f"{sequence:09d}"
        destination = target / digits[:3] / digits[3:6]
        destination.mkdir(parents=True, exist_ok=True)
        destination /= f"{digits[6:]}.osc.gz"
        with path.open("rb") as src, gzip.open(destination, "wb") as dst:
            shutil.copyfileobj(src, dst)

    timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    with (target / "state.txt").open("w", encoding="utf-8", newline="\n") as state:
        state.write(f"timestamp={timestamp}\nsequenceNumber={max(files)}\n")


def main() -> None:
    """Parse the test name and report preparation errors."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name", metavar="NAME", help="test path, e.g. basic/day")
    args = parser.parse_args()
    try:
        prepare(args.name)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")


if __name__ == "__main__":
    main()
