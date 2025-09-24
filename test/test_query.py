import pytest
import json
from shapely import Geometry, wkt
from shapely.geometry import GeometryCollection
from conftest import run, gol

def run_query(query, format):
    res = run(["query", gol, query, "-f", format])
    assert res.returncode == 0
    return res

def check_geojson(output, expected_count):
    geojson = json.loads(output)
    assert geojson["type"] == "FeatureCollection"
    assert isinstance(geojson["features"], list)
    assert len(geojson["features"]) == expected_count

def check_wkt(output, expected_count):
    geom = wkt.loads(output)
    if expected_count == 1:
        assert isinstance(geom, Geometry)
    else:
        assert isinstance(geom, GeometryCollection)
        assert len(geom.geoms) == expected_count

@pytest.mark.parametrize("query", [
    "*",
    "w[highway][name=A*]",
    "a[boundary=administrative][name='La Condamine']"
])
def test_query(query):
    res = run_query(query, "count")
    count = int(res.stdout)
    print(f"Feature count = {count}")
    assert count > 0

    res = run_query(query, "geojsonl")
    assert len(res.stdout.strip().splitlines()) == count

    res = run_query(query, "geojson")
    check_geojson(res.stdout, count)

    res = run_query(query, "wkt")
    check_wkt(res.stdout, count)

    res = run_query(query, "list")
    assert len(res.stdout.strip().splitlines()) == count

@pytest.mark.parametrize("query", [
    "a[leisure=park][name='This park does not exist!']"
])
def test_empty_query(query):
    res = run_query(query, "count")
    count = int(res.stdout)
    assert count == 0

    res = run_query(query, "geojsonl")
    assert len(res.stdout.strip().splitlines()) == count

    res = run_query(query, "geojson")
    check_geojson(res.stdout, count)

    res = run_query(query, "wkt")
    check_wkt(res.stdout, count)

    res = run_query(query, "list")
    assert len(res.stdout.strip().splitlines()) == count

def test_invalid_query_format():
    result = run(["query", gol, "n", "-f", "bananas"])
    assert result.returncode == 2
    assert "invalid" in result.stderr.lower()

