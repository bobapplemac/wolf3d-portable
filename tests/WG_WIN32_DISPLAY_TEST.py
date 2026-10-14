"""Compile pure Win32 display-state regressions with mocked display APIs."""
from pathlib import Path
import argparse
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--cc', default='gcc')
args = p.parse_args()
with tempfile.TemporaryDirectory() as folder:
    exe = Path(folder) / 'display-test.exe'
    subprocess.run([args.cc, '-std=c89', '-Wall', '-Wextra', '-Werror',
                    str(root / 'tests/WG_WIN32_DISPLAY_TEST.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
