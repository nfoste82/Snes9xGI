#ifndef REMASTER_SPHERE_SAMPLES_H
#define REMASTER_SPHERE_SAMPLES_H

#include <algorithm>
#include <cmath>
#include <cstdint>

// Nearest viewport XY distance is a conservative lower bound for every receiver,
// regardless of its height. Near/inside lights retain the full sphere quadrature.
inline uint32_t S9xRemasterSphereSamples(float x, float y, float radius, uint32_t width, uint32_t height)
{
	if (!width || !height || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius)) return 128;
	const float dx = x - std::max(0.5f, std::min(x, width - 0.5f));
	const float dy = y - std::max(0.5f, std::min(y, height - 0.5f));
	const float distance = std::hypot(dx, dy);
	if (distance <= radius || radius <= 0) return 128;
	const float angularRadius = radius / distance;
	return angularRadius < 0.01f ? 8 : angularRadius < 0.025f ? 16 :
		angularRadius < 0.05f ? 32 : angularRadius < 0.1f ? 64 : 128;
}

#endif
