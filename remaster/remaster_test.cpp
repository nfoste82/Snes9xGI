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
	frame.cameraDirection = {{ 0.25f, -0.5f, -1.0f }};
	frame.indirectBounceCount = 6;
	frame.originalSceneContribution = 0.4f;
	frame.heightPreviewMultiplier = 12;
	frame.samplesPerFrame = 4;
	frame.sampleAccumulation = false;
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
	RemasterFrameArtworkColors frameArtwork;
	frameArtwork.tileId = frameAsset.tileId;
	frameArtwork.rgb555[9] = 0x4210;
	frameArtwork.visiblePixels = UINT64_C(1) << 9;
	frame.artworkColors.push_back(frameArtwork);
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
	assert(decodedFrame.cameraDirection == frame.cameraDirection);
	assert(decodedFrame.indirectBounceCount == 6);
	assert(decodedFrame.originalSceneContribution == 0.4f);
	assert(decodedFrame.heightPreviewMultiplier == 12);
	assert(decodedFrame.samplesPerFrame == 4);
	assert(!decodedFrame.sampleAccumulation);
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
	RemasterFrame legacyFlipFrame;
	legacyFlipFrame.schemaVersion = 7;
	legacyFlipFrame.width = 2;
	legacyFlipFrame.height = 2;
	legacyFlipFrame.mainPixels.resize(4);
	legacyFlipFrame.subPixels.resize(4);
	legacyFlipFrame.tileInstances.resize(1);
	legacyFlipFrame.mainPixels[0] = { 0, 1, 63 };
	legacyFlipFrame.mainPixels[1] = { 0, 1, 62 };
	legacyFlipFrame.mainPixels[2] = { 0, 1, 55 };
	legacyFlipFrame.mainPixels[3] = { 0, 1, 54 };
	S9xRemasterInferLegacyTileInstanceFlips(legacyFlipFrame);
	assert(legacyFlipFrame.tileInstances[0].hFlip && legacyFlipFrame.tileInstances[0].vFlip);
	assert(decodedFrame.mainPixels[0].tilePixel == 9);
	assert(decodedFrame.schemaVersion == 15);
	assert(decodedFrame.artworkColors.size() == 1);
	assert(S9xRemasterFrameArtworkColorsForTile(decodedFrame, frameAsset.tileId)->rgb555[9] == 0x4210);
	assert(!S9xRemasterFrameArtworkColorsForTile(decodedFrame, secondFrameAsset.tileId));
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
	assert(std::fabs(emissionLights[0].red - 1.0f / 8.0f) < 1e-6f);
	assert(std::fabs(emissionLights[0].green - std::pow(96.0f / 255.0f, 2.2f) / 8.0f) < 1e-6f);
	assert(std::fabs(emissionLights[0].blue - std::pow(24.0f / 255.0f, 2.2f) / 8.0f) < 1e-6f);
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
	decodedFrame.assetMetadata[0].emissionRgba[12] = 128;
	decodedFrame.assetMetadata[0].emissionRgba[15] = 50;
	emissionLights = S9xRemasterFrameEmissionLights(decodedFrame);
	assert(std::fabs(emissionLights[0].red - (1.0f + 2.0f * std::pow(128.0f / 255.0f, 2.2f)) / 8.0f) < 1e-6f);
	decodedFrame.assetMetadata[0].emissionRgba[12] = 255;
	decodedFrame.assetMetadata[0].emissionRgba[15] = 25;
	decodedFrame.mainPixels[1] = RemasterFramePixel();
	decodedFrame.assetGroups.clear();
	assert(S9xRemasterFrameAssetGroupVariants(decodedFrame, "animated_floor").size() == 1);
	RemasterFrame legacyFrame = frame;
	legacyFrame.assetMetadata.clear();
	legacyFrame.artworkColors.clear();
	legacyFrame.assetGroups.clear();
	legacyFrame.tileInstances.clear();
	for (RemasterFramePixel &pixel : legacyFrame.mainPixels)
		pixel.instanceId = 0;
	std::vector<uint8_t> legacyBytes;
	assert(S9xSerializeRemasterFrame(legacyFrame, legacyBytes));
	const size_t legacyScaleOffset = 8 + 4 + 4 + 4 + 4 + legacyFrame.profileRomSha256.size();
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 27);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 15, legacyBytes.begin() + legacyScaleOffset + 27);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 9, legacyBytes.begin() + legacyScaleOffset + 15);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 5, legacyBytes.begin() + legacyScaleOffset + 9);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 4);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset, legacyBytes.begin() + legacyScaleOffset + 4);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 4,
		legacyBytes.begin() + legacyScaleOffset + 8);
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
	RemasterFrame duplicateArtworkFrame = frame;
	duplicateArtworkFrame.artworkColors.push_back(frameArtwork);
	assert(!S9xSerializeRemasterFrame(duplicateArtworkFrame, secondFrameBytes));
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
		schema_version = 10

[game]
title = "Test Game"
rom_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

[lighting_space]
		coordinate_scale = 24
		camera_direction = [0.25, -0.5, -1]
		indirect_bounces = 6
		indirect_roughness = 0.5
		original_scene_contribution = 0.4
		height_preview_multiplier = 12
		samples_per_frame = 4
		sample_accumulation = false

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
	assert(profile.cameraDirection == frame.cameraDirection);
	assert(profile.indirectBounceCount == 6);
	assert(profile.indirectRoughness == 0.5f);
	assert(profile.originalSceneContribution == 0.4f);
	assert(profile.heightPreviewMultiplier == 12);
	assert(profile.samplesPerFrame == 4);
	assert(!profile.sampleAccumulation);
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
	assert(roundTrippedProfile.indirectRoughness == 0.5f);
	assert(roundTrippedProfile.cameraDirection == frame.cameraDirection);
	assert(roundTrippedProfile.originalSceneContribution == 0.4f);
	assert(roundTrippedProfile.heightPreviewMultiplier == 12);
	assert(roundTrippedProfile.samplesPerFrame == 4);
	assert(!roundTrippedProfile.sampleAccumulation);
	const std::string savedProfilePath = "/tmp/snes9x-remaster-profile-test.toml";
	assert(S9xRemasterWriteProfile(profile, savedProfilePath, diagnostics));
	assert(S9xRemasterLoadProfile(savedProfilePath, roundTrippedProfile, diagnostics));
	std::remove(savedProfilePath.c_str());
	std::string invalidCameraProfile = serializedProfile;
	const size_t cameraBegin = invalidCameraProfile.find("camera_direction = [");
	assert(cameraBegin != std::string::npos);
	const size_t cameraEnd = invalidCameraProfile.find('\n', cameraBegin);
	invalidCameraProfile.replace(cameraBegin, cameraEnd - cameraBegin, "camera_direction = [0, 0, 0]");
	assert(!S9xRemasterParseProfile(invalidCameraProfile, roundTrippedProfile, diagnostics));
	invalidCameraProfile = serializedProfile;
	invalidCameraProfile.replace(invalidCameraProfile.find("schema_version = 10"), 19, "schema_version = 8");
	assert(!S9xRemasterParseProfile(invalidCameraProfile, roundTrippedProfile, diagnostics));

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

	RemasterFrame synchronizedFrame = frame;
	synchronizedFrame.tileInstances[0].matchStatus = RemasterProfileMatchStatus::NoMatch;
	synchronizedFrame.tileInstances[0].ruleLine = 999;
	synchronizedFrame.tileInstances[0].assetGroup = "stale_group";
	synchronizedFrame.tileInstances[0].material = "stale_material";
	synchronizedFrame.materials[0].name = "stale_material";
	S9xRemasterApplyProfileToFrame(profile, synchronizedFrame);
	assert(synchronizedFrame.profileRomSha256 == profile.romSha256);
	assert(synchronizedFrame.assetGroups.size() == profile.assetGroups.size());
	assert(synchronizedFrame.assetMetadata.size() == profile.assets.size());
	assert(synchronizedFrame.materials.size() == profile.materials.size());
	assert(synchronizedFrame.tileInstances[0].matchStatus == RemasterProfileMatchStatus::Matched);
	assert(synchronizedFrame.tileInstances[0].ruleLine == profile.rules[0].line);
	assert(synchronizedFrame.tileInstances[0].assetGroup == "animated_floor");
	assert(synchronizedFrame.tileInstances[0].material == "stone");

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
	RemasterFrame finalizedFrame;
	assert(S9xRemasterEndFrame(screen, 2, 2, 1, &finalizedFrame) & RemasterCaptureFrame);
	std::ifstream frameInput(framePath, std::ios::binary);
	std::vector<uint8_t> capturedFrameBytes((std::istreambuf_iterator<char>(frameInput)),
		std::istreambuf_iterator<char>());
	assert(capturedFrameBytes.size() > 8);
	assert(std::string(capturedFrameBytes.begin(), capturedFrameBytes.begin() + 6) == "S9XRMF");
	std::vector<uint8_t> finalizedFrameBytes;
	assert(S9xSerializeRemasterFrame(finalizedFrame, finalizedFrameBytes));
	assert(finalizedFrameBytes == capturedFrameBytes);
	assert(S9xDeserializeRemasterFrame(capturedFrameBytes, decodedFrame));
	assert(finalizedFrame.originalRgb555 == decodedFrame.originalRgb555);
	assert(finalizedFrame.mainPixels[0].instanceId == decodedFrame.mainPixels[0].instanceId);
	assert(finalizedFrame.tileInstances.size() == decodedFrame.tileInstances.size());
	assert(decodedFrame.schemaVersion == 15);
	assert(decodedFrame.lightingCoordinateScale == profile.lightingCoordinateScale);
	assert(decodedFrame.cameraDirection == profile.cameraDirection);
	assert(decodedFrame.indirectBounceCount == profile.indirectBounceCount);
	assert(decodedFrame.indirectRoughness == profile.indirectRoughness);
	assert(decodedFrame.originalSceneContribution == profile.originalSceneContribution);
	assert(decodedFrame.heightPreviewMultiplier == profile.heightPreviewMultiplier);
	assert(decodedFrame.samplesPerFrame == profile.samplesPerFrame);
	assert(decodedFrame.sampleAccumulation == profile.sampleAccumulation);
	assert(decodedFrame.mainPixels[0].tilePixel == 0);
	assert(decodedFrame.assetMetadata.size() == 1);
	assert(decodedFrame.assetMetadata[0].materialSelectors[0] == "wet_stone");
	assert(decodedFrame.assetMetadata[0].occlusion[1] == 255);
	assert(decodedFrame.assetMetadata[0].height[2] == 128);
	assert(decodedFrame.assetMetadata[0].normalXyz[2] == 255);
	assert(decodedFrame.assetMetadata[0].emissionRgba[15] == 25);
	assert(decodedFrame.assetMetadata[0].heightSampling == RemasterHeightSampling::Linear);
	assert(decodedFrame.assetMetadata[0].directLightingOppositeFacing);
	const RemasterFrameArtworkColors *capturedArtwork = S9xRemasterFrameArtworkColorsForTile(
		decodedFrame, decodedFrame.tileInstances[0].tileId);
	assert(capturedArtwork && (capturedArtwork->visiblePixels & UINT64_C(1)));
	assert(capturedArtwork->rgb555[0] == screen[0]);
	std::remove(framePath.c_str());

	RemasterAnimationCapture animationCapture;
	RemasterFrame animationFrame = decodedFrame;
	animationFrame.width = 16;
	animationFrame.height = 8;
	animationFrame.originalRgb555.assign(128, 0);
	animationFrame.mainPixels.assign(128, RemasterFramePixel());
	animationFrame.subPixels.assign(128, RemasterFramePixel());
	animationFrame.tileInstances.resize(2);
	animationFrame.tileInstances[0].tileId = { 1, 1, 4 };
	animationFrame.tileInstances[0].source = RemasterSourceType::Object;
	animationFrame.tileInstances[1].tileId = { 10, 1, 4 };
	animationFrame.tileInstances[1].source = RemasterSourceType::Object;
	animationFrame.assets.clear();
	animationFrame.artworkColors = { { { 1, 1, 4 }, {}, UINT64_C(1) },
		{ { 10, 1, 4 }, {}, UINT64_C(1) } };
	animationFrame.artworkColors[0].rgb555[0] = 0x001f;
	animationFrame.artworkColors[1].rgb555[0] = 0x03e0;
	for (size_t y = 0; y < 8; y++)
		for (size_t x = 0; x < 16; x++)
		{
			animationFrame.mainPixels[y * 16 + x].instanceId = x < 8 ? 1 : 2;
			animationFrame.mainPixels[y * 16 + x].tilePixel = static_cast<uint8_t>(y * 8 + x % 8);
		}
	S9xRemasterAddAnimationCaptureFrame(animationCapture, animationFrame);
	animationFrame.tileInstances[0].tileId = { 2, 1, 4 };
	animationFrame.tileInstances[1].tileId = { 20, 1, 4 };
	animationFrame.artworkColors = { { { 2, 1, 4 }, {}, UINT64_C(1) },
		{ { 20, 1, 4 }, {}, UINT64_C(1) } };
	animationFrame.artworkColors[0].rgb555[0] = 0x7c00;
	animationFrame.artworkColors[1].rgb555[0] = 0x4210;
	S9xRemasterAddAnimationCaptureFrame(animationCapture, animationFrame);
	RemasterFrame animationResult = S9xRemasterFinishAnimationCapture(std::move(animationCapture));
	assert(animationResult.assetGroups.size() == decodedFrame.assetGroups.size() + 2);
	assert(animationResult.tileInstances[0].assetGroup != animationResult.tileInstances[1].assetGroup);
	assert(S9xRemasterFrameAssetGroupVariants(animationResult,
		animationResult.tileInstances[0].assetGroup) ==
		(std::vector<RemasterTileContentId> { { 1, 1, 4 }, { 2, 1, 4 } }));
	assert(S9xRemasterFrameAssetGroupVariants(animationResult,
		animationResult.tileInstances[1].assetGroup) ==
		(std::vector<RemasterTileContentId> { { 10, 1, 4 }, { 20, 1, 4 } }));
	assert(animationResult.artworkColors.size() == 4);
	assert(S9xRemasterFrameArtworkColorsForTile(animationResult, { 2, 1, 4 })->rgb555[0] == 0x7c00);
	assert(S9xRemasterFrameArtworkColorsForTile(animationResult, { 20, 1, 4 })->rgb555[0] == 0x4210);
	assert(S9xSerializeRemasterFrame(animationResult, secondFrameBytes));
	assert(S9xDeserializeRemasterFrame(secondFrameBytes, decodedFrame));
	assert(S9xRemasterFrameArtworkColorsForTile(decodedFrame, { 2, 1, 4 })->rgb555[0] == 0x7c00);
	RemasterAnimationCapture classifiedAnimationCapture;
	animationFrame.artworkColors.clear();
	animationFrame.assetGroups = { { "misclassified", { { 50, 1, 4 }, { 60, 1, 4 } } } };
	animationFrame.tileInstances.resize(2);
	animationFrame.tileInstances[0].tileId = { 30, 1, 4 };
	animationFrame.tileInstances[0].assetGroup.clear();
	animationFrame.tileInstances[1].tileId = { 60, 1, 4 };
	animationFrame.tileInstances[1].assetGroup = "misclassified";
	animationFrame.mainPixels.assign(128, RemasterFramePixel());
	for (size_t pixel = 0; pixel < 128; pixel++)
		animationFrame.mainPixels[pixel].instanceId = pixel % 16 < 8 ? 1 : 2;
	S9xRemasterAddAnimationCaptureFrame(classifiedAnimationCapture, animationFrame);
	animationFrame.tileInstances[0].tileId = { 40, 1, 4 };
	S9xRemasterAddAnimationCaptureFrame(classifiedAnimationCapture, animationFrame);
	animationFrame.tileInstances[0].tileId = { 50, 1, 4 };
	animationFrame.tileInstances[0].assetGroup = "misclassified";
	S9xRemasterAddAnimationCaptureFrame(classifiedAnimationCapture, animationFrame);
	RemasterFrame classifiedAnimationResult = S9xRemasterFinishAnimationCapture(
		std::move(classifiedAnimationCapture));
	assert(classifiedAnimationResult.tileInstances[0].assetGroup == "capture_animation_1");
	assert(S9xRemasterFrameAssetGroupVariants(classifiedAnimationResult, "capture_animation_1") ==
		(std::vector<RemasterTileContentId> { { 30, 1, 4 }, { 40, 1, 4 }, { 50, 1, 4 } }));
	assert(S9xRemasterFrameAssetGroupVariants(classifiedAnimationResult, "misclassified") ==
		(std::vector<RemasterTileContentId> { { 60, 1, 4 } }));
	RemasterAnimationCapture replacedTileCapture;
	animationFrame.assetGroups.clear();
	animationFrame.tileInstances.resize(1);
	animationFrame.tileInstances[0].tileId = { 70, 1, 4 };
	animationFrame.tileInstances[0].tileNumber = 7;
	animationFrame.tileInstances[0].assetGroup.clear();
	animationFrame.mainPixels.assign(128, RemasterFramePixel());
	for (size_t pixel = 0; pixel < 64; pixel++)
		animationFrame.mainPixels[pixel].instanceId = 1;
	S9xRemasterAddAnimationCaptureFrame(replacedTileCapture, animationFrame);
	animationFrame.tileInstances[0].tileId = { 80, 1, 4 };
	animationFrame.tileInstances[0].tileNumber = 8;
	S9xRemasterAddAnimationCaptureFrame(replacedTileCapture, animationFrame);
	RemasterFrame replacedTileResult = S9xRemasterFinishAnimationCapture(std::move(replacedTileCapture));
	assert(replacedTileResult.assetGroups.empty());
	assert(replacedTileResult.tileInstances[0].assetGroup.empty());

	S9xRemasterRequestFrameCapture(framePath);
	S9xRemasterBeginFrame(2, 2, 2, 1);
	finalizedFrame.width = 99;
	assert(!(S9xRemasterEndFrame(screen, 1, 2, 1, &finalizedFrame) & RemasterCaptureFrame));
	assert(finalizedFrame.width == 0);
	std::remove(framePath.c_str());

	S9xRemasterSetLiveFramesEnabled(true);
	S9xRemasterBeginFrame(2, 2, 2, 1);
	assert(!S9xRemasterCompletedFrame());
	S9xRemasterSetSubscreen(false);
	S9xRemasterSetDraw(RemasterSourceType::Background, 1, 0x1402);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x1402);
	S9xRemasterSetTilePixel(0);
	S9xRemasterWriteOwner(0);
	assert(S9xRemasterEndFrame(screen, 2, 2, 1) == RemasterCaptureNone);
	const RemasterFrame *liveFrame = S9xRemasterCompletedFrame();
	assert(liveFrame && liveFrame->width == 2 && liveFrame->height == 1);
	assert(S9xRemasterFrameHasPixelData(*liveFrame, 2, 1));
	assert(!S9xRemasterFrameHasPixelData(*liveFrame, 1, 2));
	assert(liveFrame->originalRgb555[1] == screen[1]);
	assert(liveFrame->mainPixels[0].instanceId == 1);
	assert(liveFrame->tileInstances.size() == 1);
	S9xRemasterBeginFrame(2, 2, 2, 1);
	assert(!S9xRemasterCompletedFrame());
	S9xRemasterEndFrame(screen, 2, 2, 1);
	S9xRemasterSetLiveFramesEnabled(false);
	S9xRemasterBeginFrame(2, 2, 2, 1);
	assert(!S9xRemasterCompletedFrame());
	S9xRemasterEndFrame(screen, 2, 2, 1);

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
	assert(profile.materials.size() == 9);
	assert(profile.assetGroups.size() == 9);
	assert(profile.rules.size() == 9);
	context = {};
	context.tileId = { UINT64_C(0x34eb3798eedfa0fc), 1, 4 };
	context.source = RemasterSourceType::Background;
	context.sourceIndex = 1;
	match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Matched);
	assert(match.assetGroup && match.assetGroup->name == "torch_flame");
	assert(match.material && match.material->name == "torch_flame");
	assert(match.material->receivesGi);
	context.tileId = { UINT64_C(0xc40518b112d0814e), 1, 4 };
	match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Matched);
	assert(match.assetGroup && match.assetGroup->name == "dungeon_floor");
	RemasterFrame profiledFrame;
	S9xRemasterApplyProfileToFrame(profile, profiledFrame);
	assert(S9xRemasterFrameAssetGroupVariants(profiledFrame, "torch_flame") ==
		(std::vector<RemasterTileContentId> { { UINT64_C(0x34eb3798eedfa0fc), 1, 4 },
			{ UINT64_C(0x9c36635bd77c6108), 1, 4 }, { UINT64_C(0xa8d1d0722ff1bda3), 1, 4 } }));
	return 0;
}
