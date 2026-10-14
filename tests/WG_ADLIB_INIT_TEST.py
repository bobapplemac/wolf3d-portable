"""Test the native AdLib initializer with mocked ports in DOS and Win9x modes."""
import argparse
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='gcc')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='wolf-adlib-') as folder:
    temp = Path(folder)
    (temp / 'conio.h').write_text('int inp(unsigned); int outp(unsigned, int);\n')
    (temp / 'i86.h').write_text('void _disable(void); void _enable(void);\n')
    (temp / 'windows.h').write_text('unsigned long GetVersion(void);\n')
    for defines in [[], ['-DWG_WIN9X']]:
        exe = temp / 'adlib-test.exe'
        subprocess.run([args.cc, '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-I' + str(temp), *defines,
                        str(ROOT / 'tests/WG_ADLIB_INIT_TEST.c'), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
