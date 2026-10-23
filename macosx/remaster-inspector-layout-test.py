"""Source-level checks for the fixed-coordinate Metadata inspector layout.

Run with: python3 macosx/remaster-inspector-layout-test.py
"""

from pathlib import Path
import re
import unittest


SOURCE = Path(__file__).with_name("mac-os.mm").read_text()


def rects(source):
    return [tuple(map(int, match)) for match in re.findall(
        r"NSMakeRect\(\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\)", source)]


def intersects(a, b):
    x, y, w, h = a
    bx, by, bw, bh = b
    return x < bx + bw and bx < x + w and y < by + bh and by < y + h


class MetadataLayoutTests(unittest.TestCase):
    def test_layer_visibility_preserves_debug_view_and_lighting(self):
        cycle = SOURCE.split('- (NSString *)cycleRemasterLayerView\n', 1)[1].split('- (NSString *)remasterLayerViewName', 1)[0]
        self.assertNotIn('remasterLightingEnabled =', cycle)
        self.assertNotIn('S9xRemasterSetDebugMode(', cycle)
        self.assertNotIn('remasterReplayDebugMode =', cycle)
        toggle = SOURCE.split('- (BOOL)toggleRemasterLighting\n', 1)[1].split('- (NSString *)cycleRemasterLightingView', 1)[0]
        gi = SOURCE.split('- (NSString *)cycleRemasterLightingView\n', 1)[1].split('- (NSString *)captureRemasterTileInventory', 1)[0]
        self.assertNotIn('if (remasterLayerView)', toggle)
        self.assertNotIn('if (remasterLayerView)', gi)
        self.assertIn('retainedPacket ? remasterReplayDebugMode : RemasterDebugMode::Original', SOURCE)
        self.assertIn('DrawRemasterFrame(frame, inspectionDebugMode, nullptr, remasterLightingEnabled, remasterLightingView)', SOURCE)
        self.assertIn('remasterReplayDebugMode = inspectionDebugMode', SOURCE)

    def test_layer_shortcuts_and_fresh_frame_picking(self):
        menu = Path(__file__).with_name("Snes9x").joinpath("AppDelegate.m").read_text()
        self.assertRegex(menu, r'action:@selector\(toggleRemasterLighting:\)\s+keyEquivalent:@"s"')
        self.assertRegex(menu, r'action:@selector\(cycleRemasterLayerView:\) keyEquivalent:@"k"')
        self.assertRegex(menu, r'action:@selector\(cycleRemasterDebugMode:\)\s+keyEquivalent:@""')
        action = SOURCE.split('- (NSString *)cycleRemasterLayerView\n', 1)[1].split('- (NSString *)showRemasterTileTypes', 1)[0]
        self.assertIn('requestedLayerView.store(remasterLayerView)', action)
        self.assertIn('remasterFramePresenting = false', action)
        self.assertIn('remasterSelectedContextInstances.clear()', action)
        self.assertIn('remasterLayerRefreshPending.store(true)', action)
        self.assertIn('S9xRemasterSetProfile(remasterEditingProfile)', action)
        self.assertIn('remasterCaptureSession.fetch_add(1)', action)
        self.assertIn('remasterPausedCapturePending.store(false)', action)
        pick = SOURCE.split('- (BOOL)selectRemasterPixelAtViewPoint:', 1)[1]
        self.assertIn('remasterLayerRefreshPending.load()) return YES', pick)
        renderer = Path(__file__).parents[1].joinpath("gfx.cpp").read_text()
        self.assertIn('S9xRemasterInspectionLayers(Memory.FillRAM[0x212c]', renderer)
        self.assertIn('!S9xRemasterState().activeLayerView && S9xRemasterFramePacketActive()', renderer)
        runtime = Path(__file__).parents[1].joinpath("remaster/runtime/remaster.h").read_text()
        self.assertIn('!state.activeLayerView && frame.scenePixels.empty() && heldLamp', runtime)

    def test_inspector_reports_exact_occurrence_tile_coordinates(self):
        section = SOURCE.split('text << "Tile hash   v"', 1)[1].split('text << "Match       "', 1)[0]
        self.assertIn('!remasterNpcPicking', section)
        self.assertIn('instance->source == RemasterSourceType::Background', section)
        self.assertIn('instance->sourceIndex <= 1', section)
        self.assertIn('instance - remasterReplayFrame.tileInstances.data() + 1', section)
        self.assertIn('remasterReplayFrame.contextLocations.find(id)', section)
        self.assertIn('location != remasterReplayFrame.contextLocations.end()', section)
        self.assertIn('at.area < 320 ? "Room tile   (" : "World tile  ("', section)
        self.assertIn('at.x << ", " << at.y', section)
        self.assertIn('unsigned(at.background + 1)', section)
        self.assertIn('[0-based, 8px tiles]', section)
        self.assertIn('Unavailable (no captured location)', section)

    def test_rotation_buttons_fit_and_use_single_undo(self):
        controls = SOURCE.split('NSArray<NSString *> *clipboardTitles =', 1)[1].split('NSMutableArray<NSButton *> *shiftButtons', 1)[0]
        frames = rects(controls)
        self.assertEqual(len(frames), 8)
        for i, frame in enumerate(frames):
            self.assertLessEqual(frame[0] + frame[2], 740)
            for other in frames[i + 1:]:
                self.assertFalse(intersects(frame, other))
        self.assertEqual(controls.count('@selector(rotateRemasterLayer:)'), 2)
        self.assertIn('button.tag = 1;', controls)
        action = SOURCE.split('- (void)rotateRemasterLayer:(id)sender\n', 1)[1].split('- (void)shiftRemasterLayer:', 1)[0]
        self.assertIn('RemasterEffectiveHeightPreview()', action)
        self.assertIn('remasterObstacleGroupingActive', action)
        self.assertIn('std::set<RemasterTileContentId> unique', action)
        self.assertIn('!RemasterSelectedContext(tileId)', action)
        self.assertIn('CopyRemasterMetadataLayer(rotated, destination, layer)', action)
        self.assertIn('S9xRemasterRotateNormalLayer(g.normalXyz, clockwise, previewFlip)', action)
        self.assertEqual(action.count('registerUndoWithTarget:'), 1)
        self.assertIn('UpdateRemasterEditingProfileDirty(current)', action)
        self.assertIn('showRemasterVariantAtIndex:', action)
        refresh = SOURCE.split('- (void)refreshRemasterEditingControls\n', 1)[1]
        self.assertIn('remasterRotateLayerClockwiseButton.hidden = remasterRotateLayerCounterclockwiseButton.hidden = grouping;', refresh)
        self.assertIn('remasterRotateLayerClockwiseButton.enabled = remasterRotateLayerCounterclockwiseButton.enabled = canFlip;', refresh)

    def test_tile_height_buttons_edit_full_selection_with_one_undo(self):
        action = SOURCE.split("- (void)stepRemasterTileHeight:(NSButton *)sender\n", 1)[1].split("- (void)applyRemasterHeightToAnimation:", 1)[0]
        self.assertIn("RemasterEffectiveHeightPreview()", action)
        self.assertIn("remasterSelectedTiles.empty()", action)
        self.assertIn("S9xRemasterStepSelectedTileHeights(remasterEditingProfile, remasterSelectedTiles,", action)
        self.assertIn("remasterGeometryFacing, remasterTileContextName", action)
        self.assertNotIn("assets[remasterSelectedTile]", action)
        self.assertEqual(action.count("registerUndoWithTarget:"), 1)
        self.assertIn("UpdateRemasterEditingProfileDirty(current)", action)
        self.assertIn("showRemasterVariantAtIndex:", action)
        self.assertEqual(SOURCE.count("every height sample on all selected tiles"), 4)

    def test_group_controls_fit_without_scaling_or_overlap(self):
        section = SOURCE.split("remasterObstacleView = [[NSView alloc]", 1)[1]
        section = section.split("remasterObstacleView.hidden = YES;", 1)[0]
        self.assertNotIn("remasterObstacleView.bounds =", section)
        frame, *controls = rects(section)
        x, y, width, height = frame
        self.assertLessEqual(y + height, 232)  # Shared tile-action row.
        for i, control in enumerate(controls):
            cx, cy, cw, ch = control
            self.assertGreaterEqual(cx, 0)
            self.assertGreaterEqual(cy, 0)
            self.assertLessEqual(cx + cw, width)
            self.assertLessEqual(cy + ch, height)
            for other in controls[i + 1:]:
                self.assertFalse(intersects(control, other), (control, other))

    def test_shift_buttons_are_hidden_only_in_metadata(self):
        refresh = SOURCE.split("- (void)refreshRemasterEditingControls\n", 1)[1]
        loop = refresh.split("for (NSButton *button in remasterShiftLayerButtons)", 1)[1]
        loop = loop.split("}", 1)[0]
        self.assertIn("button.hidden = layer == RemasterEditorMetadata || grouping;", loop)

    def test_footprint_uses_actual_view_size(self):
        refresh = SOURCE.split("- (void)refreshRemasterObstacleControls\n", 1)[1]
        refresh = refresh.split("- (void)refreshRemasterEditingControls", 1)[0]
        self.assertIn("NSWidth(remasterObstacleFootprint.bounds)", refresh)
        self.assertIn("NSHeight(remasterObstacleFootprint.bounds)", refresh)
        self.assertIn("footprintHeight-(displayMember.y-minY+1)*size", refresh)
        self.assertIn("S9xRemasterObstacleEditorDisplayLayout", refresh)
        self.assertIn("&placementFlip", refresh)

    def test_group_focus_uses_exact_footprints_not_last_hash(self):
        refresh = SOURCE.split("- (void)refreshRemasterObstacleControls\n", 1)[1]
        refresh = refresh.split("- (void)refreshRemasterEditingControls", 1)[0]
        self.assertIn("S9xRemasterObstacleEditorFindGroup(remasterReplayFrame", refresh)
        self.assertIn("!remasterObstacleSelection.empty() && groupIndex==-1", refresh)
        self.assertIn("if (groupIndex==-2)", refresh)
        self.assertIn("if (remasterObstacleExplicitMembers)", refresh)
        self.assertIn("remasterEditingProfile.obstacleGroups[g].name==remasterObstacleFocusedGroup", refresh)

    def test_sprite_metadata_keeps_occurrences_and_source_kind(self):
        picking = SOURCE.split("- (BOOL)selectRemasterPixelAtViewPoint:", 1)[1]
        picking = picking.split("if (!remasterInspectorPanel)", 1)[0]
        self.assertIn("RemasterEditorObstacleGrouping", picking)
        self.assertIn("S9xRemasterObstacleEditorPick(remasterObstacleSelection", picking)
        self.assertLess(picking.index("if (selectingObstacle)"), picking.index("else if (remasterSelectLocations"))
        self.assertIn("for (const auto &cell : remasterObstacleSelection)", picking)

    def test_grouping_is_separate_and_transfer_does_not_clear_picks(self):
        self.assertIn('@"Metadata", @"Doorframe", @"Obstacle Grouping"', SOURCE)
        refresh = SOURCE.split("- (void)refreshRemasterEditingControls\n", 1)[1]
        self.assertIn("remasterObstacleView.hidden = !grouping || remasterNpcInspectorActive", refresh)
        self.assertIn("remasterContextSelectionMode.enabled = !grouping", refresh)
        self.assertIn("view.hidden = grouping", refresh)
        change = SOURCE.split("- (void)changeRemasterLayer:(id)sender\n", 1)[1].split("- (void)updateRemasterSemantics", 1)[0]
        self.assertIn("S9xRemasterObstacleEditorSelectionInstances", change)
        self.assertIn("for (const auto &cell : remasterObstacleSelection)", change)
        self.assertNotIn("changeRemasterContextSelection:", change)
        self.assertNotIn("CompleteSpriteFootprint", change)
        self.assertNotIn("RemasterFrameOccurrences", change)

    def test_grouping_highlights_instances_not_hashes(self):
        renderer = Path(__file__).with_name("mac-render.mm").read_text()
        branch = renderer.split("if (frame.obstacleLocationSelection)", 1)[1].split("else if", 1)[0]
        self.assertIn("selectedObstacleInstances.count", branch)
        self.assertIn("S9xRemasterFrameVisibleInstanceAt", branch)
        self.assertNotIn("FrameOccurrences", branch)

    def test_context_dropdown_retains_independent_click_evidence(self):
        change = SOURCE.split("- (void)changeRemasterContextSelection:(id)sender\n", 1)[1].split("- (void)assignRemasterLocationContext", 1)[0]
        self.assertNotIn("remasterObstacleSelection.clear()", change)
        self.assertIn("if (remasterObstacleGroupingActive) return", change)

    def test_grouping_cannot_paint_or_reset_metadata(self):
        for method in ("resetRemasterLayer", "copyRemasterLayer", "pasteRemasterLayer"):
            body = SOURCE.split(f"- (void){method}:(id)sender\n", 1)[1].split("\n}", 1)[0]
            self.assertIn("if (remasterObstacleGroupingActive) return", body)
        for method in ("beginRemasterTileStrokeAtPoint", "continueRemasterTileStrokeAtPoint"):
            body = SOURCE.split(f"- (void){method}:(NSPoint)point\n", 1)[1].split("\n}", 1)[0]
            self.assertIn("layer == RemasterEditorObstacleGrouping", body)
        variants = SOURCE.split("const std::vector<RemasterTileContentId> previousSelection = remasterSelectedTiles;", 1)[1].split("const bool metadataChanged", 1)[0]
        self.assertIn("if (!grouping && selected !=", variants)
        self.assertIn("else if (!grouping &&", variants)

    def test_new_packets_and_resume_discard_occurrence_evidence(self):
        opened = SOURCE.split("remasterSceneFrame = std::move(frame);", 1)[1].split("return nil;", 1)[0]
        self.assertIn("remasterObstacleSelection.clear()", opened)
        resume = SOURCE.split("- (void)resume\n", 1)[1].split("- (NSArray", 1)[0]
        self.assertIn("remasterObstacleSelection.clear()", resume)
        self.assertIn("remasterReplayFrame = RemasterFrame()", resume)

    def test_sprite_group_creation_promotes_profile_schema(self):
        create = SOURCE.split("- (void)createRemasterObstacleGroup:(id)sender\n", 1)[1]
        create = create.split("- (void)selectRemasterObstacleMember:", 1)[0]
        self.assertIn("S9xRemasterObstacleEditorBuild(remasterObstacleSelection", create)
        self.assertIn("group.spriteAssembly ? 30u : 22u", create)

    def test_support_contact_has_immediate_bulk_action_and_mixed_display(self):
        self.assertIn("remasterObstacleContact.allowsMixedState = YES;", SOURCE)
        self.assertIn("remasterObstacleContact.target = self;", SOURCE)
        self.assertIn("remasterObstacleContact.action = @selector(changeRemasterObstacleContact:);", SOURCE)
        action = SOURCE.split("- (void)changeRemasterObstacleContact:(id)sender\n", 1)[1]
        action = action.split("- (void)applyRemasterObstacleMember:", 1)[0]
        self.assertIn("[self remasterObstacleContactMembers:members error:error]", action)
        self.assertIn("S9xRemasterObstacleEditorSetContacts(group,members,contact,error)", action)
        self.assertIn("S9xRemasterObstacleEditorContactIntent(group,members,contact,error)", action)
        self.assertNotIn("[sender state]", action)
        self.assertIn("Cannot change support contacts", action)
        self.assertIn("commitRemasterObstacleProfile:candidate", action)
        self.assertIn("[self refreshRemasterObstacleControls]", action)
        self.assertNotIn("group.name =", action)
        self.assertNotIn(".z =", action)
        refresh = SOURCE.split("- (void)refreshRemasterObstacleControls\n", 1)[1]
        self.assertIn("anyContact && anyNonContact ? NSControlStateValueMixed", refresh)

    def test_apply_member_updates_selected_z_without_touching_contacts(self):
        action = SOURCE.split("- (void)applyRemasterObstacleMember:(id)sender\n", 1)[1]
        action = action.split("- (void)deleteRemasterObstacleGroup:", 1)[0]
        self.assertIn("S9xRemasterObstacleEditorSetMemberZ(group,members,int(z)", action)
        self.assertIn("remasterObstacleContactMembers:members", action)
        self.assertIn("S9xRemasterObstacleEditorPrepareContactSelection", action)
        self.assertIn("Add Missing Occurrences and Set Z", action)
        self.assertNotIn(".supportContact =", action)
        self.assertNotIn("remasterObstacleContact.state", action)

    def test_paused_capture_transfers_owned_packet_and_pinned_evidence(self):
        producer = SOURCE.split("if (remasterPausedCapturePending.exchange(false))", 1)[1].split("usleep(Settings.FrameTime)", 1)[0]
        self.assertIn("std::make_shared<RemasterFrame>(*frame)", producer)
        self.assertIn("completedDungeonEvidence", producer)
        self.assertIn("S9xRemasterWriteDungeonCaptureEvidence(written", producer)
        self.assertIn("retainedPacket:pausedPacket", producer)
        opened = SOURCE.split("retainedPacket:(std::shared_ptr<RemasterFrame>)retainedPacket\n", 1)[1].split("const std::pair<int, int> heightRange", 1)[0]
        self.assertIn("if (retainedPacket) frame = *retainedPacket", opened)
        self.assertIn("if (!retainedPacket)", opened)

    def test_dropdown_and_cmd_footprint_select_explicit_occurrences(self):
        action = SOURCE.split("- (void)selectRemasterObstacleMember:(id)sender\n", 1)[1]
        action = action.split("- (void)changeRemasterObstacleContact:", 1)[0]
        self.assertIn("NSEventModifierFlagCommand", action)
        self.assertIn("remasterObstacleExplicitMembers = true", action)
        self.assertIn("remasterObstacleSelectedMembers.erase(selected)", action)
        self.assertIn("S9xRemasterObstacleEditorResolveSelection", action)
        self.assertNotIn("members.push_back(remasterObstacleMemberIndex)", action)

    def test_bulk_contact_uses_existing_undo_dirty_and_live_commit(self):
        commit = SOURCE.split("- (BOOL)commitRemasterObstacleProfile:", 2)[2]
        commit = commit.split("- (void)createRemasterObstacleGroup:", 1)[0]
        self.assertIn("registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)", commit)
        self.assertIn("UpdateRemasterEditingProfileDirty(after)", commit)
        self.assertIn("SyncRemasterEditingMetadataToFrame()", commit)
        self.assertIn("S9xRemasterSetProfile(remasterEditingProfile)", commit)

    def test_missing_repeats_require_confirmation_before_single_commit(self):
        action = SOURCE.split("- (void)changeRemasterObstacleContact:(id)sender\n", 1)[1]
        action = action.split("- (void)applyRemasterObstacleMember:", 1)[0]
        self.assertIn("!valid && remasterEditingProfileLoaded && !remasterObstacleExplicitMembers", action)
        self.assertIn("S9xRemasterObstacleEditorPrepareContactSelection", action)
        self.assertIn("Add Missing Occurrences and Set Contacts", action)
        self.assertIn("if ([alert runModal]!=NSAlertFirstButtonReturn) { [self refreshRemasterObstacleControls]; return; }", action)
        self.assertLess(action.index("S9xRemasterObstacleEditorSetContacts"), action.index("if (valid && needsCompletion)"))
        self.assertLess(action.index("if (valid && needsCompletion)"), action.index("commitRemasterObstacleProfile:candidate"))
        self.assertEqual(action.count("commitRemasterObstacleProfile:candidate"), 1)
        self.assertNotIn("showRemasterObstacleGroup:", action)
        refresh = SOURCE.split("- (void)refreshRemasterObstacleControls\n", 1)[1]
        self.assertIn("contactProposal.members[i].supportContact", refresh)
        self.assertIn("add missing repeats…", refresh)

    def test_contact_rejection_is_visible_and_restores_saved_checkbox(self):
        action = SOURCE.split("- (void)changeRemasterObstacleContact:(id)sender\n", 1)[1].split("- (void)applyRemasterObstacleMember:", 1)[0]
        rejection = action.split("if (!valid) {", 1)[1].split("} else", 1)[0]
        self.assertLess(rejection.index("refreshRemasterObstacleControls"), rejection.index("remasterObstacleStatus.stringValue"))
        self.assertIn("remasterObstacleStatus.toolTip", rejection)
        self.assertIn("[alert runModal]; return;", rejection)
        resolve = SOURCE.split("- (BOOL)remasterObstacleContactMembers:", 2)[-1].split("- (void)changeRemasterObstacleContact:", 1)[0]
        for message in ("Load an editable profile", "No saved obstacle group", "ambiguous or stale", "focused obstacle group changed"):
            self.assertIn(message, resolve)
        refresh = SOURCE.split("- (void)refreshRemasterObstacleControls\n", 1)[1].split("- (void)refreshRemasterEditingControls", 1)[0]
        self.assertIn("remasterObstacleContact.state=NSControlStateValueOff", refresh)
        self.assertIn("if (!contactSelectionValid) remasterObstacleStatus.stringValue=@(contactError.c_str())", refresh)
        self.assertIn("remasterEditingProfile.obstacleGroups[g].name==remasterObstacleFocusedGroup", refresh)


if __name__ == "__main__":
    unittest.main()
