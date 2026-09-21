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
	frame.originalRgb555 = { 0x001f, 0x03e0 };
	frame.mainPixels.resize(2);
	frame.subPixels.resize(2);
	frame.mainPixels[0].owner = 0x02010002;
	frame.mainPixels[0].instanceId = 1;
	RemasterFrameAsset frameAsset;
	frameAsset.tileId = { hash, 1, 4 };
	std::copy(indices, indices + 64, frameAsset.indices);
	frame.assets.push_back(frameAsset);
	RemasterFrameMaterial frameMaterial;
	frameMaterial.name = "stone";
	frameMaterial.surfaceClass = RemasterSurfaceClass::Floor;
	frame.materials.push_back(frameMaterial);
	RemasterFrameTileInstance frameInstance;
	frameInstance.tileId = frameAsset.tileId;
	frameInstance.source = RemasterSourceType::Background;
	frameInstance.sourceIndex = 1;
	frameInstance.tileNumber = 2;
	frameInstance.material = "stone";
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
	assert(decodedFrame.originalRgb555 == frame.originalRgb555);
	assert(decodedFrame.tileInstances.size() == 1);
	assert(S9xRemasterFrameInstanceAt(decodedFrame, 0, 0));
	assert(S9xRemasterFrameInstanceAt(decodedFrame, 0, 0)->material == "stone");
	assert(S9xRemasterFrameOccurrences(decodedFrame, frameAsset.tileId).size() == 1);
	std::vector<uint8_t> truncatedFrameBytes = firstFrameBytes;
	truncatedFrameBytes.pop_back();
	assert(!S9xDeserializeRemasterFrame(truncatedFrameBytes, decodedFrame));
	frame.mainPixels.pop_back();
	assert(!S9xSerializeRemasterFrame(frame, secondFrameBytes));

	const std::string path = "/tmp/snes9x-remaster-inventory-test.json";
	S9xRemasterRequestTileInventory(path);
	S9xRemasterBeginFrame(1, 1, 1, 1);
	S9xRemasterSetDraw(RemasterSourceType::Background, 1, 0x1402);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x1402);
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

	const char *profileText = R"PROFILE(
schema_version = 1

[game]
title = "Test Game"
rom_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

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
	RemasterProfile profile;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	assert(S9xRemasterParseProfile(profileText, profile, diagnostics));
	assert(profile.materials.size() == 2);
	assert(profile.assetGroups.at("animated_floor").tileIds.size() == 2);
	assert(profile.rules.size() == 2);

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
	S9xRemasterWriteOwner(0);
	const uint16_t screen[] = { 0x001f, 0x03e0 };
	assert(S9xRemasterEndFrame(screen, 2, 2, 1) & RemasterCaptureFrame);
	std::ifstream frameInput(framePath, std::ios::binary);
	std::vector<uint8_t> capturedFrameBytes((std::istreambuf_iterator<char>(frameInput)),
		std::istreambuf_iterator<char>());
	assert(capturedFrameBytes.size() > 8);
	assert(std::string(capturedFrameBytes.begin(), capturedFrameBytes.begin() + 6) == "S9XRMF");
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
