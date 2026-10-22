"""Check Profile Controls spacing after its two legacy row shifts."""

from pathlib import Path
import re
import unittest


SOURCE = Path(__file__).with_name("mac-os.mm").read_text()
SECTION = SOURCE.split("- (void)showRemasterProfileSettings\n", 1)[1].split(
    "- (void)showRemasterAreaSettings\n", 1)[0]


def frame(name):
    match = re.search(rf"{name} = .*?NSMakeRect\(([^)]+)\)", SECTION)
    return tuple(map(int, match[1].split(",")))


class ProfileControlsLayoutTests(unittest.TestCase):
    def test_saved_placement_cannot_restore_obsolete_panel_size(self):
        capture = SECTION.index("const NSSize layoutSize = remasterProfileSettingsPanel.frame.size;")
        restore = SECTION.index("S9xInitializeWindowPlacement(remasterProfileSettingsPanel")
        resize = SECTION.index("restoredFrame.size = layoutSize;")
        apply = SECTION.index("[remasterProfileSettingsPanel setFrame:restoredFrame display:NO];")
        self.assertLess(capture, restore)
        self.assertLess(restore, resize)
        self.assertLess(resize, apply)
        self.assertIn("restoredFrame.origin.y += restoredFrame.size.height - layoutSize.height;", SECTION)

    def test_profile_heading_has_its_own_row(self):
        _, y, _, height = frame("reflectanceTitle")
        patch_shift = int(re.search(r"rect.origin.y \+= (\d+); view.frame", SECTION)[1])
        metrics_shift = int(re.search(r"else if .*?rect.origin.y \+= (\d+)", SECTION)[1])
        _, label_y, _, label_height = frame("remasterLoadedProfileLabel")
        self.assertGreaterEqual(label_y - (y + patch_shift + metrics_shift + height), 12)
        _, _, _, panel_height = frame("remasterProfileSettingsPanel")
        self.assertLessEqual(label_y + label_height, panel_height - 12)

    def test_metrics_text_stays_below_checkbox(self):
        _, text_y, _, text_height = frame("remasterMetricsText")
        _, checkbox_y, _, _ = frame("remasterMetricsButton")
        growth = int(re.search(r"if \(view == remasterMetricsText\) rect.size.height \+= (\d+)", SECTION)[1])
        shift = int(re.search(r"else if .*?rect.origin.y \+= (\d+)", SECTION)[1])
        self.assertGreaterEqual(checkbox_y + shift - (text_y + text_height + growth), 12)


if __name__ == "__main__":
    unittest.main()
