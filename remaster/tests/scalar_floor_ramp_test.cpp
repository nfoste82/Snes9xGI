#include "../runtime/lighting_field_cache.h"
#include "../runtime/surface_mesh_cache.h"
#include <cassert>
#include <cstdio>

using namespace RemasterSurfaceMesh;

int main()
{
	constexpr unsigned w = 24, h = 48;
	std::vector<Sample> samples(w*h), flat;
	std::vector<float> scratch;
	for (unsigned y = 0; y < h; ++y)
		for (unsigned x = 0; x < w; ++x) {
			auto &s = samples[y*w+x];
			s = {float(-16 + 2*(int(y)/8)), 1, true, true, FloorDomain};
			s.scalarFloor = true;
			if (x >= 16) s.height += 20;
		}
	flat = samples;
	assert(reconstructScalarFloor(w, h, 1, samples, scratch));
	const auto mesh = build(w, h, samples, 1);
	for (unsigned y = 6; y < h-6; ++y) {
		const auto i = y*w + 8;
		const float expected = -16 + (float(y) - 3.5f) * 0.25f;
		assert(samples[i].height == flat[i].height);
		assert(samples[i].meshHeight() == expected);
		assert(mesh[i].corners[0] == expected - 0.125f);
		assert(mesh[i].corners[3] == expected + 0.125f);
		assert(mesh[y*w+16].corners[0] - mesh[y*w+15].corners[1] == 20);
	}
	Cache cache;
	cache.update(w, h, flat, 1);
	const auto &cached = cache.update(w, h, samples, 1);
	assert(!std::memcmp(mesh.data(), cached.data(), mesh.size()*sizeof(Cell)));
	// Negative control: even a small rise may not borrow through unknown or
	// explicitly modeled samples, and a repeated pass never integrates itself.
	std::vector<Sample> pair(2, {0, 1, true, true, FloorDomain});
	pair[0].scalarFloor = true; pair[1].height = 2;
	assert(!reconstructScalarFloor(2, 1, 1, pair, scratch));
	pair[1].scalarFloor = true; pair[1].coverage = 0;
	assert(!reconstructScalarFloor(2, 1, 1, pair, scratch));
	pair[1].coverage = 1; pair[1].height = 4;
	assert(!reconstructScalarFloor(2, 1, 1, pair, scratch));
	const auto first = samples;
	assert(reconstructScalarFloor(w, h, 1, samples, scratch));
	for (size_t i = 0; i < samples.size(); ++i) assert(samples[i].floorAdjustment == first[i].floorAdjustment);
	std::puts("scalar floor ramp PASS");
}
