import subprocess

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