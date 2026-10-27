// Presentation-only notification pixels. Never part of capture data or GI inputs.
#ifndef MACOSX_REMASTER_NOTIFICATION_H
#define MACOSX_REMASTER_NOTIFICATION_H

#include <cstdint>
#include <string>
#include <vector>
#include "../var8x10font.h"

inline std::vector<uint8_t> RemasterNotificationPixels(const std::string &message,
	int width, int height, uint16_t color, bool pressedKeys, bool showOverscan)
{
	if (message.empty() || width <= 0 || height <= 0) return {};
	std::vector<uint8_t> pixels(size_t(width) * height * 4, 0);
	// Match the stock info string's proportional font, wrapping and position.
	int lines = pressedKeys ? 5 : 2;
	int minimumLines = 1;
	for (char c : message) if (c == '\n') ++minimumLines;
	if (minimumLines > lines) lines = minimumLines;
	int x = 1, y = height - 10 * lines;
	if (height % 224 && !showOverscan) y -= 8;
	else if (showOverscan) y += 8;
	const auto channel = [](unsigned value) { return uint8_t((value * 527 + 23) >> 6); };
	for (unsigned char c : message)
	{
		if (c < 32 && c != '\n') continue;
		const unsigned glyph = c == '\n' ? 0 : c - 32;
		const int left = var8x10font_kern[glyph][0];
		const int charWidth = 8 - left - var8x10font_kern[glyph][1];
		if (x + charWidth > width || c == '\n')
		{
			--lines;
			y = height - 10 * lines;
			x = 1;
			if (y >= height) break;
		}
		if (c == '\n') continue;
		for (int row = 0; row < 10; ++row)
			for (int column = 0; column < charWidth; ++column)
			{
				if (y + row < 0 || y + row >= height || x + column >= width) continue;
				const char ink = var8x10font[(glyph >> 4) * 10 + row][(glyph & 15) * 8 + left + column];
				if (ink != '#' && ink != '.') continue;
				uint8_t *pixel = pixels.data() + (size_t(y + row) * width + x + column) * 4;
				pixel[0] = ink == '#' ? channel((color >> 10) & 31) : 0;
				pixel[1] = ink == '#' ? channel((color >> 5) & 31) : 0;
				pixel[2] = ink == '#' ? channel(color & 31) : 0;
				pixel[3] = 255; // Include the black outline, even over a black source pixel.
			}
		x += charWidth - 1;
	}
	return pixels;
}

#endif
