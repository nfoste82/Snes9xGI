// S9xEngine methods, included inside its implementation in mac-os.mm.
- (void)showRemasterNpcBrowser
{
	workspaceNpcOpen = YES;
	[self savePreviousWorkspace];
	if (!remasterNpcPanel)
	{
		remasterNpcPanel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 760, 848)
			styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
			backing:NSBackingStoreBuffered defer:NO];
		remasterNpcPanel.title = @"Remaster NPC Poses";
		remasterNpcPanel.releasedWhenClosed = NO;
		remasterNpcPanel.hidesOnDeactivate = NO;
		remasterNpcPanel.delegate = self;
		NSView *content = remasterNpcPanel.contentView;
		remasterNpcTypeSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(16, 800, 540, 28) pullsDown:NO];
		remasterNpcTypeSelector.target = self; remasterNpcTypeSelector.action = @selector(changeRemasterNpcType:);
		remasterNpcTypeSelector.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
		remasterNpcTypeSelector.toolTip = @"Counts include poses across distinct artwork/palette variants. Equivalent source contexts are combined.";
		[content addSubview:remasterNpcTypeSelector];
		remasterNpcActorSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(16, 738, 728, 28) pullsDown:NO];
		remasterNpcActorSelector.toolTip = @"Graphics/palette context for the selected NPC.";
		remasterNpcActorSelector.target = self; remasterNpcActorSelector.action = @selector(changeRemasterNpcActor:);
		remasterNpcActorSelector.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
		[content addSubview:remasterNpcActorSelector];
		remasterNpcActionSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(16, 688, 728, 26) pullsDown:NO];
		remasterNpcActionSelector.target = self;
		remasterNpcActionSelector.action = @selector(changeRemasterNpcFacing:);
		remasterNpcActionSelector.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
		remasterNpcActionSelector.accessibilityLabel = @"Action";
		remasterNpcActionSelector.toolTip = @"Filter catalogued actions. Missing actions are not generated automatically; see coverage below.";
		[content addSubview:remasterNpcActionSelector];
		NSButton *load = [[NSButton alloc] initWithFrame:NSMakeRect(574, 800, 170, 28)];
		load.title = @"Load Pose Library…"; load.bezelStyle = NSBezelStyleRounded;
		load.target = self; load.action = @selector(loadRemasterNpcLibrary:);
		load.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin; [content addSubview:load];
		remasterNpcFacingSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(16, 636, 150, 28) pullsDown:NO];
		[remasterNpcFacingSelector addItemsWithTitles:@[@"All directions", @"north", @"south", @"west", @"east", @"fixed"]];
		remasterNpcFacingSelector.target = self; remasterNpcFacingSelector.action = @selector(changeRemasterNpcFacing:);
		remasterNpcFacingSelector.autoresizingMask = NSViewMinYMargin; [content addSubview:remasterNpcFacingSelector];
		remasterNpcPoseSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(180, 636, 390, 28) pullsDown:NO];
		remasterNpcPoseSelector.target = self; remasterNpcPoseSelector.action = @selector(changeRemasterNpcPose:);
		remasterNpcPoseSelector.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin; [content addSubview:remasterNpcPoseSelector];
		remasterNpcPlayButton = [[NSButton alloc] initWithFrame:NSMakeRect(590, 636, 150, 28)];
		remasterNpcPlayButton.title = @"Play Animation"; remasterNpcPlayButton.bezelStyle = NSBezelStyleRounded;
		remasterNpcPlayButton.target = self; remasterNpcPlayButton.action = @selector(toggleRemasterNpcPlayback:);
		remasterNpcPlayButton.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin; [content addSubview:remasterNpcPlayButton];
		remasterNpcDebugSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(16, 574, 240, 28) pullsDown:NO];
		[remasterNpcDebugSelector addItemsWithTitles:@[@"Debug View: Artwork", @"Debug View: Normals", @"Debug View: Height (signed local)", @"Debug View: Occlusion", @"Debug View: Emission", @"Debug View: Material"]];
		remasterNpcDebugSelector.target = self; remasterNpcDebugSelector.action = @selector(changeRemasterNpcDebugView:);
		remasterNpcDebugSelector.autoresizingMask = NSViewMinYMargin;
		remasterNpcDebugSelector.toolTip = @"Entire pose, including unsaved profile edits. Magenta means missing height/material data; normals use height-derived or flat defaults.";
		[content addSubview:remasterNpcDebugSelector];
		NSButton *selectAll = [[NSButton alloc] initWithFrame:NSMakeRect(268, 574, 142, 28)];
		selectAll.title = @"Select all tiles"; selectAll.bezelStyle = NSBezelStyleRounded;
		selectAll.target = self; selectAll.action = @selector(selectAllRemasterNpcTiles:);
		selectAll.autoresizingMask = NSViewMinYMargin;
		selectAll.toolTip = @"Select every visible tile identity in the current pose for multi-editing.";
		[content addSubview:selectAll];
		remasterNpcSaveButton = [[NSButton alloc] initWithFrame:NSMakeRect(422, 574, 322, 28)];
		remasterNpcSaveButton.title = @"Save All Changes This Session";
		remasterNpcSaveButton.font = [NSFont systemFontOfSize:11];
		remasterNpcSaveButton.bezelStyle = NSBezelStyleRounded;
		remasterNpcSaveButton.target = self;
		remasterNpcSaveButton.action = @selector(saveRemasterProfile:);
		remasterNpcSaveButton.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
		remasterNpcSaveButton.toolTip = @"Save all unsaved changes to the loaded profile, including edits made in other windows.";
		[content addSubview:remasterNpcSaveButton];
		remasterNpcHeightRampButton = [[NSButton alloc] initWithFrame:NSMakeRect(16, 538, 210, 28)];
		remasterNpcHeightRampButton.title = @"Generate Height Ramp";
		remasterNpcHeightRampButton.bezelStyle = NSBezelStyleRounded;
		remasterNpcHeightRampButton.target = self;
		remasterNpcHeightRampButton.action = @selector(generateRemasterNpcHeightRamp:);
		remasterNpcHeightRampButton.autoresizingMask = NSViewMinYMargin;
		remasterNpcHeightRampButton.toolTip = @"Fill every tile pixel: bottom row 0, increasing by 1 per row upward. Load the matching profile first. Supports Undo.";
		[content addSubview:remasterNpcHeightRampButton];
		remasterNpcHeightAllPosesButton = [[NSButton alloc] initWithFrame:NSMakeRect(238, 538, 260, 28)];
		remasterNpcHeightAllPosesButton.title = @"All catalogued poses for this NPC";
		remasterNpcHeightAllPosesButton.font = [NSFont systemFontOfSize:12];
		remasterNpcHeightAllPosesButton.buttonType = NSButtonTypeSwitch;
		remasterNpcHeightAllPosesButton.autoresizingMask = NSViewMinYMargin;
		remasterNpcHeightAllPosesButton.toolTip = @"Include every catalogued action, facing and graphics/palette context. This library is partial: unmodeled actions are not included.";
		[content addSubview:remasterNpcHeightAllPosesButton];
		remasterNpcOverwriteButton = [[NSButton alloc] initWithFrame:NSMakeRect(510, 538, 234, 28)];
		remasterNpcOverwriteButton.title = @"Overwrite authored heights/normals";
		remasterNpcOverwriteButton.font = [NSFont systemFontOfSize:12];
		remasterNpcOverwriteButton.buttonType = NSButtonTypeSwitch;
		remasterNpcOverwriteButton.state = NSControlStateValueOff;
		remasterNpcOverwriteButton.autoresizingMask = NSViewMinYMargin;
		remasterNpcOverwriteButton.toolTip = @"Unchecked: skip tiles with an existing per-pixel layer for this operation. Checked: replace that layer. Other layers are preserved.";
		[content addSubview:remasterNpcOverwriteButton];
		remasterNpcNormalControls = [[NSView alloc] initWithFrame:NSMakeRect(16, 502, 210, 64)];
		remasterNpcNormalControls.autoresizingMask = NSViewMinYMargin;
		[content addSubview:remasterNpcNormalControls];
		remasterNpcNormalSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 150, 28) pullsDown:NO];
		[remasterNpcNormalSelector addItemsWithTitles:@[@"+Z (Front)", @"-Z (Back)", @"+X (Right)", @"-X (Left)", @"+Y (Down)", @"-Y (Up)"]];
		remasterNpcNormalSelector.accessibilityLabel = @"Per-pixel normal direction";
		remasterNpcNormalSelector.toolTip = @"Canonical tile direction, matching the Pixel Inspector. Instance flips apply when displayed.";
		[remasterNpcNormalControls addSubview:remasterNpcNormalSelector];
		remasterNpcNormalApplyButton = [[NSButton alloc] initWithFrame:NSMakeRect(0, 36, 210, 28)];
		remasterNpcNormalApplyButton.title = @"Apply Normals to All Tiles";
		remasterNpcNormalApplyButton.font = [NSFont systemFontOfSize:12];
		remasterNpcNormalApplyButton.bezelStyle = NSBezelStyleRounded;
		remasterNpcNormalApplyButton.target = self;
		remasterNpcNormalApplyButton.action = @selector(applyRemasterNpcNormals:);
		remasterNpcNormalApplyButton.toolTip = @"Fill all 64 pixels of each tile with the direction below. Uses the all-poses and overwrite options on this row. Supports Undo.";
		[remasterNpcNormalControls addSubview:remasterNpcNormalApplyButton];
		NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
		[defaults registerDefaults:@{@"RemasterNpcHeightDisplayMin": @0, @"RemasterNpcHeightDisplayMax": @32}];
		remasterNpcHeightDisplayMin = [defaults doubleForKey:@"RemasterNpcHeightDisplayMin"];
		remasterNpcHeightDisplayMax = [defaults doubleForKey:@"RemasterNpcHeightDisplayMax"];
		if (!std::isfinite(remasterNpcHeightDisplayMin) || !std::isfinite(remasterNpcHeightDisplayMax) ||
			remasterNpcHeightDisplayMin < -32768 || remasterNpcHeightDisplayMax > 32767 ||
			remasterNpcHeightDisplayMax <= remasterNpcHeightDisplayMin)
		{ remasterNpcHeightDisplayMin = 0; remasterNpcHeightDisplayMax = 32; }
		remasterNpcHeightRangeView = [[NSView alloc] initWithFrame:NSMakeRect(16, 502, 480, 28)];
		remasterNpcHeightRangeView.autoresizingMask = NSViewMinYMargin;
		[content addSubview:remasterNpcHeightRangeView];
		NSTextField *minLabel = [NSTextField labelWithString:@"Min height"];
		minLabel.font = [NSFont boldSystemFontOfSize:12]; minLabel.frame = NSMakeRect(0, 5, 80, 18);
		[remasterNpcHeightRangeView addSubview:minLabel];
		NSTextField *maxLabel = [NSTextField labelWithString:@"Max height"];
		maxLabel.font = [NSFont boldSystemFontOfSize:12]; maxLabel.frame = NSMakeRect(174, 5, 80, 18);
		[remasterNpcHeightRangeView addSubview:maxLabel];
		remasterNpcHeightMinInput = [[NSTextField alloc] initWithFrame:NSMakeRect(84, 2, 70, 24)];
		remasterNpcHeightMaxInput = [[NSTextField alloc] initWithFrame:NSMakeRect(258, 2, 70, 24)];
		remasterNpcHeightMinInput.doubleValue = remasterNpcHeightDisplayMin;
		remasterNpcHeightMaxInput.doubleValue = remasterNpcHeightDisplayMax;
		for (NSTextField *input in @[remasterNpcHeightMinInput, remasterNpcHeightMaxInput])
		{
			input.target = self; input.action = @selector(changeRemasterNpcHeightRange:);
			[(NSTextFieldCell *)input.cell setSendsActionOnEndEditing:YES];
			input.toolTip = @"Display range only (-32768–32767, max greater than min). Press Return or leave the field to apply. Remembered across sessions.";
			[remasterNpcHeightRangeView addSubview:input];
		}
		[self changeRemasterNpcDebugView:nil];
		NSArray<NSString *> *selectorLabels = @[@"NPC", @"Graphics/Palette Context", @"Action", @"Facing", @"Pose", @"Debug View"];
		NSArray<NSPopUpButton *> *selectors = @[remasterNpcTypeSelector, remasterNpcActorSelector,
			remasterNpcActionSelector, remasterNpcFacingSelector, remasterNpcPoseSelector, remasterNpcDebugSelector];
		for (NSUInteger index = 0; index < selectors.count; index++)
		{
			NSPopUpButton *selector = selectors[index];
			NSTextField *label = [NSTextField labelWithString:selectorLabels[index]];
			label.font = [NSFont boldSystemFontOfSize:12];
			label.frame = NSMakeRect(NSMinX(selector.frame), NSMaxY(selector.frame) + 2,
				NSWidth(selector.frame), 18);
			label.autoresizingMask = selector.autoresizingMask;
			selector.accessibilityLabel = selectorLabels[index];
			[content addSubview:label];
		}
		remasterNpcPoseView = [[S9xRemasterNpcPoseView alloc] initWithFrame:NSMakeRect(16, 140, 728, 350)];
		remasterNpcPoseView.editor = self;
		remasterNpcPoseView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
		[content addSubview:remasterNpcPoseView];
		remasterNpcStatus = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 12, 728, 116)];
		remasterNpcStatus.editable = NO; remasterNpcStatus.selectable = YES; remasterNpcStatus.bezeled = NO;
		remasterNpcStatus.drawsBackground = NO; remasterNpcStatus.font = [NSFont systemFontOfSize:12];
		remasterNpcStatus.autoresizingMask = NSViewWidthSizable;
		remasterNpcStatus.stringValue = @"Load a generated poses.json library. Click a pose tile to select it; click a selected tile to clear selection.\nCommand-click toggles individual tiles. Select all tiles selects the current pose. Load the matching profile to edit/save.";
		[content addSubview:remasterNpcStatus];
		remasterNpcPanel.minSize = NSMakeSize(760, 708);
		S9xInitializeWindowPlacement(remasterNpcPanel, @"RemasterNpcPoses");
	}
	[remasterNpcPanel makeKeyAndOrderFront:nil];
	remasterNpcSaveButton.enabled = remasterEditingProfileLoaded && remasterEditingProfileDirty;
	if (!remasterNpcActors)
	{
		NSString *path = [[NSUserDefaults standardUserDefaults] stringForKey:@"RemasterNpcPoseLibraryPath"];
		if (!path.length || ![[NSFileManager defaultManager] fileExistsAtPath:path])
			path = [RemasterMetadataProjectRoot() stringByAppendingPathComponent:@"build/remaster-geometry/npc-workbench/poses.json"];
		if (path.length && [[NSFileManager defaultManager] fileExistsAtPath:path])
			[self readRemasterNpcLibrary:[NSURL fileURLWithPath:path]];
		else [self loadRemasterNpcLibrary:nil];
	}
}

- (void)loadRemasterNpcLibrary:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	panel.allowedFileTypes = @[@"json"];
	panel.message = @"Choose poses.json generated by tools/ALttP/alttp_npc_workbench.py --generate.";
	if ([panel runModal] == NSModalResponseOK) [self readRemasterNpcLibrary:panel.URL];
}

- (void)readRemasterNpcLibrary:(NSURL *)url
{
	NSError *error = nil;
	NSData *data = [NSData dataWithContentsOfURL:url options:0 error:&error];
	id document = data.length <= 64 * 1024 * 1024 && data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:&error] : nil;
	NSString *failure = error.localizedDescription;
	if (![document isKindOfClass:NSDictionary.class] || ![document[@"format"] isEqual:@"alttp-npc-workbench-v1"] ||
		![document[@"rom_sha256"] isEqual:@"66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb"] ||
		![document[@"actors"] isKindOfClass:NSArray.class] || [document[@"actors"] count] == 0 || [document[@"actors"] count] > 4096)
		failure = failure ?: @"Choose a supported ALTTP NPC pose manifest (poses.json).";
	if (!failure)
	{
		for (id actor in document[@"actors"])
		{
			if (![actor isKindOfClass:NSDictionary.class] || ![actor[@"name"] isKindOfClass:NSString.class] ||
				![actor[@"id"] isKindOfClass:NSString.class] || ![actor[@"poses"] isKindOfClass:NSArray.class] ||
				![actor[@"contexts"] isKindOfClass:NSArray.class] || [actor[@"poses"] count] == 0 || [actor[@"poses"] count] > 1024)
			{ failure = @"Invalid actor record in pose library."; break; }
			for (id context in actor[@"contexts"])
				if (![context isKindOfClass:NSString.class]) { failure = @"Invalid pose context."; break; }
			for (id pose in actor[@"poses"])
			{
				RemasterFrame decoded;
				if (![pose isKindOfClass:NSDictionary.class] || ![pose[@"facing"] isKindOfClass:NSString.class] ||
					![pose[@"action"] isKindOfClass:NSString.class] || ![pose[@"step"] isKindOfClass:NSNumber.class] ||
					!RemasterNpcDecodePose(pose, document[@"rom_sha256"], decoded))
				{ failure = @"Invalid pose artwork, bounds, or canonical tile identity."; break; }
			}
			if (failure) break;
		}
	}
	if (failure)
	{
		NSAlert *alert = [NSAlert new]; alert.messageText = @"Unable to Load NPC Poses";
		alert.informativeText = failure; [alert runModal]; return;
	}
	[self stopRemasterNpcPlayback];
	remasterNpcActors = document[@"actors"];
	remasterNpcRomHash = document[@"rom_sha256"];
	[[NSUserDefaults standardUserDefaults] setObject:url.path forKey:@"RemasterNpcPoseLibraryPath"];
	remasterNpcTypes = RemasterNpcGroupActors(remasterNpcActors);
	[remasterNpcTypeSelector removeAllItems];
	for (NSDictionary *type in remasterNpcTypes)
		[remasterNpcTypeSelector addItemWithTitle:[NSString stringWithFormat:@"%@ (%@ poses · %lu variants)", type[@"name"], type[@"poseCount"], (unsigned long)[type[@"actors"] count]]];
	[self changeRemasterNpcType:nil];
}

- (void)changeRemasterNpcDebugView:(id)sender
{
	const BOOL showRamp = remasterNpcDebugSelector.indexOfSelectedItem == static_cast<NSInteger>(RemasterNpcDebugView::Height);
	const BOOL showNormals = remasterNpcDebugSelector.indexOfSelectedItem == static_cast<NSInteger>(RemasterNpcDebugView::Normals);
	remasterNpcHeightRampButton.hidden = !showRamp;
	remasterNpcNormalControls.hidden = !showNormals;
	remasterNpcHeightAllPosesButton.hidden = !(showRamp || showNormals);
	remasterNpcOverwriteButton.hidden = !(showRamp || showNormals);
	remasterNpcOverwriteButton.title = showNormals ? @"Overwrite authored normals" : @"Overwrite authored heights";
	remasterNpcHeightRangeView.hidden = !showRamp;
	[remasterNpcPoseView setNeedsDisplay:YES];
}

- (void)changeRemasterNpcHeightRange:(id)sender
{
	double values[2];
	NSArray<NSTextField *> *inputs = @[remasterNpcHeightMinInput, remasterNpcHeightMaxInput];
	for (NSUInteger i = 0; i < 2; i++)
	{
		NSScanner *scanner = [NSScanner scannerWithString:inputs[i].stringValue];
		if (![scanner scanDouble:&values[i]] || !scanner.isAtEnd || !std::isfinite(values[i]) || values[i] < -32768 || values[i] > 32767)
		{
			NSBeep();
			remasterNpcHeightMinInput.doubleValue = remasterNpcHeightDisplayMin;
			remasterNpcHeightMaxInput.doubleValue = remasterNpcHeightDisplayMax;
			return;
		}
	}
	if (values[1] <= values[0])
	{
		NSBeep();
		remasterNpcHeightMinInput.doubleValue = remasterNpcHeightDisplayMin;
		remasterNpcHeightMaxInput.doubleValue = remasterNpcHeightDisplayMax;
		return;
	}
	remasterNpcHeightDisplayMin = values[0]; remasterNpcHeightDisplayMax = values[1];
	[NSUserDefaults.standardUserDefaults setDouble:values[0] forKey:@"RemasterNpcHeightDisplayMin"];
	[NSUserDefaults.standardUserDefaults setDouble:values[1] forKey:@"RemasterNpcHeightDisplayMax"];
	[remasterNpcPoseView setNeedsDisplay:YES];
}

- (void)applyRemasterNpcNormals:(id)sender
{
	if (!remasterNpcPose) return;
	if (!remasterEditingProfileLoaded || remasterEditingProfile.romSha256 != remasterNpcRomHash.UTF8String)
	{
		NSAlert *alert = [NSAlert new]; alert.messageText = @"Load the matching ALTTP profile";
		alert.informativeText = @"Normals are written to the loaded editing profile. Load it using View > Load Remaster Profile… first.";
		[alert runModal]; return;
	}
	[self stopRemasterNpcPlayback];
	static const float directions[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
	const NSInteger direction = remasterNpcNormalSelector.indexOfSelectedItem;
	if (direction < 0 || direction >= 6) return;
	const auto normals = S9xRemasterNpcUniformNormals(EncodeRemasterNormal(
		directions[direction][0], directions[direction][1], directions[direction][2]));
	const BOOL allPoses = remasterNpcHeightAllPosesButton.state == NSControlStateValueOn;
	const BOOL overwrite = remasterNpcOverwriteButton.state == NSControlStateValueOn;
	std::set<RemasterTileContentId> tiles, touched, skipped;
	for (NSDictionary *actor in (allPoses ? remasterNpcContexts : @[remasterNpcActor]))
		for (NSDictionary *pose in (allPoses ? actor[@"poses"] : @[remasterNpcPose]))
		{
			RemasterFrame frame;
			if (!RemasterNpcDecodePose(pose, remasterNpcRomHash, frame)) return;
			for (const auto &instance : frame.tileInstances) tiles.insert(instance.tileId);
		}
	for (const auto &tile : tiles)
	{
		if (!S9xRemasterNpcCanAuthorLayer(remasterEditingProfile, tile, true, overwrite))
		{ skipped.insert(tile); continue; }
		const auto found = remasterEditingProfile.assets.find(tile);
		if (found == remasterEditingProfile.assets.end() || !found->second.hasNormals || found->second.normalXyz != normals)
			touched.insert(tile);
	}
	if (!touched.empty())
	{
		if (!remasterUndoManager) remasterUndoManager = [[NSUndoManager alloc] init];
		auto snapshot = CaptureRemasterAssetSnapshot(touched);
		for (const auto &tile : touched)
		{
			auto &metadata = remasterEditingProfile.assets[tile];
			metadata.tileId = tile; metadata.hasNormals = true; metadata.normalXyz = normals;
		}
		remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion, 5u);
		[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterAssetSnapshot:) object:snapshot];
		[remasterUndoManager setActionName:allPoses ? @"Apply NPC Normals for All Poses" : @"Apply NPC Pose Normals"];
		remasterMetadataNeedsSync = remasterNonSceneDirty = remasterEditingProfileDirty = true;
		SyncRemasterEditingMetadataToFrame();
		if (remasterNpcInspectorActive && !remasterVariants.empty() &&
			remasterVariantIndex < remasterNpcVariantPoses.count && remasterNpcVariantPoses[remasterVariantIndex] == remasterNpcPose)
			[self showRemasterVariantAtIndex:remasterVariantIndex];
		[self refreshRemasterEditingControls];
	}
	remasterNpcStatus.stringValue = [NSString stringWithFormat:@"Applied %@ normals to %lu tile identities.\nPreserved %lu identities with existing per-pixel normals.\nUse Save All Changes This Session to save, or Undo to revert.",
		remasterNpcNormalSelector.titleOfSelectedItem, (unsigned long)touched.size(), (unsigned long)skipped.size()];
	[remasterNpcPoseView setNeedsDisplay:YES];
}

- (void)generateRemasterNpcHeightRamp:(id)sender
{
	if (!remasterNpcPose) return;
	if (!remasterEditingProfileLoaded || remasterEditingProfile.romSha256 != remasterNpcRomHash.UTF8String)
	{
		NSAlert *alert = [NSAlert new]; alert.messageText = @"Load the matching ALTTP profile";
		alert.informativeText = @"Height ramps are written to the loaded editing profile. Load it using View > Load Remaster Profile… first.";
		[alert runModal]; return;
	}
	[self stopRemasterNpcPlayback];
	std::map<RemasterTileContentId, std::array<int16_t, 64>> ramps;
	const BOOL allPoses = remasterNpcHeightAllPosesButton.state == NSControlStateValueOn;
	NSArray *actors = allPoses ? remasterNpcContexts : @[remasterNpcActor];
	const BOOL overwrite = remasterNpcOverwriteButton.state == NSControlStateValueOn;
	std::set<RemasterTileContentId> skipped;
	std::set<RemasterTileContentId> conflicts;
	NSMutableDictionary<NSString *, NSMutableArray *> *uses = [NSMutableDictionary new];
	NSMutableDictionary<NSString *, NSDictionary *> *firstUse = [NSMutableDictionary new];
	for (NSDictionary *actor in actors)
		for (NSDictionary *pose in (allPoses ? actor[@"poses"] : @[remasterNpcPose]))
		{
			RemasterFrame frame;
			if (!RemasterNpcDecodePose(pose, remasterNpcRomHash, frame)) return;
			int bottom = INT_MIN;
			for (NSDictionary *part in pose[@"parts"]) bottom = std::max(bottom, [part[@"y"] intValue] + 7);
			for (NSUInteger i = 0; i < [pose[@"parts"] count]; i++)
			{
				NSDictionary *part = pose[@"parts"][i];
				const auto &tile = frame.tileInstances[i].tileId;
				if (!S9xRemasterNpcCanAuthorLayer(remasterEditingProfile, tile, false, overwrite))
				{ skipped.insert(tile); continue; }
				const auto height = S9xRemasterNpcHeightRamp(bottom, [part[@"y"] intValue], [part[@"vflip"] boolValue]);
				NSString *identity = [NSString stringWithUTF8String:RemasterProfileSerialization::TileId(tile).c_str()];
				if (!uses[identity])
				{
					uses[identity] = [NSMutableArray new];
					firstUse[identity] = @{@"actor": actor, @"pose": pose, @"part": @(i)};
				}
				[uses[identity] addObject:[NSString stringWithFormat:@"Context %@ · %@ · frame %ld · %@\n  Tile at (%d, %d), OAM %u, H flip %@, V flip %@; canonical top/bottom heights %u/%u",
					actor[@"id"], pose[@"facing"], (long)[pose[@"step"] integerValue] + 1, pose[@"action"],
					[part[@"x"] intValue], [part[@"y"] intValue], frame.tileInstances[i].sourceIndex,
					[part[@"hflip"] boolValue] ? @"yes" : @"no", [part[@"vflip"] boolValue] ? @"yes" : @"no", height[0], height[56]]];
				auto inserted = ramps.emplace(tile, height);
				if (!inserted.second && inserted.first->second != height) conflicts.insert(tile);
			}
		}
	if (!conflicts.empty())
	{
		NSMutableString *report = [NSMutableString new];
		for (const auto &tile : conflicts)
		{
			NSString *identity = [NSString stringWithUTF8String:RemasterProfileSerialization::TileId(tile).c_str()];
			[report appendFormat:@"%@\n%@\n\n", identity, [uses[identity] componentsJoinedByString:@"\n"]];
		}
		NSAlert *alert = [NSAlert new]; alert.messageText = @"Shared tile heights conflict";
		alert.informativeText = [NSString stringWithFormat:@"%lu shared tile identities require different canonical height maps. Horizontal (left/right) mirroring is compatible; different vertical positions or vertical flips can conflict. Apply Compatible Tiles generates the remaining heights and leaves conflicting identities unchanged. Show Conflicting Tiles highlights only their visible pixels. Details below identify every placement.", (unsigned long)conflicts.size()];
		NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 560, 220)];
		scroll.hasVerticalScroller = YES;
		NSTextView *details = [[NSTextView alloc] initWithFrame:scroll.contentView.bounds];
		details.editable = NO; details.selectable = YES;
		details.font = [NSFont userFixedPitchFontOfSize:11]; details.string = report;
		details.verticallyResizable = YES;
		details.textContainer.widthTracksTextView = YES;
		scroll.documentView = details; alert.accessoryView = scroll;
		[alert addButtonWithTitle:@"Show Conflicting Tiles"];
		NSButton *applyCompatible = [alert addButtonWithTitle:@"Apply Compatible Tiles"];
		applyCompatible.enabled = ramps.size() > conflicts.size();
		[alert addButtonWithTitle:@"Cancel"];
		const NSModalResponse response = [alert runModal];
		if (response == NSAlertFirstButtonReturn)
		{
			NSString *identity = [NSString stringWithUTF8String:RemasterProfileSerialization::TileId(*conflicts.begin()).c_str()];
			NSDictionary *witness = firstUse[identity];
			NSUInteger context = [remasterNpcContexts indexOfObjectIdenticalTo:witness[@"actor"]];
			[remasterNpcActorSelector selectItemAtIndex:context];
			[remasterNpcFacingSelector selectItemAtIndex:0];
			[self changeRemasterNpcActor:nil];
			NSUInteger pose = [remasterNpcFilteredPoses indexOfObjectIdenticalTo:witness[@"pose"]];
			[remasterNpcPoseSelector selectItemAtIndex:pose];
			[self changeRemasterNpcPose:nil];
			const uint32_t instance = [witness[@"part"] unsignedIntValue] + 1;
			remasterSelectedTiles.clear(); remasterSelectionValid = false;
			for (size_t at = 0; at < remasterNpcPreviewFrame.mainPixels.size(); at++)
				if (remasterNpcPreviewFrame.mainPixels[at].instanceId == instance)
				{
					[self pickRemasterNpcPoint:NSMakePoint(at % remasterNpcPreviewFrame.width, at / remasterNpcPreviewFrame.width) extendingSelection:NO];
					break;
				}
			remasterSelectedTiles.assign(conflicts.begin(), conflicts.end());
			remasterSelectionValid = true;
			remasterTilePixelSelected = false;
			[self refreshRemasterEditingControls];
			remasterNpcStatus.stringValue = [NSString stringWithFormat:@"Height ramp conflict: %lu tile identities highlighted in yellow.\nUse the pose dropdown to inspect their other placements.\n%@\nNo height changes were applied.", (unsigned long)conflicts.size(), identity];
			remasterInspectorText.string = [@"Height ramp conflict placements\n\n" stringByAppendingString:report];
			[remasterNpcPoseView setNeedsDisplay:YES];
		}
		if (response != NSAlertSecondButtonReturn) return;
		for (const auto &tile : conflicts) ramps.erase(tile);
	}
	std::set<RemasterTileContentId> touched;
	for (const auto &entry : ramps)
	{
		auto found = remasterEditingProfile.assets.find(entry.first);
		if (found == remasterEditingProfile.assets.end() || !found->second.hasHeight || found->second.height != entry.second)
			touched.insert(entry.first);
	}
	if (touched.empty())
	{
		remasterNpcStatus.stringValue = [NSString stringWithFormat:@"No height changes needed. Preserved %lu tile identities with existing per-pixel heights.\nEnable Overwrite authored heights to replace existing heights.", (unsigned long)skipped.size()];
		return;
	}
	if (!remasterUndoManager) remasterUndoManager = [[NSUndoManager alloc] init];
	auto snapshot = CaptureRemasterAssetSnapshot(touched);
	for (const auto &tile : touched)
	{
		auto &metadata = remasterEditingProfile.assets[tile];
		metadata.tileId = tile; metadata.hasHeight = true; metadata.height = ramps.at(tile);
	}
	remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion, 2u);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterAssetSnapshot:) object:snapshot];
	[remasterUndoManager setActionName:allPoses ? @"Generate NPC Heights for All Poses" : @"Generate NPC Height Ramp"];
	remasterMetadataNeedsSync = remasterNonSceneDirty = remasterEditingProfileDirty = true;
	SyncRemasterEditingMetadataToFrame();
	if (remasterNpcInspectorActive && !remasterVariants.empty() &&
		remasterVariantIndex < remasterNpcVariantPoses.count && remasterNpcVariantPoses[remasterVariantIndex] == remasterNpcPose)
		[self showRemasterVariantAtIndex:remasterVariantIndex];
	[self refreshRemasterEditingControls];
	remasterNpcStatus.stringValue = [NSString stringWithFormat:@"Generated height ramps for %lu tile identities.\nPreserved %lu identities with existing per-pixel heights; skipped %lu conflicting identities.\nUse Save All Changes This Session to save, or Undo to revert.",
		(unsigned long)touched.size(), (unsigned long)skipped.size(), (unsigned long)conflicts.size()];
	[remasterNpcPoseView setNeedsDisplay:YES];
}

- (void)changeRemasterNpcType:(id)sender
{
	[self stopRemasterNpcPlayback];
	NSInteger index = remasterNpcTypeSelector.indexOfSelectedItem;
	if (index < 0 || index >= (NSInteger)remasterNpcTypes.count) return;
	remasterNpcContexts = remasterNpcTypes[index][@"actors"];
	remasterNpcCanvasBounds = RemasterNpcBoundsForActors(remasterNpcContexts);
	[remasterNpcActorSelector removeAllItems];
	NSUInteger variant = 0;
	for (NSDictionary *actor in remasterNpcContexts)
	{
		[remasterNpcActorSelector addItemWithTitle:[NSString stringWithFormat:@"Variant %lu (%lu poses · %lu source contexts)",
			(unsigned long)++variant, (unsigned long)[actor[@"poses"] count], (unsigned long)[actor[@"sourceContextIds"] count]]];
		remasterNpcActorSelector.lastItem.toolTip = [NSString stringWithFormat:@"%@\n%@",
			[actor[@"sourceContextIds"] componentsJoinedByString:@", "], [actor[@"contexts"] componentsJoinedByString:@" · "]];
	}
	remasterNpcActorSelector.toolTip = @"Only distinct full-animation artwork/palette variants are listed. Hover an entry for its source contexts.";
	[remasterNpcFacingSelector selectItemAtIndex:0];
	[self changeRemasterNpcActor:nil];
}

- (void)stopRemasterNpcPlayback
{
	[remasterNpcTimer invalidate]; remasterNpcTimer = nil; remasterNpcPlaying = false;
	remasterNpcPlayButton.title = @"Play Animation";
}

- (void)changeRemasterNpcActor:(id)sender
{
	[self stopRemasterNpcPlayback];
	NSInteger index = remasterNpcActorSelector.indexOfSelectedItem;
	if (index < 0 || index >= (NSInteger)remasterNpcContexts.count) return;
	remasterNpcActor = remasterNpcContexts[index];
	[remasterNpcActionSelector removeAllItems];
	[remasterNpcActionSelector addItemWithTitle:@"All actions"];
	NSMutableOrderedSet *actions = [NSMutableOrderedSet new];
	for (NSDictionary *pose in remasterNpcActor[@"poses"]) [actions addObject:pose[@"action"]];
	[remasterNpcActionSelector addItemsWithTitles:actions.array];
	[self changeRemasterNpcFacing:nil];
}

- (void)changeRemasterNpcFacing:(id)sender
{
	[self stopRemasterNpcPlayback];
	NSMutableArray *filtered = [NSMutableArray new];
	[remasterNpcPoseSelector removeAllItems];
	for (NSDictionary *pose in remasterNpcActor[@"poses"])
	{
		if (remasterNpcActionSelector.indexOfSelectedItem > 0 &&
			![pose[@"action"] isEqual:remasterNpcActionSelector.titleOfSelectedItem]) continue;
		if (remasterNpcFacingSelector.indexOfSelectedItem > 0 &&
			![pose[@"facing"] isEqual:remasterNpcFacingSelector.titleOfSelectedItem]) continue;
		[filtered addObject:pose];
		[remasterNpcPoseSelector addItemWithTitle:[NSString stringWithFormat:@"%@ · frame %ld · %@",
			pose[@"facing"], (long)[pose[@"step"] integerValue] + 1, pose[@"action"]]];
	}
	remasterNpcFilteredPoses = filtered;
	[self changeRemasterNpcPose:nil];
}

- (void)changeRemasterNpcPose:(id)sender
{
	if (sender) [self stopRemasterNpcPlayback];
	NSInteger index = remasterNpcPoseSelector.indexOfSelectedItem;
	if (index < 0 || index >= (NSInteger)remasterNpcFilteredPoses.count)
	{
		remasterNpcPose = nil; remasterNpcPreviewFrame = RemasterFrame();
		remasterNpcStatus.stringValue = @"This NPC has no modeled poses in the selected direction.";
		[remasterNpcPoseView setNeedsDisplay:YES]; return;
	}
	remasterNpcPose = remasterNpcFilteredPoses[index];
	RemasterNpcDecodePose(remasterNpcPose, remasterNpcRomHash, remasterNpcPreviewFrame, &remasterNpcCanvasBounds);
	NSString *coverage = [remasterNpcActor[@"coverage"][@"gaps"] componentsJoinedByString:@" "] ?: @"Legacy library: action coverage has not been audited.";
	remasterNpcStatus.toolTip = [@"Coverage: PARTIAL — " stringByAppendingString:coverage];
	remasterNpcStatus.stringValue = [NSString stringWithFormat:@"%@\n%@ · %@ · modeled, unverified%@\nCoverage: PARTIAL — %@\n%@",
		[remasterNpcActor[@"contexts"] componentsJoinedByString:@" · "], remasterNpcPose[@"facing"], remasterNpcPose[@"action"],
		[remasterNpcActor[@"graphics_inherited"] boolValue] ? @" · inherited graphics slots" : @"",
		coverage,
		remasterEditingProfileLoaded && remasterEditingProfile.romSha256 == remasterNpcRomHash.UTF8String ?
			@"Matching profile loaded; normal, height, emission and other layers use the existing editor." :
			@"Read-only artwork: load the matching ALTTP remaster profile to enable editing."];
	[remasterNpcPoseView setNeedsDisplay:YES];
}

- (void)toggleRemasterNpcPlayback:(id)sender
{
	if (remasterNpcPlaying) { [self stopRemasterNpcPlayback]; return; }
	if (remasterNpcFilteredPoses.count < 2) return;
	remasterNpcPlaying = true; remasterNpcPlayButton.title = @"Stop Animation";
	__weak S9xEngine *weakSelf = self;
	remasterNpcTimer = [NSTimer scheduledTimerWithTimeInterval:0.18 repeats:YES block:^(NSTimer *timer) {
		if (!remasterNpcPanel.visible) { [weakSelf stopRemasterNpcPlayback]; return; }
		NSInteger current = remasterNpcPoseSelector.indexOfSelectedItem;
		NSString *facing = remasterNpcPose[@"facing"];
		for (NSUInteger n = 1; n <= remasterNpcFilteredPoses.count; n++)
		{
			NSUInteger next = (current + n) % remasterNpcFilteredPoses.count;
			if ([remasterNpcFilteredPoses[next][@"facing"] isEqual:facing] &&
				[remasterNpcFilteredPoses[next][@"action"] isEqual:remasterNpcPose[@"action"]])
			{ [remasterNpcPoseSelector selectItemAtIndex:next]; [weakSelf changeRemasterNpcPose:nil]; break; }
		}
	}];
}

- (void)configureRemasterNpcAnimationForPart:(NSUInteger)partIndex
{
	NSDictionary *source = remasterNpcPose;
	remasterNpcVariantPoses = RemasterNpcAnimationPoses(remasterNpcActor, source, partIndex);
	remasterNpcVariantFrames.clear(); remasterVariants.clear();
	remasterVariantIndex = 0;
	for (NSDictionary *pose in remasterNpcVariantPoses)
	{
		RemasterFrame frame;
		if (!RemasterNpcDecodePose(pose, remasterNpcRomHash, frame, &remasterNpcCanvasBounds)) continue;
		const NSUInteger counterpart = RemasterNpcCounterpartPart(source, partIndex, pose);
		if (pose == source) remasterVariantIndex = remasterVariants.size();
		remasterVariants.push_back(frame.tileInstances[counterpart].tileId);
		remasterNpcVariantFrames.push_back(std::move(frame));
	}
}

- (void)pickRemasterNpcPoint:(NSPoint)point extendingSelection:(BOOL)extend
{
	if (!remasterNpcPreviewFrame.width || !NSPointInRect(point, NSMakeRect(0, 0, remasterNpcPreviewFrame.width, remasterNpcPreviewFrame.height))) return;
	if (remasterEditingProfileLoaded && remasterEditingProfile.romSha256 != remasterNpcRomHash.UTF8String)
	{
		NSAlert *alert = [NSAlert new]; alert.messageText = @"Load the matching ALTTP profile";
		alert.informativeText = @"The loaded editing profile belongs to another ROM."; [alert runModal]; return;
	}
	[self stopRemasterNpcPlayback];
	if (running && !pauseEmulation) [self pause];
	remasterCaptureSession.fetch_add(1);
	remasterPausedCapturePending.store(false);
	const bool continuing = remasterNpcInspectorActive;
	if (!continuing)
	{
		remasterSelectedTiles.clear(); remasterVariants.clear(); remasterSelectionValid = false;
	}
	remasterNpcInspectorFrame = remasterNpcPreviewFrame;
	remasterNpcInspectorActive = true;
	remasterObstacleSelection.clear();
	remasterHeightInstanceId = 0;
	remasterMetadataNeedsSync = true;
	SyncRemasterEditingMetadataToFrame();
	// No gameplay plane is known for draw-only poses; authored local height remains
	// editable, but placement-effective values must never masquerade as evidence.
	[remasterHeightDomainSelector selectItemAtIndex:0];
	remasterNpcPicking = true;
	[self selectRemasterPixelAtViewPoint:point extendingSelection:extend];
	remasterNpcPicking = false;
	[remasterHeightDomainSelector selectItemAtIndex:0];
	[self refreshRemasterEditingControls];
	remasterInspectorPanel.hidesOnDeactivate = NO;
	[remasterInspectorPanel orderFront:nil];
	[remasterNpcPoseView setNeedsDisplay:YES];
}

- (void)selectAllRemasterNpcTiles:(id)sender
{
	const auto tiles = RemasterNpcVisibleTiles(remasterNpcPreviewFrame);
	if (tiles.empty()) return;
	// Establish the inspector using a visible pixel, then replace its selection
	// in one operation. Repeated presses must keep every tile selected.
	const auto first = std::find_if(remasterNpcPreviewFrame.mainPixels.begin(), remasterNpcPreviewFrame.mainPixels.end(),
		[](const auto &pixel) { return pixel.instanceId != 0; });
	const size_t offset = first - remasterNpcPreviewFrame.mainPixels.begin();
	[self pickRemasterNpcPoint:NSMakePoint(offset % remasterNpcPreviewFrame.width, offset / remasterNpcPreviewFrame.width)
		extendingSelection:NO];
	if (!remasterNpcInspectorActive || (remasterEditingProfileLoaded && remasterEditingProfile.romSha256 != remasterNpcRomHash.UTF8String)) return;
	remasterSelectedTiles = tiles;
	remasterSelectedTile = tiles.front();
	remasterSelectionValid = true;
	remasterTilePixelSelected = false;
	const auto &pixel = remasterNpcPreviewFrame.mainPixels[offset];
	[self configureRemasterNpcAnimationForPart:pixel.instanceId - 1];
	[self showRemasterVariantAtIndex:remasterVariantIndex];
	[self refreshRemasterEditingControls];
	[remasterNpcPoseView setNeedsDisplay:YES];
}
