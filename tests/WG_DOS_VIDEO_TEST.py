"""Test DOS exit-screen composition and real-mode font-vector preservation."""
import argparse
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='gcc')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='wolf-dos-video-') as folder:
    temp = Path(folder)
    (temp / 'i86.h').write_text("union REGS { struct { unsigned short ax,bx,cx,dx; } w; struct { unsigned char al,ah,bl,bh; } h; struct { unsigned eax,ebx,ecx,edx,cflag; } x; }; int int386(int, union REGS *, union REGS *);\n")
    exe = temp / 'video-test.exe'
    subprocess.run([args.cc, '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-DWOLF3D_STATIC', '-I' + str(temp),
                    '-I' + str(ROOT / 'lib/wolf3d/include'),
                    str(ROOT / 'tests/WG_DOS_VIDEO_TEST.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
