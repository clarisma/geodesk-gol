from conftest import run, mapdata_dir
import os

def test_save():
    files = [
        "bavaria",
        "berlin",
        "de-2024-11-28",
        "ireland",
        "it-2025-02-14",
        "fr-2024-04-11",
        "liguria",
        "philippines",
    ]

    print("file,pbf_size,gol_size,gob_size,gol_ratio")

    for file in files:
        run(["build", file, mapdata_dir + file, "-l", "0,3,6,9,12", "-n", "10000000", "-Y"])
        run(["save", file, "-Y"])
        size_pbf = os.path.getsize(mapdata_dir + file + ".osm.pbf")
        size_gol = os.path.getsize(file + ".gol")
        size_gob = os.path.getsize(file + ".gob")
        print(f"{file},{size_pbf},{size_gol},{size_gob},{size_gol/size_pbf:.3f},{size_gob/size_pbf:.3f}")

if __name__ == "__main__":
    test_save()