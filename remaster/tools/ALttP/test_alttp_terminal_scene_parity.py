"""Pinned integration check: terminal placement values versus selected scene.

The capture is optional local evidence, not a checked-in ROM fixture.  Set
ALTTP_TERMINAL_PARITY_AUDIT to the immutable 025 audit directory.  The oracle
tests consumer parity only: agreeing with a captured scene does not approve its
relative wall gradient or its floor datum.
"""
import json
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
AUDIT = os.environ.get("ALTTP_TERMINAL_PARITY_AUDIT")


@unittest.skipUnless(AUDIT, "requires pinned 025 audit evidence")
class TerminalSceneParity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.audit = Path(AUDIT).resolve()
        inputs, own = cls.audit / "inputs", cls.audit / "own"
        cls.capture = inputs / "frame_room002_025.s9xrmf"
        if hashlib.sha256(Path(str(cls.capture) + ".profile.toml").read_bytes()).hexdigest() != "6586d19c77c94089fa8b7531787ac9e5d685af552d32a2ea89c20bae32578291":
            raise ValueError("capture profile changed")
        cls.oracle = json.loads((own / "scene-relative-oracles.json").read_text())
        # Pin identities to the independently retained original selected scene.
        # Heights below are NOT derived from that scene: they come from the
        # user's floor datum and the authored transformed wall cross-section.
        debug_path = own / "production-debug.jsonl"
        if hashlib.sha256(debug_path.read_bytes()).hexdigest() != "42cad37a9ef0b7f0398a961aa12a59f657c765bcd6d73a4e6535a78680a2976b":
            raise ValueError("original selected-scene identity evidence changed")
        cls.original_pixels = list(map(json.loads, debug_path.read_text().splitlines()))
        cls.tmp = tempfile.TemporaryDirectory()
        tmp = Path(cls.tmp.name)
        # Build the independently authored EndFrame probe against the current
        # source, rather than replaying the historic packet's baked heights.
        source = (cls.audit / "tool-snapshot/after-probe.cpp").read_text()
        sealed = "/Users/nic.foster/Projects/Snes9xGI/build/room-height-repairs/joint-visible-shell-continuation-20261027T142311Z/final-source"
        if sealed not in source:
            raise ValueError("unexpected probe source")
        probe = tmp / "probe.cpp"
        probe.write_text(source.replace(sealed, str(ROOT)))
        for src, exe, extra in [
            (probe, tmp / "probe", ["-I", str(ROOT / "remaster/tests")]),
            (ROOT / "remaster/tools/ALttP/alttp_capture_debug_sources.cpp", tmp / "debug", []),
            (ROOT / "remaster/tools/ALttP/alttp_terminal_height_inventory.cpp", tmp / "terminal", []),
        ]:
            subprocess.run(["c++", "-std=c++17", "-O2", *extra, str(src), "-o", str(exe)], cwd=ROOT, check=True)
        prefix = tmp / "production"
        subprocess.run([str(tmp / "probe"), str(cls.capture), str(prefix)], cwd=ROOT,
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        debug = subprocess.run([str(tmp / "debug"), str(prefix) + ".s9xrmf"], cwd=ROOT,
                               check=True, text=True, stdout=subprocess.PIPE)
        cls.pixels = list(map(json.loads, debug.stdout.splitlines()))
        result = subprocess.run([str(tmp / "terminal"), str(own / "live-solver-input.raw"),
                                 str(own / "live-solver-input.raw.attr"),
                                 str(cls.capture) + ".profile.toml", "1229", "100", "1",
                                 "--rom", str(inputs / "rom.sfc")],
                                cwd=ROOT, check=True, text=True, stdout=subprocess.PIPE)
        cls.terminal = {(r["bg"], r["x"], r["y"]): r
                        for r in map(json.loads, result.stdout.splitlines())}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def selected_samples(self):
        cells = {tuple(p["cell"]): p for p in self.oracle["placements"]}
        for pixel in self.pixels:
            source = pixel["scene" if pixel["use_scene"] else "main"]
            if source.get("source") != 2 or source.get("ink", 0) <= 0:
                continue
            x, y = (1109 + pixel["sx"]) & 511, (1 + pixel["sy"]) & 511
            key = (source["bg"], x // 8, y // 8)
            if key in cells:
                yield key, (y % 8) * 8 + x % 8, source, cells[key]

    def test_selected_wall_samples(self):
        oracle = self.oracle
        self.assertFalse(oracle["approval"])
        cells = {tuple(p["cell"]): p for p in oracle["placements"]}
        self.assertEqual((len(cells), oracle["observed"], oracle["mismatch"]),
                         (14, 896, 896))
        failures, tested = [], 0
        for key, _, source, _ in self.selected_samples():
            placed = self.terminal[key]
            self.assertEqual(placed["hash"], source["hash"])
            self.assertTrue(placed["has_effective_height"] and source["has_height"])
            value = source["base"] + source["local"]
            actual = placed["actual"][source["k"]]
            if actual != value:
                failures.append((key, source["k"], actual, value))
            tested += 1
        self.assertEqual(tested, 896)
        self.assertEqual(len(failures), 0, f"terminal differs from selected scene: {failures[:3]}")

    def test_historical_high_band_oracle_is_conditional(self):
        # Preserve the old 896-sample artifact, not its assumption as an
        # acceptance condition. It held the discontinuous high band fixed and
        # consequently raised the reported correct floor by 12. The supplied
        # joint floor/face intent disproves that conditional choice; it does
        # not justify changing authored shapes or unrelated floor components.
        self.assertEqual((self.oracle["observed"], self.oracle["mismatch"]), (896, 896))
        self.assertEqual(len(self.oracle["placements"]), 14)

    def test_full_exposed_cross_bg_chain(self):
        # User: BG1 floor is correct; wall rises smoothly from that floor all
        # the way up, BG2 continues where BG1 leaves off. Conditional on the
        # matched ROM sections actually being this continuous exposed face, the
        # 40 wall pixels have exact authored unit relief and the floor is 0.
        # Test all pixels, both joins and the floor, not only terminal offsets.
        def section(pixels):
            out = {}
            for pixel in pixels:
                source = pixel['scene' if pixel['use_scene'] else 'main']
                if source.get('source') != 2 or source.get('ink', 0) <= 0:
                    continue
                wx = (1109 + pixel['sx']) & 511
                wy = (1 + pixel['sy']) & 511
                if 88 <= wy < 184 and 104 <= wx <= 144:
                    out[wx, wy] = source
            return out

        original, samples = section(self.original_pixels), section(self.pixels)
        self.assertEqual(len(original), 3936)
        self.assertEqual(set(samples), set(original), 'selected visible face ownership changed')
        failures, seams = [], []
        for wy in range(88, 184):
            for wx in range(104, 145):
                source, pinned = samples[wx, wy], original[wx, wy]
                self.assertEqual((source['bg'], source['hash'], source['k']),
                                 (pinned['bg'], pinned['hash'], pinned['k']))
                self.assertEqual(source['bg'], 2 if wx < 120 else 1)
                self.assertTrue(source['has_height'] and pinned['has_height'])
                # A source switch, damaged relief or flip must not accidentally
                # satisfy a placement-only gradient calculation.
                local = 7 - wx % 8 if wx < 144 else 0
                self.assertEqual((pinned['local'], source['local']), (local, local))
                if wx == 144:
                    self.assertEqual(pinned['base'] + pinned['local'], 0,
                                     'pinned user-approved floor datum changed')
                value = source['base'] + source['local']
                expected = max(143 - wx, 0)
                if value != expected:
                    failures.append((wx, wy, value, expected))
            heights = [samples[wx, wy]['base'] + samples[wx, wy]['local']
                       for wx in (111, 112, 119, 120, 127, 128, 135, 136, 143, 144)]
            if heights != [32, 31, 24, 23, 16, 15, 8, 7, 0, 0]:
                seams.append((wy, heights))
        self.assertEqual((len(failures), len(seams)), (0, 0),
                         f'floor-anchored face fails {len(failures)}/3936: {failures[:3]}; '
                         f'joint seams/floor fail {len(seams)}/96: {seams[:3]}')


if __name__ == "__main__":
    unittest.main()
