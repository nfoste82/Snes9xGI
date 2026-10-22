// Link's reserved OAM assembly shares one floor base, not per-part support.
#include "frame.h"
#include <cassert>
#include <iostream>
#include <memory>

int main()
{
	for (int block : {56, 100})
		for (int plane : {0, 1})
			for (int base : {-16, 0, 50, 255})
			{
				RemasterDungeonFloorContext context;
				context.verifiedAlttpRom = context.indoors = true;
				context.roomIndex = 7;
				context.linkX = 80; context.linkY = 80; context.linkPlane = plane;
				auto map = std::make_unique<RemasterDungeonHeightMap>();
				map->roomIndex = 7;
				map->offsets.fill(13);
				map->hasPlaneOffsets = true;
				map->planeOffsets[0].fill(24);
				map->planeOffsets[1].fill(32);
				const int support = 12 * 64 + 11; // Link + (8,20), not sprite bottoms
				map->planeOffsets[plane][support] = base;
				RemasterFrame frame;
				frame.width = 32; frame.height = 8;
				frame.mainPixels.resize(256); frame.subPixels.resize(256);
				for (int n = 0; n < 12; n++)
				{
					RemasterFrameTileInstance part;
					part.source = RemasterSourceType::Object;
					part.sourceIndex = block + n;
					part.vramAddress = n == 0 ? 0x8000 : 0x9000;
					part.heightOffset = n - 3;
					part.hasPlacementHeight = true;
					part.placementHeight.fill(n + 1);
					part.hFlip = n & 1; part.ppuPriority = n % 4;
					frame.tileInstances.push_back(part);
					// Include clipped/hidden main-screen parts and sub-only parts.
					if (n < 8) frame.mainPixels[n].instanceId = n + 1;
					else frame.subPixels[n].instanceId = n + 1;
				}
				// Same shadow/equipment artwork outside the block is not Link-owned.
				auto other = frame.tileInstances[11]; other.sourceIndex = block - 1;
				frame.tileInstances.push_back(other);
				frame.mainPixels[31].instanceId = 13;
				const auto before = frame.tileInstances;
				S9xRemasterApplyDungeonHeightMap(frame, context, *map);
				for (int n = 0; n < 12; n++)
				{
					assert(frame.tileInstances[n].heightOffset == before[n].heightOffset + (base == 255 ? 0 : base));
					assert(frame.tileInstances[n].placementHeight == before[n].placementHeight);
					assert(frame.tileInstances[n].hasPlacementHeight);
				}
				assert(frame.tileInstances[12].heightOffset == other.heightOffset + (plane ? 32 : 24));
				// Applying BG1 must not add the actor offset a second time.
				const auto applied = frame.tileInstances;
				S9xRemasterApplyDungeonHeightMap(frame, context, *map, nullptr, nullptr, 0);
				for (size_t n = 0; n < applied.size(); n++)
					assert(frame.tileInstances[n].heightOffset == applied[n].heightOffset);
				// Conflicting body-DMA observations cannot choose either block.
				frame.tileInstances = before;
				frame.tileInstances[12].sourceIndex = block == 56 ? 100 : 56;
				frame.tileInstances[12].vramAddress = 0x8000;
				S9xRemasterApplyDungeonHeightMap(frame, context, *map);
				assert(frame.tileInstances[1].heightOffset == before[1].heightOffset + (plane ? 32 : 24));
				// Maps without per-plane support still share the one legacy base.
				frame.tileInstances = before;
				map->hasPlaneOffsets = false;
				map->offsets[support] = -7;
				S9xRemasterApplyDungeonHeightMap(frame, context, *map);
				for (int n = 0; n < 12; n++)
					assert(frame.tileInstances[n].heightOffset == before[n].heightOffset - 7);
				map->hasPlaneOffsets = true;
				// Without verified body ownership retain the existing per-OAM path.
				for (bool verified : {false, true})
				{
					frame.tileInstances = before;
					context.verifiedAlttpRom = verified;
					if (verified) frame.tileInstances[0].vramAddress = 0x8400;
					S9xRemasterApplyDungeonHeightMap(frame, context, *map);
					assert(frame.tileInstances[1].heightOffset == before[1].heightOffset + (plane ? 32 : 24));
				}
			}
	std::cout << "Link attachment shared support PASS\n";
}
