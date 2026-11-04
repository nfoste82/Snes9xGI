"""Effective probe inputs must stay pinned, including absent optional sidecars."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class ColdDoorPinsTest(unittest.TestCase):
    def run_benchmark(self, mutation=None, present=False, suffix='.terrain-input.bin'):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            capture = root / 'frame.s9xrmf'
            for required in ('', '.dungeon-input.bin', '.dungeon-templates.bin',
                             '.dungeon.json', '.profile.toml'):
                Path(str(capture) + required).write_bytes(b'pinned')
            optional = Path(str(capture) + suffix)
            if present:
                optional.write_bytes(b'original')
            probe = root / 'probe'
            probe.write_text('''#!/usr/bin/env python3
import pathlib, sys
c, out = sys.argv[1:]
pathlib.Path(out + '-timing.csv').write_text('sample,endframe_ms\\n0,1\\n1,1\\n2,1\\n3,1\\n')
pathlib.Path(out + '-alternating.bin').write_bytes(b'same')
''' + (f"pathlib.Path(c + {suffix!r}).{mutation}\n" if mutation else ''))
            probe.chmod(0o755)
            output = root / 'output'
            result = subprocess.run([sys.executable, str(Path(__file__).with_name('benchmark_cold_doors.py')),
                                     '--before', str(probe), '--after', str(probe), '--output', str(output),
                                     '--samples', '4', '--parity', str(capture)], capture_output=True, text=True)
            pins = json.loads((output / 'pins.json').read_text())
            return result, pins, str(optional), str(capture)

    def test_absence_is_pinned(self):
        result, pins, _, capture = self.run_benchmark()
        self.assertEqual(result.returncode, 0, result.stderr)
        for suffix in ('.terrain-input.bin', '.sprite-input.bin', '.replacement-input.bin'):
            self.assertIn(capture + suffix, pins)
            self.assertIsNone(pins[capture + suffix])

    def test_presence_is_pinned(self):
        for suffix in ('.terrain-input.bin', '.sprite-input.bin', '.replacement-input.bin'):
            with self.subTest(suffix=suffix):
                result, pins, optional, _ = self.run_benchmark(present=True, suffix=suffix)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(pins.get(optional), hashlib.sha256(b'original').hexdigest())

    def test_new_sidecar_rejected(self):
        for suffix in ('.terrain-input.bin', '.sprite-input.bin', '.replacement-input.bin'):
            with self.subTest(suffix=suffix):
                result, _, _, _ = self.run_benchmark("write_bytes(b'new')", suffix=suffix)
                self.assertNotEqual(result.returncode, 0, 'new effective probe input went unnoticed')
                self.assertIn('benchmark input changed:', result.stderr)

    def test_changed_sidecar_rejected(self):
        for suffix in ('.terrain-input.bin', '.sprite-input.bin', '.replacement-input.bin'):
            with self.subTest(suffix=suffix):
                result, _, _, _ = self.run_benchmark("write_bytes(b'changed')", present=True, suffix=suffix)
                self.assertNotEqual(result.returncode, 0, 'changed effective probe input went unnoticed')
                self.assertIn('benchmark input changed:', result.stderr)

    def test_disappeared_sidecar_rejected(self):
        for suffix in ('.terrain-input.bin', '.sprite-input.bin', '.replacement-input.bin'):
            with self.subTest(suffix=suffix):
                result, _, _, _ = self.run_benchmark('unlink(missing_ok=True)', present=True, suffix=suffix)
                self.assertNotEqual(result.returncode, 0, 'missing effective probe input went unnoticed')
                self.assertIn('benchmark input changed:', result.stderr)

    def test_required_input_mutation_rejected(self):
        result, _, _, _ = self.run_benchmark("write_bytes(b'changed')", suffix='.dungeon-input.bin')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('benchmark input changed:', result.stderr)


if __name__ == '__main__':
    unittest.main()
