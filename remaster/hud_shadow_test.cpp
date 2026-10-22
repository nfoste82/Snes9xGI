#include "lighting_field_cache.h"
#include <cassert>
#include <iostream>

int main()
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
	std::cout << "HUD overlay/world shadow separation PASS\n";
}
