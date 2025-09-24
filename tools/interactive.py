try:
    from geodesk import *
except ImportError:
    r = input('GeoDesk for Python is not installed. Install it now> [Y/n]').strip()
    if r not in ('','Y','y'):
        quit()
    import subprocess, sys
    try:
        res = subprocess.check_call([sys.executable,'-m','pip','install','geodesk2'])
    except subprocess.CalledProcessError:
        quit()
    from geodesk2 import *
features = de = Features('c:\\geodesk\\tests\\de')