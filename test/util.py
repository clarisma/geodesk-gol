import subprocess
from pathlib import Path
from textwrap import dedent
import osmium

class GolTool:
    def __init__(self, exe):
        self.exe = exe

    def run(self, args, *, input=None, env=None):
        return subprocess.run(
            [str(self.exe), *args],
            input=input,
            env=env,
            stdin=subprocess.DEVNULL if input is None else None,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            check=False,
        )

def write_opl_file(text: str, path: Path):
    path.write_text(dedent(text).strip() + "\n", encoding="utf-8")

def convert_osm_file(src_path: Path, target_path: Path, header=None):
    with osmium.SimpleWriter(str(target_path), header=header) as writer:
        for obj in osmium.FileProcessor(str(src_path)):
            writer.add(obj)
