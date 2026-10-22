// Continuous actor support across both intra-room stair orientations/planes.
#include "frame.h"
#include <cassert>
#include <iostream>
#include <memory>

int main()
{
	for (bool highNorth : {false, true}) for (bool changesPlane : {false, true})
		for (int plane : {0, 1}) for (int block : {56, 100})
	{
		RemasterDungeonFloorContext c;
		c.verifiedAlttpRom = c.indoors = true;
		c.roomIndex = 7; c.linkPlane = plane;
		c.stairCount = 1;
		c.stairs[0] = {10, 12, 0, 0, highNorth, changesPlane};
		c.linkX = 11 * 8; // foot center inside the two-column stair corridor
		auto map = std::make_unique<RemasterDungeonHeightMap>();
		map->roomIndex = 7; map->hasPlaneOffsets = true;
		map->offsets.fill(91);
		map->planeOffsets[0].fill(17); map->planeOffsets[1].fill(23);
		const int north = 11 * 64 + 11, south = 16 * 64 + 11;
		const int np = changesPlane ? (highNorth ? 0 : 1) : plane;
		const int sp = changesPlane ? (highNorth ? 1 : 0) : plane;
		const int a = highNorth ? 48 : -16, b = highNorth ? -16 : 48;
		for (int dx : {0, 1}) {
			map->planeOffsets[np][north + dx] = a;
			map->planeOffsets[sp][south + dx] = b;
		}
		// Ascending/descending, every pixel, with either collision plane selected.
		for (int direction : {-1, 1}) for (int step = 0; step <= 32; step++)
		{
			const int progress = direction > 0 ? step : 32 - step;
			c.linkY = 12 * 8 + progress - 20;
			c.submodule = highNorth ? 8 : 16;
			const int expected = (a * (32 - progress) + b * progress) / 32;
			assert(S9xRemasterLinkFloorHeight(c, *map) == expected);
			RemasterFrame f;
			f.width = f.height = 1;
			f.mainPixels.resize(1); f.subPixels.resize(1);
			for (int n = 0; n < 12; n++) {
				RemasterFrameTileInstance part;
				part.source = RemasterSourceType::Object; part.sourceIndex = block + n;
				part.vramAddress = n ? 0x9000 : 0x8000;
				part.heightOffset = n - 3;
				part.hasPlacementHeight = true; part.placementHeight.fill(n + 1);
				f.tileInstances.push_back(part);
			}
			f.subPixels[0].instanceId = 12; // sub-only equipment also inherits
			S9xRemasterApplyDungeonHeightMap(f, c, *map);
			for (int n = 0; n < 12; n++) {
				assert(f.tileInstances[n].heightOffset == n - 3 + expected);
				assert(f.tileInstances[n].placementHeight[0] == n + 1);
			}
			S9xRemasterApplyDungeonHeightMap(f, c, *map, nullptr, nullptr, 0);
			assert(f.tileInstances[0].heightOffset == expected - 3);
		}
		c.linkY = 12 * 8 + 16 - 20;
		const int ordinary = plane ? 23 : 17;
		// Settled approach and dialog use the same spatial height, without history.
		c.submodule = 0; assert(S9xRemasterLinkFloorHeight(c, *map) == 16);
		c.module = 14; c.savedModule = 7; c.submodule = 2;
		assert(S9xRemasterLinkFloorHeight(c, *map) == 16);
		c.savedModule = 9; assert(S9xRemasterLinkFloorHeight(c, *map) == ordinary);
		c.module = 7; c.submodule = 0;
		// Missing/conflicting landings, unverified ROM, other transitions and
		// legacy maps retain their existing support rather than inventing ramps.
		map->planeOffsets[np][north] = 255;
		assert(S9xRemasterLinkFloorHeight(c, *map) == ordinary);
		map->planeOffsets[np][north] = a + 1;
		assert(S9xRemasterLinkFloorHeight(c, *map) == ordinary);
		map->planeOffsets[np][north] = a;
		// Non-integral rises round in the existing signed placement units.
		for (int dx : {0, 1}) map->planeOffsets[sp][south + dx] = b + 1;
		assert(S9xRemasterLinkFloorHeight(c, *map) == 17);
		for (int dx : {0, 1}) map->planeOffsets[sp][south + dx] = b;
		// A second overlapping stair with incompatible landing support is not
		// resolved by record order.
		c.stairCount = 2; c.stairs[1] = c.stairs[0]; c.stairs[1].y++;
		assert(S9xRemasterLinkFloorHeight(c, *map) == ordinary);
		c.stairCount = 1;
		c.verifiedAlttpRom = false;
		assert(S9xRemasterLinkFloorHeight(c, *map) == ordinary);
		c.verifiedAlttpRom = true; c.submodule = 2;
		assert(S9xRemasterLinkFloorHeight(c, *map) == ordinary);
		c.submodule = 0; map->hasPlaneOffsets = false;
		assert(S9xRemasterLinkFloorHeight(c, *map) == 91);
		map->hasPlaneOffsets = true;
		// Outside the corridor, including adjacent underpasses, never interpolate.
		c.linkX -= 16;
		assert(S9xRemasterLinkFloorHeight(c, *map) == ordinary);
		c.linkX += 16; c.linkY = 12 * 8 - 1 - 20;
		assert(S9xRemasterLinkFloorHeight(c, *map) == map->planeOffsets[plane][north + 1]);
		c.linkY = 12 * 8 + 33 - 20;
		assert(S9xRemasterLinkFloorHeight(c, *map) == map->planeOffsets[plane][south + 1]);
		map->roomIndex = 8;
		assert(S9xRemasterLinkFloorHeight(c, *map) == 255);
	}
	std::cout << "Link stair interpolation PASS\n";
}
