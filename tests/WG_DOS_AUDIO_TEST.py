"""Compile and run the DOS audio driver against simulated ISA/DSP hardware.
Usage: python tests/WG_DOS_AUDIO_TEST.py [--cc gcc]
No DOS hardware or game data is required; this is a maintainer test only.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='gcc')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='wolf-dos-audio-') as folder:
    temp = Path(folder)
    (temp / 'conio.h').write_text('int inp(unsigned); int outp(unsigned, int);\n')
    (temp / 'dos.h').write_text('#define __interrupt\n#define __far\nvoid (*_dos_getvect(unsigned))(void);\nvoid _dos_setvect(unsigned, void (*)(void));\nvoid _disable(void);\nvoid _enable(void);\n')
    (temp / 'i86.h').write_text('union REGS {\n    struct { unsigned short ax, bx, cx, dx; } w;\n    struct { unsigned eax, ebx, ecx, edx, cflag; } x;\n};\nint int386(int, union REGS *, union REGS *);\n')
    exe = temp / 'audio-test.exe'
    subprocess.run([args.cc, '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-Wno-int-to-pointer-cast', '-I' + str(temp),
                    '-I' + str(ROOT / 'lib/wolf3d/include'),
                    str(ROOT / 'tests/WG_DOS_AUDIO_TEST.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
