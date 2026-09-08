import platform
import subprocess
from pathlib import Path
import pytest
from util import GolTool

def get_executable():
    return Path("d:\\geodesk\\tests\\gol.exe")

def run(args, *, input=None, env=None):
    exe = get_executable()
    result = subprocess.run(
        [str(exe)] + args,
        input=input,
        env=env,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding='utf-8',
        check=False
    )
    return result

gol = "d:\\geodesk\\tests\\monaco"
mapdata_dir = "e:\\geodesk\\mapdata\\"

def pytest_addoption(parser):
    parser.addoption("--gol", default="d:\\geodesk\\tests\\gol.exe",
        help="Path to gol executable")

@pytest.fixture(scope="session")
def gol_tool(pytestconfig):
    return GolTool(Path(pytestconfig.getoption("--gol")))