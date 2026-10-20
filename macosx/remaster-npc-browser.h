// Included by mac-os.mm: native pose artwork/picking, no renderer or emulation.
static NSArray *RemasterNpcContextSignature(NSDictionary *actor)
{
	NSMutableArray *poses = [NSMutableArray new];
	for (NSDictionary *pose in actor[@"poses"])
	{
		NSMutableArray *parts = [NSMutableArray new];
		for (NSDictionary *part in pose[@"parts"])
			// Ignore VRAM addresses/context IDs, but retain canonical identities,
			// palettes (including hidden pixels), placement and inspector provenance.
			[parts addObject:@[part[@"x"], part[@"y"], part[@"slot"], part[@"hflip"], part[@"vflip"],
				part[@"tile_hash"], part[@"indices"], part[@"colors"]]];
		[poses addObject:@[pose[@"facing"], pose[@"action"], pose[@"step"], [parts copy]]];
	}
	return [poses copy];
}

static NSArray *RemasterNpcGroupActors(NSArray *actors)
{
	NSMutableArray *groups = [NSMutableArray new];
	NSMutableDictionary *byName = [NSMutableDictionary new];
	for (NSDictionary *actor in actors)
	{
		NSString *name = actor[@"name"];
		NSMutableDictionary *group = byName[name];
		if (!group)
		{
			group = [@{@"name": name, @"actors": [NSMutableArray new], @"poseCount": @0,
				@"sourcePoseCount": @0, @"sourceContextCount": @0, @"variants": [NSMutableDictionary new]} mutableCopy];
			byName[name] = group; [groups addObject:group];
		}
		group[@"sourcePoseCount"] = @([group[@"sourcePoseCount"] unsignedIntegerValue] + [actor[@"poses"] count]);
		group[@"sourceContextCount"] = @([group[@"sourceContextCount"] unsignedIntegerValue] + 1);
		NSArray *signature = RemasterNpcContextSignature(actor);
		NSMutableDictionary *variant = group[@"variants"][signature];
		if (!variant)
		{
			variant = [actor mutableCopy];
			variant[@"sourceContextIds"] = [NSMutableArray new];
			variant[@"contexts"] = [NSMutableArray new];
			variant[@"graphics_inherited"] = @NO;
			group[@"variants"][signature] = variant;
			[group[@"actors"] addObject:variant];
			group[@"poseCount"] = @([group[@"poseCount"] unsignedIntegerValue] + [actor[@"poses"] count]);
		}
		[variant[@"sourceContextIds"] addObject:actor[@"id"]];
		for (NSString *context in actor[@"contexts"])
			if (![variant[@"contexts"] containsObject:context]) [variant[@"contexts"] addObject:context];
		variant[@"graphics_inherited"] = @([variant[@"graphics_inherited"] boolValue] || [actor[@"graphics_inherited"] boolValue]);
	}
	for (NSMutableDictionary *group in groups) [group removeObjectForKey:@"variants"];
	return groups;
}

static NSRect RemasterNpcArtworkRect(NSView *view, bool artwork = false)
{
	if (!remasterNpcPreviewFrame.width || !remasterNpcPreviewFrame.height) return NSZeroRect;
	const CGFloat paneWidth = std::max<CGFloat>(1, (NSWidth(view.bounds) - 24) / 2);
	const CGFloat paneHeight = std::max<CGFloat>(1, NSHeight(view.bounds) - 28);
	CGFloat scale = std::floor(std::min(paneWidth / remasterNpcPreviewFrame.width,
		paneHeight / remasterNpcPreviewFrame.height));
	scale = std::max<CGFloat>(1, scale);
	const NSSize size = NSMakeSize(scale * remasterNpcPreviewFrame.width, scale * remasterNpcPreviewFrame.height);
	return NSMakeRect(NSMinX(view.bounds) + (artwork ? paneWidth + 24 : 0) + (paneWidth - size.width) / 2,
		NSMinY(view.bounds) + 28 + (paneHeight - size.height) / 2, size.width, size.height);
}

// Shared with scene picking so ordinary clicks and Command-clicks stay identical.
static void RemasterToggleTileSelection(std::vector<RemasterTileContentId> &selection,
	const RemasterTileContentId &tile, bool extend)
{
	auto selected = std::find(selection.begin(), selection.end(), tile);
	if (!extend)
	{
		const bool alreadySelected = selected != selection.end();
		selection.clear();
		if (!alreadySelected) selection.push_back(tile);
	}
	else if (selected == selection.end()) selection.push_back(tile);
	else selection.erase(selected);
}

static std::vector<RemasterTileContentId> RemasterNpcVisibleTiles(const RemasterFrame &frame)
{
	std::vector<RemasterTileContentId> tiles;
	for (const auto &pixel : frame.mainPixels)
	{
		if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size()) continue;
		const auto &tile = frame.tileInstances[pixel.instanceId - 1].tileId;
		if (std::find(tiles.begin(), tiles.end(), tile) == tiles.end()) tiles.push_back(tile);
	}
	return tiles;
}

@implementation S9xRemasterNpcPoseView
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirty
{
	[[NSColor colorWithCalibratedWhite:0.12 alpha:1] setFill];
	NSRectFill(self.bounds);
	RemasterFrame frame = remasterNpcPreviewFrame;
	if (!frame.width) return;
	if (remasterEditingProfileLoaded && remasterEditingProfile.romSha256 == frame.profileRomSha256)
		S9xRemasterApplyProfileToFrame(remasterEditingProfile, frame);
	NSDictionary *labelAttributes = @{NSFontAttributeName: [NSFont boldSystemFontOfSize:12],
		NSForegroundColorAttributeName: NSColor.secondaryLabelColor};
	const CGFloat paneWidth = (NSWidth(self.bounds) - 24) / 2;
	NSString *debugTitle = [remasterNpcDebugSelector.titleOfSelectedItem stringByReplacingOccurrencesOfString:@"Debug View: " withString:@""] ?: @"Debug View";
	[debugTitle drawAtPoint:NSMakePoint(NSMinX(self.bounds) + 8, NSMinY(self.bounds) + 4) withAttributes:labelAttributes];
	[@"Original Artwork" drawAtPoint:NSMakePoint(NSMinX(self.bounds) + paneWidth + 32, NSMinY(self.bounds) + 4) withAttributes:labelAttributes];
	for (unsigned pane = 0; pane < 2; pane++)
	{
	const NSRect rect = RemasterNpcArtworkRect(self, pane == 1);
	const CGFloat scale = NSWidth(rect) / frame.width;
	for (unsigned y = 0; y < frame.height; y++) for (unsigned x = 0; x < frame.width; x++)
	{
		const size_t at = y * frame.width + x;
		const auto &pixel = frame.mainPixels[at];
		if (!pixel.instanceId) continue;
		const auto color = S9xRemasterNpcDebugColor(frame, at, pane == 1 ? RemasterNpcDebugView::Artwork : static_cast<RemasterNpcDebugView>(remasterNpcDebugSelector.indexOfSelectedItem),
			remasterNpcHeightDisplayMin, remasterNpcHeightDisplayMax);
		[[NSColor colorWithCalibratedRed:color[0] / 255.0 green:color[1] / 255.0
			blue:color[2] / 255.0 alpha:1] setFill];
		NSRectFill(NSMakeRect(rect.origin.x + x * scale, rect.origin.y + y * scale, scale, scale));
		if (pane == 0 && remasterNpcInspectorActive && remasterSelectionValid &&
			std::find(remasterSelectedTiles.begin(), remasterSelectedTiles.end(),
				frame.tileInstances[pixel.instanceId - 1].tileId) != remasterSelectedTiles.end())
		{
			[[NSColor colorWithCalibratedRed:1 green:1 blue:0 alpha:0.5] setFill];
			NSRectFillUsingOperation(NSMakeRect(rect.origin.x + x * scale, rect.origin.y + y * scale, scale, scale), NSCompositingOperationSourceOver);
		}
	}
	// Keep the assembled surface unobstructed. Tile-instance outlines look like
	// seams in normal/height maps, especially on large multipart actors. Picking
	// uses the ownership buffer, and selected identities retain their yellow tint.
}
}
- (void)mouseDown:(NSEvent *)event
{
	const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
	NSRect rect = RemasterNpcArtworkRect(self);
	if (!NSPointInRect(point, rect)) rect = RemasterNpcArtworkRect(self, true);
	if (!NSPointInRect(point, rect) || !remasterNpcPreviewFrame.width) return;
	const CGFloat scale = NSWidth(rect) / remasterNpcPreviewFrame.width;
	[self.editor pickRemasterNpcPoint:NSMakePoint(std::floor((point.x - rect.origin.x) / scale),
		std::floor((point.y - rect.origin.y) / scale)) extendingSelection:(event.modifierFlags & NSEventModifierFlagCommand) != 0];
}
@end

static bool RemasterNpcByteArray(id values, unsigned count, unsigned maximum)
{
	if (![values isKindOfClass:NSArray.class] || [values count] != count) return false;
	for (id number in values)
		if (![number isKindOfClass:NSNumber.class] || [number doubleValue] != [number integerValue] ||
			[number integerValue] < 0 || [number integerValue] > maximum) return false;
	return true;
}

static RemasterNpcPoseBounds RemasterNpcBoundsForActors(NSArray *actors)
{
	RemasterNpcPoseBounds bounds;
	for (NSDictionary *actor in actors)
		for (NSDictionary *pose in actor[@"poses"])
			for (NSDictionary *part in pose[@"parts"])
				bounds.include([part[@"x"] intValue], [part[@"y"] intValue]);
	return bounds;
}

static bool RemasterNpcDecodePose(NSDictionary *pose, NSString *hash, RemasterFrame &frame,
	const RemasterNpcPoseBounds *canvas = nullptr)
{
	if (![pose isKindOfClass:NSDictionary.class] || ![pose[@"parts"] isKindOfClass:NSArray.class]) return false;
	std::vector<RemasterNpcPosePart> parts;
	for (id entry in pose[@"parts"])
	{
		if (![entry isKindOfClass:NSDictionary.class] || !RemasterNpcByteArray(entry[@"indices"], 64, 15) ||
			!RemasterNpcByteArray(entry[@"colors"], 64, 32767)) return false;
		for (NSString *key in @[@"x", @"y", @"slot", @"address"])
			if (![entry[key] isKindOfClass:NSNumber.class] || [entry[key] doubleValue] != [entry[key] integerValue]) return false;
		if ([entry[@"x"] integerValue] < -256 || [entry[@"x"] integerValue] > 256 ||
			[entry[@"y"] integerValue] < -256 || [entry[@"y"] integerValue] > 256 ||
			[entry[@"slot"] integerValue] < 0 || [entry[@"slot"] integerValue] >= 128 ||
			[entry[@"address"] integerValue] < 0 || [entry[@"address"] integerValue] > 32767 ||
			![entry[@"hflip"] isKindOfClass:NSNumber.class] || ![entry[@"vflip"] isKindOfClass:NSNumber.class]) return false;
		RemasterNpcPosePart part;
		part.x = [entry[@"x"] intValue]; part.y = [entry[@"y"] intValue];
		part.slot = [entry[@"slot"] unsignedCharValue]; part.address = [entry[@"address"] unsignedShortValue];
		part.hFlip = [entry[@"hflip"] boolValue]; part.vFlip = [entry[@"vflip"] boolValue];
		for (unsigned p = 0; p < 64; p++)
		{
			part.asset.indices[p] = [entry[@"indices"][p] unsignedCharValue];
			part.rgb555[p] = S9xRemasterPaletteRgb555([entry[@"colors"][p] unsignedShortValue]);
		}
		part.asset.tileId = {S9xRemasterHashTile(4, part.asset.indices), 1, 4};
		NSString *identity = [NSString stringWithUTF8String:RemasterProfileSerialization::TileId(part.asset.tileId).c_str()];
		if (![entry[@"tile_hash"] isEqual:identity]) return false;
		parts.push_back(part);
	}
	if (!S9xRemasterBuildNpcPoseFrame(parts, hash.UTF8String ?: "", frame, canvas)) return false;
	const NSUInteger facing = [@[@"north", @"south", @"east", @"west"] indexOfObject:pose[@"facing"]];
	for (auto &instance : frame.tileInstances) instance.geometryFacing = facing == NSNotFound ? 0 : facing + 1;
	return true;
}

// The adapter emits each OAM slot's subtiles in canonical row-major order,
// independent of screen position and H/V flips. Match that provenance, not art
// similarity or absolute position (parts can move during animation).
static NSUInteger RemasterNpcCounterpartPart(NSDictionary *source, NSUInteger partIndex, NSDictionary *target)
{
	NSArray *parts = source[@"parts"], *candidates = target[@"parts"];
	if (partIndex >= parts.count) return NSNotFound;
	id slot = parts[partIndex][@"slot"];
	NSUInteger ordinal = 0, sourceCount = 0;
	for (NSUInteger i = 0; i < parts.count; i++)
		if ([parts[i][@"slot"] isEqual:slot]) { sourceCount++; if (i < partIndex) ordinal++; }
	NSUInteger targetCount = 0, found = NSNotFound;
	for (NSUInteger i = 0; i < candidates.count; i++)
		if ([candidates[i][@"slot"] isEqual:slot])
		{ if (targetCount == ordinal) found = i; targetCount++; }
	return sourceCount == targetCount ? found : NSNotFound;
}

static NSArray *RemasterNpcAnimationPoses(NSDictionary *actor, NSDictionary *source, NSUInteger partIndex)
{
	NSMutableArray *poses = [NSMutableArray new];
	for (NSDictionary *pose in actor[@"poses"])
		if ([pose[@"facing"] isEqual:source[@"facing"]] && [pose[@"action"] isEqual:source[@"action"]] &&
			RemasterNpcCounterpartPart(source, partIndex, pose) != NSNotFound)
			[poses addObject:pose];
	[poses sortUsingComparator:^NSComparisonResult(NSDictionary *a, NSDictionary *b) {
		return [a[@"step"] compare:b[@"step"]];
	}];
	return poses;
}
