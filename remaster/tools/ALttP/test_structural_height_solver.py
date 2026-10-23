"""Synthetic geometry only: run with python -m unittest discover -s remaster/tools/ALttP -p test_structural_height_solver.py."""
import copy
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image

import structural_height_solver as solver


def rect(x0, y0, x1, y1):
    return {"polygons": [{"outer": [[x0, y0], [x1, y0], [x1, y1], [x0, y1]]}]}


def flat(node="base"):
    return {"kind": "flat", "node": node}


def ramp(axis=(1, 0), start=(0, 0), span=7):
    return {"kind": "ramp", "low": "base", "high": "top", "axis": list(axis),
            "start": list(start), "span": span}


def surface(key, geometry, height=None, priority=0):
    return {"id": key, "type": "flat" if height is None else "transition", "priority": priority,
            "geometry": geometry, "height": height or flat(), "reason": "synthetic structural fixture",
            "evidence": "authored endpoint constraints", "confidence": 0.8}


def scene(surfaces=None, width=8, height=8):
    return {"format": "structural-height-1", "width": width, "height": height,
            "height_unit": "authored-unit", "height_range": [0, 20],
            "nodes": {"base": {"anchor": 0}, "top": {}},
            "relations": [{"kind": "rise", "from": "base", "to": "top", "amount": 2, "scale": 10}],
            "surfaces": surfaces if surfaces is not None else [surface("face", rect(0, 0, width-1, height-1))]}


def contact(a, b, **kwargs):
    return dict(id=a + "-" + b, a=a, b=b, tolerance=0, boundary=True, **kwargs)


class StructuralHeightTests(unittest.TestCase):
    def test_equal_height_faces_different_art_spans(self):
        s = scene([surface("short", rect(0, 0, 1, 7), ramp(span=1)),
                   surface("long", rect(3, 0, 7, 7), ramp(start=(3, 0), span=4))])
        s["domain"] = {"polygons": rect(0, 0, 1, 7)["polygons"] + rect(3, 0, 7, 7)["polygons"]}
        r = solver.solve_scene(s)
        self.assertEqual(r["report"]["status"], "complete")
        self.assertEqual(r["heights"][1], r["heights"][7])
        self.assertEqual(r["heights"][0], r["heights"][3])

    def test_cardinal_and_oblique_axes(self):
        for axis, start, span, low, high in [((1, 0), (0, 0), 7, 0, 7),
                                            ((-1, 0), (7, 0), 7, 7, 0),
                                            ((0, 1), (0, 0), 7, 0, 56),
                                            ((0, -1), (0, 7), 7, 56, 0),
                                            ((1, 1), (0, 0), 7*2**0.5, 0, 63)]:
            with self.subTest(axis=axis):
                r = solver.solve_scene(scene([surface("ramp", rect(0, 0, 7, 7), ramp(axis, start, span))]))
                self.assertAlmostEqual(r["heights"][low], 0)
                self.assertAlmostEqual(r["heights"][high], 20)

    def test_corner_continuity_min_and_max(self):
        for kind in ("min", "max"):
            # min's right edge and max's left edge exactly equal the adjoining y ramp.
            x = 0 if kind == "min" else 4
            strip_x = 4 if kind == "min" else 0
            expression = {"kind": kind, "children": [ramp(start=(x, 0), span=3), ramp((0, 1))]}
            s = scene([surface("corner", rect(x, 0, x+3, 7), expression),
                       surface("strip", rect(strip_x, 0, strip_x+3, 7), ramp((0, 1)))])
            s["contacts"] = [contact("corner", "strip")]
            r = solver.solve_scene(s)
            self.assertEqual(r["report"]["status"], "complete")
            self.assertEqual(r["report"]["contacts"][0]["samples"], 8)
            self.assertEqual(r["report"]["contacts"][0]["max_error"], 0)
            for y in range(8):
                self.assertAlmostEqual(r["heights"][y*8+strip_x], 20*y/7)

    def test_documented_quickstart(self):
        document = Path(__file__).with_name("STRUCTURAL-HEIGHT-FORMAT.md").read_text()
        example = json.loads(document.split("```json\n", 1)[1].split("```", 1)[0])
        result = solver.solve_scene(example)
        self.assertEqual(result["report"]["status"], "complete")
        self.assertEqual(result["report"]["coverage"]["solved"], 1024**2)
        self.assertEqual(result["report"]["boundary_pixel_pairs"], 2048)
        with tempfile.TemporaryDirectory() as d:
            solver.write_outputs(result, d)
            expected = {"height.png", "height.f64", "confidence.png", "coverage.png", "unknown.png", "surfaceids.png", "report.json"}
            self.assertEqual({p.name for p in Path(d).iterdir()}, expected)
            self.assertEqual((Path(d)/"height.f64").stat().st_size, 1024**2*8)
            with Image.open(Path(d)/"surfaceids.png") as image:
                self.assertEqual(image.getpixel((0, 0)), (1, 0, 0))
                self.assertEqual(image.getpixel((500, 0)), (2, 0, 0))
                self.assertEqual(image.getpixel((1023, 0)), (3, 0, 0))

    def test_nodes_signed_same_disconnected_and_conflicting(self):
        s = scene()
        s["nodes"].update(below={}, alias={}, lost={}, isolated={"anchor": 3})
        s["relations"] += [{"kind": "rise", "from": "base", "to": "below", "amount": -2, "scale": 3},
                           {"kind": "same", "from": "below", "to": "alias"}]
        r = solver.solve_scene(s)["report"]
        self.assertEqual(r["nodes"]["alias"]["value"], -6)
        self.assertEqual(r["nodes"]["lost"]["status"], "unresolved")
        self.assertEqual(r["nodes"]["isolated"]["status"], "solved")
        s["relations"].append({"kind": "same", "from": "base", "to": "top"})
        r = solver.solve_scene(s)
        self.assertEqual(r["report"]["nodes"]["base"]["status"], "conflict")
        self.assertTrue(r["report"]["node_conflicts"])
        self.assertEqual(r["report"]["coverage"]["unresolved"], 64)

    def test_conflicting_anchors(self):
        s = scene()
        s["nodes"]["top"]["anchor"] = 19
        self.assertTrue(solver.solve_scene(s)["report"]["node_conflicts"])

    def test_contacts_fail_max_not_mean_and_no_smoothing(self):
        s = scene([surface("a", rect(0, 0, 3, 7)),
                   surface("b", rect(4, 0, 7, 7), {"kind": "plane", "node": "base", "origin": [4, 0], "gradient": [0, 1]})])
        s["contacts"] = [dict(id="edge", a="a", b="b", tolerance=4, boundary=True)]
        r = solver.solve_scene(s)
        c = r["report"]["contacts"][0]
        self.assertEqual(c["failed_samples"], 3)
        self.assertEqual(c["max_error"], 7)
        self.assertEqual(r["heights"][60], 7)
        self.assertEqual(r["report"]["status"], "review_incomplete")

    def test_missing_partial_and_nonspatial_contacts(self):
        s = scene([surface("a", rect(0, 0, 3, 7)), surface("b", rect(4, 0, 7, 7))])
        self.assertEqual(solver.solve_scene(s)["report"]["missing_contact_pixel_pairs"], 8)
        s["contacts"] = [dict(id="one", a="a", b="b", tolerance=0, pairs=[[[3, 0], [4, 0]]])]
        self.assertEqual(solver.solve_scene(s)["report"]["missing_contact_pixel_pairs"], 7)
        s["contacts"][0]["pairs"] = [[[0, 0], [7, 0]]]
        self.assertEqual(solver.solve_scene(s)["report"]["contacts"][0]["examples"][0]["reason"], "not_spatial_seam")

    def test_segments_and_signed_delta(self):
        s = scene([surface("a", rect(0, 0, 3, 7)), surface("b", rect(4, 0, 7, 7), flat("top"))])
        s["contacts"] = [dict(id="step", a="a", b="b", tolerance=0, delta=20,
                              segments=[{"a": [[3, 0], [3, 7]], "b": [[4, 0], [4, 7]]}])]
        self.assertEqual(solver.solve_scene(s)["report"]["status"], "complete")

    def test_holes_caps_priority_and_unresolved(self):
        geometry = rect(0, 0, 7, 7)
        geometry["polygons"][0]["holes"] = [[[2, 2], [5, 2], [5, 5], [2, 5]]]
        s = scene([surface("floor", geometry), surface("cap", rect(0, 0, 1, 1), flat("top"), 1)])
        s["domain"] = copy.deepcopy(geometry)
        s["contacts"] = [contact("floor", "cap", delta=20)]
        r = solver.solve_scene(s)
        self.assertEqual(r["report"]["coverage"]["outside"], 16)
        self.assertEqual(r["heights"][0], 20)
        self.assertEqual(r["report"]["status"], "complete")
        s["nodes"]["top"] = {}
        s["relations"] = []
        r = solver.solve_scene(s)
        self.assertEqual(r["report"]["coverage"]["unresolved"], 4)
        self.assertEqual(r["report"]["contacts"][0]["examples"][0]["reason"], "unresolved_height")

    def test_equal_priority_owners_rejected_even_equal_heights(self):
        s = scene([surface("a", rect(0, 0, 7, 7)), surface("b", rect(0, 0, 1, 1))])
        self.assertEqual(solver.solve_scene(s)["report"]["coverage"]["overlap"], 4)
        s["surfaces"].append(surface("cover", rect(0, 0, 1, 1), priority=2))
        s["contacts"] = [contact("a", "cover")]
        self.assertEqual(solver.solve_scene(s)["report"]["status"], "complete")

    def test_mask_multipolygon_and_gaps(self):
        with tempfile.TemporaryDirectory() as d:
            Image.new("L", (8, 8), 0).save(Path(d)/"mask.png")
            s = scene([surface("mask", {"mask": "mask.png", "polygons": rect(0, 0, 1, 1)["polygons"] + rect(6, 6, 7, 7)["polygons"]})])
            r = solver.solve_scene(s, d)
            self.assertEqual(r["report"]["coverage"]["solved"], 8)
            self.assertEqual(r["report"]["coverage"]["gap"], 56)

    def test_artwork_cannot_change_height_and_cli_outputs(self):
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            path = d/"scene.json"
            path.write_text(json.dumps(scene()))
            previous = None
            for luminance in (0, 255):
                art, out = d/f"art{luminance}.png", d/f"out{luminance}"
                Image.new("L", (8, 8), luminance).save(art)
                self.assertEqual(solver.main([str(path), "--outputdir", str(out), "--artworkPNG", str(art)]), 0)
                raw = (out/"height.png").read_bytes()
                if previous:
                    self.assertEqual(previous, raw)
                previous = raw
                with Image.open(out/"height.png") as image:
                    self.assertEqual(image.mode, "L")
                    self.assertEqual(image.getpixel((0, 0)), 1)  # true zero is not unknown black
                self.assertEqual(struct.unpack("<d", (out/"height.f64").read_bytes()[:8])[0], 0)
                self.assertTrue((out/"comparison.png").exists())
            s = scene([surface("partial", rect(0, 0, 1, 1))])
            path.write_text(json.dumps(s))
            args = [str(path), "--outputdir", str(d/"incomplete")]
            self.assertEqual(solver.main(args), 1)
            self.assertEqual(solver.main(args + ["--allow-incomplete"]), 0)
            with Image.open(d/"incomplete"/"unknown.png") as image:
                self.assertEqual(image.getpixel((7, 7)), 255)
            self.assertEqual(json.loads((d/"incomplete"/"report.json").read_text())["status"], "review_incomplete")

    def test_no_numpy_backend_matches(self):
        s = scene([surface("corner", rect(0, 0, 7, 7), {"kind": "max", "children": [ramp(), ramp((0, 1))]})])
        accelerated = solver.solve_scene(s)
        with patch.object(solver, "np", None):
            fallback = solver.solve_scene(s)
        self.assertEqual(list(accelerated["heights"]), list(fallback["heights"]))
        self.assertEqual(accelerated["coverage"], fallback["coverage"])

    def test_full_1024_and_assembly_span(self):
        s = scene([surface("assembly", rect(0, 0, 1023, 1023), ramp(span=1023))], 1024, 1024)
        r = solver.solve_scene(s)
        self.assertEqual(r["report"]["coverage"]["solved"], 1024**2)
        self.assertAlmostEqual(r["heights"][512], 20*512/1023)
        self.assertEqual(r["heights"][1023], 20)

    def test_invalid_expressions_and_nonfinite_numbers(self):
        for invalid in ({"kind": "luminance"}, {"kind": "flat", "node": "missing"},
                        {"kind": "flat", "node": "base", "offset": 8}, ramp(axis=(0, 0))):
            with self.subTest(expression=invalid), self.assertRaises(solver.FormatError):
                solver.solve_scene(scene([surface("bad", rect(0, 0, 7, 7), invalid)]))
        s = scene()
        s["nodes"]["base"]["anchor"] = float("nan")
        with self.assertRaises(solver.FormatError):
            solver.solve_scene(s)


if __name__ == "__main__":
    unittest.main()
