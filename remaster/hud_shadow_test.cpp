#include "lighting_field_cache.h"
#include <cassert>
#include <iostream>

int main(int argc, char **argv)
{
	auto frame = std::make_unique<RemasterFrame>();
	auto &f = *frame;
	f.width = 256; f.height = 1; f.lightingCoordinateScale = 255;
	f.profileRomSha256 = "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
	f.mainPixels.resize(256); f.subPixels.resize(256);
	RemasterFrameTileInstance tree;
	tree.tileId = {123, 1, 4}; tree.source = RemasterSourceType::Background;
	tree.sourceIndex = 1;
	f.tileInstances.push_back(tree);
	RemasterFrameAssetMetadata geometry;
	geometry.tileId = tree.tileId; geometry.hasHeight = true; geometry.height.fill(40);
	f.assetMetadata.push_back(geometry);
	for (auto &p : f.mainPixels) p = {0x02010000, 1, 0};
	f.scenePixels = f.mainPixels;
	f.mainPixels[12] = {0x02020000, 0, 0xff}; // unknown BG3 HUD ink
	RemasterLightingFieldCache fields, unobscured;
	unobscured.update(f, true);
	assert(unobscured.samples[12].known && unobscured.samples[12].height == 40);
	assert(unobscured.participation[24] == 0 && unobscured.participation[25] == 0);
	assert(unobscured.occlusion[24] == 255 && unobscured.height[25] == 1);
	fields.update(f, false); // legacy captures have no hidden-world evidence
	assert(!fields.samples[12].known && fields.occlusion[24] == 0);
	fields.update(f, true);
	assert(fields.samples[12].known);
	auto hiddenMesh = RemasterSurfaceMesh::build(256, 1, fields.samples, 1);
	f.mainPixels = f.scenePixels;
	fields.update(f, true); // same underlying identity, HUD participation changed
	assert(fields.participation[24] == 255 && fields.participation[25] == 255);
	auto clearMesh = RemasterSurfaceMesh::build(256, 1, fields.samples, 1);
	for (size_t i = 0; i < hiddenMesh.size(); i++)
		assert(std::memcmp(&hiddenMesh[i], &clearMesh[i], sizeof(hiddenMesh[i])) == 0);
	f.mainPixels[12] = {0x02020000, 0, 0xff};
	fields.update(f, true);
	assert(fields.participation[24] == 0 && fields.samples[12].known);
	// Even a scene that still contains BG3 must never turn the HUD into geometry.
	f.scenePixels[12] = f.mainPixels[12]; fields.update(f, true);
	assert(!fields.samples[12].known && fields.occlusion[24] == 0);
	// Provisional BG ground remains continuous too, without inventing relief.
	f.scenePixels[12] = f.scenePixels[11]; f.assetMetadata[0].hasHeight = false;
	fields.update(f, true);
	assert(fields.samples[12].known && fields.samples[12].fallbackGround && fields.samples[12].height == 0);
	// Indoor BG1 and BG2 use their actual signed placement bases/relief,
	// including successive scene frames while stairs/scroll change ownership.
	for (unsigned layer : {0u, 1u})
	{
		f.tileInstances[0].sourceIndex = layer;
		f.tileInstances[0].heightOffset = -16;
		f.tileInstances[0].hFlip = true;
		f.assetMetadata[0].hasHeight = true;
		for (int p = 0; p < 64; p++) f.assetMetadata[0].height[p] = p;
		for (size_t i = 0; i < f.scenePixels.size(); i++)
			f.scenePixels[i] = {0x02000000u | (layer << 16), 1, uint8_t(i % 64)};
		f.mainPixels = f.scenePixels;
		f.mainPixels[12] = {0x02020000, 0, 0xff};
		fields.update(f, true);
		RemasterLightingFieldCache full;
		full.update(f, true);
		assert(fields.samples[12].known && fields.samples[12].height == full.samples[12].height);
		assert(fields.samples[12].height == -16 + 12); // tilePixel is already canonical
		assert(fields.participation[24] == 0 && fields.occlusion[24] == 255);
		hiddenMesh = RemasterSurfaceMesh::build(256, 1, fields.samples, 1);
		f.mainPixels = f.scenePixels;
		fields.update(f, true);
		clearMesh = RemasterSurfaceMesh::build(256, 1, fields.samples, 1);
		for (size_t i = 0; i < hiddenMesh.size(); i++)
			assert(std::memcmp(&hiddenMesh[i], &clearMesh[i], sizeof(hiddenMesh[i])) == 0);
	}
	for (int argument = 1; argument < argc; argument++)
	{
		auto capture = std::make_unique<RemasterFrame>();
		assert(S9xReadRemasterFrame(argv[argument], *capture));
		assert(capture->scenePixels.size() == capture->mainPixels.size());
		assert(capture->sceneRgb555 == capture->originalRgb555);
		RemasterLightingFieldCache covered, clear;
		covered.update(*capture, true);
		size_t recovered = 0;
		for (size_t i = 0; i < capture->mainPixels.size(); i++)
			if (S9xRemasterHeldLampHudPixel(*capture, i) && covered.samples[i].known) {
				recovered++;
				assert(!covered.participation[i*2] && !covered.participation[i*2+1]);
			}
		capture->mainPixels = capture->scenePixels;
		clear.update(*capture, true);
		auto a = RemasterSurfaceMesh::build(capture->width, capture->height, covered.samples, capture->lightingCoordinateScale/255);
		auto b = RemasterSurfaceMesh::build(capture->width, capture->height, clear.samples, capture->lightingCoordinateScale/255);
		assert(a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size()*sizeof(a[0])) == 0);
		assert(recovered > 0);
		std::cout << argv[argument] << ": " << recovered << " HUD-covered world pixels; exact uncovered mesh/stock RGB\n";
	}
	std::cout << "HUD overlay/world shadow separation PASS\n";
}
