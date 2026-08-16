"""
Generates halfworld.osm.pbf, the fixture used by test_root_tile.py.

Two 3-node ways with anonymous nodes at lat 10, one near lon -100 and one
near lon +100, i.e. 200 degrees apart. Small enough to end up in a single
root tile, where the coordinate delta between node/3 and node/4 exceeds
2^31 units.

Regenerate with pyosmium installed:  python3 make_halfworld.py
(test_root_tile.py imports NODES/WAYS from here as the expected geometry.)
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "halfworld.osm.pbf")

# id -> (lon, lat)
NODES = {
    1: (-100.000, 10.0),
    2: (-99.999, 10.0),
    3: (-99.998, 10.0),
    4: (100.000, 10.0),
    5: (100.001, 10.0),
    6: (100.002, 10.0),
}

WAYS = {
    1: ([1, 2, 3], {"highway": "residential", "name": "West"}),
    2: ([4, 5, 6], {"highway": "residential", "name": "East"}),
}


def main():
    import osmium  # only needed to (re)generate the fixture

    if os.path.exists(OUT):
        os.remove(OUT)
    writer = osmium.SimpleWriter(OUT)
    for node_id, (lon, lat) in sorted(NODES.items()):
        writer.add_node(osmium.osm.mutable.Node(
            id=node_id, version=1, location=(lon, lat), tags={}))
    for way_id, (nodes, tags) in sorted(WAYS.items()):
        writer.add_way(osmium.osm.mutable.Way(
            id=way_id, version=1, nodes=nodes, tags=tags))
    writer.close()
    print(f"wrote {OUT} ({os.path.getsize(OUT)} bytes)")


if __name__ == "__main__":
    main()
