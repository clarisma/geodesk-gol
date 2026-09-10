from pathlib import Path
import osmium

DATA_DIR = Path(__file__).parent / "data"

def convert_to_pbf(source, target):
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
    convert_to_pbf(osm_file, base_pbf)

    # Build the GOL that will subsequently be updated.
    gol_tool.run(["build", updated_gol, base_pbf, "-Y", "-w"])

    # Create the expected final PBF by independently applying the OSC.
    apply_changes(base_pbf, osc_file, rebuilt_pbf)

    # Build a fresh GOL representing the expected final state.
    gol_tool.run(["build", rebuilt_gol, rebuilt_pbf, "-Y", "-w"])

    # Exercise the update functionality being tested.
    gol_tool.run(["update", updated_gol, osc_file, "-d"])

    # TODO: Compare updated_gol with rebuilt_gol.

def test_update(gol_tool, tmp_path):
    print(f"tmp_path = {tmp_path}")
    cases = [
        "cascade",
        "deleted-relation",
        "tags-changed",
        "duplicates",
        # "orphans",
    ]
    for case in cases:
        perform_update_test(case, gol_tool, tmp_path)
    assert False
