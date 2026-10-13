// Corpus validation uses the native JSON decoder and the same pose view source.
#import <Cocoa/Cocoa.h>
#include "../remaster/remaster.h"
#include "../remaster/npc_pose.h"
#include <iostream>

@interface S9xEngine : NSObject
- (void)pickRemasterNpcPoint:(NSPoint)point extendingSelection:(BOOL)extend;
@end
@implementation S9xEngine
- (void)pickRemasterNpcPoint:(NSPoint)point extendingSelection:(BOOL)extend {}
@end
@interface S9xRemasterNpcPoseView : NSView
@property (nonatomic, weak) S9xEngine *editor;
@end
static RemasterFrame remasterNpcPreviewFrame;
static bool remasterNpcInspectorActive = false, remasterSelectionValid = false;
static std::vector<RemasterTileContentId> remasterSelectedTiles;
static bool remasterEditingProfileLoaded = false;
static RemasterProfile remasterEditingProfile;
static NSPopUpButton *remasterNpcDebugSelector;
static double remasterNpcHeightDisplayMin = 0, remasterNpcHeightDisplayMax = 32;
#include "remaster-npc-browser.h"

int main(int argc, char **argv)
{
	@autoreleasepool {
		if (argc != 2) return 2;
		NSData *data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]];
		NSDictionary *document = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
		unsigned poses = 0, picks = 0;
		NSDictionary *part0 = @{@"slot": @0}, *part1 = @{@"slot": @1};
		NSDictionary *phase0 = @{@"facing": @"south", @"action": @"walk", @"step": @0,
			@"parts": @[part0, part0, part0, part0, part1]};
		NSDictionary *phase1 = @{@"facing": @"south", @"action": @"walk", @"step": @1,
			@"parts": @[part1, part0, part0, part0, part0]};
		NSMutableDictionary *otherFacing = [phase1 mutableCopy]; otherFacing[@"facing"] = @"north";
		NSMutableDictionary *otherAction = [phase1 mutableCopy]; otherAction[@"action"] = @"attack";
		NSMutableDictionary *resized = [phase1 mutableCopy]; resized[@"parts"] = @[part0, part1];
		if (RemasterNpcCounterpartPart(phase0, 2, phase1) != 3 ||
			RemasterNpcCounterpartPart(phase0, 4, phase1) != 0 ||
			RemasterNpcCounterpartPart(phase0, 2, resized) != NSNotFound) return 1;
		NSArray *animation = RemasterNpcAnimationPoses(@{@"poses": @[otherFacing, phase1, resized, otherAction, phase0]}, phase0, 2);
		if (![animation isEqual:@[phase0, phase1]]) return 1;
		const RemasterTileContentId tileA{1, 1, 4}, tileB{2, 1, 4};
		std::vector<RemasterTileContentId> selection;
		RemasterToggleTileSelection(selection, tileA, false);
		if (selection != std::vector<RemasterTileContentId>{tileA}) return 1;
		RemasterToggleTileSelection(selection, tileB, true);
		if (selection != std::vector<RemasterTileContentId>{tileA, tileB}) return 1;
		RemasterToggleTileSelection(selection, tileA, true);
		if (selection != std::vector<RemasterTileContentId>{tileB}) return 1;
		RemasterToggleTileSelection(selection, tileA, true);
		RemasterToggleTileSelection(selection, tileA, false);
		if (!selection.empty()) return 1; // Ordinary selected click clears all.
		RemasterToggleTileSelection(selection, tileA, false);
		RemasterToggleTileSelection(selection, tileB, false);
		if (selection != std::vector<RemasterTileContentId>{tileB}) return 1;
		RemasterToggleTileSelection(selection, tileB, true);
		if (!selection.empty()) return 1;
		NSArray *groups = RemasterNpcGroupActors(document[@"actors"]);
		bool sawLink = false, sawGanon = false, sawZora = false;
		for (NSDictionary *actor in document[@"actors"])
		{
			if ([actor[@"name"] isEqual:@"Link"])
			{ if ([actor[@"poses"] count] != 24) return 1; sawLink = true; }
			if ([actor[@"name"] isEqual:@"Ganon"])
			{ if ([actor[@"poses"] count] != 34) return 1; sawGanon = true; }
			if ([actor[@"name"] isEqual:@"King Zora"])
			{ if ([actor[@"gfx_index"] intValue] != 14) return 1; sawZora = true; }
		}
		if (!sawLink || !sawGanon || !sawZora) return 1;
		// Source IDs/VRAM addresses do not distinguish artwork; a difference
		// in a later phase, palette, identity, flip or placement must survive.
		NSDictionary *original = document[@"actors"][0];
		NSMutableDictionary *duplicate = [original mutableCopy]; duplicate[@"id"] = @"duplicate";
		NSMutableArray *duplicatePoses = [original[@"poses"] mutableCopy];
		NSMutableDictionary *lastPose = [duplicatePoses.lastObject mutableCopy];
		NSMutableArray *lastParts = [lastPose[@"parts"] mutableCopy];
		NSMutableDictionary *lastPart = [lastParts[0] mutableCopy];
		lastPart[@"address"] = @(([lastPart[@"address"] intValue] + 32) % 32768);
		lastParts[0] = lastPart; lastPose[@"parts"] = lastParts;
		duplicatePoses[duplicatePoses.count - 1] = lastPose; duplicate[@"poses"] = duplicatePoses;
		NSArray *combined = RemasterNpcGroupActors(@[original, duplicate]);
		if ([combined[0][@"actors"] count] != 1 || [combined[0][@"actors"][0][@"sourceContextIds"] count] != 2) return 1;
		NSArray *baseline = RemasterNpcContextSignature(duplicate);
		for (NSString *key in @[@"x", @"y", @"slot", @"hflip", @"vflip", @"tile_hash", @"colors"])
		{
			id value = lastPart[key];
			if ([key isEqual:@"tile_hash"]) lastPart[key] = @"different-identity";
			else if ([key isEqual:@"colors"])
			{
				NSMutableArray *colors = [value mutableCopy]; colors[0] = @(([colors[0] intValue] + 1) % 32768); lastPart[key] = colors;
			}
			else lastPart[key] = @([value intValue] + 1);
			if ([RemasterNpcContextSignature(duplicate) isEqual:baseline] ||
				[RemasterNpcGroupActors(@[original, duplicate])[0][@"actors"] count] != 2) return 1;
			lastPart[key] = value;
		}
		NSUInteger groupedSourcePoses = 0, groupedSourceActors = 0;
		bool sawBlue = false;
		for (NSDictionary *group in groups)
		{
			NSUInteger count = 0;
			const auto canvas = RemasterNpcBoundsForActors(group[@"actors"]);
			for (NSDictionary *actor in group[@"actors"])
			{
				if (![actor[@"name"] isEqual:group[@"name"]]) return 1;
				for (NSDictionary *pose in actor[@"poses"])
				{
					RemasterFrame stable;
					if (!RemasterNpcDecodePose(pose, document[@"rom_sha256"], stable, &canvas) ||
						stable.width != canvas.x1 - canvas.x0 + 8 || stable.height != canvas.y1 - canvas.y0 + 8) return 1;
					for (NSUInteger i = 0; i < [pose[@"parts"] count]; i++)
					{
						NSDictionary *part = pose[@"parts"][i];
						for (unsigned p = 0; p < 64; p++)
						{
							const unsigned x = [part[@"x"] intValue] - canvas.x0 + 4 + ([part[@"hflip"] boolValue] ? 7 - p % 8 : p % 8);
							const unsigned y = [part[@"y"] intValue] - canvas.y0 + 4 + ([part[@"vflip"] boolValue] ? 7 - p / 8 : p / 8);
							const auto &pixel = stable.mainPixels[y * stable.width + x];
							if (pixel.instanceId == i + 1 && pixel.tilePixel != p) return 1;
						}
					}
				}
				count += [actor[@"poses"] count];
				groupedSourceActors += [actor[@"sourceContextIds"] count];
				groupedSourcePoses += [actor[@"poses"] count] * [actor[@"sourceContextIds"] count];
			}
			if (count != [group[@"poseCount"] unsignedIntegerValue]) return 1;
			if ([group[@"name"] isEqual:@"Blue Guard"])
			{
				if (count != 32 || [group[@"actors"] count] != 2 ||
					[group[@"sourcePoseCount"] unsignedIntegerValue] != 416 ||
					[group[@"sourceContextCount"] unsignedIntegerValue] != 26) return 1;
				sawBlue = true;
			}
		}
		if (!sawBlue || groups.count < 50 || groupedSourceActors != [document[@"actors"] count]) return 1;
		for (NSDictionary *actor in document[@"actors"]) for (NSDictionary *pose in actor[@"poses"])
		{
			RemasterFrame frame;
			if (!RemasterNpcDecodePose(pose, document[@"rom_sha256"], frame)) return 1;
			poses++;
			for (NSUInteger partIndex = 0; partIndex < [pose[@"parts"] count]; partIndex++)
			{
				NSArray *phases = RemasterNpcAnimationPoses(actor, pose, partIndex);
				if (![phases containsObject:pose]) return 1;
				NSInteger lastStep = -1;
				for (NSDictionary *phase in phases)
				{
					NSUInteger target = RemasterNpcCounterpartPart(pose, partIndex, phase);
					if (target == NSNotFound || ![phase[@"parts"][target][@"slot"] isEqual:pose[@"parts"][partIndex][@"slot"]] ||
						[phase[@"step"] integerValue] < lastStep) return 1;
					lastStep = [phase[@"step"] integerValue];
				}
			}
			const auto allTiles = RemasterNpcVisibleTiles(frame);
			if (allTiles.empty()) return 1;
			for (size_t i = 0; i < allTiles.size(); i++)
			{
				if (std::find(allTiles.begin(), allTiles.begin() + i, allTiles[i]) != allTiles.begin() + i) return 1;
				if (S9xRemasterFrameOccurrences(frame, allTiles[i]).empty()) return 1;
			}
			for (unsigned y = 0; y < frame.height; y++) for (unsigned x = 0; x < frame.width; x++)
			{
				const auto &pixel = frame.mainPixels[y * frame.width + x];
				if (!pixel.instanceId) continue;
				const auto *instance = S9xRemasterFrameVisibleInstanceAt(frame, x, y);
				if (!instance || !(instance->tileId == frame.tileInstances[pixel.instanceId - 1].tileId) || pixel.tilePixel >= 64) return 1;
				if (std::find(allTiles.begin(), allTiles.end(), instance->tileId) == allTiles.end()) return 1;
				const auto *asset = S9xRemasterFrameAssetForTile(frame, instance->tileId);
				if (!asset || !asset->indices[pixel.tilePixel]) return 1;
				picks++;
			}
		}
		NSMutableDictionary *invalid = [document[@"actors"][0][@"poses"][0] mutableCopy];
		unsigned agahnimContexts = 0;
		for (NSDictionary *actor in document[@"actors"])
		{
			if (![actor[@"name"] isEqual:@"Agahnim"]) continue;
			agahnimContexts++;
			std::map<std::string, std::array<uint8_t, 64>> ramps;
			std::set<std::string> conflicts;
			for (NSDictionary *pose in actor[@"poses"])
			{
				int bottom = INT_MIN;
				for (NSDictionary *part in pose[@"parts"]) bottom = std::max(bottom, [part[@"y"] intValue] + 7);
				std::map<std::string, std::array<uint8_t, 64>> local;
				for (NSDictionary *part in pose[@"parts"])
				{
					std::string tile = [part[@"tile_hash"] UTF8String];
					const auto ramp = S9xRemasterNpcHeightRamp(bottom, [part[@"y"] intValue], [part[@"vflip"] boolValue]);
					auto samePose = local.emplace(tile, ramp);
					if (!samePose.second && samePose.first->second != ramp) return 1;
					auto acrossPoses = ramps.emplace(tile, ramp);
					if (!acrossPoses.second && acrossPoses.first->second != ramp) conflicts.insert(tile);
				}
			}
			if (conflicts != std::set<std::string>{"v1:4bpp:959bc138ca495b1a"}) return 1;
		}
		if (!agahnimContexts) return 1;
		if (groupedSourcePoses != poses) return 1;
		NSMutableArray *parts = [invalid[@"parts"] mutableCopy];
		NSMutableDictionary *part = [parts[0] mutableCopy];
		part[@"tile_hash"] = @"v1:4bpp:0000000000000000"; parts[0] = part; invalid[@"parts"] = parts;
		RemasterFrame frame;
		if (RemasterNpcDecodePose(invalid, document[@"rom_sha256"], frame)) return 1;
		part[@"slot"] = @999;
		if (RemasterNpcDecodePose(invalid, document[@"rom_sha256"], frame)) return 1;
		std::cout << "Animation counterpart mapping, selection toggles and select-all coverage: " << poses << " poses, " << picks << " canonical opaque picks passed\n";
	}
}
