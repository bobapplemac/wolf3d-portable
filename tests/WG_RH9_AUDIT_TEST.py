"""Run inside the RH9 builder: reject raised ABI and CPU requirements."""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Rh9Audit(unittest.TestCase):
    def test_abi_and_cpu_rejections(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            for name, old, body, valid in (
                ('valid',True,'return puts("ok");',True),
                ('cmov',True,'__asm__ volatile("cmovne %eax, %eax"); return puts("ok");',False),
                ('modern',False,'return clock_gettime(1,0);',False)):
                with self.subTest(name=name):
                    folder=root/name;folder.mkdir()
                    source=root/(name+'.c')
                    source.write_text('extern int puts(const char*); extern int clock_gettime(int,void*); int main(void){'+body+'}\n')
                    flags=['--sysroot=/opt/rh9','-B/opt/rh9/usr/lib/','-fgnu89-inline'] if old else []
                    subprocess.run(['gcc','-m32','-march=pentium','-no-pie','-fno-stack-protector','-static-libgcc',*flags,str(source),'-o',str(folder/'wolf3d')],check=True)
                    result=subprocess.run(['sh',str(ROOT/'tools/W3P_RH9_AUDIT.sh'),str(folder)],capture_output=True,text=True)
                    self.assertEqual(result.returncode==0,valid,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()
