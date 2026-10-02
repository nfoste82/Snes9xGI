#ifndef REMASTER_EMITTER_PATCHES_H
#define REMASTER_EMITTER_PATCHES_H
#include <array>
#include <vector>
#include <cmath>
#include "frame.h"
#include "surface_mesh.h"

struct RemasterEmitterPatch
{
	uint32_t count = 0;
	std::array<uint32_t, 25> members = {};
};
static_assert(sizeof(RemasterEmitterPatch) == 104, "Emitter patch ABI");

inline std::vector<RemasterEmitterPatch> S9xRemasterEmitterPatches(const RemasterFrame &frame,
	const uint8_t *emission, const uint8_t *participation, const float *surface,
	const std::vector<RemasterSurfaceMesh::Cell> &mesh)
{
	const size_t size = frame.mainPixels.size();
	std::vector<uint8_t> used(size, 0);
	std::vector<std::array<float, 3>> colors(size);
	std::vector<RemasterEmitterPatch> result;
	auto pixelAt = [&](size_t i) -> const RemasterFramePixel & {
		const auto &main = frame.mainPixels[i];
		return !main.instanceId && uint8_t(main.owner >> 24) == uint8_t(RemasterSourceType::Backdrop) ? frame.subPixels[i] : main;
	};
	auto active = [&](size_t i) { return participation[i*2] > 127 && emission[i*4+3] &&
		(emission[i*4] || emission[i*4+1] || emission[i*4+2]); };
	// Decode each visible emitter once, rather than repeating pow for every
	// rejected rectangle/member comparison (especially costly at footprint5).
	for (size_t i = 0; i < size; i++) if (active(i)) {
		float maximum = float(std::max(emission[i*4], std::max(emission[i*4+1], emission[i*4+2])));
		for (unsigned c = 0; c < 3; c++) colors[i][c] = std::pow(float(emission[i*4+c]) / maximum, 2.2f);
	}
	unsigned extent = std::max(1u, std::min(5u, unsigned(frame.emitterPatchSize)));
	for (uint32_t origin = 0; origin < size; origin++) {
		if (used[origin] || !active(origin)) continue;
		RemasterEmitterPatch patch;
		patch.members[patch.count++] = origin;
		used[origin] = 1;
		const auto &owner = pixelAt(origin);
		// Full rectangles keep representatives on visible emitting support. Sparse
		// or incompatible candidates stay singleton rather than bridging a hole.
		bool merged = false;
		for (unsigned candidate = extent*extent; candidate > 1 && owner.instanceId && !merged; candidate--)
		for (unsigned nx = 1; nx <= extent && !merged; nx++) {
			unsigned ny = candidate/nx;
			if (candidate%nx || ny > extent) continue;
			unsigned x = origin % frame.width, y = origin / frame.width;
			if (x+nx > frame.width || y+ny > frame.height) continue;
			bool compatible = true;
			for (unsigned dy = 0; dy < ny && compatible; dy++) for (unsigned dx = 0; dx < nx && compatible; dx++) {
				uint32_t i = (y+dy)*frame.width+x+dx;
				const auto &p = pixelAt(i);
				compatible &= (i == origin || !used[i]) && active(i) && p.owner == owner.owner &&
					p.instanceId == owner.instanceId && p.tilePixel%8/extent == owner.tilePixel%8/extent &&
					p.tilePixel/8/extent == owner.tilePixel/8/extent &&
					mesh[i].emissionDepth == mesh[origin].emissionDepth &&
					mesh[i].thickness == mesh[origin].thickness && mesh[i].solidWall == mesh[origin].solidWall &&
					surface[i*4] == surface[origin*4];
				if (!compatible) break;
				// Centroid endpoints are only safe on a common flat support.
				for (unsigned c = 0; c < 4; c++) compatible &=
					mesh[i].corners[c] == surface[i*4] && mesh[origin].corners[c] == surface[origin*4];
				for (unsigned c = 1; c < 4; c++) compatible &= surface[i*4+c] == surface[origin*4+c];
				for (unsigned c = 0; c < 3; c++) compatible &= mesh[i].normal[c] == mesh[origin].normal[c];
				float dot = 0;
				for (unsigned c = 1; c < 4; c++) dot += surface[i*4+c]*surface[origin*4+c];
				compatible &= dot > 0.98f;
				for (unsigned c = 0; c < 3; c++) compatible &= std::fabs(colors[i][c]-colors[origin][c]) <= 0.4f;
			}
			if (!compatible) continue;
			patch.count = 0;
			for (unsigned dy = 0; dy < ny; dy++) for (unsigned dx = 0; dx < nx; dx++) {
				uint32_t i = (y+dy)*frame.width+x+dx;
				patch.members[patch.count++] = i; used[i] = 1;
			}
			merged = true;
		}
		result.push_back(patch);
	}
	return result;
}
#endif
