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
