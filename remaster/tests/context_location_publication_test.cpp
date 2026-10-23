// Location selection must work before authoring multiple named contexts.
#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>

uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = 0;

int main(int argc, char **argv)
{
	RemasterProfile profile;
	profile.gameTitle = "Location publication";
	profile.romSha256 = std::string(64, 'a');
	const RemasterTileContentId tile{0xfda59404d2049e61, 1, 4};
	RemasterAreaContext area; area.id = 0x51;
	std::array<int, 4> sx{}, sy{}; sx[1] = 64; sy[1] = 128;
	uint16_t screen[64]{};
	S9xRemasterSetLiveFramesEnabled(true);
	for (int contexts = 0; contexts <= 2; ++contexts)
	for (unsigned mode = 0; mode < 8; ++mode)
	{
		if (contexts == 1) profile.tileContexts[tile]["Door"].hasHeight = true;
		if (contexts == 2) profile.tileContexts[tile]["Elsewhere"].hasHeight = true;
		S9xRemasterSetProfile(profile);
		S9xRemasterBeginFrame(64, 8, 8, 8);
		auto &state = S9xRemasterState();
		RemasterFrameTileInstance instance;
		instance.tileId = tile; instance.source = RemasterSourceType::Background; instance.sourceIndex = 1;
		instance.hFlip = mode & 1; instance.vFlip = mode & 2;
		state.tileInstances.push_back(instance);
		for (unsigned p = 0; p < 64; ++p)
		{
			const unsigned canonical = (instance.vFlip ? 7 - p / 8 : p / 8) * 8 +
				(instance.hFlip ? 7 - p % 8 : p % 8);
			state.mainOwners[p] = mode & 4 ? 0x01000000 : 0x02000000;
			if (mode & 4)
			{
				state.subOwners[p] = 0x02000000;
				state.subInstanceIds[p] = 1;
				state.subTilePixels[p] = canonical;
			}
			else
			{
				state.mainInstanceIds[p] = 1;
				state.mainTilePixels[p] = canonical;
			}
		}
		RemasterFrame frame;
		S9xRemasterEndFrame(screen, 8, 8, 8, &frame, nullptr, nullptr, &area, 0, nullptr, &sx, &sy);
		assert(frame.contextLocations.size() == 1);
		const auto &at = frame.contextLocations.at(1);
		assert(at.area == 0x51 && at.background == 1 && at.x == 8 && at.y == 16);
		assert(S9xRemasterFrameVisibleInstanceAt(frame, 4, 4)->tileId == tile);
		// Retaining pick evidence must not implicitly choose an unbound context.
		assert(frame.contextGeometry.size() == (contexts == 1 ? 1u : 0u));
		std::vector<uint8_t> bytes;
		assert(S9xSerializeRemasterFrame(frame, bytes));
		RemasterFrame replay;
		assert(S9xDeserializeRemasterFrame(bytes, replay));
		assert(replay.contextLocations.size() == 1);
		assert(S9xRemasterContextSelection(profile, replay, {tile}, contexts == 1 ? "Door" : "") == std::set<uint32_t>{1});
		if (contexts == 2)
		{
			auto assigned = profile;
			assigned.tileContexts[tile]["Door"].height.fill(8);
			assigned.contextBindings[at] = {tile, "Door"};
			S9xRemasterApplyProfileToFrame(assigned, replay);
			assert(S9xRemasterContextSelection(assigned, replay, {tile}, "Door") == std::set<uint32_t>{1});
			assert(replay.contextGeometry.size() == 1 && replay.contextGeometry.at(1).height[0] == 8);
			S9xRemasterApplyProfileToFrame(profile, replay); // Undo removes only the binding.
			assert(replay.contextGeometry.empty() && replay.contextLocations.size() == 1);
		}
	}
	if (argc == 2)
	{
		RemasterFrame captured;
		assert(S9xReadRemasterFrame(argv[1], captured));
		unsigned visible = 0, located = 0;
		std::set<uint32_t> ids;
		for (const auto *pixels : {&captured.mainPixels, &captured.subPixels})
			for (const auto &pixel : *pixels)
				if (pixel.instanceId && pixel.instanceId <= captured.tileInstances.size() &&
					captured.tileInstances[pixel.instanceId - 1].tileId == tile) ids.insert(pixel.instanceId);
		for (auto id : ids) { ++visible; located += captured.contextLocations.count(id); }
		assert(visible);
		std::cout << "Capture doorway identity: " << visible << " visible occurrences, " << located << " retained location records\n";
	}
	std::cout << "PASS EndFrame publishes selectable BG locations with zero, one or multiple contexts, all flips, main/subscreen and capture roundtrip\n";
}
