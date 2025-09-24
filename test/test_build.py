import contextlib
import os
import pytest
from conftest import run, mapdata_dir

def test_build():
    """
    for file in [ "test.gol", "test.tes", "test2.gol" ]:
        with contextlib.suppress(FileNotFoundError):
            os.remove(file)
            """

    res = run(["build", "test", mapdata_dir + "liguria", "-Y"])
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