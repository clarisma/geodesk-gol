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
        r200 v1 MMr200@
        """, tmp_path / source_opl_path)
    source_pbf_path = tmp_path / "source.opl"
    header = osmium.io.Header()
    header.set(
        "osmosis_replication_base_url",
        "https://https://data.geodesk.com/test/update/basic/day")
    header.set("osmosis_replication_sequence_number", "479123")
    util.convert_osm_file(source_opl_path, source_pbf_path)

    # Build the GOL

    res = gol_tool.run(["build", "test", source_pbf_path, "-w", "-Y"])
    assert res.returncode == 0

    # Update the GOL

    res = gol_tool.run(["update", "test"])
    assert res.returncode == 0

    res = run(["save", "test"])
    assert res.returncode == 0

    res = run(["load", "test2", "test", "-Y"])
    assert res.returncode == 0

def test_build_bad_param():
    res = run(["build", "test", mapdata_dir + "liguria", "-l", "bad_bad_bad"])
    assert res.returncode == 2

def test_version():
    res = run(["-V"])
    assert res.returncode == 0
    assert "gol" in res.stdout