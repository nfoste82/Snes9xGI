#include "../runtime/lighting_field_cache.h"
#include "../runtime/surface_mesh_cache.h"
#include <cassert>
#include <cstdio>

using namespace RemasterSurfaceMesh;

static void testOverworldPlacementLimits()
{
	RemasterFrame f;
	f.width = 2; f.height = 1;
	f.profileRomSha256 = "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
	f.mainPixels.resize(2); f.subPixels.resize(2);
	RemasterFrameAssetMetadata m;
	m.tileId = {123, 1, 4}; m.hasHeight = true;
	for (unsigned i = 0; i < 2; ++i) {
		RemasterFrameTileInstance t;
		t.tileId = m.tileId; t.source = RemasterSourceType::Background; t.sourceIndex = 1;
		t.savedPlacementOffsetStatus = i ? 3 : 2; // mixed calculated and authored
		f.tileInstances.push_back(t);
		f.mainPixels[i] = {uint32_t(RemasterSourceType::Background) << 24, i + 1, 0};
	}
	RemasterLightingFieldCache fields;
	Cache mesh;
	for (bool wall : {false, true}) {
		m.tileRole = wall ? RemasterTileRole::Wall : RemasterTileRole::Floor;
		m.height.fill(wall ? 10 : 0);
		f.assetMetadata = {m};
		const int limit = wall ? 6 : 12;
		for (int rise : {limit, -limit, limit + 1, -limit - 1, 6, 7}) {
			f.tileInstances[1].heightOffset = f.tileInstances[1].savedPlacementOffset = rise;
			for (uint32_t area : {321u, 320u, 0u, 320u}) {
				f.area.id = area;
				fields.update(f, false);
				const auto &cells = mesh.update(f.width, f.height, fields.samples, f.lightingCoordinateScale / 255.0f);
				const bool overworld = area == 320 || area == 321;
				assert(fields.samples[0].doubledPlacementScale == overworld);
				assert(fields.samples[1].doubledPlacementScale == overworld);
				assert(fields.height[2] == S9xRemasterSignedHeightField(rise, m.height[0]));
				assert((cells[0].corners[1] == cells[1].corners[0]) ==
					(std::abs(rise) <= (overworld ? limit : wall ? 1 : 6)));
				assert(fields.reconstructedSlopes == (overworld && std::abs(rise) <= 6));
				if (wall) assert(std::abs(fields.samples[1].meshHeight() - cells[1].wallBase -
					10 * f.lightingCoordinateScale / 255.0f) < 0.0001f);
			}
		}
	}
	// No expanded allowance if one endpoint lacks saved placement evidence.
	m.tileRole = RemasterTileRole::Floor; m.height.fill(0); f.assetMetadata = {m};
	f.tileInstances[1].heightOffset = 12;
	f.tileInstances[1].savedPlacementOffset = 0;
	f.tileInstances[1].savedPlacementOffsetStatus = 0;
	fields.update(f, false);
	assert(fields.samples[0].doubledPlacementScale && !fields.samples[1].doubledPlacementScale);
	const auto &cells = mesh.update(f.width, f.height, fields.samples, f.lightingCoordinateScale / 255.0f);
	assert(cells[0].corners[1] != cells[1].corners[0]);
}

int main()
{
	testOverworldPlacementLimits();
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
	assert(reconstructScalarSurfaces(w, h, 1, samples, scratch));
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
	assert(!reconstructScalarSurfaces(2, 1, 1, pair, scratch));
	pair[1].scalarFloor = true; pair[1].coverage = 0;
	assert(!reconstructScalarSurfaces(2, 1, 1, pair, scratch));
	pair[1].coverage = 1; pair[1].height = 4;
	assert(!reconstructScalarSurfaces(2, 1, 1, pair, scratch));
	const auto first = samples;
	assert(reconstructScalarSurfaces(w, h, 1, samples, scratch));
	for (size_t i = 0; i < samples.size(); ++i) assert(samples[i].scalarAdjustment == first[i].scalarAdjustment);

	// A wall slopes vertically in its own authored relief, while the saved
	// scalar base changes across horizontal tile seams. Only the base blends.
	std::vector<Sample> wall(w * 4);
	for (unsigned y = 0; y < 4; ++y)
		for (unsigned x = 0; x < w; ++x) {
			auto &s = wall[y*w+x];
			const float base = float(-16 + 3*(int(x)/8));
			s = {base + 5 + float(y), 1, true, true, WallDomain, true, base};
			s.scalarWall = true;
		}
	assert(reconstructScalarSurfaces(w, 4, 1, wall, scratch));
	const auto wallMesh = build(w, 4, wall, 1);
	for (unsigned y = 0; y < 4; ++y)
		for (unsigned x = 5; x < 19; ++x) {
			const auto i = y*w+x;
			assert(wallMesh[i].wallBase == -16 + (float(x) - 3.5f) * 0.375f);
			assert(wall[i].meshHeight() - wallMesh[i].wallBase == 5 + float(y));
			assert(wallMesh[i].corners[1] == wallMesh[i+1].corners[0]);
			assert(wallMesh[i].corners[2] == wallMesh[i+1].corners[3]);
		}
	// A small net rise with a large opposing jump in local relief can weld,
	// but must not cause a tile-wide redistribution of that placement base.
	pair.assign(2, wall.front());
	pair[1].wallBase -= 4; pair[1].height += 3;
	assert(!reconstructScalarSurfaces(2, 1, 1, pair, scratch));
	const auto seam = build(2, 1, pair, 1);
	assert(seam[0].corners[1] == seam[1].corners[0]);
	assert(seam[0].wallBase != seam[1].wallBase);
	std::puts("scalar floor and wall ramp PASS");
}
