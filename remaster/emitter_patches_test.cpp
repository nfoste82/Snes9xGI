#include "emitter_patches.h"
#include <cassert>
int main()
{
	RemasterFrame frame;
	frame.width = frame.height = 8;
	frame.mainPixels.resize(64); frame.subPixels.resize(64);
	std::vector<uint8_t> emission(256, 255), participation(128, 255);
	std::vector<float> surface(256, 0);
	std::vector<RemasterSurfaceMesh::Cell> mesh(64);
	for (unsigned i = 0; i < 64; i++) {
		frame.mainPixels[i].owner = 0x02000001;
		frame.mainPixels[i].instanceId = 1;
		frame.mainPixels[i].tilePixel = i;
		surface[i*4+3] = 1;
	}
	auto build = [&] { return S9xRemasterEmitterPatches(frame, emission.data(), participation.data(), surface.data(), mesh); };
	assert(build().size() == 16);
	for (unsigned n = 1; n <= 5; n++) {
		frame.emitterPatchSize = n;
		auto patches = build();
		std::array<unsigned,64> seen = {};
		for (const auto &p : patches) {
			assert(p.count && p.count <= n*n);
			for (unsigned k = 0; k < p.count; k++) seen[p.members[k]]++;
		}
		for (unsigned count : seen) assert(count == 1);
		if (n == 1) assert(patches.size() == 64);
	}
	frame.emitterPatchSize = 2;
	emission[3] = 0;
	auto hole = build();
	for (const auto &p : hole) for (unsigned k = 0; k < p.count; k++) assert(p.members[k] != 0);
	emission[3] = 255;
	frame.mainPixels[0].instanceId = 2;
	assert(build()[0].count == 1);
	frame.mainPixels[0].instanceId = 1;
	emission[0] = 255; emission[1] = emission[2] = 0;
	assert(build()[0].count == 1);
	emission[1] = emission[2] = 255;
	mesh[0].emissionDepth = 2;
	assert(build()[0].count == 1);
	mesh[0].emissionDepth = 0;
	surface[0] = 0.5f;
	for (auto &corner : mesh[0].corners) corner = 0.5f;
	assert(build()[0].count == 1);
	surface[0] = 0;
	for (auto &corner : mesh[0].corners) corner = 0;
	surface[1] = 0.1f;
	assert(build()[0].count == 1);
	for (unsigned k = 1; k <= 8; k++) {
		frame.representativeEmitters = k;
		auto clusters = build(), repeated = build();
		assert(clusters.size() <= k && clusters.size() == repeated.size());
		std::array<unsigned,64> seen = {};
		for (unsigned c = 0; c < clusters.size(); c++) {
			assert(clusters[c].mode & 1u);
			assert(clusters[c].members == repeated[c].members);
			for (unsigned j = 0; j < clusters[c].count; j++) seen[clusters[c].members[j]]++;
		}
		for (unsigned n : seen) assert(n == 1);
		if (k == 1) assert(clusters[0].count == 64);
	}
	frame.representativeEmitters = 1;
	emission[3] = 0;
	assert(build()[0].count == 63);
	emission[3] = 255;
	frame.mainPixels[0].instanceId = 2;
	assert(build().size() == 2);
	frame.mainPixels[0].instanceId = 1;
	frame.subPixels[0] = frame.mainPixels[0];
	frame.mainPixels[0].instanceId = 0; frame.mainPixels[0].owner = 0;
	assert(build().size() == 2); // Selected main/sub channels cannot merge.
	mesh[0].emissionDepth = 2;
	frame.subPixels[0].tilePixel = 64;
	auto fallback = build();
	assert(fallback[0].count == 1 && fallback[0].mode == 3);
}
