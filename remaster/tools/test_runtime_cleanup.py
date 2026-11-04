"""Prevent retired experiments from leaking back into the live implementation."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class RuntimeCleanupTests(unittest.TestCase):
    def test_no_historical_behavior_switches_in_runtime(self):
        for path in (ROOT / "remaster/runtime").glob("*.h"):
            self.assertNotRegex(path.read_text(), r"\bALTTP_TEST_PRIOR_", str(path))

    def test_no_unused_live_declaration_assessment(self):
        for name in ("gfx.cpp", "remaster/runtime/frame.h", "remaster/runtime/remaster.h"):
            self.assertNotRegex((ROOT / name).read_text(),
                                r"constructionDeclarations|dungeonStaticCandidates|dungeon_static_assembly.h", name)

    def test_no_removed_lighting_implementations(self):
        runtime = (ROOT / "remaster/runtime/frame.h").read_text()
        self.assertNotIn("S9xRemasterFrameEmissionLights", runtime)
        self.assertNotIn("S9xRemasterDungeonDoorTangentCap", runtime)
        offscreen = (ROOT / "remaster/runtime/offscreen_emitters.h").read_text()
        self.assertNotIn("ALTTP_TEST_LEGACY_OFFSCREEN_SPHERES", offscreen)
        shader = (ROOT / "macosx/shaders.metal").read_text()
        self.assertNotIn("remasterAccumulateSamples", shader)
        renderer = (ROOT / "macosx/mac-render.mm").read_text()
        self.assertNotRegex(renderer, r"transportSampleCount|directMean\[|accumulationEncodeMs")


if __name__ == "__main__":
    unittest.main()
