"""Windows black-box tests for the optional Win2k backend launcher (not a build dependency)."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--package', type=Path, required=True)
parser.add_argument('--fixtures', type=Path, required=True)
parser.add_argument('--gdi-package', type=Path)
parser.add_argument('--sdl-package', type=Path)
args = parser.parse_args()

with tempfile.TemporaryDirectory(prefix='wolf backend ') as directory:
    root = Path(directory)
    assert not (args.package/'wolf3d-sdl1.dll').exists()
    for file in args.package.iterdir():
        if file.is_file(): shutil.copy2(file, root / file.name)
    shutil.copy2(args.fixtures / 'wg-backend-test.exe', root / 'test.exe')
    def run(exe='wolf3d.exe', options=(), expected=0, contains='', **env):
        result = subprocess.run([str(root / exe), '--no-pause', *options], cwd=root,
            env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', **env),
            capture_output=True, text=True, timeout=20, creationflags=subprocess.CREATE_NO_WINDOW)
        output = result.stdout + result.stderr
        assert result.returncode == expected, (options, result.returncode, output)
        assert contains in output, (options, output)
        return output
    run(options=['--help'], contains='--sdl-render auto|opengl|surface')
    run(options=['--backend','sdl1','--sdl-render','invalid'], expected=1, contains='requires auto, opengl or surface')
    run(options=['--backend','sdl1','--sdl-render','surface','--diag','--opl','silent'], contains='Backend: sdl1')
    run(options=['--diag','--opl','silent'], contains='SDL1 availability: SDL.dll loaded directly; required exports verified')
    run(options=['--backend', 'gdi', '--help'], contains='Backend: gdi')
    run(options=['--backend', 'unknown'], expected=1, contains='--backend requires')
    run(options=['--backend'], expected=1, contains='--backend requires')
    (root / 'wolf3d.ini').write_text('--backend sdl1\n--no-pause\n'.replace('\\n','\n'))
    run(options=['--backend', 'gdi', '--help'], contains='Backend: gdi')
    (root / 'SDL.dll').rename(root / 'SDL.saved')
    run(options=['--no-config', '--help'], contains='Backend: gdi')
    run(options=['--help'], expected=1, contains='SDL.dll could not be loaded')
    run(options=['--backend', 'gdi', '--help'], contains='Backend: gdi')
    (root / 'wolf3d.ini').unlink()
    (root / 'SDL.dll').write_bytes(b'not a Windows DLL')
    run(options=['--help'], contains='Backend: gdi')
    run(options=['--backend','sdl1','--help'], expected=1, contains='SDL.dll could not be loaded')
    (root / 'SDL.dll').unlink()
    (root / 'SDL.saved').rename(root / 'SDL.dll')
    (root / 'SDL.dll').rename(root / 'SDL.saved')
    shutil.copy2(args.fixtures / 'wg-sdl-incomplete.dll', root / 'SDL.dll')
    run(options=['--help'], contains='Backend: gdi')
    run(options=['--backend', 'sdl1', '--help'], expected=1, contains='missing required export')
    (root / 'SDL.dll').unlink()
    (root / 'SDL.saved').rename(root / 'SDL.dll')
    run('test.exe', contains='SDL test backend executed', WG_TEST_BACKEND_RESULT='0')
    run('test.exe', contains='GDI test backend executed', WG_TEST_BACKEND_RESULT='-2')
    output=run('test.exe', ['--backend','sdl1'], expected=1, contains='initialization failed', WG_TEST_BACKEND_RESULT='-2')
    assert 'GDI test backend executed' not in output
    output=run('test.exe', expected=1, WG_TEST_BACKEND_RESULT='1')
    assert 'GDI test backend executed' not in output # Never hide game/runtime errors.
    # Exercise the actual SDL host failure path, with a stub GDI host to avoid UI.
    for choice, expected in [('auto',0),('sdl1',1)]:
        environment=dict(os.environ, SDL_VIDEODRIVER='nonexistent-driver', SDL_AUDIODRIVER='dummy')
        result=subprocess.run([str(root/'test.exe'),'--no-pause','--no-config','--backend',choice],
            cwd=root,env=environment,capture_output=True,text=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
        assert result.returncode==expected, result.stdout+result.stderr
        assert ('GDI test backend executed' in result.stdout)==(choice=='auto'),result.stdout
    for folder,compiled,excluded in [(args.gdi_package,'gdi','sdl1'),(args.sdl_package,'sdl1','gdi')]:
        if not folder: continue
        assert not (folder/'wolf3d-sdl1.dll').exists()
        if compiled=='gdi':
            assert not (folder/'SDL.dll').exists() and not (folder/'wolf3d-sdl1.dll').exists()
        for choice,code in [(compiled,0),(excluded,1)]:
            result=subprocess.run([str(folder.resolve()/'wolf3d.exe'),'--no-config','--no-pause','--backend',choice,'--help'],
                cwd=folder, capture_output=True,text=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
            assert result.returncode==code,result.stdout+result.stderr
            if code:assert 'not compiled' in result.stdout+result.stderr
print('Backend loading, missing dependencies/exports, config precedence, strict selection and initialization-only fallback passed.')
