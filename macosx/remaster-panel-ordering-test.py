"""Guard normal, non-hiding ordering for the two remaster settings panels."""

from pathlib import Path
import re
import unittest


SOURCE = Path(__file__).with_name("mac-os.mm").read_text()


class RemasterPanelOrderingTests(unittest.TestCase):
    def test_settings_panels_keep_normal_ordering_when_app_deactivates(self):
        for panel in ("remasterProfileSettingsPanel", "remasterAreaPanel"):
            with self.subTest(panel=panel):
                for prop, expected in (
                    ("hidesOnDeactivate", "NO"),
                    ("floatingPanel", "NO"),
                    ("level", "NSNormalWindowLevel"),
                ):
                    assignments = re.findall(
                        rf"\b{panel}\.{prop}\s*=\s*([^;]+);", SOURCE
                    )
                    self.assertEqual(assignments, [expected])


if __name__ == "__main__":
    unittest.main()
