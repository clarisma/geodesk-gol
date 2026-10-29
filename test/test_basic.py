import geodesk
import osmium
import util

def test_basic(gol_tool, tmp_path):

    # Create a simple PBF

    source_opl_path = tmp_path / "source.opl"
    util.write_opl_file("""
        n1 v1 x-90 y45
        n2 v1 x90 y45
        n3 v1 x90 y-45
        n4 v1 x-90 y-45
        n5 v1 x-120 y-45
        n8 v1 x0 y10 Tplace=city,name=Paris
        w10 v1 Nn1,n2,n3,n4,n1
        r100 v1 Mw10@
        r200 v1 Mr100@
        """, tmp_path / source_opl_path)
    source_pbf_path = tmp_path / "source.osm.pbf"
    header = osmium.io.Header()
    header.set(
        "osmosis_replication_base_url",
        "https://test.geodesk.com/update/basic/day")
    header.set("osmosis_replication_sequence_number", "479123")
    util.convert_osm_file(source_opl_path, source_pbf_path, header)

    # Build the GOL

    test_gol_path = tmp_path / "test.gol"
    res = gol_tool.run(["build", test_gol_path, source_pbf_path, "-w", "-Y"])
    assert res.returncode == 0

    # Update the GOL

    res = gol_tool.run(["update", test_gol_path])
    assert res.returncode == 0

    # n2 was moved to lat 60, which in turn should have modified
    # w10, r100 and r200
    # n5 was assumed by w10, so it should no longer be an orphan
    updated = geodesk.Features(str(test_gol_path))
    assert updated.revision == 479125
    bbox = geodesk.Box(n=90, w=-180, e=180, s=60)
    assert updated.way(10) in updated(bbox)
    assert updated.relation(100) in updated(bbox)
    assert updated.relation(200) in updated(bbox)
    assert updated.node(8).name == "京都市"
    assert updated.way(10).is_area
    assert updated.way(10).nodes.count == 6
    assert updated("n[geodesk:orphan]").count == 0
    assert updated("n[place=city][name=*市]").count == 1
    assert updated.count == 4

    # Save to GOB and load it again

    test_gob_path = tmp_path / "test.gob"
    loaded_gol_path = tmp_path / "loaded.gol"

    res = gol_tool.run(["save", test_gol_path, "-w"])
    assert res.returncode == 0

    res = gol_tool.run(["load", loaded_gol_path, test_gob_path, "-w"])
    assert res.returncode == 0
    loaded = geodesk.Features(str(loaded_gol_path))
    assert loaded.count == 4

    # Export to PBF

    exported_pbf_path = tmp_path / "exported.osm.pbf"
    res = gol_tool.run(["query", loaded_gol_path, "*", "-o", exported_pbf_path])
    assert res.returncode == 0

    # Re-build GOL from exported PBF

    rebuilt_gol_path = tmp_path / "rebuilt.gol"
    res = gol_tool.run(["build", rebuilt_gol_path, exported_pbf_path, "-w", "-Y"])
    assert res.returncode == 0

    rebuilt = geodesk.Features(str(rebuilt_gol_path))
    assert rebuilt.count == 4




