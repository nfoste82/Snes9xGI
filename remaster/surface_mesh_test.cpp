#include "surface_mesh_material.h"
#include "surface_mesh_cache.h"
#include <cstring>
#include <cstdio>

int main()
{
	using namespace RemasterSurfaceMesh;
	unsigned checks = 0, failures = 0;
	auto check = [&](const char *name, bool pass) {
		checks++; if (!pass) { failures++; std::fprintf(stderr, "FAIL %s\n", name); }
	};
	auto near = [](float a, float b) { return std::abs(a - b) < 0.0001f; };
	// Exact differential oracle: sparse edits, moving footprints, viewport
	// borders, continuity changes and quantum/size invalidation across frames.
	{
		Cache cache;
		unsigned seed = 9127;
		auto random = [&]() { seed = seed * 1664525u + 1013904223u; return seed; };
		unsigned width = 67, height = 53;
		std::vector<Sample> input(width * height);
		for (auto &s : input) { s.known = true; s.coverage = 1; s.sheet = true; s.domain = 1; }
		bool equal = true, reuse = false;
		for (unsigned frame = 0; frame < 240; frame++)
		{
			if (frame == 120) { width = 71; height = 49; input.resize(width * height); }
			if (frame == 80)
				for (unsigned y = 0; y < height; y++) for (unsigned x = 0; x < width; x++)
				{
					Sample &s = input[size_t(y) * width + x];
					s = Sample{}; s.known = true; s.coverage = 1; s.sheet = true; s.domain = 1;
					s.height = float(x * 2 + y * 3);
				}
			for (unsigned edit = 0; edit < frame % 9; edit++)
			{
				Sample &s = input[random() % input.size()];
				switch (random() % 7)
				{
					case 0: s.height = float(random() % 32); break;
					case 1: s.known = !s.known; break;
					case 2: s.coverage = float(random() % 3) / 2; break;
					case 3: s.domain = random() % 4; break;
					case 4: s.sheet = !s.sheet; break;
					case 5: s.solidWall = !s.solidWall; break;
					case 6: s.wallBase = float(random() % 16); break;
				}
			}
			const float quantum = frame < 160 ? 0.25f : 1.0f;
			const auto full = build(width, height, input, quantum);
			const auto &cached = cache.update(width, height, input, quantum);
			equal &= cached.size() == full.size() && std::memcmp(cached.data(), full.data(), full.size() * sizeof(Cell)) == 0;
			reuse |= cache.rebuiltTiles == 0;
		}
		check("incremental cache equals full geometry after changing resolved inputs", equal);
		check("unchanged geometry skips reconstruction", reuse);
	}
	constexpr unsigned w = 16, h = 16;
	std::vector<Sample> samples(w * h);
	for (Sample &s : samples) { s.known = true; s.coverage = 1; s.sheet = true; s.domain = 1; }
	auto mesh = build(w, h, samples, 0.25f);
	check("continuous floor uses sheets", mesh[0].thickness == 0);
	check("flat floor corners and normal", near(mesh[127].corners[0], 0) && near(mesh[127].normal[2], 1));
	for (unsigned y = 0; y < h; y++) for (unsigned x = 0; x < w; x++) samples[y * w + x].height = 2 * (x + 0.5f) + 3 * (y + 0.5f);
	mesh = build(w, h, samples, 0.25f);
	bool exact = true, seams = true;
	for (unsigned y = 0; y < h; y++) for (unsigned x = 0; x < w; x++)
	{
		const Cell &c = mesh[y * w + x];
		exact &= near(c.corners[0], 2 * x + 3 * y) && near(c.corners[2], 2 * (x + 1) + 3 * (y + 1));
		if (x + 1 < w) seams &= near(c.corners[1], mesh[y * w + x + 1].corners[0]) && near(c.corners[2], mesh[y * w + x + 1].corners[3]);
	}
	check("steep planar ramp reconstructs exact corners including borders", exact);
	check("wall ramps weld across 8x8 tile seams", seams);
	check("geometric ramp normal", near(mesh[0].normal[0], -2 / std::sqrt(14.0f)) && near(mesh[0].normal[1], -3 / std::sqrt(14.0f)));
	for (bool inside : {false, true})
	{
		for (unsigned y = 0; y < h; y++) for (unsigned x = 0; x < w; x++)
			samples[y * w + x].height = inside ? std::max(float(x), float(y)) : std::min(float(x), float(y));
		mesh = build(w, h, samples, 1);
		bool joined = true;
		for (unsigned y = 0; y + 1 < h; y++) for (unsigned x = 0; x + 1 < w; x++)
			joined &= near(mesh[y * w + x].corners[2], mesh[y * w + x + 1].corners[3]) &&
				near(mesh[y * w + x].corners[2], mesh[(y + 1) * w + x].corners[1]);
		check(inside ? "inside corner has welded seams" : "outside corner has welded seams", joined);
		const Cell &a = mesh[2 * w + 12], &b = mesh[12 * w + 2];
		check("corner keeps distinct face normals", std::abs(a.normal[0] - b.normal[0]) > 0.5f && std::abs(a.normal[1] - b.normal[1]) > 0.5f);
	}
	for (unsigned y = 0; y < h; y++) for (unsigned x = 0; x < w; x++) samples[y * w + x].height = x < 8 ? 0 : 20;
	mesh = build(w, h, samples, 0.25f);
	check("hard height edge is not stretched into a ramp", near(mesh[7].corners[1], 0) && near(mesh[8].corners[0], 20));
	for (unsigned y = 0; y < h; y++) for (unsigned x = 0; x < w; x++)
	{
		samples[y * w + x].height = float(x);
		samples[y * w + x].sheet = false;
		samples[y * w + x].domain = x < 8 ? 100 : 101;
	}
	mesh = build(w, h, samples, 0.25f);
	check("objects retain finite thickness", mesh[7].thickness == 1);
	// Change the second object's height: first object's geometry must not change.
	const Cell before = mesh[7];
	for (unsigned y = 0; y < h; y++) for (unsigned x = 8; x < w; x++) samples[y * w + x].height += 30;
	mesh = build(w, h, samples, 0.25f);
	check("touching dynamic objects do not weld", near(before.corners[1], mesh[7].corners[1]));
	for (Sample &s : samples) { s.height = 0; s.domain = 1; s.sheet = true; }
	samples[7].coverage = 0; samples[8].height = 30; samples[8].known = false;
	mesh = build(w, h, samples, 0.25f);
	check("transparent and unknown samples do not deform neighbors", near(mesh[6].corners[1], 0) && near(mesh[9].corners[0], 0));
	// The unclassified jail crossbar (c43a8c5e3814af21) is a finite
	// elevated detail, not an extension of the wall it touches. Shading normals
	// are deliberately not inputs to the production classification helper.
	for (auto surfaceClass : {RemasterSurfaceClass::Unclassified, RemasterSurfaceClass::Prop,
		RemasterSurfaceClass::WallFace, RemasterSurfaceClass::WallTop, RemasterSurfaceClass::Floor})
	{
		std::vector<Sample> pair(2);
		for (auto &s : pair) { s.known = true; s.coverage = 1; s.height = 50; s.wallBase = 7; }
		classify(pair[0], RemasterSourceType::Background, 1, RemasterSurfaceClass::WallFace, "wall_faces", 1);
		classify(pair[1], RemasterSourceType::Background, 1, surfaceClass, "", 2);
		const auto adjacent = build(2, 1, pair, 1);
		const bool structural = surfaceClass == RemasterSurfaceClass::WallFace || surfaceClass == RemasterSurfaceClass::WallTop;
		check("only explicitly structural materials receive base closure", adjacent[1].solidWall == float(structural));
		check("wall placement base and authored surface height survive", near(adjacent[0].wallBase, 7) && near(adjacent[0].corners[0], 50));
		if (surfaceClass == RemasterSurfaceClass::Unclassified || surfaceClass == RemasterSurfaceClass::Prop)
			check("elevated detail touching wall retains unit finite shell", adjacent[1].thickness == 1 && near(adjacent[1].corners[0], 50));
	}
	Sample rail, sprite;
	classify(rail, RemasterSourceType::Background, 1, RemasterSurfaceClass::WallTop, "stair_rails", 3);
	classify(sprite, RemasterSourceType::Object, 1, RemasterSurfaceClass::WallFace, "wall_faces", 4);
	check("stair rail wall-top material stays finite", !rail.sheet && !rail.solidWall);
	check("sprite wall material stays finite", !sprite.sheet && !sprite.solidWall);
	for (Sample &s : samples) { s = {}; s.height = 6; s.known = true; s.coverage = 1; s.domain = 11; }
	for (unsigned y = 0; y < h; y++) for (unsigned x = 12; x < w; x++) samples[y * w + x].height = 17 - float(x);
	mesh = build(w, h, samples, 1);
	bool noRidge = true, welded = true;
	for (unsigned y = 0; y < h; y++) for (unsigned x = 0; x < w; x++)
	{
		for (float z : mesh[y * w + x].corners) noRidge &= z <= 6.0001f;
		if (x + 1 < w) welded &= near(mesh[y * w + x].corners[1], mesh[y * w + x + 1].corners[0]);
	}
	check("flat tabletop joining lower edge never reconstructs a raised ridge", noRidge);
	check("monotone tabletop lower edge still welds", welded);
	check("monotone lower edge retains its ramp", mesh[14].corners[0] > mesh[14].corners[1]);
	for (auto surfaceClass : {RemasterSurfaceClass::Unclassified, RemasterSurfaceClass::Prop})
	{
		for (unsigned y = 0; y < h; y++) for (unsigned x = 0; x < w; x++)
		{
			Sample &s = samples[y * w + x]; s = {};
			s.height = 6; s.known = true; s.coverage = 1;
			classify(s, RemasterSourceType::Background, 1, surfaceClass, "", 1 + (y / 8) * 2 + x / 8);
		}
		mesh = build(w, h, samples, 200.0f / 255);
		bool flat = true;
		for (const Cell &c : mesh)
		{
			flat &= c.thickness == 1 && c.solidWall == 0 && near(c.normal[2], 1);
			for (float z : c.corners) flat &= near(z, 6);
		}
		check("flat tiled tabletop shells stay coplanar without assembly grouping", flat);
		if (surfaceClass == RemasterSurfaceClass::Prop)
			check("prop draw domains remain separate at tabletop seam", samples[7].domain != samples[8].domain);
	}
	std::printf("%u/%u mesh checks passed; %u failed\n", checks - failures, checks, failures);
	return failures ? 1 : 0;
}
