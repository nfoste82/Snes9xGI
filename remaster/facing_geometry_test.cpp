#include "remaster.h"
#include "npc_pose.h"
#include "lighting_field_cache.h"
#include <cassert>
#include <iostream>

int main()
{
	RemasterProfile p; p.schemaVersion = 23; p.gameTitle = "Facing geometry"; p.romSha256 = std::string(64, 'a');
	const RemasterTileContentId bow{UINT64_C(0xd4863e268dedd8d6), 1, 4};
	auto &a = p.assets[bow]; a.tileId = bow; a.geometryByFacing = true;
	a.hasHeight = true; a.height.fill(9); a.hasNormals = true; a.normalXyz = S9xRemasterNpcUniformNormals({{128,128,255}});
	for (unsigned f = 1; f <= 4; f++)
	{
		auto &g = S9xRemasterGeometryForEditing(a, f);
		g.hasHeight = g.hasNormals = true; g.height.fill(f * 10);
		g.normalXyz = S9xRemasterNpcUniformNormals({{uint8_t(f * 40),255,128}});
		g.heightSampling = f == 2 ? RemasterHeightSampling::Linear : RemasterHeightSampling::Nearest;
	}
	assert(a.height[0] == 9 && a.facingGeometry[0].height[0] == 10 && a.facingGeometry[3].height[0] == 40);
	assert(!S9xRemasterNpcCanAuthorLayer(p, bow, true, true) && !S9xRemasterNpcCanAuthorLayer(p, bow, false, true));
	std::string text; std::vector<RemasterProfileDiagnostic> errors;
	assert(S9xRemasterSerializeProfile(p, text, errors));
	RemasterProfile decoded; assert(S9xRemasterParseProfile(text, decoded, errors));
	assert(decoded.schemaVersion == 24 && decoded.assets.at(bow).geometryByFacing);
	for (unsigned f = 0; f < 4; f++)
	{
		assert(decoded.assets.at(bow).facingGeometry[f].height == a.facingGeometry[f].height);
		assert(decoded.assets.at(bow).facingGeometry[f].normalXyz == a.facingGeometry[f].normalXyz);
		assert(decoded.assets.at(bow).facingGeometry[f].heightSampling == a.facingGeometry[f].heightSampling);
	}
	a.geometryByFacing = false;
	assert(S9xRemasterGeometryForEditing(a, 1).height[0] == 9);
	assert(S9xRemasterSerializeProfile(p, text, errors) && S9xRemasterParseProfile(text, decoded, errors));
	assert(!decoded.assets.at(bow).geometryByFacing && decoded.assets.at(bow).facingGeometry[3].height[0] == 40);
	a.geometryByFacing = true;
	RemasterFrame frame; frame.width = 5; frame.height = 1;
	frame.mainPixels.resize(5); frame.subPixels.resize(5); frame.originalRgb555.resize(5);
	frame.tileInstances.resize(5);
	for (unsigned f = 0; f < 5; f++) { frame.tileInstances[f].tileId = bow; frame.tileInstances[f].source = RemasterSourceType::Object; frame.tileInstances[f].geometryFacing = f; frame.mainPixels[f].instanceId = f + 1; frame.mainPixels[f].tilePixel = 0; }
	S9xRemasterApplyProfileToFrame(p, frame);
	RemasterLightingFieldCache cache; cache.update(frame, false);
	for (unsigned f = 0; f < 5; f++) assert(std::fabs(cache.height[f*2] * 255 - (f ? f*10 : 9)) < 0.001f);
	cache.update(frame, false); assert(cache.updatedPixels == 0);
	a.facingGeometry[1].height.fill(77); S9xRemasterApplyProfileToFrame(p, frame); cache.update(frame, false);
	assert(std::fabs(cache.height[4] * 255 - 77) < 0.001f);
	assert(std::fabs(cache.height[2] * 255 - 10) < 0.001f);
	a.facingGeometry[1].hasHeight = false; a.facingGeometry[1].heightSampling = RemasterHeightSampling::Nearest;
	S9xRemasterApplyProfileToFrame(p, frame); cache.update(frame, false);
	assert(std::fabs(cache.height[4] * 255 - 9) < 0.001f); // fallback is per-layer
	assert(S9xRemasterNpcDebugColor(frame, 2, RemasterNpcDebugView::Normals)[1] > 200);
	std::vector<uint8_t> bytes; RemasterFrame replay;
	assert(S9xSerializeRemasterFrame(frame, bytes) && S9xDeserializeRemasterFrame(bytes, replay));
	assert(replay.schemaVersion == 33 && replay.assetMetadata[0].geometryByFacing);
	assert(replay.tileInstances[4].geometryFacing == 4 && replay.assetMetadata[0].facingGeometry[3].height[0] == 40);
	bytes.pop_back(); assert(!S9xDeserializeRemasterFrame(bytes, replay));
	std::cout << "Facing-specific geometry editing isolation, persistence, fallback and lighting passed\n";
}
