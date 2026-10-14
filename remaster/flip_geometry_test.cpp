#include "remaster.h"
#include "npc_pose.h"
#include "lighting_field_cache.h"
#include <cassert>
#include <iostream>

int main()
{
	RemasterProfile profile; profile.gameTitle = "Bow flip geometry"; profile.romSha256 = std::string(64, 'a');
	const RemasterTileContentId bow{UINT64_C(0xd4863e268dedd8d6), 1, 4};
	auto &asset = profile.assets[bow]; asset.tileId = bow; asset.geometryByFlip = true;
	asset.hasHeight = true; asset.height.fill(9);
	for (unsigned flip = 0; flip < 4; flip++)
	{
		auto &g = S9xRemasterFlipGeometryForEditing(asset, flip + 1);
		g.hasHeight = g.hasNormals = true;
		g.normalXyz = S9xRemasterNpcUniformNormals({{128, 255, 128}});
		for (unsigned display = 0; display < 64; display++)
		{
			const unsigned canonical = S9xRemasterGeometryCanonicalPixel(display, flip);
			assert(S9xRemasterGeometryCanonicalPixel(canonical, flip) == display);
			g.height[canonical] = 10 * flip + display / 8;
		}
	}
	assert(!S9xRemasterNpcCanAuthorLayer(profile, bow, true, true));
	std::string text; std::vector<RemasterProfileDiagnostic> errors; RemasterProfile decoded;
	assert(S9xRemasterSerializeProfile(profile, text, errors) && S9xRemasterParseProfile(text, decoded, errors));
	assert(decoded.schemaVersion == 25 && decoded.assets.at(bow).geometryByFlip);
	assert(text.find("flip_vert_height") != std::string::npos);
	for (unsigned flip = 0; flip < 4; flip++) assert(decoded.assets.at(bow).flipGeometry[flip].height == asset.flipGeometry[flip].height);
	RemasterFrame frame; frame.width = 32; frame.height = 8;
	frame.mainPixels.resize(256); frame.subPixels.resize(256); frame.originalRgb555.resize(256); frame.tileInstances.resize(4);
	for (unsigned flip = 0; flip < 4; flip++)
	{
		auto &instance = frame.tileInstances[flip]; instance.tileId = bow; instance.source = RemasterSourceType::Object;
		instance.hFlip = flip & 1; instance.vFlip = flip & 2;
		instance.geometryFacing = 4; instance.normalYaw = 1; // Actor direction/yaw must not select or reflect a flip map.
		for (unsigned display = 0; display < 64; display++)
		{
			auto &pixel = frame.mainPixels[(display / 8) * 32 + flip * 8 + display % 8];
			pixel.instanceId = flip + 1; pixel.tilePixel = S9xRemasterGeometryCanonicalPixel(display, flip);
		}
	}
	S9xRemasterApplyProfileToFrame(profile, frame); RemasterLightingFieldCache cache; cache.update(frame, false);
	for (unsigned y = 0; y < 8; y++) for (unsigned x = 0; x < 32; x++)
	{
		const unsigned at = y * 32 + x;
		assert(std::fabs(cache.height[at * 2] * 255 - (10 * (x / 8) + y)) < 0.001f);
		assert(cache.surface[at * 4 + 2] > 0.99f); // V and H+V bow tiles retain authored +Y.
		assert(S9xRemasterNpcDebugColor(frame, at, RemasterNpcDebugView::Normals)[1] == 255);
	}
	cache.update(frame, false); assert(cache.updatedPixels == 0);
	asset.flipGeometry[2].height.fill(77); S9xRemasterApplyProfileToFrame(profile, frame); cache.update(frame, false);
	assert(std::fabs(cache.height[16 * 2] * 255 - 77) < 0.001f);
	assert(std::fabs(cache.height[24 * 2] * 255 - 30) < 0.001f);
	asset.flipGeometry[2].hasHeight = false; S9xRemasterApplyProfileToFrame(profile, frame); cache.update(frame, false);
	assert(std::fabs(cache.height[16 * 2] * 255 - 9) < 0.001f);
	// Changing only flip flags changes the selected set and invalidates cached geometry.
	frame.tileInstances[3].hFlip = false; cache.update(frame, false);
	assert(cache.updatedPixels == 64 && std::fabs(cache.height[24 * 2] * 255 - 9) < 0.001f);
	std::vector<uint8_t> bytes; RemasterFrame replay;
	assert(S9xSerializeRemasterFrame(frame, bytes) && S9xDeserializeRemasterFrame(bytes, replay));
	assert(replay.schemaVersion == 33 && replay.assetMetadata[0].geometryByFlip);
	assert(replay.assetMetadata[0].flipGeometry[3].height == asset.flipGeometry[3].height);
	asset.geometryByFlip = false;
	assert(S9xRemasterSerializeProfile(profile, text, errors) && S9xRemasterParseProfile(text, decoded, errors));
	assert(!decoded.assets.at(bow).geometryByFlip && decoded.assets.at(bow).flipGeometry[3].hasNormals);
	std::cout << "Flip geometry: four states, +Y bow normals, display/canonical mapping, gradients, persistence and cache PASS\n";
}
