#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>

int main ()
{
	using Surface = RemasterDungeonFloorContext::Surface;
	auto maps = std::make_unique<std::array<uint8_t, 16384>>();
	auto graphics = std::make_unique<std::array<uint8_t, 65536>>();
	auto tiles = std::make_unique<std::array<RemasterDungeonFloorContext::Placement, 4096>>();
	uint8_t pixels[64] = {};
	for (unsigned y = 0; y < 8; y++) for (unsigned x = 0; x < 8; x++)
	{
		pixels[y * 8 + x] = (x + y) % 15 + 1;
		for (unsigned b = 0; b < 4; b++)
			(*graphics)[(b / 2) * 16 + y * 2 + b % 2] |= ((pixels[y * 8 + x] >> b) & 1) << (7 - x);
	}
	const RemasterTileContentId id {S9xRemasterHashTile(4, pixels), 1, 4};
	RemasterProfile profile;
	auto &door = profile.assets[id];
	door.tileId = id; door.tileRole = RemasterTileRole::Door;
	door.hasHeight = true;
	for (unsigned i = 0; i < 64; i++) door.height[i] = i / 8;
	const auto stairs = door.height;
	S9xRemasterPaintDoorframe(door, 17, true);
	RemasterDungeonFloorContext context;
	context.verifiedAlttpRom = context.indoors = true;
	context.constructedMaps = maps->data(); context.graphics = graphics->data();
	for (const auto &stale : {"dungeon_floor", "wall_faces", "obstacles", "stair_treads"})
	{
		profile.assetGroups.clear(); profile.assetGroups[stale].tileIds = {id};
		for (unsigned flip = 0; flip < 4; flip++)
		{
			maps->fill(0xff);
			(*maps)[0] = 0; (*maps)[1] = flip << 6;
			S9xRemasterDungeonPlacements(context, profile, *tiles);
			assert((*tiles)[0].surface == Surface::Door);
			assert(!(*tiles)[0].floorPlaneMask && !(*tiles)[0].groundedFloorMask);
			assert((*tiles)[0].height == stairs && door.height == stairs);
		}
	}
	std::cout << "Door role rejects stale floor/wall/stair group promotion\n";
}
