"""Portable synthetic tests; optional pinned-ROM integration via ALTTP_VISUAL_ROM."""
import contextlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image
import alttp_visual_context as vc


class VisualContextTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bundle = self.root / "bundle"
        self.bundle.mkdir()
        # Four distinctly colored quadrants catch wrapping and crop misalignment.
        image = Image.new("RGB", (512, 512), "red")
        image.paste("blue", (256, 0, 512, 256))
        image.paste("yellow", (0, 256, 256, 512))
        image.paste("green", (256, 256, 512, 512))
        image.save(self.bundle / "artwork.png")
        lut = [0] * 512
        lut[7] = 0x18
        words = [0] * 4096
        words[0] = 0xC007
        raw = [lut[w & 511] for w in words]
        attrs = [vc.adjusted_attribute(r, w) for r, w in zip(raw, words)]
        vc.write_json(self.bundle / "collision.json", {
            "grid_size": [64, 64], "map8_words": words, "raw_attributes": raw,
            "adjusted_attributes": attrs, "lookup_table": lut,
        })
        vc.write_json(self.bundle / "commands.json", vc.command_descriptor(self.bundle))
        self.manifest = {
            "format": vc.FORMAT, "size": [512, 512], "area_id": 27,
            "state": "base_rom_no_runtime_edits", "rom_sha256": "synthetic",
            "palette": {"display": "synthetic test colors"},
            "collision_legend": vc.COLORS,
        }
        self.seal()

    def seal(self):
        self.manifest["files_sha256"] = {n: vc.digest(self.bundle / n) for n in vc.FILES - {"bundle.json"}}
        vc.write_json(self.bundle / "bundle.json", self.manifest)

    def test_collision_flip_exhaustive_exact(self):
        for raw in range(256):
            for flip in (0, 0x4000, 0x8000, 0xC000):
                expected = raw | bool(flip & 0x4000) if 0x10 <= raw < 0x1C else raw
                self.assertEqual(vc.adjusted_attribute(raw, flip | 511), expected)
        self.assertEqual(vc.adjusted_attribute(0x18, 0x8000), 0x18)
        self.assertEqual(vc.adjusted_attribute(0x19, 0x4000), 0x19)  # OR, not XOR

    def test_bounds_and_explicit_clamp(self):
        self.assertEqual(vc.bounds((1024, 1024), 768, 800, 256, 224), [768, 800, 256, 224])
        for x, y in ((-1, 0), (0, -1), (769, 0), (0, 801)):
            with self.assertRaises(ValueError):
                vc.bounds((1024, 1024), x, y, 256, 224)
        self.assertEqual(vc.bounds((1024, 1024), -30, 9999, 256, 224, True), [0, 800, 256, 224])
        for width, height in ((0, 224), (1025, 224), (256, -1)):
            with self.assertRaises(ValueError):
                vc.bounds((1024, 1024), 0, 0, width, height, True)

    def test_artifact_dimensions_and_crop_no_wrap(self):
        out = self.root / "view"
        result = vc.render_view(self.bundle, out, 256, 288, 256, 224, 3)
        self.assertEqual(result["output_size"], [768, 672])
        for name in ("artwork.png", "collision.png"):
            with Image.open(out / name) as image:
                self.assertEqual(image.size, (768, 672))
                if name == "artwork.png":
                    self.assertEqual(image.getpixel((767, 671)), (0, 128, 0))
        vc.render_view(self.bundle, out, 0, 0, 512, 512, 1, max_size=128)
        with Image.open(out / "artwork.png") as image:
            self.assertEqual(image.size, (128, 128))
        self.assertEqual(json.loads((out / "view.json").read_text())["actual_rect"], [0, 0, 512, 512])

    def test_inspect_no_semantic_fields_or_guessed_walkability(self):
        result = vc.inspect_cell(self.bundle, 0, 0)
        self.assertEqual(result["raw_collision"], 0x18)
        self.assertEqual(result["adjusted_collision"], 0x19)
        self.assertTrue(result["flip_v"])
        for forbidden in ("hash", "role", "height", "assembly", "support", "base", "local", "content_id"):
            self.assertNotIn(forbidden, result)
        self.assertEqual(result["walkability"], "not_evaluated_state_and_actor_dependent")
        self.assertEqual(vc.behavior(0), "normal_sample_not_floor_proof")
        for x, y in ((-1, 0), (512, 0), (0, 512)):
            with self.assertRaises(ValueError):
                vc.inspect_cell(self.bundle, x, y)

    def test_integrity_and_mixed_bundle_rejected(self):
        (self.bundle / "candidate.json").write_text('{"height": 50}')
        with self.assertRaisesRegex(ValueError, "only the four"):
            vc.load_bundle(self.bundle)
        (self.bundle / "candidate.json").unlink()
        (self.bundle / "collision.json").write_text("{}")
        with self.assertRaisesRegex(ValueError, "integrity"):
            vc.load_bundle(self.bundle)

    def test_collision_tampering_rejected_even_if_resealed(self):
        path = self.bundle / "collision.json"
        data = json.loads(path.read_text())
        data["adjusted_attributes"][0] = 0x18
        vc.write_json(path, data)
        self.seal()
        with self.assertRaisesRegex(ValueError, "lookup/flip"):
            vc.load_bundle(self.bundle)

    def test_cli_pan_state_and_bundle_isolation(self):
        def cli(*args):
            with contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(vc.main([*args, "--bundle", str(self.bundle)]), 0)
            return json.loads(output.getvalue())
        first = cli("view", "--x", "0", "--y", "0")
        second = cli("step", "--dx", "9999", "--dy", "9999", "--clamp")
        self.assertEqual(second["actual_rect"], [256, 288, 256, 224])
        self.assertTrue(second["clamped"])
        self.assertEqual(Path(first["pan_state"]), Path(second["pan_state"]))
        self.assertEqual({p.name for p in self.bundle.iterdir()}, vc.FILES)
        overview = cli("overview", "--max-size", "256")
        self.assertEqual(overview["output_size"], [256, 256])
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            cli("view", "--output", str(self.bundle))

    def test_palette_load_offsets_and_inheritance(self):
        class FakeRom:
            def get_word(self, address):
                return address
        palette, info = vc.rom_palette(FakeRom(), 0x1B, 4)
        self.assertEqual(info["aux_selectors"], [0, 4, 7])
        self.assertEqual(palette[0x21], 0x9BE6C8)
        self.assertEqual(palette[0x61 + 6], 0x9BE6C8 + 68)
        self.assertEqual(palette[0x59], 0x9BE86C + 4 * 42)
        self.assertEqual(palette[0x71], 0x9BE604 + 7 * 14)
        self.assertIsNone(palette[0x11])
        inherited, _ = vc.rom_palette(FakeRom(), 0x1B, 0)
        self.assertIsNone(inherited[0x59])
        self.assertEqual(vc.rgb555(0x7FFF), (255, 255, 255))

    @unittest.skipUnless(os.environ.get("ALTTP_VISUAL_ROM"), "set ALTTP_VISUAL_ROM for ROM integration")
    def test_real_castle_bundle_rom_only(self):
        output = self.root / "castle"
        # Fail if preparation tries to open geometry/capture/semantic artifacts.
        original = Path.open
        allowed = Path(os.environ["ALTTP_VISUAL_ROM"]).resolve()

        def guarded(path, mode="r", *args, **kwargs):
            if "r" in mode and path.resolve() != allowed and output.resolve() not in path.resolve().parents:
                raise AssertionError(f"unexpected non-ROM input: {path}")
            return original(path, mode, *args, **kwargs)

        with patch.object(Path, "open", guarded):
            vc.prepare(allowed, 0x1B, output)
        manifest, collision, image = vc.load_bundle(output)
        self.assertEqual(image.size, (1024, 1024))
        self.assertEqual(manifest["quadrants"], [0x1B, 0x1C, 0x23, 0x24])
        self.assertEqual(len(collision["map8_words"]), 16384)
        self.assertEqual(manifest["palette"]["display"], "rom_color")
        self.assertEqual(sum(manifest["artwork_cell_counts"].values()), 16384)
        self.assertTrue(set(manifest["artwork_cell_counts"]) <= {"resolved", "resolved_animated_phase"})
        forbidden = {"hash", "role", "height", "assembly", "support", "base", "local", "content_id", "placements"}

        def check(value):
            if isinstance(value, dict):
                self.assertFalse(forbidden.intersection(value))
                for child in value.values():
                    check(child)
            elif isinstance(value, list):
                for child in value:
                    check(child)
        for path in output.glob("*.json"):
            check(json.loads(path.read_text()))


if __name__ == "__main__":
    unittest.main()
