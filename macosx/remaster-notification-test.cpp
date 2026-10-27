// Run from the repository root:
// clang++ -std=c++17 -Wall -Wextra macosx/remaster-notification-test.cpp -o <test-path>
#include "remaster-notification.h"
#include <cassert>
#include <cstdio>

int main()
{
	assert(RemasterNotificationPixels("", 256, 224, 0x7fff, false, false).empty());
	assert(RemasterNotificationPixels("Capture written", 0, 224, 0x7fff, false, false).empty());
	const std::string message = "Capture written: frame_room061_001.s9xrmf";
	const auto pixels = RemasterNotificationPixels(message, 256, 224, 0x7fff, false, false);
	unsigned foreground = 0, outline = 0, transparent = 0;
	for (size_t i = 0; i < pixels.size(); i += 4)
	{
		if (!pixels[i + 3]) { ++transparent; continue; }
		assert(i / 4 / 256 >= 204); // Same lower-screen placement as stock info text.
		assert(pixels[i + 3] == 255);
		if (pixels[i]) { ++foreground; assert(pixels[i] == 255 && pixels[i + 1] == 255 && pixels[i + 2] == 255); }
		else { ++outline; assert(pixels[i + 1] == 0 && pixels[i + 2] == 0); }
	}
	assert(foreground && outline && transparent);
	const auto raised = RemasterNotificationPixels(message, 256, 224, 0x7fff, true, false);
	for (size_t i = 0; i < pixels.size() - 30 * 256 * 4; ++i)
		assert(raised[i] == pixels[i + 30 * 256 * 4]);
	// Long filenames/newlines and tiny/overscan frames remain clipped and safe.
	for (int height : {1, 8, 224, 239})
		for (bool overscan : {false, true})
			assert(RemasterNotificationPixels(std::string(600, 'W') + "\nDone", 256,
				height, 0x7c00, false, overscan).size() == size_t(256 * height * 4));
	std::puts("PASS notification text, opaque outlines, transparency, wrapping, placement and clipping");
}
