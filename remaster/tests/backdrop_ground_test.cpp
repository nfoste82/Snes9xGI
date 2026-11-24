#include "../runtime/lighting_field_cache.h"
#include "../runtime/surface_mesh_cache.h"
#include <cstdlib>
#include <iostream>

static void check(bool value, const char *message)
{
	if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

static void testSignedCellDatum()
{
	RemasterFrame f;
	f.width = 5; f.height = 2; f.area.id = 320;
	f.profileRomSha256 = "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
	f.mainPixels.resize(10); f.subPixels.resize(10); f.observedBgPixels[1].resize(10);
	for (auto &p : f.mainPixels) p.owner = uint32_t(RemasterSourceType::Backdrop) << 24;
	RemasterFrameTileInstance tile;
	tile.tileId = {123, 1, 4}; tile.source = RemasterSourceType::Background; tile.sourceIndex = 1;
	tile.hFlip = tile.vFlip = true;
	tile.heightOffset = tile.savedPlacementOffset = -16; tile.savedPlacementOffsetStatus = 3;
	f.tileInstances.push_back(tile);
	// Cell boundary at x=3. A flipped opaque sample at x=1 proves the
	// datum of x=0 and x=2, never x=3 in the wholly unobserved next cell.
	f.observedBgPixels[1][1] = f.mainPixels[1] = {uint32_t(RemasterSourceType::Background) << 24, 1, 1};
	RemasterLightingFieldCache cache;
	cache.update(f, false);
	const auto z = S9xRemasterSignedHeightField(-16, 0);
	check(cache.height[0] == z && cache.height[4] == z && cache.height[6] == 0, "same clipped/flipped cell provides signed datum only inside that cell");
	check(cache.samples[0].domain == cache.samples[1].domain && cache.samples[0].height == cache.samples[1].height,
		"backdrop and visible provisional ground share the same height and domain");
	check(!S9xRemasterFrameVisibleInstanceAt(f, 0, 0) && !cache.authoredNormals[0], "no ownership or authored coverage invented");
	cache.update(f, false);
	check(cache.updatedPixels == 0, "unchanged signed backdrop cache reuse");
	f.tileInstances[0].savedPlacementOffset = -11;
	cache.update(f, false);
	check(cache.height[0] == S9xRemasterSignedHeightField(-11, 0), "datum changes invalidate otherwise unchanged transparent pixels");
	f.tileInstances[0].savedPlacementOffsetStatus = 1;
	cache.update(f, false);
	check(cache.height[0] == 0, "missing datum restores legacy backdrop");
	f.tileInstances[0].savedPlacementOffsetStatus = 2;
	f.tileInstances.push_back(f.tileInstances[0]); f.tileInstances[1].savedPlacementOffset = -2;
	f.observedBgPixels[1][2] = {uint32_t(RemasterSourceType::Background) << 24, 2, 0};
	cache.update(f, false);
	check(cache.height[0] == 0, "conflicting same-cell evidence does not assign a datum");
	using namespace RemasterSurfaceMesh;
	std::vector<Sample> samples(3, {0, 1, true, true, FloorDomain});
	samples[0].height = 2;
	samples[2].domain = uint64_t(3) << 48;
	Cache mesh;
	check(mesh.update(3, 1, samples, 1)[1].corners[1] == -1, "separate surface retains its boundary slope");
	samples[2].fallbackGround = true;
	const auto welded = mesh.update(3, 1, samples, 1);
	check(welded[1].corners[1] == 0 && welded[2].corners[0] == 0, "same-height floor seam welds in cached mesh");
	samples[2].wallBase = -2;
	const auto separate = mesh.update(3, 1, samples, 1);
	check(separate[1].corners[1] == -1 && separate[2].corners[0] == 0, "different placement datums remain separate");
}

int main(int argc, char **argv)
{
	// Exercise the incremental non-scroll cache on these synthetic frames.
	setenv("S9X_REMASTER_SHIFT_FIELDS", "0", 1);
	testSignedCellDatum();
	RemasterFrame frame;
	frame.width = 2; frame.height = 1;
	frame.area.id = 320;
	frame.profileRomSha256 = "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
	frame.mainPixels.resize(2); frame.subPixels.resize(2);
	for (auto &pixel : frame.mainPixels) pixel.owner = uint32_t(RemasterSourceType::Backdrop) << 24;
	RemasterFrameTileInstance tree;
	tree.source = RemasterSourceType::Background;
	tree.tileId.hash = 123; tree.tileId.hashVersion = 1; tree.tileId.bitDepth = 4;
	frame.tileInstances.push_back(tree);
	RemasterFrameAssetMetadata metadata;
	metadata.tileId = tree.tileId; metadata.hasHeight = true; metadata.height.fill(37);
	frame.assetMetadata.push_back(metadata);
	frame.mainPixels[1].instanceId = 1; frame.mainPixels[1].tilePixel = 0;
	frame.mainPixels[1].owner = uint32_t(RemasterSourceType::Background) << 24;
	RemasterLightingFieldCache cache;
	auto update = [&] { cache.update(frame, false); };
	update();
	check(cache.samples[0].fallbackGround && cache.samples[0].sheet && cache.samples[0].known,
		"outdoor backdrop is a provisional ground sheet");
	check(cache.height[0] == 0 && cache.height[1] == 1 && cache.surface[3] == 1,
		"backdrop zero height and +Z normal");
	check(cache.occlusion[0] == 255 && cache.samples[0].coverage == 1 && !cache.authoredNormals[0],
		"ground participates without invented authored normals");
	check(!cache.samples[1].fallbackGround && cache.height[2] == 37 / 255.0f,
		"authored tree height unchanged");
	check(!S9xRemasterFrameVisibleInstanceAt(frame, 0, 0), "backdrop does not fabricate a selectable tile");
	update(); check(cache.updatedPixels == 0, "unchanged backdrop cache reuse");
	frame.area.id = 0; update();
	check(!cache.samples[0].known && !cache.height[1], "indoor backdrop remains unknown after area change");
	frame.area.id = 321; update();
	check(cache.samples[0].fallbackGround, "dark-world backdrop supported");
	frame.profileRomSha256.clear(); update();
	check(!cache.samples[0].known, "other ROM backdrop remains unknown");
	frame.profileRomSha256 = "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
	frame.subPixels[0] = frame.mainPixels[1]; update();
	check(!cache.samples[0].fallbackGround && cache.height[0] == 37 / 255.0f,
		"real subscreen contributor wins over ground fallback");
	frame.subPixels[0] = {};
	frame.mainPixels[0].owner = 0xffffffffu; update();
	check(!cache.samples[0].known, "unsupported ownership is not ground");
	frame.mainPixels[0].owner = uint32_t(RemasterSourceType::Backdrop) << 24;
	frame.scenePixels = frame.mainPixels;
	cache.update(frame, true);
	check(cache.samples[0].fallbackGround, "independent scene backdrop supported");
	frame.scenePixels[0] = frame.mainPixels[1];
	cache.update(frame, true);
	check(!cache.samples[0].fallbackGround && cache.height[0] == 37 / 255.0f,
		"independent scene geometry not overwritten");
	for (int arg = 1; arg < argc; ++arg)
	{
		RemasterFrame captured;
		check(S9xReadRemasterFrame(argv[arg], captured), "read capture");
		cache.update(captured, false);
		const RemasterBackdropGroundBases bases(captured, true);
		size_t ground = 0, authored = 0;
		for (size_t i = 0; i < captured.mainPixels.size(); ++i)
		{
			if (S9xRemasterFrameBackdropGroundAt(captured, i))
			{
				++ground;
				check(cache.samples[i].fallbackGround && cache.height[i*2] ==
					S9xRemasterSignedHeightField(bases.at(i % captured.width, i / captured.width), 0) && cache.height[i*2+1] == 1,
					"capture backdrop ground has no height holes");
				check(!S9xRemasterFrameVisibleInstanceAt(captured, i % captured.width, i / captured.width),
					"capture backdrop has no selectable tile");
			}
			else
			{
				const auto *pixel = S9xRemasterFrameVisibleTilePixelAt(captured, i % captured.width, i / captured.width);
				if (!pixel || pixel->tilePixel >= 64) continue;
				const auto index = pixel->instanceId - 1;
				const auto *source = S9xRemasterFrameMetadataForTile(captured, captured.tileInstances[index].tileId);
				const auto resolved = S9xRemasterResolveFrameGeometry(source ? *source : RemasterFrameAssetMetadata{}, captured, index);
				if (!resolved.hasHeight || S9xRemasterHeldLampHudPixel(captured, i) ||
					S9xRemasterInstanceGeometryUnresolved(captured.tileInstances[index])) continue;
				++authored;
				check(cache.height[i*2] == S9xRemasterSignedHeightField(captured.tileInstances[index].heightOffset,
					resolved.height[pixel->tilePixel]) && !cache.samples[i].fallbackGround,
					"capture authored geometry preserved exactly");
			}
		}
		check(ground > 0 && authored > 0, "fixture contains backdrop ground and authored geometry");
		std::cout << argv[arg] << ": " << ground << " provisional backdrop pixels, "
			<< authored << " authored height pixels preserved\n";
	}
	std::cout << "backdrop ground PASS\n";
}
