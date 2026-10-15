"""Packaged RH9 launcher checks using the old loader; no game data/hardware."""
from pathlib import Path
import os
import subprocess
import sys
package=Path(sys.argv[1]).resolve()
env=dict(os.environ,SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy')
loader=['/opt/rh9/lib/ld-linux.so.2','--library-path','/opt/rh9/lib:/opt/rh9/usr/lib:/opt/rh9/usr/X11R6/lib',str(package/'wolf3d')]
def run(*args,expected=0):
    p=subprocess.run(loader+['--no-config',*args],env=env,capture_output=True,text=True,timeout=15)
    assert p.returncode==expected,p.stdout+p.stderr
    return p.stdout+p.stderr
assert 'SDL 1.2' in run('--help')
assert '--sdl-render' in run('--help')
assert 'requires auto, opengl or surface' in run('--sdl-render','invalid',expected=1)
assert 'OpenGL initialization failed' in run('--sdl-render','opengl','--diag',expected=1)
assert 'Presentation: SDL software surfaces' in run('--sdl-render','auto','--diag','--opl','silent')
assert 'Presentation: SDL software surfaces' in run('--sdl-render','surface','--diag','--opl','silent')
assert 'selected' in run('--diag','--opl','auto')
assert 'requires 1, 2, 3 or 4' in run('--window-scale','0',expected=1)
failure=run('--diag','--opl','adlib',expected=1)
assert 'CAP_SYS_RAWIO' in failure or 'Available OPL drivers' in failure
assert 'device 11025 Hz' in run('--diag','--opl','silent','--sample-rate','11025')
print('Packaged RH9 CLI, auto audio, explicit rejection and sample-rate checks passed.')
