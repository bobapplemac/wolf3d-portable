"""Windows maintainer test: real inherited console and B800 renderer."""
import argparse
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('executable', type=Path)
p.add_argument('--cc', default='gcc')
args = p.parse_args()
with tempfile.TemporaryDirectory() as folder:
    exe = Path(folder) / 'console-test.exe'
    subprocess.run([args.cc, '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(root / 'lib/wolf3d/include'),
                    str(root / 'tests/WG_WIN2K_CONSOLE_TEST.c'),
                    str(root / 'platforms/WG_TEXT_OUTPUT.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(args.executable.resolve())], check=True, timeout=45)
