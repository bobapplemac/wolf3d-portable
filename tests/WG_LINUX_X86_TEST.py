"""Run on the multilib Linux builder; verify the legacy package audit fails closed."""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
class LegacyX86Audit(unittest.TestCase):
    def test_architecture_and_simd(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name, flags, body, valid in (
                ('i686', ['-m32', '-march=pentiumpro', '-mno-sse', '-mno-mmx'], 'return 1;', True),
                ('wrong-arch', ['-m64'], 'return 1;', False),
                ('sse', ['-m32'], '__asm__ volatile("xorps %xmm0, %xmm0"); return 1;', False)):
                with self.subTest(name=name):
                    folder = root / name
                    folder.mkdir()
                    source = root / (name + '.c')
                    source.write_text('int fixture(void) {' + body + '}\n')
                    subprocess.run(['gcc', *flags, '-shared', '-nostdlib', str(source), '-o', str(folder / 'fixture.so')], check=True)
                    result = subprocess.run(['sh', str(ROOT / 'tools/W3P_X86_AUDIT.sh'), str(folder)], capture_output=True, text=True)
                    self.assertEqual(result.returncode == 0, valid, result.stdout + result.stderr)
if __name__ == '__main__':
    unittest.main()
