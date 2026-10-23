#include "../runtime/lighting_field_cache.h"
#include "../runtime/surface_mesh_cache.h"
#include "../runtime/surface_mesh_debug.h"
#include <cassert>
#include <iostream>
#include <memory>

int main(int argc, char **argv)
{
	RemasterFrame f;
	f.width = 8; f.height = 1; f.lightingCoordinateScale = 255;
	f.mainPixels.resize(8); f.subPixels.resize(8); f.originalRgb555.resize(8);
	for (unsigned i = 0; i < 8; i++) {
		RemasterFrameTileInstance t; t.tileId = {i+1,1,4}; t.source = RemasterSourceType::Background;
		f.tileInstances.push_back(t); f.mainPixels[i].instanceId = i+1; f.mainPixels[i].tilePixel = 0;
	}
	f.tileInstances[1].heightOffset = -32;
	f.tileInstances[2].source = RemasterSourceType::Object;
	f.tileInstances[3].material = "ui";
	RemasterFrameMaterial ui; ui.name = "ui"; ui.surfaceClass = RemasterSurfaceClass::UserInterface;
	f.materials.push_back(ui);
	f.tileInstances[4].geometryStatus = RemasterOccurrenceGeometryStatus::Unresolved;
	f.tileInstances[5].geometryStatus = RemasterOccurrenceGeometryStatus::Conflict;
	RemasterFrameAssetMetadata normal; normal.tileId = f.tileInstances[6].tileId;
	normal.hasNormals = true;
	for (unsigned p = 0; p < 64; p++) { normal.normalXyz[p*3]=255; normal.normalXyz[p*3+1]=normal.normalXyz[p*3+2]=128; }
	f.assetMetadata.push_back(normal);
	RemasterFrameAssetMetadata authored; authored.tileId = f.tileInstances[7].tileId;
	authored.hasHeight = true; authored.height.fill(10); authored.tileRole = RemasterTileRole::Obstacle;
	f.assetMetadata.push_back(authored);
	RemasterLightingFieldCache fields;
	RemasterSurfaceMesh::Cache cache;
	auto checkCache = [&]() {
		fields.update(f,false);
		RemasterLightingFieldCache full; full.update(f,false,true);
		assert(fields.height == full.height && fields.surface == full.surface && fields.occlusion == full.occlusion);
		const auto &cached = cache.update(f.width,f.height,fields.samples,1);
		const auto built = RemasterSurfaceMesh::build(f.width,f.height,full.samples,1);
		assert(std::memcmp(cached.data(),built.data(),built.size()*sizeof(built[0])) == 0);
		return built;
	};
	auto mesh = checkCache();
	assert(fields.samples[0].known && fields.samples[0].fallbackGround && fields.samples[0].sheet);
	assert(fields.surface[3] == 1 && !fields.authoredNormals[0]);
	assert(mesh[0].thickness == 0 && mesh[0].solidWall == 0);
	for (float z : mesh[0].corners) assert(z == 0);
	auto debug = RemasterSurfaceMesh::debugView(f.width,f.height,nullptr,fields.samples,mesh,16);
	bool green = false;
	for (size_t i = 0; i < debug.size(); i += 4)
		green |= debug[i+1] > debug[i] && debug[i] > debug[i+2];
	assert(green);
	assert(fields.samples[1].height == -32 && fields.samples[1].fallbackGround);
	for (float z : mesh[1].corners) assert(z == -32);
	for (unsigned i : {2u,3u,4u,5u}) assert(!fields.samples[i].known && !fields.samples[i].fallbackGround);
	assert(fields.samples[6].fallbackGround && fields.authoredNormals[6] && fields.surface[6*4+1] > .99f);
	assert(!fields.samples[7].fallbackGround && mesh[7].thickness == 1 && fields.samples[7].height == 10);
	checkCache(); assert(fields.updatedPixels == 0);
	// Live authoring and undo must replace/remove the provisional geometry.
	authored.tileId = f.tileInstances[0].tileId; authored.height.fill(20);
	f.assetMetadata.push_back(authored); mesh = checkCache();
	assert(!fields.samples[0].fallbackGround && mesh[0].thickness == 1 && fields.samples[0].height == 20);
	f.assetMetadata.pop_back(); mesh = checkCache(); assert(fields.samples[0].fallbackGround && mesh[0].thickness == 0);
	// Placement relief without canonical metadata is authoritative too.
	f.tileInstances[0].hasPlacementHeight = true; f.tileInstances[0].placementHeight.fill(7);
	mesh = checkCache(); assert(!fields.samples[0].fallbackGround && fields.samples[0].height == 7);
	f.tileInstances[0].hasPlacementHeight = false;
	f.tileInstances[0].source = RemasterSourceType::Object;
	mesh = checkCache(); assert(!fields.samples[0].known);
	f.tileInstances[0].source = RemasterSourceType::Background;
	// Same fallback must work through subscreen ownership and scene-channel input.
	f.subPixels = f.mainPixels;
	for (auto &p : f.mainPixels) { p.instanceId = 0; p.owner = uint32_t(RemasterSourceType::Backdrop)<<24; }
	mesh = checkCache(); assert(fields.samples[0].fallbackGround);
	f.scenePixels = f.subPixels;
	RemasterLightingFieldCache scene; scene.update(f,true); assert(scene.samples[0].fallbackGround);
	// Absent ownership is not a fabricated surface, even with a previous cache entry.
	f.mainPixels[0].owner = 0; mesh = checkCache(); assert(!fields.samples[0].known);
	for (int arg = 1; arg < argc; arg++) {
		auto capture = std::make_unique<RemasterFrame>(); assert(S9xReadRemasterFrame(argv[arg],*capture));
		RemasterLightingFieldCache actual; actual.update(*capture,false);
		unsigned fallback = 0;
		for (size_t i = 0; i < actual.samples.size(); i++) if (actual.samples[i].fallbackGround) {
			const auto &s = actual.samples[i]; assert(s.known && s.sheet && !s.solidWall); fallback++;
		}
		assert(fallback > 0);
		std::cout << argv[arg] << ": " << fallback << " provisional ground pixels\n";
	}
	std::cout << "fallback ground, exclusion, authored precedence and cache transitions PASS\n";
}
