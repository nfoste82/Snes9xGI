"""Production wiring + executable deterministic producer/callback regression."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CaptureLifecycleTests(unittest.TestCase):
    def test_native_main_queue_scheduler(self):
        with tempfile.TemporaryDirectory() as out:
            binary = str(Path(out) / "native-lifecycle")
            subprocess.run(["clang++", "-std=c++14", "-pthread", "-fobjc-arc",
                            "-framework", "Foundation", "-framework", "CoreFoundation",
                            str(ROOT / "macosx/remaster-capture-ui-test.mm"),
                            "-o", binary], check=True)
            subprocess.run([binary], check=True, timeout=30)

    def test_deterministic_producer_callback(self):
        with tempfile.TemporaryDirectory() as out:
            binary = str(Path(out) / "lifecycle")
            subprocess.run(["clang++", "-std=c++14", "-pthread", "-Wall", "-Wextra",
                            str(ROOT / "remaster/tests/paused_capture_lifecycle_test.cpp"),
                            "-o", binary], check=True)
            subprocess.run([binary], check=True)

    def test_production_uses_tested_scheduler(self):
        source = (ROOT / "macosx/mac-os.mm").read_text()
        self.assertIn("remasterCaptureSession.produce(", source)
        self.assertIn("remasterCaptureSession.deliver(session", source)
        self.assertNotIn("remasterPausedCapturePending", source)
        self.assertNotIn("frameAdvance = false;\n                    S9xMainLoop", source)
        self.assertIn("frameAdvance.exchange(false)", source)
        self.assertIn("static std::atomic<bool> frameAdvance", source)
        self.assertNotIn("remasterCaptureSession.cancel()", source)
        self.assertIn("remasterCaptureSession.cancelOnUI(", source)
        advance = source.split("static void RequestRemasterFrameAdvance()", 1)[1].split("\n}", 1)[0]
        self.assertIn("remasterCaptureSession.requestAdvance(frameAdvance, DispatchRemasterCaptureUI)", advance)
        self.assertNotIn("frameAdvance.store(true)", source)
        self.assertNotIn("frameAdvance = true", source)
        self.assertEqual(source.count("RequestRemasterFrameAdvance();"), 2)
        chooser = (ROOT / "macosx/Snes9x/AppDelegate.m").read_text().split(
            "- (IBAction)openRemasterFrame:(id)sender", 1)[1].split("- (IBAction)", 1)[0]
        self.assertLess(chooser.index("cancelRemasterLiveCapture"), chooser.index("runModal"))
        for entry in ["- (void)chooseRemasterRecording:(id)sender\n",
                      "- (void)presentRemasterRecordingFrame\n",
                      "- (NSString *)openRemasterFrame:(NSURL *)fileURL retainedPacket:"]:
            body = source.split(entry, 1)[1].split("\n}", 1)[0]
            self.assertIn("cancelRemasterLiveCapture", body)


if __name__ == "__main__":
    unittest.main()
