#include "../runtime/frame.h"
#include <cassert>
#include <iostream>

int main()
{
	for (uint8_t layer = 0; layer < 2; ++layer)
	{
		RemasterFrame frame;
		frame.width = 6; frame.height = 1;
		frame.mainPixels.resize(6); frame.subPixels.resize(6);
		for (auto &pixel : frame.mainPixels)
			pixel.owner = uint32_t(RemasterSourceType::Backdrop) << 24;
		RemasterFrameTileInstance selected, other;
		selected.source = other.source = RemasterSourceType::Background;
		selected.sourceIndex = layer;
		selected.tileId.hashVersion = 1; selected.tileId.bitDepth = 4; selected.tileId.hash = 123;
		other.sourceIndex = 1 - layer;
		other.tileId.hashVersion = 1; other.tileId.bitDepth = 4; other.tileId.hash = 456;
		frame.tileInstances = {selected, other};
		auto set = [](RemasterFramePixel &pixel, uint32_t id) {
			pixel.owner = uint32_t(RemasterSourceType::Background) << 24;
			pixel.instanceId = id;
		};
		set(frame.subPixels[0], 1); // BG selectable through main backdrop.
		set(frame.mainPixels[1], 1); // Ordinary main-screen selection.
		set(frame.mainPixels[2], 2); set(frame.subPixels[2], 1); // Hidden selected BG.
		set(frame.mainPixels[3], 1); set(frame.subPixels[3], 1); // No duplicate.
		set(frame.subPixels[4], 99); // Invalid ownership must not highlight.
		set(frame.subPixels[5], 1);
		frame.mainPixels[5].owner = 0xffffffffu; // Unsupported main is not backdrop.
		frame.scenePixels = frame.subPixels; // Lighting ownership must not change selection.
		const auto offsets = S9xRemasterFrameVisibleOccurrences(frame, selected.tileId);
		assert((offsets == std::vector<uint32_t>{0, 1, 3}));
		for (uint32_t x = 0; x < frame.width; ++x)
		{
			const auto *picked = S9xRemasterFrameVisibleInstanceAt(frame, x, 0);
			const bool highlighted = std::find(offsets.begin(), offsets.end(), x) != offsets.end();
			assert(highlighted == (picked && picked->tileId == selected.tileId));
		}
		assert(S9xRemasterFrameVisibleOccurrences(frame, other.tileId) == std::vector<uint32_t>{2});
	}
	std::cout << "tile selection highlight PASS (BG1/BG2 main/subscreen precedence)\n";
}
