"""
Generates hemispheres.osm.pbf, the fixture used by test_pbf_export.py.

A handful of ways with anonymous (untagged) nodes on both sides of the
equator and of the prime meridian:

  way/1  Berlin      (lat > 0, lon > 0)   open way
  way/2  Vitoria     (lat < 0, lon < 0)   open way, one tagged node (node/13)
  way/3  Vitoria     (lat < 0, lon < 0)   closed way (building)
  way/4  Cape Town   (lat < 0, lon > 0)   open way

Regenerate with pyosmium installed:  python3 make_hemispheres.py
(test_pbf_export.py imports NODES/WAYS from here as the expected geometry.)
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "hemispheres.osm.pbf")

# id -> (lon, lat, tags)
NODES = {
    # Berlin
    1: (13.4010, 52.5200, {}),
    2: (13.4020, 52.5205, {}),
    3: (13.4030, 52.5210, {}),
    4: (13.4040, 52.5215, {}),
    # Vitoria (Espirito Santo, Brazil)
    5: (-40.3100, -20.3100, {}),
    6: (-40.3090, -20.3105, {}),
    7: (-40.3080, -20.3110, {}),
    8: (-40.3070, -20.3115, {}),
    9: (-40.3200, -20.3200, {}),
    10: (-40.3190, -20.3200, {}),
    11: (-40.3190, -20.3210, {}),
    12: (-40.3200, -20.3210, {}),
    13: (-40.3085, -20.3107, {"highway": "crossing"}),
    # Cape Town
    14: (18.4230, -33.9250, {}),
    15: (18.4240, -33.9255, {}),
    16: (18.4250, -33.9260, {}),
    17: (18.4260, -33.9265, {}),
}

WAYS = {
    1: ([1, 2, 3, 4], {"highway": "residential", "name": "North Street"}),
    2: ([5, 6, 13, 7, 8], {"highway": "residential", "name": "South Street"}),
    3: ([9, 10, 11, 12, 9], {"building": "yes", "name": "South Building"}),
    4: ([14, 15, 16, 17], {"highway": "residential", "name": "East Street"}),
}


def main():
    import osmium  # only needed to (re)generate the fixture

    if os.path.exists(OUT):
        os.remove(OUT)
    writer = osmium.SimpleWriter(OUT)
    for node_id, (lon, lat, tags) in sorted(NODES.items()):
        writer.add_node(osmium.osm.mutable.Node(
            id=node_id, version=1, location=(lon, lat), tags=tags))
    for way_id, (nodes, tags) in sorted(WAYS.items()):
        writer.add_way(osmium.osm.mutable.Way(
            id=way_id, version=1, nodes=nodes, tags=tags))
    writer.close()
    print(f"wrote {OUT} ({os.path.getsize(OUT)} bytes)")


if __name__ == "__main__":
    main()
