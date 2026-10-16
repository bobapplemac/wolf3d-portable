"""Developer-only runtime tests of a packaged combined host."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('package', type=Path)
    parser.add_argument('--sdl', choices=('sdl1', 'sdl3'), required=True)
    parser.add_argument('--native', choices=('gdi', 'kms-fbdev'), default='gdi')
    parser.add_argument('--system-sdl-available', action='store_true',
                        help='Linux host also provides SDL through the system library search path')
    args = parser.parse_args()
    dll = ('SDL.dll' if args.sdl == 'sdl1' else 'SDL3.dll') if os.name == 'nt' else 'libSDL3.so.0'
    with tempfile.TemporaryDirectory(prefix='wolf video ') as folder:
        root = Path(folder)
        for file in args.package.iterdir():
            if file.suffix.lower() in ('.exe', '.dll') or '.so' in file.name or file.name == 'wolf3d':
                shutil.copy2(file, root / file.name)
        exe = root / ('wolf3d.exe' if os.name == 'nt' else 'wolf3d')
        config = root / ('wolf3d.ini' if os.name == 'nt' else 'wolf3d.conf')

        def run(*options, success=True, selected=None):
            process = subprocess.run([str(exe), '--no-pause', *options], cwd=root,
                                     capture_output=True, text=True, timeout=20)
            output = process.stdout + process.stderr
            assert (process.returncode == 0) == success, (options, process.returncode, output)
            if selected:
                assert 'Selected video backend: ' + selected in output, output
            return output

        run('--no-config', '--help', selected='auto')
        run('--no-config', '--backend', args.native, '--help', selected=args.native)
        run('--no-config', '--backend', 'nonexistent', '--help', success=False)
        run('--no-config', '--backend', success=False)
        # Whole-line comments need no space after the marker. Invalid backend
        # values ensure accidentally parsing a commented line fails visibly.
        for marker in ('#', '# ', ';', '; ', '  #', '\t#'):
            config.write_text(marker + '--backend nonexistent\n')
            run('--help', selected='auto')
            config.write_text(marker + '--backend nonexistent\n--backend ' + args.native + '\n')
            run('--help', selected=args.native)
        # Config defaults are parsed once; the command line wins.
        config.write_text('--backend ' + args.native + '\n')
        run('--help', selected=args.native)
        run('--backend', args.sdl, '--help', selected=args.sdl)
        (root / dll).rename(root / (dll + '.saved'))
        run('--no-config', '--help', selected='auto')
        run('--no-config', '--backend', args.sdl, '--help', selected=args.sdl)
        # An invalid DLL must yield an application error, not a Windows dialog.
        (root / dll).write_bytes(b'not a PE library')
        run('--no-config', '--help', selected='auto')
        run('--no-config', '--backend', args.sdl, '--help', selected=args.sdl)
    print('Video selection, config precedence, missing/invalid SDL, and strict selection passed.')


if __name__ == '__main__':
    main()
