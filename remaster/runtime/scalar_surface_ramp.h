#ifndef S9X_REMASTER_SCALAR_SURFACE_RAMP_H
#define S9X_REMASTER_SCALAR_SURFACE_RAMP_H

#include "surface_mesh.h"

namespace RemasterSurfaceMesh
{
// Placement scalars are samples of the base spaced eight pixels apart, not
// high-resolution relief arrays. Reconstruct between them with a
// centered, tile-wide box (half weight at the endpoints). On successive flat
// tiles with equal rises this yields a linear ramp through the tile centers.
// Unobserved, authored-detail and ledge boundaries extend the last valid sample
// instead of borrowing across the boundary. Raw heights still determine welds.
inline bool reconstructScalarSurfaces(unsigned width, unsigned height, float quantum,
	std::vector<Sample> &samples, std::vector<float> &horizontal)
{
	if (samples.size() != size_t(width) * height || !width || !height) return false;
	const float limit = 3.0f * quantum + 0.0001f;
	auto eligible = [](const Sample &s) {
		return s.known && s.coverage > 0 && s.sheet &&
			((s.scalarFloor && s.domain == FloorDomain) || (s.scalarWall && s.domain == WallDomain));
	};
	auto base = [&](size_t p) { return samples[p].scalarWall ? samples[p].wallBase : samples[p].height; };
	auto canBlend = [&](size_t p, size_t q) {
		const auto &a = samples[p], &b = samples[q];
		if (!eligible(b) || a.domain != b.domain) return false;
		const float rise = b.height - a.height;
		if (!a.scalarWall) return std::abs(rise) <= limit;
		const float baseRise = b.wallBase - a.wallBase;
		// Blend the wall placement only, not its authored relief. A base
		// transition must agree with the local face and the top seam budget.
		return std::abs(baseRise) <= limit && std::abs(rise) <= limit &&
			(baseRise == 0 || std::abs(rise - baseRise) <= quantum * 1.05f);
	};
	auto average = [&](size_t index, unsigned coordinate, unsigned size, int stride, auto value) {
		const float center = value(index);
		float delta = 0;
		for (int side : {-1, 1})
		{
			size_t p = index;
			for (int d = 1; d <= 4; ++d)
			{
				const int next = int(coordinate) + side * d;
				if (next < 0 || next >= int(size)) {
					delta += (value(p) - center) * (4.5f - d);
					break;
				}
				const size_t q = side < 0 ? p - stride : p + stride;
				if (!canBlend(p, q)) {
					delta += (value(p) - center) * (4.5f - d);
					break;
				}
				p = q;
				delta += (value(p) - center) * (d == 4 ? 0.5f : 1.0f);
			}
		}
		return center + delta * 0.125f;
	};
	horizontal.resize(samples.size());
	for (unsigned y = 0; y < height; ++y)
		for (unsigned x = 0; x < width; ++x)
		{
			const size_t i = size_t(y) * width + x;
			horizontal[i] = eligible(samples[i]) ? average(i, x, width, 1, base) : base(i);
		}
	bool changed = false;
	for (unsigned y = 0; y < height; ++y)
		for (unsigned x = 0; x < width; ++x)
		{
			const size_t i = size_t(y) * width + x;
			samples[i].scalarAdjustment = eligible(samples[i]) ?
				average(i, y, height, int(width), [&](size_t p) { return horizontal[p]; }) - base(i) : 0;
			changed |= samples[i].scalarAdjustment != 0;
		}
	return changed;
}
}

#endif
