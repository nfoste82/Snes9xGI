#include "../runtime/remaster.h"
#include <cassert>

int main()
{
	using Surface = RemasterDungeonFloorContext::Surface;
	std::array<uint8_t, 8192> attributes; attributes.fill(1);
	std::array<RemasterDungeonFloorContext::Placement, 4096> tiles = {};
	// Two disconnected ordinary floors, separated by a three-tile wall.
	for (int y : {12, 19}) for (int yy = y; yy < y + 3; yy++) for (int x = 8; x < 18; x++)
	{
		attributes[yy * 64 + x] = 0;
		tiles[yy * 64 + x].surface = Surface::Floor;
		tiles[yy * 64 + x].hasHeight = true;
	}
	for (int y = 15; y < 19; y++) for (int x = 8; x < 18; x++)
	{
		auto &tile = tiles[y * 64 + x]; tile.surface = Surface::Wall; tile.hasHeight = true;
		for (int yy = 0; yy < 8; yy++) for (int xx = 0; xx < 8; xx++) tile.height[yy * 8 + xx] = 7 - yy;
	}
	RemasterDungeonFloorContext context;
	context.verifiedAlttpRom = context.indoors = true; context.roomIndex = 7;
	context.linkX = 80; context.linkY = 160;
	context.collisionAttributes = attributes.data(); context.placements = tiles.data();
	context.stairCount = 1; context.stairs[0].x = 10; context.stairs[0].y = 15;
	context.stairs[0].highIsNorth = true;
	RemasterDungeonHeightMap map;
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.hasRoomGeometry);
	assert(map.planeOffsets[0][13 * 64 + 10] == 32); // not arbitrary 50
	assert(map.planeOffsets[0][20 * 64 + 10] == 0);
	assert(map.offsets[15 * 64 + 10] == 24);
	assert(map.offsets[18 * 64 + 10] == 0);
	// Runtime application consumes cached variable bases, not upper-floor50.
	RemasterFrame frame; frame.width = frame.height = 8;
	RemasterFrameTileInstance instance; instance.source = RemasterSourceType::Background; instance.sourceIndex = 1;
	frame.tileInstances.push_back(instance); frame.mainPixels.resize(64); frame.subPixels.resize(64);
	for (auto &pixel : frame.mainPixels) pixel.instanceId = 1;
	context.backgroundScrollX = 10 * 8; context.backgroundScrollY = 13 * 8;
	S9xRemasterApplyDungeonHeightMap(frame, context, map);
	assert(frame.tileInstances[0].heightOffset == 32);
	assert(map.structuralConflicts == 0);
	assert(map.unknownPlaneCells[0] == 0);
	// Authored overlay relief over occupied support must not change that
	// support's seam level. It remains visual metadata, not a floor height.
	for (int x=8;x<18;x++) {
		tiles[19*64+x].surface=Surface::Unknown;
		tiles[19*64+x].height.fill(3);
	}
	assert(S9xRemasterBuildDungeonHeightMap(context,50,map));
	const auto coveredBases=map.offsets;
	for(int x=8;x<18;x++)tiles[19*64+x].height.fill(0);
	assert(S9xRemasterBuildDungeonHeightMap(context,50,map));
	assert(map.offsets==coveredBases);
	for (int x=8;x<18;x++) { tiles[19*64+x].surface=Surface::Floor; tiles[19*64+x].height.fill(0); }
	// A horizontal wall face beside a solved identical face shares a full
	// tangent seam. It must not need its own direct floor contact.
	tiles[17 * 64 + 18] = tiles[17 * 64 + 17];
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.offsets[17 * 64 + 18] == 8);
	tiles[17 * 64 + 18] = {};
	// Obstacle retains relief and inherits support, never becoming a wall step.
	tiles[20 * 64 + 18].surface = Surface::Obstacle;
	tiles[20 * 64 + 18].hasHeight = true; tiles[20 * 64 + 18].height.fill(6);
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.offsets[20 * 64 + 18] == 0);
	assert(map.fallbackDirections[20 * 64 + 18] == 0);
	// Authored wall profile changed: four 2-unit-per-pixel ramps imply rise 64.
	for (int y = 15; y < 19; y++) for (int x = 8; x < 18; x++)
		for (auto &h : tiles[y * 64 + x].height) h *= 2;
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.offsets[13 * 64 + 10] == 64);
	// Stairless support components use an explicit relative datum, not entry.
	context.stairCount = 0;
	context.linkY = 13 * 8; // entry on HIGH floor
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.offsets[13 * 64 + 10] == 64);
	assert(map.offsets[20 * 64 + 10] == 0);
	// Flip transforms the canonical ramp before accumulating rise.
	context.stairCount = 1;
	for (int y = 15; y < 19; y++) for (int x = 8; x < 18; x++)
	{
		auto &tile = tiles[y * 64 + x]; tile.vflip = true;
		for (int yy = 0; yy < 8; yy++) for (int xx = 0; xx < 8; xx++) tile.height[yy * 8 + xx] = yy;
	}
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.offsets[13 * 64 + 10] == 32);
	context.stairCount = 0;
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.offsets[13 * 64 + 10] == 32);
	assert(map.offsets[20 * 64 + 10] == 0);
	assert(!map.relativeAnchors.empty());
	assert(map.relativeAnchors[0].supportNodes == 2);
	frame.tileInstances[0].heightOffset = 0;
	S9xRemasterApplyDungeonHeightMap(frame, context, map);
	assert(frame.tileInstances[0].heightOffset == 32);
	// Wall-only isolated assembly has no relative floor datum.
	tiles[2 * 64 + 2] = tiles[15 * 64 + 10];
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.offsets[2 * 64 + 2] == 255);
	// Isolated explicitly flat plateau carries diagnostic relative-zero provenance.
	tiles[2 * 64 + 3].surface = Surface::Floor; tiles[2 * 64 + 3].hasHeight = true;
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.offsets[2 * 64 + 3] == 0);
	assert(std::any_of(map.relativeAnchors.begin(), map.relativeAnchors.end(), [] (const auto &a) { return a.isolatedFlat; }));
	const uint64_t planeKey = S9xRemasterDungeonHeightInputKey(context, 50);
	context.linkPlane = 1;
	assert(S9xRemasterDungeonHeightInputKey(context, 50) != planeKey);
	context.linkPlane = 0;
	// Unrelated groups never erase semantic role; obstacle wins over wall/top.
	std::array<uint8_t, 16384> semanticMaps = {};
	std::array<uint8_t, 65536> graphics = {};
	uint8_t indices[64] = {};
	RemasterTileContentId id = {S9xRemasterHashTile(4, indices), 1, 4};
	RemasterProfile profile;
	for (const char *name : {"wall_faces", "wall_tops", "zz_unrelated", "obstacles"}) profile.assetGroups[name].tileIds.push_back(id);
	context.constructedMaps = semanticMaps.data(); context.graphics = graphics.data();
	std::array<RemasterDungeonFloorContext::Placement, 4096> classified = {};
	S9xRemasterDungeonPlacements(context, profile, classified);
	assert(classified[0].surface == Surface::Obstacle);
	profile.assetGroups.erase("obstacles");
	S9xRemasterDungeonPlacements(context, profile, classified);
	assert(classified[0].surface == Surface::Wall);
	// A repeated dungeon pattern in solid space is not an isolated floor
	// datum. Test each plane separately and leave explicit wall tops intact.
	profile.assetGroups.clear();
	profile.assetGroups["dungeon_floor"].tileIds.push_back(id);
	RemasterAssetMetadata patternMetadata; patternMetadata.hasHeight = true;
	profile.assets[id] = patternMetadata;
	attributes[0] = 1; attributes[4096] = 0;
	S9xRemasterDungeonPlacements(context, profile, classified);
	assert(classified[0].surface == Surface::Floor);
	assert(classified[0].structuralFloorPlaneMask == 2);
	attributes[4096] = 2;
	S9xRemasterDungeonPlacements(context, profile, classified);
	assert(classified[0].structuralFloorPlaneMask == 0);
	assert(classified[0].height == patternMetadata.height);
	profile.assetGroups["wall_tops"].tileIds.push_back(id);
	S9xRemasterDungeonPlacements(context, profile, classified);
	assert(classified[0].structuralFloorPlaneMask == 3);
	// The graph must actually consume the per-plane restriction, not merely
	// expose a diagnostic flag. An unsupported cap cannot ground its wall.
	tiles[2*64+3].structuralFloorPlaneMask = 0;
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.offsets[2*64+3] == 255 && map.offsets[2*64+2] == 255);
	tiles[2*64+3].structuralFloorPlaneMask = 3;
	context.collisionMode = 1;
	assert(!S9xRemasterBuildDungeonHeightMap(context, 50, map));
	context.collisionMode = 0;
	// Two different wall paths relating the same floors must conflict, never
	// choose the closer contact or silently clamp the floor to the stair rise.
	for (auto &h : tiles[16 * 64 + 10].height) h *= 2;
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.structuralConflicts > 0);
	assert(map.offsets[13 * 64 + 10] == 255);
	// Complete constructed input is not publishable during room construction,
	// even if its room ID still matches the last cached room.
	std::array<uint8_t, 16384> maps = {};
	context.constructedMaps = maps.data(); context.submodule = 2;
	assert(!S9xRemasterDungeonHeightInputsPublishable(context, context.roomIndex));
	context.submodule = 0;
	assert(S9xRemasterDungeonHeightInputsPublishable(context, context.roomIndex));
	// Exact affine northeast-facing wall connects diagonal floor contacts.
	tiles = {}; attributes.fill(1); context.stairCount = 0;
	context.constructedMaps = nullptr; context.graphics = nullptr;
	context.linkX = 12 * 8; context.linkY = 11 * 8;
	for (auto xy : {std::array<int,2>{12,12}, {8,16}})
		for (int y = xy[1]; y < xy[1] + 2; y++) for (int x = xy[0]; x < xy[0] + 2; x++)
		{
			attributes[y*64+x] = 0; tiles[y*64+x].surface = Surface::Floor; tiles[y*64+x].hasHeight = true;
		}
	auto &diagonalWall = tiles[14*64+11];
	diagonalWall.surface = Surface::Wall; diagonalWall.hasHeight = true;
	for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) diagonalWall.height[y*8+x] = 7-x+y;
	// Low contact NE, high contact SW, with a flat plateau at each contact.
	tiles[13*64+12].surface = Surface::Floor; tiles[13*64+12].hasHeight = true;
	tiles[15*64+10].surface = Surface::Floor; tiles[15*64+10].hasHeight = true;
	tiles[16*64+10].surface = Surface::Floor; tiles[16*64+10].hasHeight = true;
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.offsets[14*64+11] == 0);
	assert(map.offsets[15*64+10] == 16);
	assert(map.offsets[13*64+12] == 0);
	// Rasterized diagonal assembly: a clipped cap plus affine middle and
	// clipped floor foot share full cardinal seams. Corner shortcuts must not
	// bypass those seams; all H/V transformed occurrences preserve the bases.
	for (int flip = 0; flip < 4; flip++)
	{
		tiles = {}; attributes.fill(1);
		context.linkX=20*8; context.linkY=19*8;
		for (int y=20;y<23;y++) for(int x=20;x<24;x++)
		{
			attributes[y*64+x]=0; tiles[y*64+x].surface=Surface::Floor; tiles[y*64+x].hasHeight=true;
		}
		// Reflect positions and canonical profiles together, testing runtime
		// sampling rather than modifying authored metadata.
		for(int n=0;n<3;n++)
		{
			const int x=(flip&1)?12-n:10+n, y=17;
			auto &t=tiles[y*64+x]; t.surface=Surface::Wall; t.hasHeight=true;
			t.hflip=flip&1; t.vflip=flip&2;
			for(int yy=0;yy<8;yy++) for(int xx=0;xx<8;xx++)
				t.height[yy*8+xx]=n==0?std::min(13,14-xx-yy):n==1?14-xx-yy:std::max(0,10-xx-yy);
		}
		// Isolated flat floor gives a relative datum at the low end, with
		// compatible edge samples rather than a nearest-floor fill.
		auto &floor=tiles[17*64+((flip&1)?9:13)]; floor.surface=Surface::Floor; floor.hasHeight=true;
		for(int yy=0;yy<8;yy++) for(int xx=0;xx<8;xx++) floor.height[yy*8+xx]=std::max(0,3-((flip&2)?7-yy:yy));
		assert(S9xRemasterBuildDungeonHeightMap(context,50,map));
		assert(map.offsets[17*64+((flip&1)?12:10)]==12);
		assert(map.offsets[17*64+11]==4);
		assert(map.offsets[17*64+((flip&1)?10:12)]==0);
	}
	// Min-ramp corner retains two cardinal facings, not an invented diagonal mask.
	tiles = {}; attributes.fill(1); context.linkX = 11*8; context.linkY = 10*8;
	for (int y = 10; y < 15; y++) for (int x = 10; x < 15; x++)
	{
		attributes[y*64+x] = 0; tiles[y*64+x].surface = Surface::Floor; tiles[y*64+x].hasHeight = true;
	}
	auto &corner = tiles[10*64+10]; corner.surface = Surface::Wall; attributes[10*64+10] = 1;
	for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) corner.height[y*8+x] = std::min(7-x, 7-y);
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.offsets[10*64+10] == 0);
	assert(map.structuralConflicts == 0);
	// Same support component cannot give one corner two incompatible local seams.
	tiles[10*64+11].height.fill(4);
	assert(S9xRemasterBuildDungeonHeightMap(context, 50, map));
	assert(map.structuralConflicts > 0);
	assert(map.offsets[10*64+10] == 255);
	return 0;
}
