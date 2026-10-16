"""SDL3 CLI/config scaling and actual window-size checks; developer-only."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('package', type=Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='wolf sdl3 scale ') as directory:
    root = Path(directory)
    for source in args.package.iterdir():
        if source.is_file():
            shutil.copy2(source, root / source.name)
    exe = root / ('wolf3d.exe' if os.name == 'nt' else 'wolf3d')
    config = root / ('wolf3d.ini' if os.name == 'nt' else 'wolf3d.conf')
    env = dict(os.environ, SDL_VIDEO_DRIVER='dummy', SDL_AUDIO_DRIVER='dummy',
               SDL_RENDER_DRIVER='software')

    def run(options, scale=None):
        result = subprocess.run([str(exe), '--no-pause', '--backend', 'sdl3', *options],
                                cwd=root, env=env, capture_output=True, text=True, timeout=20)
        output = result.stdout + result.stderr
        if scale is None:
            assert result.returncode != 0 and '--window-scale requires' in output, output
        else:
            assert result.returncode == 0, output
            assert 'SDL3 window size: %dx%d' % (320 * scale, 240 * scale) in output, output

    smoke = '--sdl3-video-smoke'
    run([smoke], 3)
    for scale in range(1, 9):
        # Smoke must parse subsequent options too, just like the regular run.
        run([smoke, '--window-scale', str(scale)], scale)
    run(['--window-scale', '2', smoke], 2)
    for value in ('0', '9', '-1', '1.5', '2x', 'garbage', '', '999999999999999999999999'):
        run([smoke, '--window-scale', value])
    run([smoke, '--window-scale'])
    config.write_text('--window-scale 1\n')
    run([smoke], 1)
    run([smoke, '--window-scale', '4'], 4)
    config.write_text('--window-scale invalid\n')
    run([smoke])
    run([smoke, '--window-scale', '2'], 2)
    run(['--no-config', smoke], 3)
print('SDL3 default, actual sizes 1-8, invalid arguments, config and CLI overrides passed.')
