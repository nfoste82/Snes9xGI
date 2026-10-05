#ifndef REMASTER_EMISSION_APPEARANCE_H
#define REMASTER_EMISSION_APPEARANCE_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// CPU counterpart of remasterEmissionAppearance/remasterCompositeToDisplay.
// Transport uses 100 radiance units at intensity 25; previews use artwork units.
inline std::array<uint8_t, 3> S9xRemasterEmissionPreview(const uint8_t *rgba)
{
	std::array<float, 3> linear;
	float peak = 0;
	for (unsigned c = 0; c < 3; c++) {
		linear[c] = std::pow(rgba[c] / 255.0f, 2.2f) * (rgba[3] / 25.0f);
		peak = std::max(peak, linear[c]);
	}
	float scale = peak > 0.75f ? (0.75f + 0.25f * (1 - std::exp(-(peak - 0.75f) / 0.25f))) / peak : 1;
	std::array<uint8_t, 3> color;
	for (unsigned c = 0; c < 3; c++)
		color[c] = static_cast<uint8_t>(std::lround(std::pow(linear[c] * scale, 1.0f / 2.2f) * 255));
	return color;
}

#endif
