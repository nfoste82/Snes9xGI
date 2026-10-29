"""Check Profile Controls spacing and removal of the retired patch-size row."""

from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest


SOURCE = Path(__file__).with_name("mac-os.mm").read_text()
SECTION = SOURCE.split("- (void)showRemasterProfileSettings\n", 1)[1].split(
    "- (void)showRemasterAreaSettings\n", 1)[0]


def frame(name):
    match = re.search(rf"{name} = .*?NSMakeRect\(([^)]+)\)", SECTION)
    return tuple(map(int, match[1].split(",")))


class ProfileControlsLayoutTests(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("S9X_PROFILE_CONTROLS_NATIVE_TEST") == "1",
                         "opt-in isolated AppKit fixture; never launches the emulator")
    def test_native_scrolling_resize_and_screen_clamp(self):
        # Compile the actual production helpers and panel subclass, not a parallel
        # model. No orderFront, game, profile, defaults or placement observers.
        production = SOURCE.split("// Keep viewport geometry independent of the full, unscaled controls document.\n", 1)[1].split(
            "@interface S9xRemasterTileView", 1)[0]
        fixture = r'''
#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>
static void check(BOOL ok, const char *message) {
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}
PRODUCTION
int main() { @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
    for (NSValue *value in @[[NSValue valueWithRect:NSMakeRect(100, 70, 1440, 800)],
                             [NSValue valueWithRect:NSMakeRect(-1600, 120, 1280, 720)]]) {
        NSRect visible = value.rectValue;
        for (NSValue *saved in @[[NSValue valueWithRect:NSMakeRect(9000, -5000, 430, 1300)],
                                 [NSValue valueWithRect:NSMakeRect(-9000, 9000, 12, 12)],
                                 [NSValue valueWithRect:NSMakeRect(150, 100, 600, 500)]]) {
            NSRect result = S9xProfileControlsConstrainedFrame(saved.rectValue, visible);
            check(NSContainsRect(visible, result), "saved viewport inside nonzero visibleFrame");
            check(NSWidth(result) >= 450 && NSHeight(result) >= 342, "minimum viewport size");
        }
    }
    S9xRemasterProfilePanel *panel = [[S9xRemasterProfilePanel alloc]
        initWithContentRect:NSMakeRect(0, 0, 450, 760)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    NSView *content = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 430, 1278)];
    content.autoresizingMask = NSViewNotSizable;
    // Independent terminal geometry from the fixed layout, including its shifts.
    NSView *heading = [[NSView alloc] initWithFrame:NSMakeRect(20, 1228, 390, 24)];
    NSTextField *metrics = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 43, 390, 221)];
    NSButton *save = [[NSButton alloc] initWithFrame:NSMakeRect(300, 12, 110, 30)];
    [content addSubview:heading]; [content addSubview:metrics]; [content addSubview:save];
    NSScrollView *scroll = S9xProfileControlsInstallScrollView(panel, content);
    for (NSValue *size in @[[NSValue valueWithSize:NSMakeSize(450, 320)],
                            [NSValue valueWithSize:NSMakeSize(450, 760)],
                            [NSValue valueWithSize:NSMakeSize(650, 900)]]) {
        [panel setContentSize:size.sizeValue];
        [scroll tile];
        check(NSEqualSizes(scroll.frame.size, panel.contentView.bounds.size), "scroll viewport autoresizes");
        check(NSEqualSizes(content.frame.size, NSMakeSize(430, 1278)), "document never scales");
        check(NSWidth(scroll.contentView.bounds) >= 430, "legacy scrollbar does not clip right labels");
        check(!scroll.hasHorizontalScroller && scroll.hasVerticalScroller, "vertical scrollbar exists");
        [content scrollPoint:NSMakePoint(0, NSHeight(content.bounds))];
        check(NSContainsRect(scroll.documentVisibleRect, heading.frame), "top heading reachable");
        [content scrollPoint:NSZeroPoint];
        check(NSContainsRect(scroll.documentVisibleRect, metrics.frame), "all metrics reachable");
        check(NSContainsRect(scroll.documentVisibleRect, save.frame), "save button reachable");
        metrics.stringValue = @"Refreshed metrics"; save.enabled = NO;
        check(NSMinY(scroll.documentVisibleRect) == 0, "refresh retains user scroll position");
    }
    [panel setFrame:NSMakeRect(9000, -5000, 450, 1300) display:NO];
    check(NSContainsRect(panel.screen.visibleFrame, panel.frame), "late oversized restore clamped");
    [panel cancelOperation:nil];
    puts("PASS: native scroll reachability, resize, late restore, nonzero screen origins");
} return 0; }
'''.replace("PRODUCTION", production)
        with tempfile.TemporaryDirectory(prefix="profile-controls-scroll-") as directory:
            source = Path(directory) / "fixture.mm"
            binary = Path(directory) / "fixture"
            source.write_text(fixture)
            subprocess.run(["xcrun", "clang++", "-fobjc-arc", "-framework", "Cocoa",
                            str(source), "-o", str(binary)], check=True, timeout=60)
            result = subprocess.run([str(binary)], check=True, capture_output=True,
                                    text=True, timeout=20)
            self.assertIn("PASS: native scroll reachability", result.stdout)

    def test_profile_default_labels(self):
        self.assertIn('@"Representative Emitters Per Tile [Default]"', SECTION)
        self.assertIn('@"Original Scene RGB Contribution [Default]"', SECTION)
        self.assertGreaterEqual(frame("originalTitle")[2], 370)

    def test_offscreen_controls_and_independent_runtime_gate(self):
        self.assertIn('@"Offscreen Representative Emitters Per Tile [Default]"', SECTION)
        self.assertNotIn("remasterOffscreenRepresentativeDefault", SOURCE)
        self.assertNotIn("Use visible setting (effective per area)", SECTION)
        self.assertIn("notification.object == remasterOffscreenRepresentativeInput", SOURCE)
        self.assertIn("notification.object == remasterAreaOffscreenRepresentativeInput", SOURCE)
        self.assertIn("if (offscreenChanged) S9xRemasterSetAreaOffscreenRepresentatives(remasterEditingProfile,", SOURCE)
        self.assertIn("sceneKey, storedOffscreen, remasterSavedAreaSceneSettings.count(sceneKey) != 0)", SOURCE)
        self.assertIn("const int storedOffscreen = inheritOffscreen ? -1 : int(offscreen);", SOURCE)
        self.assertIn("restoreRemasterOffscreenAreaSetting:", SOURCE)
        self.assertIn("including HUD-covered emission", SECTION)
        render = Path(__file__).with_name("mac-render.mm").read_text()
        self.assertIn("S9xRemasterHiddenDirectSamples(*frame, collapsedDepth, clusteringAvailable)", render)
        self.assertIn("const bool clustered = clusteringAvailable && frame->representativeEmitters > 0;", render)
        self.assertLess(render.index("bool adaptiveDepth ="), render.index("id<MTLFunction> directTransport ="))
        self.assertIn("[snapshot[@\"count\"] intValue], remasterSavedAreaSceneSettings.count(key) != 0)", SOURCE)
        _, enabled_y, _, _ = frame("remasterOffscreenRepresentativeEnabled")
        _, slider_y, _, slider_h = frame("remasterOffscreenRepresentativeSlider")
        self.assertGreaterEqual(enabled_y - slider_y - slider_h, 6)

    def test_escape_does_not_close_profile_controls(self):
        self.assertIn("[[S9xRemasterProfilePanel alloc]", SECTION)
        panel = SOURCE.split("@implementation S9xRemasterProfilePanel\n", 1)[1].split(
            "@end", 1)[0]
        cancel = re.search(r"- \(void\)cancelOperation:\(id\)sender\s*\{([^}]*)\}", panel)
        self.assertIsNotNone(cancel)
        self.assertEqual(re.sub(r"//[^\n]*", "", cancel[1]).strip(), "")
        self.assertIn("NSWindowStyleMaskClosable", SECTION)

    def test_offscreen_quality_is_directly_below_visible_quality(self):
        # Existing metrics expansion precedes the new quality block insertion.
        metrics_shift = int(re.search(r"else if .*?rect.origin.y \+= (\d+)", SECTION)[1])
        insertion = re.search(r"view.frame.origin.y >= (\d+)\) \{\s*NSRect rect = view.frame; rect.origin.y \+= (\d+)", SECTION)
        threshold, shift = map(int, insertion.groups())
        def profile_y(name):
            _, y, _, h = frame(name)
            y += metrics_shift
            return y + (shift if y >= threshold else 0), h
        visible_y, _ = profile_y("remasterRepresentativeSlider")
        _, hidden_y, _, hidden_h = frame("remasterOffscreenRepresentativeEnabled")
        _, hidden_slider_y, _, _ = frame("remasterOffscreenRepresentativeSlider")
        bounce_y, bounce_h = profile_y("title")
        self.assertEqual(visible_y - hidden_y - hidden_h, 16)
        self.assertGreaterEqual(hidden_slider_y - bounce_y - bounce_h, 12)

        area = SOURCE.split("- (void)showRemasterAreaSettings\n", 1)[1].split(
            "- (void)refreshRemasterAreaSettings\n", 1)[0]
        def area_frame(name):
            match = re.search(rf"{name} = .*?NSMakeRect\(([^)]+)\)", area)
            return tuple(map(int, match[1].split(",")))
        _, visible_y, _, _ = area_frame("remasterAreaRepresentativeSlider")
        _, hidden_y, _, hidden_h = area_frame("remasterAreaOffscreenRepresentativeEnabled")
        _, inherit_y, _, _ = area_frame("remasterAreaOffscreenRepresentativeDefault")
        _, boost_y, _, boost_h = area_frame("remasterAreaReflectanceSlider")
        self.assertIn("if (view.frame.origin.y >= 186)", area)
        self.assertEqual(visible_y + 90 + 100 - hidden_y - hidden_h, 16)
        self.assertGreaterEqual(inherit_y - boost_y - boost_h, 36)

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
        _, boost_y, _, boost_height = area_frame("remasterAreaReflectanceSlider")
        _, default_y, _, default_height = area_frame("remasterAreaReflectanceDefault")
        self.assertGreaterEqual(count_y + 90 - boost_y - boost_height, 12)
        self.assertGreaterEqual(boost_y - default_y - default_height, 6)
        self.assertIn('reflectanceTitle.stringValue = @"Reflected Light Boost [Default]";', SECTION)

    def test_area_boost_preserves_inheritance_and_validates_live_text(self):
        self.assertIn("notification.object == remasterAreaReflectanceInput", SOURCE)
        self.assertIn("if (boostChanged) stored.reflectanceBoost = storedBoost;", SOURCE)
        self.assertIn("const float storedBoost = inheritBoost ? -1 : scene.reflectanceBoost;", SOURCE)
        self.assertIn("scene.reflectanceBoost < 0 || scene.reflectanceBoost > 8", SOURCE)

    def test_saved_placement_cannot_restore_obsolete_panel_size(self):
        self.assertIn("NSWindowStyleMaskResizable", SECTION)
        self.assertIn("S9xProfileControlsConstrainedFrame", SOURCE)
        panel = SOURCE.split("@implementation S9xRemasterProfilePanel\n", 1)[1].split("@end", 1)[0]
        self.assertIn("- (void)setFrame:(NSRect)frame display:(BOOL)display", panel)
        self.assertIn("screen.visibleFrame", panel)
        self.assertIn("S9xProfileControlsConstrainedFrame(frame,", panel)
        self.assertNotIn("restoredFrame.size = layoutSize;", SECTION)

    def test_scroll_document_preserves_full_layout_and_initial_top(self):
        self.assertIn("S9xProfileControlsInstallScrollView(remasterProfileSettingsPanel, content)", SECTION)
        self.assertIn("NSView *content = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 430, 1166)]", SECTION)
        self.assertIn("[content setFrameSize:NSMakeSize(430, 1278)]", SECTION)
        self.assertIn("NSViewNotSizable", SECTION)
        self.assertIn("scroll.hasVerticalScroller = YES;", SOURCE)
        self.assertIn("scroll.hasHorizontalScroller = NO;", SOURCE)
        self.assertIn("scroll.scrollerStyle = NSScrollerStyleLegacy;", SOURCE)
        self.assertIn("scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;", SOURCE)
        self.assertIn("scroll.documentView = content;", SOURCE)
        self.assertIn("[content scrollPoint:NSMakePoint(0, NSHeight(content.bounds))]", SECTION)
        self.assertNotIn("NSView *content = remasterProfileSettingsPanel.contentView;", SECTION)
        # Metrics and save refresh use retained controls, not contentView traversal.
        self.assertIn("remasterMetricsText.stringValue", SOURCE)
        self.assertIn("remasterSettingsSaveButton.enabled = remasterEditingProfileDirty;", SECTION)

    def test_profile_heading_has_its_own_row(self):
        _, y, _, height = frame("reflectanceTitle")
        metrics_shift = int(re.search(r"else if .*?rect.origin.y \+= (\d+)", SECTION)[1])
        _, label_y, _, label_height = frame("remasterLoadedProfileLabel")
        self.assertGreaterEqual(label_y - (y + metrics_shift + height), 12)
        # Full document, not the deliberately shorter window viewport.
        _, _, _, panel_height = frame("content")
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
