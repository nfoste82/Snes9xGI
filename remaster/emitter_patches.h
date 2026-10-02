#ifndef REMASTER_EMITTER_PATCHES_H
#define REMASTER_EMITTER_PATCHES_H
#include <array>
#include <vector>
#include <cmath>
#include <map>
#include <tuple>
#include <cstddef>
#include "frame.h"
#include "surface_mesh.h"

struct RemasterEmitterPatch
{
	uint32_t count = 0;
	uint32_t mode = 0; // bit0: actual-member representative; bit1: expanded depth layout
	std::array<uint32_t, 64> members = {};
};
static_assert(sizeof(RemasterEmitterPatch) == 264 && offsetof(RemasterEmitterPatch, members) == 8, "Emitter patch ABI");

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
	if (frame.representativeEmitters) {
		// Separate screen channels and placements even when their tile hashes match.
		std::map<std::tuple<bool, uint32_t, uint32_t>, std::vector<uint32_t>> instances;
		for (uint32_t i = 0; i < size; i++) if (active(i)) {
			const auto &p = pixelAt(i);
			if (!p.instanceId || p.tilePixel >= 64) {
				RemasterEmitterPatch singleton; singleton.count = 1; singleton.mode = 1u | (mesh[i].emissionDepth > 0 ? 2u : 0u);
				singleton.members[0] = i; result.push_back(singleton); continue;
			}
			instances[{&p == &frame.subPixels[i], p.owner, p.instanceId}].push_back(i);
		}
		for (auto &entry : instances) {
			auto &pixels = entry.second;
			std::sort(pixels.begin(), pixels.end(), [&](uint32_t a, uint32_t b) {
				return std::make_pair(pixelAt(a).tilePixel, a) < std::make_pair(pixelAt(b).tilePixel, b);
			});
			// A normal tile has <=64 visible pixels. Oversized/duplicated inputs are
			// conservatively chunked rather than overflowing/truncating membership.
			for (size_t start = 0; start < pixels.size(); start += 64) {
				unsigned n = unsigned(std::min<size_t>(64, pixels.size()-start));
				unsigned k = std::min(n, unsigned(std::min<uint8_t>(8, frame.representativeEmitters)));
				std::array<std::array<float,5>,64> features = {};
				std::array<float,64> powers = {};
				for (unsigned j = 0; j < n; j++) {
					uint32_t p = pixels[start+j]; unsigned xy = pixelAt(p).tilePixel;
					features[j] = {float(xy%8)/7, float(xy/8)/7, colors[p][0], colors[p][1], colors[p][2]};
					float maximum = float(std::max(emission[p*4], std::max(emission[p*4+1], emission[p*4+2])))/255;
					powers[j] = std::pow(maximum,2.2f)*float(emission[p*4+3])/255;
				}
				auto distance = [](const auto &a, const auto &b) {
					float d = 0; for (unsigned c = 0; c < 5; c++) d += (a[c]-b[c])*(a[c]-b[c]); return d;
				};
				std::array<std::array<float,5>,8> centers = {};
				std::array<bool,64> seeded = {};
				for (unsigned c = 0; c < k; c++) {
					unsigned best = 0; float score = -1;
					for (unsigned j = 0; j < n; j++) if (!seeded[j]) {
						float nearest = 1;
						if (c) { nearest = INFINITY; for (unsigned b = 0; b < c; b++) nearest = std::min(nearest,distance(features[j],centers[b])); }
						float candidate = powers[j]*nearest;
						if (candidate > score) { score = candidate; best = j; }
					}
					seeded[best] = true; centers[c] = features[best];
				}
				std::array<unsigned,64> assignment = {};
				for (unsigned iteration = 0; iteration < 6; iteration++) {
					std::array<std::array<float,5>,8> sums = {}; std::array<float,8> weights = {};
					for (unsigned j = 0; j < n; j++) {
						unsigned best = 0; float nearest = INFINITY;
						for (unsigned c = 0; c < k; c++) { float d = distance(features[j],centers[c]); if (d < nearest) { nearest=d; best=c; } }
						assignment[j] = best; weights[best] += powers[j];
						for (unsigned c = 0; c < 5; c++) sums[best][c] += features[j][c]*powers[j];
					}
					for (unsigned c = 0; c < k; c++) if (weights[c] > 0)
						for (unsigned d = 0; d < 5; d++) centers[c][d] = sums[c][d]/weights[c];
				}
				std::array<RemasterEmitterPatch,8> clusters;
				for (unsigned j = 0; j < n; j++) {
					auto &cluster = clusters[assignment[j]]; uint32_t p = pixels[start+j];
					cluster.members[cluster.count++] = p; cluster.mode |= 1u | (mesh[p].emissionDepth > 0 ? 2u : 0u);
				}
				for (unsigned c = 0; c < k; c++) if (clusters[c].count) result.push_back(clusters[c]);
			}
		}
		return result;
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
