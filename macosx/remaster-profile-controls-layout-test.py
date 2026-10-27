"""Check Profile Controls spacing and removal of the retired patch-size row."""

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
    def test_escape_does_not_close_profile_controls(self):
        self.assertIn("[[S9xRemasterProfilePanel alloc]", SECTION)
        panel = SOURCE.split("@implementation S9xRemasterProfilePanel\n", 1)[1].split(
            "@end", 1)[0]
        cancel = re.search(r"- \(void\)cancelOperation:\(id\)sender\s*\{([^}]*)\}", panel)
        self.assertIsNotNone(cancel)
        self.assertEqual(re.sub(r"//[^\n]*", "", cancel[1]).strip(), "")
        self.assertIn("NSWindowStyleMaskClosable", SECTION)

    def test_area_scene_controls_have_separate_rows_and_fixed_restore_size(self):
        area = SOURCE.split("- (void)showRemasterAreaSettings\n", 1)[1].split(
            "- (void)refreshRemasterAreaSettings\n", 1)[0]
        def area_frame(name):
            match = re.search(rf"{name} = .*?NSMakeRect\(([^)]+)\)", area)
            return tuple(map(int, match[1].split(",")))
        _, original_y, _, original_height = area_frame("remasterAreaOriginalSlider")
        _, enabled_y, _, enabled_height = area_frame("remasterAreaRepresentativeEnabled")
        _, count_y, _, count_height = area_frame("remasterAreaRepresentativeSlider")
        self.assertGreaterEqual(original_y - enabled_y - enabled_height, 12)
        self.assertGreaterEqual(enabled_y - count_y - count_height, 6)
        self.assertIn("restoredFrame.size = layoutSize;", area)
        self.assertIn("view.frame = frame;", area)

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
        metrics_shift = int(re.search(r"else if .*?rect.origin.y \+= (\d+)", SECTION)[1])
        _, label_y, _, label_height = frame("remasterLoadedProfileLabel")
        self.assertGreaterEqual(label_y - (y + metrics_shift + height), 12)
        _, _, _, panel_height = frame("remasterProfileSettingsPanel")
        self.assertLessEqual(label_y + label_height, panel_height - 12)

    def test_metrics_text_stays_below_checkbox(self):
        _, text_y, _, text_height = frame("remasterMetricsText")
        _, checkbox_y, _, _ = frame("remasterMetricsButton")
        growth = int(re.search(r"if \(view == remasterMetricsText\) rect.size.height \+= (\d+)", SECTION)[1])
        shift = int(re.search(r"else if .*?rect.origin.y \+= (\d+)", SECTION)[1])
        self.assertGreaterEqual(checkbox_y + shift - (text_y + text_height + growth), 12)

    def test_lamp_has_separate_rows_below_heading(self):
        _, heading_y, _, _ = frame("remasterLoadedProfileLabel")
        _, title_y, _, title_height = frame("lampTitle")
        _, slider_y, _, slider_height = frame("remasterLinkLampBrightnessSlider")
        self.assertGreaterEqual(heading_y - title_y - title_height, 10)
        self.assertGreaterEqual(title_y - slider_y - slider_height, 8)
        _, reflectance_y, _, reflectance_height = frame("reflectanceTitle")
        self.assertGreaterEqual(slider_y - (reflectance_y + 80 + reflectance_height), 12)

    def test_patch_size_removed_without_leaving_a_gap(self):
        self.assertNotIn("Emitter Patch Size", SOURCE)
        self.assertNotIn("remasterEmitterPatch", SOURCE)
        _, samples_y, _, _ = frame("remasterSamplesSlider")
        _, enabled_y, _, enabled_height = frame("remasterRepresentativeEnabled")
        _, representative_y, _, representative_height = frame("remasterRepresentativeSlider")
        _, bounce_y, _, bounce_height = frame("title")
        self.assertGreaterEqual(samples_y - enabled_y - enabled_height, 6)
        self.assertGreaterEqual(enabled_y - representative_y - representative_height, 4)
        self.assertGreaterEqual(representative_y - bounce_y - bounce_height, 0)


if __name__ == "__main__":
    unittest.main()
