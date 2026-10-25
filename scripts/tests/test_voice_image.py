import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]

class VoiceImageTests(unittest.TestCase):
    def test_only_executables_in_flash_and_partition_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            root=pathlib.Path(directory)
            resources=root/'sdk/arcs-sdk/samples/algorithms/wake_up/wakeup_single/res'
            resources.mkdir(parents=True)
            build=root/'build'; build.mkdir()
            (resources/'ap.bin').write_bytes(b'AP-code')
            (build/'listenm-pet.bin').write_bytes(b'CP-code')
            command=[sys.executable,str(ROOT/'scripts/pack-voice-image.py'),str(root/'sdk'),str(build)]
            subprocess.run(command,check=True,capture_output=True)
            data=(build/'listenm-voice-full.bin').read_bytes()
            self.assertEqual(len(data),0x1000000)
            self.assertEqual(data[:7],b'AP-code')
            self.assertEqual(data[0x200000:0x800000],b'\xff'*0x600000)
            self.assertEqual(data[0x800000:0x800007],b'CP-code')
            with (build/'listenm-pet.bin').open('wb') as stream: stream.truncate(0x800001)
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('exceeds its Flash partition',result.stderr)

    def test_mini3_layout_and_reserved_ranges(self):
        with tempfile.TemporaryDirectory() as directory:
            root=pathlib.Path(directory)
            resources=root/'sdk/res/arcs-mini3'; resources.mkdir(parents=True)
            build=root/'voice-mini3'; build.mkdir()
            boot=root/'mini3-recovery'; boot.mkdir()
            (boot/'boot.bin').write_bytes(b'BOOT')
            (resources/'ap.bin').write_bytes(b'AP-code')
            (build/'listenm-pet.bin').write_bytes(b'CP-code')
            command=[sys.executable,str(ROOT/'scripts/pack-voice-image.py'),str(root/'sdk'),str(build),'arcs_mini3']
            subprocess.run(command,check=True,capture_output=True)
            data=(build/'listenm-voice-full.bin').read_bytes()
            self.assertEqual(data[:4],b'BOOT')
            self.assertEqual(data[0x40000:0x40007],b'AP-code')
            self.assertEqual(data[0x600000:0x600007],b'CP-code')
            self.assertEqual(data[0x3d000:0x40000],b'\xff'*0x3000)
            self.assertEqual(data[0xa00000:],b'\xff'*0x600000)
            for path,size in [(boot/'boot.bin',0x3d001),(resources/'ap.bin',0xc0001),(build/'listenm-pet.bin',0x400001)]:
                original=path.read_bytes()
                with path.open('wb') as stream: stream.truncate(size)
                result=subprocess.run(command,capture_output=True,text=True)
                self.assertNotEqual(result.returncode,0)
                self.assertIn('exceeds its Flash partition',result.stderr)
                path.write_bytes(original)
