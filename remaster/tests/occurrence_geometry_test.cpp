#include "../runtime/lighting_field_cache.h"
#include <cassert>
#include <iostream>

static RemasterFrame scene()
{
	RemasterFrame f; f.width = 3; f.height = 1; f.lightingCoordinateScale = 255;
	f.originalRgb555.resize(3); f.mainPixels.resize(3); f.subPixels.resize(3);
	RemasterFrameAssetMetadata m; m.tileId = {1234, 1, 4}; m.tileRole = RemasterTileRole::Wall;
	m.hasHeight = m.hasNormals = m.hasOcclusion = true;
	m.height.fill(99); m.normalXyz.fill(255); m.occlusion.fill(200);
	m.geometryByFacing = m.geometryByFlip = true;
	m.flipGeometry[3].hasHeight = true; m.flipGeometry[3].height.fill(300);
	m.flipGeometry[3].hasNormals = true; m.flipGeometry[3].normalXyz.fill(0);
	m.facingGeometry[0].hasHeight = m.facingGeometry[0].hasNormals = true;
	m.facingGeometry[0].height.fill(400); m.facingGeometry[0].normalXyz.fill(0);
	f.assetMetadata.push_back(m);
	RemasterFrameMaterial material; material.name = "wall"; material.surfaceClass = RemasterSurfaceClass::WallFace;
	f.materials.push_back(material);
	for (unsigned i = 0; i < 3; i++)
	{
		RemasterFrameTileInstance t; t.tileId = m.tileId; t.source = RemasterSourceType::Background;
		t.material = "wall"; t.hFlip = t.vFlip = true;
		t.geometryFacing = 1;
		t.geometryStatus = RemasterOccurrenceGeometryStatus::Resolved;
		t.placementTileRole = i == 0 ? RemasterTileRole::Wall : RemasterTileRole::Floor;
		t.heightOffset = -20; t.hasPlacementHeight = true;
		for (unsigned k = 0; k < 64; k++) t.placementHeight[k] = int(k) - 8;
		t.hasPlacementNormals = t.hasPlacementOcclusion = true;
		for (unsigned k = 0; k < 64; k++) {
			t.placementNormalXyz[k*3] = 255; t.placementNormalXyz[k*3+1] = 128; t.placementNormalXyz[k*3+2] = 128;
			t.placementOcclusion[k] = k;
		}
		if (i == 2) {
			t.geometryStatus = RemasterOccurrenceGeometryStatus::Unresolved;
			t.placementTileRole = RemasterTileRole::Unknown;
			t.hasPlacementNormals = t.hasPlacementOcclusion = false;
			// Even stale placement relief/base cannot bypass explicit unresolved.
			t.heightOffset = 999;
		}
		f.tileInstances.push_back(t); f.mainPixels[i].instanceId = i+1; f.mainPixels[i].tilePixel = 7;
	}
	return f;
}

int main()
{
	auto f = scene(); std::vector<uint8_t> bytes, again;
	assert(S9xSerializeRemasterFrame(f, bytes)); RemasterFrame replay;
	assert(S9xDeserializeRemasterFrame(bytes, replay)); assert(replay.schemaVersion == REMASTER_FRAME_SCHEMA_VERSION);
	assert(S9xSerializeRemasterFrame(replay, again) && bytes == again);
	// Truncated optional byte arrays cannot become silently absent overrides.
	for (size_t n = 0; n < bytes.size(); n++) {
		auto truncated = bytes; truncated.resize(n); RemasterFrame invalid;
		assert(!S9xDeserializeRemasterFrame(truncated, invalid));
	}
	for (unsigned i = 0; i < 2; i++) assert(RemasterLightingFieldCache::instanceEqual(f.tileInstances[i], replay.tileInstances[i]));
	assert(S9xRemasterInstanceGeometryUnresolved(replay.tileInstances[2]));
	for (bool byFlip : {false, true}) {
		auto m = f.assetMetadata[0]; m.geometryByFlip = byFlip;
		auto r = S9xRemasterResolveInstanceGeometry(m, f.tileInstances[0]);
		assert(r.height == f.tileInstances[0].placementHeight && r.normalXyz == f.tileInstances[0].placementNormalXyz);
		assert(r.occlusion == f.tileInstances[0].placementOcclusion && !r.normalIgnoreTileTransform);
		r = S9xRemasterResolveInstanceGeometry(m, f.tileInstances[2]);
		assert(!r.hasHeight && !r.hasNormals && !r.hasOcclusion && r.tileRole == RemasterTileRole::Unknown);
	}
	RemasterLightingFieldCache fields; fields.update(replay, false);
	RemasterLightingFieldCache full; full.update(f, false, true);
	assert(full.height == fields.height && full.surface == fields.surface && full.occlusion == fields.occlusion);
	assert(fields.surface[0] == -21 && fields.surface[4] == -21);
	assert(fields.surface[1] < -0.99f && fields.authoredNormals[0]); // vector flips once, pixels never reflip
	assert(fields.occlusion[0] == 7 && fields.occlusion[2] == 7);
	assert(fields.samples[0].solidWall && fields.samples[0].sheet);
	assert(!fields.samples[1].solidWall && fields.samples[1].sheet);
	assert(!fields.samples[2].known && !fields.samples[2].solidWall && !fields.samples[2].sheet);
	assert(!fields.height[5] && !fields.authoredNormals[2] && !fields.occlusion[5]);
	assert(S9xRemasterFrameHeightRange(replay) == std::make_pair(-21, -20));
	fields.update(replay, false); assert(fields.updatedPixels == 0);
	replay.tileInstances[0].placementTileRole = RemasterTileRole::Floor;
	fields.update(replay, false); assert(fields.updatedPixels == 1 && !fields.samples[0].solidWall);
	replay.tileInstances[0].placementNormalIgnoreTileTransform = true;
	replay.tileInstances[0].placementOcclusion[7] = 33;
	fields.update(replay, false); assert(fields.updatedPixels == 1 && fields.surface[1] > .99f && fields.occlusion[0] == 33);
	replay.tileInstances[1].geometryStatus = RemasterOccurrenceGeometryStatus::Conflict;
	replay.tileInstances[1].placementTileRole = RemasterTileRole::Unknown;
	replay.tileInstances[1].hasPlacementNormals = replay.tileInstances[1].hasPlacementOcclusion = false;
	fields.update(replay, false); assert(!fields.samples[1].known);
	// Overrides work without any asset metadata, too.
	replay.assetMetadata.clear(); fields.update(replay, false);
	assert(fields.surface[0] == -21 && fields.samples[0].sheet);
	// Invalid status/role/flag combinations are rejected by the writer.
	for (int kind = 0; kind < 4; kind++) {
		auto bad = f;
		auto &t = bad.tileInstances[0];
		if (kind == 0) t.geometryStatus = RemasterOccurrenceGeometryStatus::Count;
		if (kind == 1) t.placementTileRole = RemasterTileRole::Count;
		if (kind == 2) t.geometryStatus = RemasterOccurrenceGeometryStatus::Unresolved;
		if (kind == 3) { t.hasPlacementNormals = false; t.placementNormalIgnoreTileTransform = true; }
		assert(!S9xSerializeRemasterFrame(bad, again));
	}
	// Minimal one-instance v35 fixture: v36 added exactly three bytes after
	// palette artwork, before the four-byte empty palette-artwork table.
	RemasterFrame legacy; legacy.width = legacy.height = 1;
	legacy.originalRgb555.resize(1); legacy.mainPixels.resize(1); legacy.subPixels.resize(1);
	RemasterFrameTileInstance t; t.tileId = {1234,1,4}; legacy.tileInstances.push_back(t);
	assert(S9xSerializeRemasterFrame(legacy, bytes));
	const size_t extension = bytes.size()-7;
	for (unsigned field = 0; field < 3; field++) {
		auto corrupt = bytes; corrupt[extension+field] = 255;
		assert(!S9xDeserializeRemasterFrame(corrupt, replay));
	}
	for (size_t n = extension; n < bytes.size(); n++) {
		auto truncated = bytes; truncated.resize(n); assert(!S9xDeserializeRemasterFrame(truncated, replay));
	}
	bytes.erase(bytes.begin()+extension, bytes.begin()+extension+3); bytes[8] = 35;
	assert(S9xDeserializeRemasterFrame(bytes, replay));
	assert(replay.tileInstances[0].geometryStatus == RemasterOccurrenceGeometryStatus::Inherit);
	auto canonical = scene().assetMetadata[0];
	assert(S9xRemasterResolveInstanceGeometry(canonical, replay.tileInstances[0]).height[0] == 99);
	// Canonical metadata role also reaches structural policy and invalidates cache.
	f = scene(); f.tileInstances[0] = t; f.tileInstances[0].source = RemasterSourceType::Background;
	fields.update(f, false); assert(fields.samples[0].solidWall);
	f.assetMetadata[0].tileRole = RemasterTileRole::Floor;
	fields.update(f, false); assert(fields.updatedPixels && fields.samples[0].sheet && !fields.samples[0].solidWall);
	// Legacy diagonal normal repair must still compare original authored caps,
	// not the replacement heights returned by the effective resolver.
	f = scene(); auto &cap = f.assetMetadata[0]; cap.geometryByFlip = cap.geometryByFacing = false;
	cap.height.fill(24); cap.normalXyz.fill(128);
	for (int k = 0; k < 64; k++) cap.normalXyz[k*3+2] = 255;
	auto &diagonal = f.tileInstances[0]; diagonal.geometryStatus = RemasterOccurrenceGeometryStatus::Inherit;
	diagonal.placementTileRole = RemasterTileRole::Unknown;
	diagonal.hasPlacementNormals = diagonal.hasPlacementOcclusion = false;
	diagonal.hFlip = diagonal.vFlip = false;
	for (int k = 0; k < 64; k++) diagonal.placementHeight[k] = 31-k%8-k/8;
	f.mainPixels[0].tilePixel = 9;
	fields.update(f, false); assert(fields.surface[1] > .57f && fields.surface[2] > .57f);
	std::cout << "occurrence geometry v36 roundtrip/legacy/lighting/mesh regression passed\n";
}
