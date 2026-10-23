"""Synthetic failure-class checks and opt-in pinned-ROM integration."""
import builtins
import contextlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image
import alttp_structural_context as sc
import alttp_visual_context as vc


class StructuralTests(unittest.TestCase):
    def test_detailed_collision_distinctions_and_ambiguity(self):
        groups = [(0, 1, 4, 8, 9, 10, 11), (0x10, 0x18), (0x28, 0x29, 0x2a, 0x2c, 0x2d),
                  (0x4c, 0x4e), (0x0e, 0x0f)]
        for group in groups:
            self.assertEqual(len({sc.behavior(a)["label"] for a in group}), len(group))
        self.assertIn("bits << 4", sc.behavior(11)["detector_effect"])
        self.assertIn("= 4", sc.behavior(0x10)["detector_effect"])
        self.assertIn("= 0", sc.behavior(0x11)["detector_effect"])
        self.assertIn("not_inferred", sc.behavior(0x28)["travel_direction"])
        for a in range(256):
            info = sc.behavior(a)
            self.assertIn(sc.COMMIT, info["source"])
            self.assertIn(f"0x{a:02x}", info["handler"])
            self.assertIn("not_evaluated", info["walkability"])
        self.assertEqual(sc.behavior(0x58)["interpretation"], "ambiguous")
        for raw in range(256):
            for flags in (0, 0x4000, 0x8000, 0xc000):
                self.assertEqual(vc.adjusted_attribute(raw, flags),
                                 raw | ((flags >> 14) & 1) if 0x10 <= raw < 0x1c else raw)

    def test_word_fields_no_masking_of_priority_palette_or_flips(self):
        self.assertEqual(sc.word_fields(0xffff), {"word": 65535, "tile_number": 1023,
            "collision_index": 511, "palette": 7, "priority": True, "flip_h": True, "flip_v": True})

    def test_reflection_requires_member_reordering_and_flip_xor(self):
        words = (0x2401, 0x4802, 0x8c03, 0xd004)
        self.assertEqual(sc.transform(words, 2, True), (0x0802, 0x6401, 0x9004, 0xcc03))
        self.assertEqual(sc.transform(words, 2, False, True), (0x0c03, 0x5004, 0xa401, 0xc802))
        for h, v in ((True, False), (False, True), (True, True)):
            self.assertEqual(sc.transform(sc.transform(words, 2, h, v), 2, h, v), words)

    def test_occurrence_counts_global_coordinates_and_rejected_near_matches(self):
        # Full 1024 source denominator and cross-quadrant repeat, not viewport counts.
        m = {"size": [1024, 1024], "world_origin": [1536, 1536]}
        comp = {"map8_words": [0xffff] * 16384, "map16_ids": [999] * 4096}
        selected = (1, 2, 3, 4)
        for x, y, words, ident in ((0, 0, selected, 7), (512, 512, selected, 7),
                (528, 512, sc.transform(selected, 2, True), 8),
                (544, 512, (2, 1, 4, 3), 9),  # reordered but missing XOR: reject
                (560, 512, tuple(w | 0x2000 for w in selected), 10)):
            comp["map16_ids"][(y // 16) * 64 + x // 16] = ident
            for j, word in enumerate(words):
                comp["map8_words"][(y // 8 + j // 2) * 128 + x // 8 + j % 2] = word
        exact = sc.occurrences(m, comp, 3, 7, 16, "id")
        mirrored = sc.occurrences(m, comp, 3, 7, 16, "flipped-words")
        self.assertEqual(len(exact), 2)
        self.assertEqual(len(mirrored), 3)
        self.assertEqual(exact[1]["world_origin"], [2048, 2048])
        self.assertEqual(mirrored[2]["matching_transforms"], [{"h": True, "v": False}])
        with self.assertRaises(ValueError):
            sc.occurrences(m, comp, 1024, 0, 16, "id")

    def test_composition_raw_bytes_and_member_coordinates(self):
        from alttp_atlas_overworld import OverworldQuadrant, MAP32_TO_MAP16_TABLES, MAP16_TO_MAP8_TABLE
        class Rom:
            def read_bytes(self, address, length):
                if length == 6:
                    return bytes([MAP32_TO_MAP16_TABLES.index(address) + 1, 0, 0, 0, 0, 0])
                if length == 8:
                    n = (address - MAP16_TO_MAP8_TABLE) // 8
                    return b"".join((n * 4 + i).to_bytes(2, "little") for i in range(4))
                return bytes(length)
        q = OverworldQuadrant(27, 0x828000, 0x828010, 2, 2, 256, 256,
            (0,) * 256, tuple(1 + (i % 32) % 2 + 2 * ((i // 32) % 2) for i in range(1024)), (0xc007,) * 4096)
        comp = sc.compose(Rom(), {27: q}, [27], [1536, 1536], 512)
        self.assertEqual(comp["map32_definitions"]["0"]["member_ids"], [1, 2, 3, 4])
        self.assertEqual(comp["map16_definitions"]["4"]["member_words"], [16, 17, 18, 19])
        m = {"size": [512, 512], "world_origin": [1536, 1536], "quadrants": [27]}
        c = {"raw_attributes": [0x18] * 4096, "adjusted_attributes": [0x19] * 4096}
        info = sc.cell_info(m, c, comp, 31, 29)
        self.assertEqual(info["map32"]["map8_member_xy"], [3, 3])
        self.assertEqual(info["map32"]["map16_member_xy"], [1, 1])
        self.assertEqual(info["map16"]["map8_member_xy"], [1, 1])
        self.assertEqual(info["map16"]["origin"], [16, 16])
        self.assertEqual(info["world_pixel"], [1567, 1565])

    def test_leak_guard_recursive_and_output_isolation(self):
        for field in sc.FORBIDDEN:
            with self.assertRaises(ValueError):
                sc.leak_guard({"nested": [{field: []}]})
        for output in (Path("bundle"), Path("bundle/views")):
            with self.assertRaises(ValueError):
                sc.outside(output, Path("bundle"))

    @unittest.skipUnless(os.environ.get("ALTTP_STRUCTURAL_ROM"), "set ALTTP_STRUCTURAL_ROM for integration")
    def test_pinned_rom_bundle_and_cli_with_input_read_allowlist(self):
        # Import decoder modules before enabling read audit (Python source imports are not data inputs).
        import alttp_atlas
        import alttp_atlas_overworld
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            visual, bundle = root / "visual", root / "structural"
            rom = Path(os.environ["ALTTP_STRUCTURAL_ROM"]).resolve()
            vc.prepare(rom, 0x1b, visual)
            original = builtins.open
            original_io = io.open
            def guard(opener):
                def guarded(path, mode="r", *args, **kwargs):
                    if isinstance(path, (str, bytes, os.PathLike)) and "r" in mode:
                        p = Path(os.fsdecode(path)).resolve()
                        # Only ROM, known visual files, or tool-owned temporary outputs.
                        approved_temp = any(part.startswith("structural-rom-") for part in p.parts)
                        if p != rom and p not in {visual.resolve() / n for n in vc.FILES} and root.resolve() not in p.parents and not approved_temp:
                            raise AssertionError(f"unexpected data read: {p}")
                    return opener(path, mode, *args, **kwargs)
                return guarded
            with patch("builtins.open", guard(original)), patch("io.open", guard(original_io)):
                sc.prepare(rom, visual, bundle)
            m, c, comp, rgb = sc.load_bundle(bundle)
            self.assertEqual(rgb.size, (1024, 1024))
            self.assertEqual(m["denominators"], {"map8": 16384, "map16": 4096, "map32": 1024})
            self.assertEqual(sum(m["attribute_counts"].values()), 16384)
            self.assertTrue(all(sc.behavior(a)["interpretation"] != "ambiguous" for a in c["adjusted_attributes"]))
            before = {n: vc.digest(bundle / n) for n in sc.FILES}
            def cli(*args):
                with contextlib.redirect_stdout(io.StringIO()) as out:
                    self.assertEqual(sc.main([*args, "--bundle", str(bundle)]), 0)
                return json.loads(out.getvalue())
            result = cli("view", "--x", "511", "--y", "509", "--width", "31", "--height", "29")
            with Image.open(result["artwork_png"]) as im:
                self.assertEqual(im.size, (93, 87))
                self.assertEqual(im.getpixel((0, 0)), rgb.getpixel((511, 509)))
            cli("inspect", "--x", "520", "--y", "520")
            cli("overview", "--max-size", "512")
            result = cli("examples", "--x", "208", "--y", "648", "--unit", "16", "--match", "flipped-words", "--limit", "2")
            evidence = json.loads(Path(result["evidence_json"]).read_text())
            self.assertEqual(evidence["total"], len(evidence["all_occurrences"]))
            self.assertEqual(len(evidence["entries"][0]["members"]), 4)
            cli("metatiles", "--unit", "32", "--limit", "2")
            self.assertEqual(before, {n: vc.digest(bundle / n) for n in sc.FILES})
            for path in bundle.glob("*.json"):
                sc.leak_guard(json.loads(path.read_text()))
            # Mixed inputs and resealed composition tampering must be rejected.
            vc.write_json(bundle / "candidate.json", {"height": 100})
            with self.assertRaisesRegex(ValueError, "six declared"):
                sc.load_bundle(bundle)
            (bundle / "candidate.json").unlink()
            comp["map16_definitions"][str(comp["map16_ids"][0])]["member_words"][0] ^= 1
            vc.write_json(bundle / "composition.json", comp)
            m["files_sha256"]["composition.json"] = vc.digest(bundle / "composition.json")
            vc.write_json(bundle / "bundle.json", m)
            with self.assertRaisesRegex(ValueError, "members disagree"):
                sc.load_bundle(bundle)
            # Even rehashed visual RGB cannot become an alternative evidence input.
            with Image.open(visual / "artwork.png") as im:
                edited = im.copy()
            edited.putpixel((0, 0), (255, 0, 255))
            edited.save(visual / "artwork.png")
            vm = json.loads((visual / "bundle.json").read_text())
            vm["files_sha256"]["artwork.png"] = vc.digest(visual / "artwork.png")
            vc.write_json(visual / "bundle.json", vm)
            with self.assertRaisesRegex(ValueError, "canonical ROM"):
                sc.prepare(rom, visual, root / "rejected")


if __name__ == "__main__":
    unittest.main()
