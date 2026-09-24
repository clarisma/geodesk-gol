from pathlib import Path
import osmium

DATA_DIR = Path(__file__).parent / "data"

def convert_osm_file(source, target):
    processor = osmium.FileProcessor(source)
    with osmium.SimpleWriter(target,
        header=processor.header, overwrite=True) as writer:
        for obj in processor:
            writer.add(obj)

def apply_changes(source, changes_file, target):
    changes = osmium.MergeInputReader()
    changes.add_file(str(changes_file))
    reader = osmium.io.Reader(source)
    writer = osmium.io.Writer(str(target), reader.header())
    try:
        changes.apply_to_reader(reader, writer)
    finally:
        reader.close()
        writer.close()

def perform_update_test(name, gol_tool, tmp_path):
    osm_file = DATA_DIR / f"{name}.osm"
    osc_file = DATA_DIR / f"{name}.osc"

    base_pbf = tmp_path / f"{name}.osm.pbf"
    rebuilt_pbf = tmp_path / f"{name}-rebuilt.osm.pbf"

    updated_gol = tmp_path / name
    rebuilt_gol = tmp_path / f"{name}-rebuilt"

    # Create the PBF used as input for both branches.
    convert_osm_file(osm_file, base_pbf)

    # Build the GOL that will subsequently be updated.
    res = gol_tool.run(["build", updated_gol, base_pbf, "-Y", "-w"])
    assert res.returncode == 0

    # Create the expected final PBF by independently applying the OSC.
    changed_osm = tmp_path / f"{name}-changed.osm"
    apply_changes(osm_file, osc_file, changed_osm)
    apply_changes(base_pbf, osc_file, rebuilt_pbf)
    rebuilt_osm = tmp_path / f"{name}-osmium-updated.osm"
    rebuilt_opl = tmp_path / f"{name}-osmium-updated.opl"
    convert_osm_file(rebuilt_pbf, rebuilt_osm)
    convert_osm_file(rebuilt_pbf, rebuilt_opl)

    # Build a fresh GOL representing the expected final state.
    res = gol_tool.run(["build", rebuilt_gol, rebuilt_pbf, "-Y", "-w"])
    assert res.returncode == 0

    # Exercise the update functionality being tested.
    gol_tool.run(["update", updated_gol, osc_file, "-d"])

    updated_xml = tmp_path / f"{name}-updated.xml"
    rebuilt_xml = tmp_path / f"{name}-rebuilt.xml"

    res = gol_tool.run(["query", updated_gol, "*", "-o", updated_xml])
    assert res.returncode == 0
    res = gol_tool.run(["query", rebuilt_gol, "*", "-o", rebuilt_xml])
    assert res.returncode == 0

    # TODO: We're not committing the tiles yet
    """
    assert updated_xml.read_text().splitlines() == \
        rebuilt_xml.read_text().splitlines()
    """

def test_update(gol_tool, tmp_path):
    print(f"tmp_path = {tmp_path}")
    cases = [
        "cascade",
        "deleted-relation",
        "duplicates",
        "geom-only",
        "missing-nodes",
        "relation-refcycle",
        "tags-changed",
        # "orphans",
    ]
    for case in cases:
        perform_update_test(case, gol_tool, tmp_path)
    assert False
