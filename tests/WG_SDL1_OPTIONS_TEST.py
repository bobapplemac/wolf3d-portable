"""Packaged SDL1 conflict checks; Python is only a developer test dependency."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('package', type=Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='wolf conflicts ') as directory:
    root = Path(directory)
    for source in args.package.iterdir():
        if source.is_file():
            shutil.copy2(source, root / source.name)
    exe = root / 'wolf3d.exe'
    config = root / 'wolf3d.ini'

    def run(options, expected, success=False):
        result = subprocess.run([str(exe), '--no-pause', '--backend', 'sdl1',
                                 *options, '--diag', '--opl', 'silent'],
                                cwd=root, capture_output=True, text=True, timeout=20)
        output = result.stdout + result.stderr
        assert (result.returncode == 0) == success and expected in output, (options, result.returncode, output)

    conflicts = [
        (['--sdl-render', 'opengl'], ['--sdl-video', 'directx'], 'directx conflicts'),
        (['--sdl-render', 'opengl'], ['--sdl-scale', 'hardware'], '--sdl-scale conflicts'),
        (['--sdl-render', 'opengl'], ['--sdl-scale', 'software'], '--sdl-scale conflicts'),
    ]
    for gl in (['--gl-acceleration', 'auto'], ['--gl-acceleration', 'hardware'],
               ['--gl-acceleration', 'software'], ['--gl-library', 'missing-test-gl.dll']):
        for renderer in ('auto', 'surface'):
            conflicts.append((gl, ['--sdl-render', renderer], 'require --sdl-render opengl'))
        conflicts.append((gl, ['--sdl-video', 'directx'], 'directx conflicts'))
        conflicts.append((gl, ['--sdl-scale', 'software'], '--sdl-scale conflicts'))
    for left, right, message in conflicts:
        for first, second in ((left, right), (right, left)):
            run(first + second, message)
            config.write_text(' '.join(first) + '\n')
            run(second, message)
            config.unlink()

    for driver in ('directx', 'windib'):
        left, right = ['--sdl-render', 'surface'], ['--sdl-video', driver]
        for options in (left + right, right + left):
            run(options, 'Selected video backend: sdl1', success=True)

    # A nonexistent GL library gives a deterministic initialization failure:
    # these compatible choices must get past option validation to reach it.
    for options in ([], ['--sdl-video', 'windib'], ['--sdl-render', 'opengl']):
        run(options + ['--gl-library', 'missing-test-gl.dll'], 'OpenGL library')
    config.write_text('--sdl-video directx\n--sdl-render surface\n')
    run(['--sdl-video', 'windib', '--sdl-render', 'opengl',
         '--gl-library', 'missing-test-gl.dll'], 'OpenGL library')
print('SDL1 conflicts, argument order, config conflicts, and CLI overrides passed.')
