/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include "remaster.h"
#include "profile.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;

int main ()
{
	uint8_t indices[64] = {};
	for (size_t i = 0; i < 64; i++)
		indices[i] = i & 15;

	const uint64_t hash = S9xRemasterHashTile(4, indices);
	assert(hash == UINT64_C(0xf99d5746200467e1));
	assert(hash != S9xRemasterHashTile(2, indices));

	RemasterFrame frame;
	frame.width = 2;
	frame.height = 1;
	frame.profileRomSha256 = "0123456789abcdef";
	frame.lightingCoordinateScale = 24.0f;
	frame.indirectBounceCount = 6;
	frame.originalRgb555 = { 0x001f, 0x03e0 };
	frame.mainPixels.resize(2);
	frame.subPixels.resize(2);
	frame.mainPixels[0].owner = 0x02010002;
	frame.mainPixels[0].instanceId = 1;
	frame.mainPixels[0].tilePixel = 9;
	RemasterFrameAsset frameAsset;
	frameAsset.tileId = { hash, 1, 4 };
	std::copy(indices, indices + 64, frameAsset.indices);
	frame.assets.push_back(frameAsset);
	RemasterFrameAsset secondFrameAsset;
	secondFrameAsset.tileId = { UINT64_C(0x1111111111111111), 1, 4 };
	std::fill(secondFrameAsset.indices, secondFrameAsset.indices + 64, 3);
	frame.assets.push_back(secondFrameAsset);
	RemasterFrameAssetGroup frameGroup;
	frameGroup.name = "animated_floor";
	frameGroup.tileIds = { frameAsset.tileId, secondFrameAsset.tileId };
	frame.assetGroups.push_back(frameGroup);
	RemasterFrameAssetMetadata frameMetadata;
	frameMetadata.tileId = frameAsset.tileId;
	frameMetadata.hasMaterialSelectors = true;
	frameMetadata.materialSelectors.fill("");
	frameMetadata.materialSelectors[0] = "stone";
	frameMetadata.hasOcclusion = true;
	frameMetadata.occlusion[1] = 255;
	frameMetadata.hasHeight = true;
	frameMetadata.height[2] = 128;
	frameMetadata.heightSampling = RemasterHeightSampling::Linear;
	frameMetadata.hasNormals = true;
	frameMetadata.normalXyz.fill(128);
	frameMetadata.normalXyz[2] = 255;
	frameMetadata.normalXyz[3] = 255;
	frameMetadata.hasEmission = true;
	frameMetadata.directLightingOppositeFacing = true;
	frameMetadata.emissionRgba[12] = 255;
	frameMetadata.emissionRgba[13] = 96;
	frameMetadata.emissionRgba[14] = 24;
	frameMetadata.emissionRgba[15] = 25;
	frameMetadata.emissionRgba[36] = 255;
	frameMetadata.emissionRgba[37] = 96;
	frameMetadata.emissionRgba[38] = 24;
	frameMetadata.emissionRgba[39] = 25;
	frame.assetMetadata.push_back(frameMetadata);
	RemasterFrameMaterial frameMaterial;
	frameMaterial.name = "stone";
	frameMaterial.surfaceClass = RemasterSurfaceClass::Floor;
	frameMaterial.receivesGi = false;
	frameMaterial.castsShadow = true;
	frameMaterial.zMin = 2.0f;
	frameMaterial.zMax = 6.0f;
	frame.materials.push_back(frameMaterial);
	RemasterFrameTileInstance frameInstance;
	frameInstance.tileId = frameAsset.tileId;
	frameInstance.source = RemasterSourceType::Background;
	frameInstance.sourceIndex = 1;
	frameInstance.tileNumber = 2;
	frameInstance.assetGroup = "animated_floor";
	frameInstance.material = "stone";
	frameInstance.hFlip = true;
	frame.tileInstances.push_back(frameInstance);
	std::vector<uint8_t> firstFrameBytes;
	std::vector<uint8_t> secondFrameBytes;
	assert(S9xSerializeRemasterFrame(frame, firstFrameBytes));
	assert(S9xSerializeRemasterFrame(frame, secondFrameBytes));
	assert(firstFrameBytes == secondFrameBytes);
	assert(firstFrameBytes.size() > 8);
	assert(std::string(firstFrameBytes.begin(), firstFrameBytes.begin() + 6) == "S9XRMF");
	RemasterFrame decodedFrame;
	assert(S9xDeserializeRemasterFrame(firstFrameBytes, decodedFrame));
	assert(decodedFrame.width == 2 && decodedFrame.height == 1);
	assert(decodedFrame.lightingCoordinateScale == 24.0f);
	assert(decodedFrame.indirectBounceCount == 6);
	assert(decodedFrame.originalRgb555 == frame.originalRgb555);
	assert(decodedFrame.tileInstances.size() == 1);
	float normalX = 0.6f;
	float normalY = -0.8f;
	float normalZ = 0.25f;
	S9xRemasterTransformNormalForTileInstance(decodedFrame.tileInstances[0], normalX, normalY, normalZ);
	assert(normalX == -0.6f && normalY == -0.8f && normalZ == 0.25f);
	decodedFrame.tileInstances[0].vFlip = true;
	normalX = 0.6f;
	normalY = -0.8f;
	S9xRemasterTransformNormalForTileInstance(decodedFrame.tileInstances[0], normalX, normalY, normalZ);
	assert(normalX == -0.6f && normalY == 0.8f && normalZ == 0.25f);
	decodedFrame.tileInstances[0].vFlip = false;
	assert(decodedFrame.mainPixels[0].tilePixel == 9);
	assert(decodedFrame.assetGroups.size() == 1);
	assert(decodedFrame.assetMetadata.size() == 1);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->materialSelectors[0] == "stone");
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->occlusion[1] == 255);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->height[2] == 128);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->normalXyz[3] == 255);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->emissionRgba[15] == 25);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->heightSampling == RemasterHeightSampling::Linear);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->directLightingOppositeFacing);
	assert(S9xRemasterFrameInstanceAt(decodedFrame, 0, 0));
	assert(S9xRemasterFrameInstanceAt(decodedFrame, 0, 0)->material == "stone");
	assert(S9xRemasterFrameInstanceAt(decodedFrame, 0, 0)->hFlip);
	const RemasterFrameMaterial *resolvedMaterial = S9xRemasterFrameMaterialForPixel(decodedFrame,
		decodedFrame.mainPixels[0]);
	assert(resolvedMaterial && resolvedMaterial->surfaceClass == RemasterSurfaceClass::Floor);
	assert(!resolvedMaterial->receivesGi && resolvedMaterial->castsShadow);
	assert(resolvedMaterial->zMin == 2.0f && resolvedMaterial->zMax == 6.0f);
	decodedFrame.assetMetadata[0].materialSelectors[9] = "missing";
	assert(!S9xRemasterFrameMaterialForPixel(decodedFrame, decodedFrame.mainPixels[0]));
	decodedFrame.assetMetadata[0].materialSelectors[9].clear();
	assert(S9xRemasterFrameMaterialForPixel(decodedFrame, decodedFrame.mainPixels[0]) == resolvedMaterial);
	assert(S9xRemasterFrameOccurrences(decodedFrame, frameAsset.tileId).size() == 1);
	assert(S9xRemasterFrameAssetForTile(decodedFrame, frameAsset.tileId));
	assert(S9xRemasterFrameAssetGroupVariants(decodedFrame, "animated_floor").size() == 2);
	std::vector<RemasterFrameLight> emissionLights = S9xRemasterFrameEmissionLights(decodedFrame);
	assert(emissionLights.size() == 1);
	assert(emissionLights[0].x == 0.5f && emissionLights[0].y == 0.5f);
	assert(emissionLights[0].z == decodedFrame.lightingCoordinateScale / 255.0f);
	assert(emissionLights[0].red > emissionLights[0].green);
	const float fullRed = emissionLights[0].red;
	decodedFrame.assetMetadata[0].emissionRgba[39] = 12;
	emissionLights = S9xRemasterFrameEmissionLights(decodedFrame);
	assert(emissionLights.size() == 1 && emissionLights[0].red < fullRed);
	decodedFrame.assetMetadata[0].emissionRgba[39] = 25;
	decodedFrame.assetMetadata[0].height[3] = 255;
	decodedFrame.mainPixels[1].instanceId = 1;
	decodedFrame.mainPixels[1].tilePixel = 3;
	emissionLights = S9xRemasterFrameEmissionLights(decodedFrame);
	assert(emissionLights.size() == 1 && emissionLights[0].red > fullRed);
	assert(emissionLights[0].x == 1.0f);
	assert(emissionLights[0].z == decodedFrame.lightingCoordinateScale * 0.5f);
	assert(emissionLights[0].radius == 96.0f);
	decodedFrame.mainPixels[1] = RemasterFramePixel();
	decodedFrame.assetGroups.clear();
	assert(S9xRemasterFrameAssetGroupVariants(decodedFrame, "animated_floor").size() == 1);
	RemasterFrame legacyFrame = frame;
	legacyFrame.assetMetadata.clear();
	legacyFrame.assetGroups.clear();
	legacyFrame.tileInstances.clear();
	for (RemasterFramePixel &pixel : legacyFrame.mainPixels)
		pixel.instanceId = 0;
	std::vector<uint8_t> legacyBytes;
	assert(S9xSerializeRemasterFrame(legacyFrame, legacyBytes));
	const size_t legacyScaleOffset = 8 + 4 + 4 + 4 + 4 + legacyFrame.profileRomSha256.size();
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 4);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset, legacyBytes.begin() + legacyScaleOffset + 4);
	legacyBytes[8] = 6;
	const size_t legacyGroupCountOffset = 8 + 4 + 4 + 4 + 4 + legacyFrame.profileRomSha256.size() + 4;
	const size_t legacyPixelOffset = 8 + 4 + 4 + 4 + 4 + legacyFrame.profileRomSha256.size() + 6 * 4 +
		legacyFrame.originalRgb555.size() * 2;
	for (size_t i = legacyFrame.mainPixels.size() + legacyFrame.subPixels.size(); i > 0; i--)
		legacyBytes.erase(legacyBytes.begin() + legacyPixelOffset + (i - 1) * 9 + 8);
	legacyBytes[8] = 3;
	assert(S9xDeserializeRemasterFrame(legacyBytes, decodedFrame));
	assert(decodedFrame.schemaVersion == 3 && decodedFrame.assetMetadata.empty());
	legacyBytes[8] = 2;
	legacyBytes.erase(legacyBytes.begin() + legacyGroupCountOffset + 4,
		legacyBytes.begin() + legacyGroupCountOffset + 8);
	assert(S9xDeserializeRemasterFrame(legacyBytes, decodedFrame));
	assert(decodedFrame.schemaVersion == 2 && decodedFrame.assetMetadata.empty());
	legacyBytes[8] = 1;
	legacyBytes.erase(legacyBytes.begin() + legacyGroupCountOffset,
		legacyBytes.begin() + legacyGroupCountOffset + 4);
	assert(S9xDeserializeRemasterFrame(legacyBytes, decodedFrame));
	assert(decodedFrame.schemaVersion == 1 && decodedFrame.assetGroups.empty());
	std::vector<uint8_t> truncatedFrameBytes = firstFrameBytes;
	truncatedFrameBytes.pop_back();
	assert(!S9xDeserializeRemasterFrame(truncatedFrameBytes, decodedFrame));
	RemasterFrame invalidIdFrame = frame;
	invalidIdFrame.assets[0].tileId.bitDepth = 0;
	assert(!S9xSerializeRemasterFrame(invalidIdFrame, secondFrameBytes));
	frame.mainPixels.pop_back();
	assert(!S9xSerializeRemasterFrame(frame, secondFrameBytes));

	const std::string path = "/tmp/snes9x-remaster-inventory-test.json";
	S9xRemasterRequestTileInventory(path);
	S9xRemasterBeginFrame(1, 1, 1, 1);
	S9xRemasterSetDraw(RemasterSourceType::Background, 1, 0x1402);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x1402);
	S9xRemasterSetTilePixel(0);
	S9xRemasterWriteOwner(0);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x1402);
	assert(S9xRemasterEndFrame());

	std::ifstream input(path);
	std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	std::ostringstream id;
	id << "v1:4bpp:" << std::hex << std::setfill('0') << std::setw(16) << hash;
	assert(json.find(id.str()) != std::string::npos);
	assert(json.find("\"observations\": 2") != std::string::npos);
	assert(json.find("\"visible_pixels\": 1") != std::string::npos);
	assert(json.find("\"visible_bounds\": { \"min_x\": 0, \"min_y\": 0, \"max_x\": 0, \"max_y\": 0 }") != std::string::npos);
	assert(json.find("\"visible_cells\": [{ \"x\": 0, \"y\": 0, \"pixels\": 1 }]") != std::string::npos);
	assert(json.find("\"contexts\": [{ \"source\": \"background\"") != std::string::npos);
	assert(json.find("\"profile_loaded\": false") != std::string::npos);
	std::remove(path.c_str());

	std::ostringstream profileSource;
	profileSource << R"PROFILE(
		schema_version = 6

[game]
title = "Test Game"
rom_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

[lighting_space]
coordinate_scale = 24
indirect_bounces = 6

[materials.stone]
surface_class = "floor"
roughness = 0.8

[materials.wet_stone]
surface_class = "floor"
roughness = 0.4

[asset_groups.animated_floor]
tile_hashes = ["v1:4bpp:f99d5746200467e1", "v1:4bpp:1111111111111111"]
material = "stone"

[[rules]]
asset_group = "animated_floor"

[[rules]]
asset_group = "animated_floor"
source = "background"
source_index = 1
palette = 5
material = "wet_stone"
)PROFILE";
	profileSource << "\n[[assets]]\ntile_hash = \"v1:4bpp:f99d5746200467e1\"\nmaterials = [";
	for (size_t i = 0; i < 64; i++)
		profileSource << (i ? ", " : "") << (i == 0 ? "\"wet_stone\"" : "\"\"");
	profileSource << "]\nocclusion = [";
	for (size_t i = 0; i < 64; i++)
		profileSource << (i ? ", " : "") << (i == 1 ? 255 : 0);
	profileSource << "]\nheight = [";
	for (size_t i = 0; i < 64; i++)
		profileSource << (i ? ", " : "") << (i == 2 ? 128 : 0);
	profileSource << "]\nheight_sampling = \"linear\"\n";
	profileSource << "normal_xyz = [";
	for (size_t i = 0; i < 192; i++)
		profileSource << (i ? ", " : "") << (i % 3 == 2 ? 255 : 128);
	profileSource << "]\n";
	profileSource << "direct_lighting_opposite_facing = true\n";
	profileSource << "emission_rgba = [";
	for (size_t i = 0; i < 256; i++)
		profileSource << (i ? ", " : "") << (i == 12 ? 255 : (i == 13 ? 96 : (i == 14 ? 24 : (i == 15 ? 25 : 0))));
	profileSource << "]\n";
	const std::string profileText = profileSource.str();
	RemasterProfile profile;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	assert(S9xRemasterParseProfile(profileText, profile, diagnostics));
	assert(profile.materials.size() == 2);
	assert(profile.lightingCoordinateScale == 24.0f);
	assert(profile.indirectBounceCount == 6);
	assert(profile.assetGroups.at("animated_floor").tileIds.size() == 2);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).materialSelectors[0] == "wet_stone");
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).occlusion[1] == 255);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).height[2] == 128);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).heightSampling == RemasterHeightSampling::Linear);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).normalXyz[2] == 255);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).emissionRgba[15] == 25);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).directLightingOppositeFacing);
	assert(profile.rules.size() == 2);
	std::string serializedProfile;
	assert(S9xRemasterSerializeProfile(profile, serializedProfile, diagnostics));
	RemasterProfile roundTrippedProfile;
	assert(S9xRemasterParseProfile(serializedProfile, roundTrippedProfile, diagnostics));
	assert(roundTrippedProfile.assets.at(RemasterTileContentId { hash, 1, 4 }).height[2] == 128);
	assert(roundTrippedProfile.assets.at(RemasterTileContentId { hash, 1, 4 }).normalXyz[2] == 255);
	assert(roundTrippedProfile.assets.at(RemasterTileContentId { hash, 1, 4 }).directLightingOppositeFacing);
	const std::string savedProfilePath = "/tmp/snes9x-remaster-profile-test.toml";
	assert(S9xRemasterWriteProfile(profile, savedProfilePath, diagnostics));
	assert(S9xRemasterLoadProfile(savedProfilePath, roundTrippedProfile, diagnostics));
	std::remove(savedProfilePath.c_str());

	RemasterProfileMatchContext context;
	context.tileId = { hash, 1, 4 };
	context.source = RemasterSourceType::Background;
	context.sourceIndex = 1;
	context.palette = 5;
	RemasterProfileMatch match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Matched);
	assert(match.material && match.material->name == "wet_stone");
	assert(match.assetGroup && match.assetGroup->name == "animated_floor");

	context.palette = 2;
	match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Matched);
	assert(match.material && match.material->name == "stone");

	S9xRemasterSetProfile(profile);
	const std::string profiledPath = "/tmp/snes9x-remaster-profiled-inventory-test.json";
	S9xRemasterRequestTileInventory(profiledPath);
	S9xRemasterBeginFrame(1, 1, 1, 1);
	S9xRemasterSetDraw(RemasterSourceType::Background, 1, 0x1402);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x1402);
	S9xRemasterSetDraw(RemasterSourceType::Background, 1, 0x1402);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x0002);
	assert(S9xRemasterEndFrame());
	std::ifstream profiledInput(profiledPath);
	json.assign(std::istreambuf_iterator<char>(profiledInput), std::istreambuf_iterator<char>());
	assert(json.find("\"material\": \"wet_stone\"") != std::string::npos);
	assert(json.find("\"material\": \"stone\"") != std::string::npos);
	assert(json.find("\"profile_unmatched_observations\": 0") != std::string::npos);
	assert(json.find("\"profile_loaded\": true") != std::string::npos);
	std::remove(profiledPath.c_str());

	const std::string framePath = "/tmp/snes9x-remaster-frame-test.s9xrmf";
	S9xRemasterRequestFrameCapture(framePath);
	S9xRemasterBeginFrame(2, 2, 2, 1);
	S9xRemasterSetSubscreen(false);
	S9xRemasterSetDraw(RemasterSourceType::Background, 1, 0x1402);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x1402);
	S9xRemasterSetTilePixel(0);
	S9xRemasterWriteOwner(0);
	const uint16_t screen[] = { 0x001f, 0x03e0 };
	assert(S9xRemasterEndFrame(screen, 2, 2, 1) & RemasterCaptureFrame);
	std::ifstream frameInput(framePath, std::ios::binary);
	std::vector<uint8_t> capturedFrameBytes((std::istreambuf_iterator<char>(frameInput)),
		std::istreambuf_iterator<char>());
	assert(capturedFrameBytes.size() > 8);
	assert(std::string(capturedFrameBytes.begin(), capturedFrameBytes.begin() + 6) == "S9XRMF");
	assert(S9xDeserializeRemasterFrame(capturedFrameBytes, decodedFrame));
	assert(decodedFrame.schemaVersion == 10);
	assert(decodedFrame.lightingCoordinateScale == profile.lightingCoordinateScale);
	assert(decodedFrame.indirectBounceCount == profile.indirectBounceCount);
	assert(decodedFrame.mainPixels[0].tilePixel == 0);
	assert(decodedFrame.assetMetadata.size() == 1);
	assert(decodedFrame.assetMetadata[0].materialSelectors[0] == "wet_stone");
	assert(decodedFrame.assetMetadata[0].occlusion[1] == 255);
	assert(decodedFrame.assetMetadata[0].height[2] == 128);
	assert(decodedFrame.assetMetadata[0].normalXyz[2] == 255);
	assert(decodedFrame.assetMetadata[0].emissionRgba[15] == 25);
	assert(decodedFrame.assetMetadata[0].heightSampling == RemasterHeightSampling::Linear);
	assert(decodedFrame.assetMetadata[0].directLightingOppositeFacing);
	std::remove(framePath.c_str());
	S9xRemasterRequestFrameCapture(framePath);
	S9xRemasterBeginFrame(2, 2, 2, 1);
	assert(!(S9xRemasterEndFrame(screen, 1, 2, 1) & RemasterCaptureFrame));
	std::remove(framePath.c_str());

	const char *ambiguousProfile = R"PROFILE(
schema_version = 1
[game]
title = "Test Game"
rom_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
[materials.first]
surface_class = "floor"
[materials.second]
surface_class = "wall_face"
[[rules]]
tile_hash = "v1:4bpp:f99d5746200467e1"
source = "background"
material = "first"
[[rules]]
tile_hash = "v1:4bpp:f99d5746200467e1"
palette = 0
material = "second"
[[rules]]
tile_hash = "v1:4bpp:f99d5746200467e1"
palette = 0
material = "first"
)PROFILE";
	assert(S9xRemasterParseProfile(ambiguousProfile, profile, diagnostics));
	context.palette = 0;
	match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Ambiguous);
	assert(match.conflictingRuleLines.size() == 3);

	const char *invalidStringProfile = R"PROFILE(
schema_version = 1
[game]
title = "Test"junk"
rom_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
)PROFILE";
	assert(!S9xRemasterParseProfile(invalidStringProfile, profile, diagnostics));

	const char *invalidSourceProfile = R"PROFILE(
schema_version = 1
[game]
title = "Test Game"
rom_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
[materials.stone]
surface_class = "floor"
[[rules]]
tile_hash = "v1:4bpp:f99d5746200467e1"
source_index = 1
material = "stone"
)PROFILE";
	assert(!S9xRemasterParseProfile(invalidSourceProfile, profile, diagnostics));

	const char *invalidProfile = R"PROFILE(
schema_version = 1
[game]
title = "Test Game"
rom_sha256 = "not-a-sha"
[materials.stone]
unexpected = true
[asset_groups.broken]
tile_hashes = ["v1:4bpp:INVALID000000000"]
material = "missing"
)PROFILE";
	assert(!S9xRemasterParseProfile(invalidProfile, profile, diagnostics));
	assert(diagnostics.size() >= 3);

	assert(S9xRemasterLoadProfile("alttp-profile.toml", profile, diagnostics));
	assert(profile.materials.size() == 8);
	assert(profile.assetGroups.size() == 8);
	assert(profile.rules.size() == 8);
	return 0;
}
