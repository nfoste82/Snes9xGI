"""Guard the profiling action against AppDelegate's disabled-while-running default."""
import re
import unittest
from pathlib import Path


class PerformanceCaptureMenuTest(unittest.TestCase):
    def test_capture_enabled_during_gameplay(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "macosx/Snes9x/AppDelegate.m").read_text()
        validation = source.split("- (BOOL)validateMenuItem:", 1)[1].split("@dynamic", 1)[0]
        branch = re.search(
            r"else if \(action == @selector\(captureRemasterPerformance:\)\)\s*\{([^}]+)\}",
            validation,
        )
        self.assertIsNotNone(branch, "Capture must not fall through to !isRunningEmulation")
        self.assertIn("[self.s9xEngine isRunning] && ![self.s9xEngine isPaused]", branch[1])


if __name__ == "__main__":
    unittest.main()
