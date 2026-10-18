"""Helpers for GeoDesk's inline-OPL update tests."""

from difflib import unified_diff
from pathlib import Path
from textwrap import dedent

import geodesk
import osmium

def _run(gol_tool, *args):
    result = gol_tool.run([str(arg) for arg in args])
    assert result.returncode == 0, (
        f"gol {' '.join(map(str, args))} failed "
        f"(exit code {result.returncode})\n"
        f"stdout:\n{result.stdout}\n"
        f"stderr:\n{result.stderr}"
    )
    return result

def _write_opl(text: str, path: Path):
    path.write_text(dedent(text).strip() + "\n", encoding="utf-8")

def _convert(source: Path, target: Path):
    with osmium.SimpleWriter(str(target)) as writer:
        for obj in osmium.FileProcessor(str(source)):
            writer.add(obj)

def _apply_changes(base_pbf: Path, changes_osc: Path, target_pbf: Path):
    changes = osmium.MergeInputReader()
    changes.add_file(str(changes_osc))
    reader = osmium.io.Reader(str(base_pbf))
    try:
        writer = osmium.io.Writer(str(target_pbf), reader.header())
        try:
            changes.apply_to_reader(reader, writer)
        finally:
            writer.close()
    finally:
        reader.close()


def run_update_case(gol_tool, tmp_path, *, before: str, changes: str):
    """Compare an updated GOL against an independent rebuild.

    `before` contains OPL for the initial snapshot; `changes` contains
    OPL for changed objects only. Changes should have newer versions,
    and deletions use `dD`. Input must follow OSM object ordering.
    """
    before_opl = tmp_path / "before.opl"
    changes_opl = tmp_path / "changes.opl"
    base_pbf = tmp_path / "base.osm.pbf"
    changes_osc = tmp_path / "changes.osc"
    expected_pbf = tmp_path / "expected.osm.pbf"
    updated_gol = tmp_path / "updated"
    rebuilt_gol = tmp_path / "rebuilt"
    updated_xml = tmp_path / "updated.xml"
    rebuilt_xml = tmp_path / "rebuilt.xml"

    _write_opl(before, before_opl)
    _write_opl(changes, changes_opl)
    _convert(before_opl, base_pbf)
    _convert(changes_opl, changes_osc)
    _apply_changes(base_pbf, changes_osc, expected_pbf)

    _run(gol_tool, "build", updated_gol, base_pbf, "-Y", "-w")
    _run(gol_tool, "build", rebuilt_gol, expected_pbf, "-Y", "-w")

    update_result = _run(gol_tool, "update", updated_gol, changes_osc, "-d")
    _run(gol_tool, "check", updated_gol, "-d")

    _run(gol_tool, "query", updated_gol, "*", "-o", updated_xml)
    _run(gol_tool, "query", rebuilt_gol, "*", "-o", rebuilt_xml)

    actual = updated_xml.read_text(encoding="utf-8").splitlines()
    expected = rebuilt_xml.read_text(encoding="utf-8").splitlines()
    if actual != expected:
        diff = "\n".join(unified_diff(
            expected, actual, fromfile="rebuilt", tofile="updated",
            lineterm=""))
        raise AssertionError(f"Updated GOL differs from rebuild:\n{diff}")

    update_result.after = geodesk.Features(str(updated_gol))
    return update_result

def is_orphan(node):
    return dict(node.tags) == { "geodesk:orphan": "yes" }

def is_duplicate(node):
    return dict(node.tags) == { "geodesk:duplicate": "yes" }