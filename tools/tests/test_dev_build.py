"""Development builds must be explicit and isolated from shipped outputs."""
from pathlib import Path
import subprocess
import unittest

ROOT=Path(__file__).resolve().parents[2]

def make_dry(*args):
    return subprocess.run(['make','--no-print-directory','-n',*args],cwd=ROOT,text=True,capture_output=True)

class DevBuildTest(unittest.TestCase):
    def test_default_build_is_normal_and_dev_build_is_isolated(self):
        normal=make_dry('build')
        dev=make_dry('build','DEV_MODE=1')
        self.assertEqual(normal.returncode,0,normal.stderr)
        self.assertEqual(dev.returncode,0,dev.stderr)
        self.assertIn('-DSH_DEV_MODE=0',normal.stdout)
        self.assertIn('--build-path build/full/arduino',normal.stdout)
        self.assertIn('--output-dir dist/full',normal.stdout)
        self.assertIn('-DSH_DEV_MODE=1',dev.stdout)
        self.assertIn('--build-path build/dev/full/arduino',dev.stdout)
        self.assertIn('--output-dir dist/dev/full',dev.stdout)
        self.assertNotIn('--output-dir dist/full',dev.stdout)

    def test_dev_target_launches_the_isolated_firmware_and_real_cart(self):
        result=make_dry('dev')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('file="dist/dev/full/sub-hauler.ino.elf"',result.stdout)
        self.assertIn('file="fxdata/fxdata.bin"',result.stdout)

    def test_invalid_mode_does_not_fall_back_to_normal_outputs(self):
        for value in ('2','', 'yes'):
            with self.subTest(value=value):
                result=make_dry('build','DEV_MODE='+value)
                self.assertNotEqual(result.returncode,0)
                self.assertIn('DEV_MODE must be 0 or 1',result.stderr)

if __name__=='__main__':unittest.main()
