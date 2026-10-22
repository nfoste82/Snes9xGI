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
        self.assertIn("(anyOn && anyOff) || [sender state]==NSControlStateValueOn", action)
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
        self.assertIn("!valid && !remasterObstacleExplicitMembers", action)
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


if __name__ == "__main__":
    unittest.main()
