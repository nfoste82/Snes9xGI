#include "../runtime/remaster.h"
#include "../runtime/npc_pose.h"
#include "../runtime/lighting_field_cache.h"
#include <cassert>
#include <iostream>

int main()
{
	const RemasterTileContentId bow{UINT64_C(0xd4863e268dedd8d6), 1, 4};
	RemasterProfile profile;
	profile.schemaVersion = 22; profile.romSha256 = std::string(64, 'a');
	profile.gameTitle = "Normal tile transform regression";
	auto &asset = profile.assets[bow]; asset.tileId = bow;
	asset.hasNormals = true;
	asset.normalXyz = S9xRemasterNpcUniformNormals({{128, 255, 128}});
	std::vector<RemasterProfileDiagnostic> errors;
	std::string text;
	assert(S9xRemasterSerializeProfile(profile, text, errors));
	RemasterProfile decoded;
	assert(S9xRemasterParseProfile(text, decoded, errors));
	assert(!decoded.assets.at(bow).normalIgnoreTileTransform);
	asset.normalIgnoreTileTransform = true;
	assert(S9xRemasterSerializeProfile(profile, text, errors));
	assert(text.find("schema_version = 23") != std::string::npos);
	assert(S9xRemasterParseProfile(text, decoded, errors));
	assert(decoded.assets.at(bow).normalIgnoreTileTransform);
	std::string old = text; old.replace(old.find("schema_version = 23"), 19, "schema_version = 22");
	assert(!S9xRemasterParseProfile(old, decoded, errors));
	std::string invalid = text;
	invalid.replace(invalid.find("normal_ignore_tile_transform = true"), 34, "normal_ignore_tile_transform = 1");
	assert(!S9xRemasterParseProfile(invalid, decoded, errors));
	// A tile option is retained even before authoring a normal array.
	asset.hasNormals = false;
	assert(S9xRemasterSerializeProfile(profile, text, errors));
	assert(S9xRemasterParseProfile(text, decoded, errors));
	assert(decoded.assets.at(bow).normalIgnoreTileTransform);
	asset.hasNormals = true;
	RemasterFrame frame;
	frame.width = 4; frame.height = 1;
	frame.mainPixels.resize(4); frame.subPixels.resize(4);
	frame.originalRgb555.resize(4);
	frame.tileInstances.resize(4);
	for (unsigned i = 0; i < 4; i++)
	{
		auto &instance = frame.tileInstances[i]; instance.tileId = bow; instance.source = RemasterSourceType::Object;
		instance.hFlip = i & 1; instance.vFlip = i & 2;
		frame.mainPixels[i].instanceId = i + 1; frame.mainPixels[i].tilePixel = i * 9;
	}
	S9xRemasterApplyProfileToFrame(profile, frame);
	RemasterLightingFieldCache cache; cache.update(frame, false);
	for (unsigned i = 0; i < 4; i++)
	{
		assert(cache.surface[i * 4 + 2] > 0.99f);
		assert(S9xRemasterNpcDebugColor(frame, i, RemasterNpcDebugView::Normals)[1] == 255);
	}
	cache.update(frame, false); assert(cache.updatedPixels == 0);
	asset.normalIgnoreTileTransform = false;
	S9xRemasterApplyProfileToFrame(profile, frame); cache.update(frame, false);
	assert(cache.updatedPixels == 4); // Option changes invalidate cached lighting.
	assert(cache.surface[2 * 4 + 2] < -0.99f);
	assert(S9xRemasterNpcDebugColor(frame, 2, RemasterNpcDebugView::Normals)[1] == 0);
	asset.normalIgnoreTileTransform = true;
	S9xRemasterApplyProfileToFrame(profile, frame);
	std::vector<uint8_t> bytes; RemasterFrame replay;
	assert(S9xSerializeRemasterFrame(frame, bytes));
	assert(S9xDeserializeRemasterFrame(bytes, replay));
	assert(replay.schemaVersion == REMASTER_FRAME_SCHEMA_VERSION && replay.assetMetadata[0].normalIgnoreTileTransform);
	cache.update(replay, false); assert(cache.surface[2 * 4 + 2] > 0.99f);
	bytes.pop_back(); assert(!S9xDeserializeRemasterFrame(bytes, replay));
	RemasterFrameTileInstance turned; turned.hFlip = turned.vFlip = true; turned.normalYaw = 1;
	float x = 1, y = 2, z = 3;
	S9xRemasterTransformNormalForTileInstance(turned, x, y, z, true);
	assert(x == 1 && y == 2 && z == 3);
	std::cout << "Fixed normal direction: profile/frame round trips, flips, yaw and cache invalidation passed\n";
}
