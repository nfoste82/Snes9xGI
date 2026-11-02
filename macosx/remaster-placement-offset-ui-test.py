"""Source contracts only; not a GUI shortcut/appearance claim."""
from pathlib import Path
import unittest

class OffsetsUI(unittest.TestCase):
    def test_shortcut_and_close(self):
        source = Path("macosx/Snes9x/AppDelegate.m").read_text()
        item = source.split('NSMenuItem *offsetsItem =', 1)[1].split('[viewMenuItem.submenu addItem:offsetsItem]', 1)[0]
        self.assertIn('keyEquivalent:@"w"', item)
        self.assertIn('NSEventModifierFlagCommand | NSEventModifierFlagShift', item)
        self.assertIn('@selector(toggleRemasterHeightOffsets:)', item)
        xib = Path("macosx/Snes9x/Base.lproj/MainMenu.xib").read_text()
        self.assertIn('title="Close" keyEquivalent="w"', xib)
        self.assertIn('selector="performClose:"', xib)

    def test_no_emulation_and_restore(self):
        source = Path("macosx/mac-os.mm").read_text()
        action = source.split('- (void)toggleRemasterHeightOffsets\n', 1)[1].split('- (NSString *)captureRemasterTileInventory', 1)[0]
        self.assertNotIn('S9xMainLoop(', action)
        self.assertIn('remasterLightingView=previousView', action)
        self.assertIn('remasterLightingEnabled=previousEnabled', action)
        self.assertIn('remasterDebugLightRedraw.store', action)
        redraw = source.split('const uint32_t lightRedraw =', 1)[1].split('usleep(Settings.FrameTime)', 1)[0]
        self.assertIn('S9xRemasterCompletedFrame()', redraw)
        self.assertIn('S9xRemasterPreviewPlacementOffsets(profile,preview)', redraw)
        self.assertIn('state.profilePending?state.requestedProfile:state.activeProfile', redraw)
        self.assertNotIn('S9xMainLoop(', redraw)

    def test_editor_wires(self):
        source = Path('macosx/mac-os.mm').read_text()
        self.assertIn('@"Height", @"Height Offset", @"Normal"', source)
        self.assertIn('itemAtIndex:RemasterEditorHeightOffset].hidden=!worldScene', source)
        self.assertIn('remasterOffsetInput.action=@selector(editRemasterPlacementOffset:)', source)
        action = source.split('- (void)editRemasterPlacementOffset:(id)sender\n',1)[1].split('- (void)changeRemasterDoorwayControls',1)[0]
        self.assertIn('S9xRemasterEditPlacementOffsets(', action)
        self.assertIn('S9xRemasterParsePlacementScalar(', action)
        self.assertIn('restoreRemasterProfileFromText:', action)
        self.assertEqual(action.count('S9xRemasterSetProfile('), 1)
        self.assertNotIn('S9xMainLoop(', action)
        self.assertNotIn('writeTo', action)

    def test_exact_selected_representative(self):
        source = Path('macosx/mac-os.mm').read_text()
        pick = source.split('- (BOOL)selectRemasterPixelAtViewPoint:',1)[1].split('- (void)showRemasterVariantAtIndex:',1)[0]
        self.assertIn('S9xRemasterPickPlacementOffset(', pick)
        preview = source.split('- (void)showRemasterVariantAtIndex:(size_t)index\n',1)[1].split('- (void)changeRemasterLayer:',1)[0]
        self.assertIn('S9xRemasterPlacementRepresentative(', preview)
        self.assertIn('S9xRemasterPlacementLocalGeometry(', preview)
        self.assertIn('RemasterOffsetCategory()?remasterHeightInstanceId:S9xRemasterInspectorHeightInstance', preview)
        self.assertIn('remasterTilePreview.image=nil;remasterArtworkPreview.image=nil;', preview)
        self.assertIn('!RemasterOffsetCategory() && remasterVariants.size() > 1', preview)
        self.assertIn('remasterSelectedTile=selected.tileId', preview)
        renderer = Path('macosx/mac-render.mm').read_text()
        self.assertIn('if (frame.placementOffsetSelection)', renderer)
        self.assertIn('S9xRemasterPlacementOffsetPick(frame,n)', renderer)

    def test_type_location_control_and_transaction_scope(self):
        source = Path('macosx/mac-os.mm').read_text()
        refresh = source.split('const bool offsetCategory=RemasterOffsetCategory();',1)[1].split('remasterNpcSaveButton.enabled',1)[0]
        self.assertIn('remasterContextSelectionMode.enabled=YES', refresh)
        self.assertIn('selectItemAtIndex:remasterOffsetLocations?1:0', refresh)
        self.assertIn('remasterTilePreview.enabled=remasterArtworkPreview.enabled=NO', refresh)
        toggle = source.split('- (void)changeRemasterContextSelection:(id)sender\n',1)[1].split('- (void)assignRemasterLocationContext:',1)[0]
        offset = toggle.split('if (remasterObstacleGroupingActive)',1)[0]
        self.assertIn('remasterOffsetSelection.clear();remasterOffsetTypes.clear()', offset)
        self.assertNotIn('remasterSelectLocations=', offset)
        self.assertNotIn('S9xRemasterSetProfile', offset)
        action = source.split('- (void)editRemasterPlacementOffset:(id)sender\n',1)[1].split('- (void)changeRemasterDoorwayControls',1)[0]
        self.assertLess(action.index('S9xRemasterRefreshPlacementSelection'), action.index('S9xRemasterEditPlacementOffsets'))
        self.assertIn('selected visible BG2 locations', source)
        self.assertIn('Mixed values.', source)

if __name__ == '__main__':
    unittest.main()
