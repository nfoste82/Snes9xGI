#include "../runtime/remaster.h"
#include "../runtime/profile_merge.h"
#include <cassert>
#include <iostream>

uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;

int main()
{
	RemasterProfile profile;
	profile.schemaVersion = 32;
	profile.gameTitle = "Area scene settings";
	profile.romSha256 = std::string(64, 'a');
	profile.originalSceneContribution = 0.8f;
	profile.representativeEmitters = 4;
	assert(S9xRemasterAreaSceneSettings(profile, {97, false}).originalSceneContribution == 0.035f);
	assert(S9xRemasterAreaSceneSettings(profile, {320, false}).originalSceneContribution == 0.035f);
	assert(S9xRemasterAreaSceneSettings(profile, {}).originalSceneContribution == 0.8f);
	assert(S9xRemasterAreaSceneSettings(profile, {97, false}).representativeEmitters == 4);
	profile.areaSceneSettings["room_061"] = {0.1f, 1};
	profile.areaSceneSettings["room_062"] = {0, 8};
	profile.areaSceneSettings["overworld"] = {0.2f, 0};
	assert(S9xRemasterAreaSceneSettings(profile, {97, true}).representativeEmitters == 1);
	assert(S9xRemasterAreaSceneSettings(profile, {98, false}).representativeEmitters == 8);
	assert(S9xRemasterAreaSceneSettings(profile, {99, false}).representativeEmitters == 4);
	assert(S9xRemasterAreaSceneSettings(profile, {320, false}) == S9xRemasterAreaSceneSettings(profile, {321, false}));
	std::string text;
	std::vector<RemasterProfileDiagnostic> errors;
	assert(S9xRemasterSerializeProfile(profile, text, errors));
	RemasterProfile decoded;
	assert(S9xRemasterParseProfile(text, decoded, errors));
	assert(decoded.schemaVersion == 33 && decoded.areaSceneSettings == profile.areaSceneSettings);
	const auto valid = text;
	for (const auto &line : {"representative_emitters = 9", "representative_emitters = -1", "representative_emitters = 1.5",
		"original_scene_contribution = -0.1", "original_scene_contribution = 1.1", "original_scene_contribution = nan", "unknown = 1"}) {
		assert(!S9xRemasterParseProfile(valid + "\n[area_scene_settings.room_063]\n" + line + "\n", decoded, errors));
	}
	assert(!S9xRemasterParseProfile(valid + "\n[area_scene_settings.room_061]\n", decoded, errors));
	assert(!S9xRemasterParseProfile(valid + "\n[area_scene_settings.light_world]\n", decoded, errors));
	text.replace(text.find("schema_version = 33"), 19, "schema_version = 32");
	assert(!S9xRemasterParseProfile(text, decoded, errors));
	assert(S9xRemasterParseProfile(valid + "\n[area_scene_settings.room_063]\nrepresentative_emitters = 2\n", decoded, errors));
	assert(S9xRemasterAreaSceneSettings(decoded, {99, false}).originalSceneContribution == 0.035f);
	assert(S9xRemasterParseProfile(valid + "\n[area_scene_settings.room_064]\noriginal_scene_contribution = 0.3\n", decoded, errors));
	assert(S9xRemasterAreaSceneSettings(decoded, {100, false}).representativeEmitters == 4);
	RemasterFrame frame;
	frame.width = frame.height = 1; frame.originalRgb555 = {0}; frame.mainPixels.resize(1); frame.subPixels.resize(1);
	for (uint16_t id : {97, 98, 99, 320, 321}) {
		frame.area = {id, false}; S9xRemasterApplyProfileToFrame(profile, frame);
		const auto expected = S9xRemasterAreaSceneSettings(profile, frame.area);
		assert(frame.originalSceneContribution == expected.originalSceneContribution && frame.representativeEmitters == expected.representativeEmitters);
		std::vector<uint8_t> bytes; RemasterFrame replay;
		assert(S9xSerializeRemasterFrame(frame, bytes) && S9xDeserializeRemasterFrame(bytes, replay));
		assert(replay.originalSceneContribution == expected.originalSceneContribution && replay.representativeEmitters == expected.representativeEmitters);
	}
	S9xRemasterSetProfile(profile); S9xRemasterBeginFrame(1, 1, 1, 1);
	profile.areaSceneSettings["room_061"] = {0.4f, 2};
	S9xRemasterSetAreaSceneSettings(profile.areaSceneSettings);
	assert(S9xRemasterState().activeProfile.areaSceneSettings.at("room_061").representativeEmitters == 1);
	S9xRemasterBeginFrame(1, 1, 1, 1);
	assert(S9xRemasterState().activeProfile.areaSceneSettings == profile.areaSceneSettings);
	// Exercise production finalization, not just retained-frame application.
	S9xRemasterSetLiveFramesEnabled(true); S9xRemasterBeginFrame(1, 1, 1, 1);
	uint16_t pixel = 0; RemasterAreaContext room{97, false};
	S9xRemasterEndFrame(&pixel, 1, 1, 1, nullptr, nullptr, nullptr, &room);
	const auto *completed = S9xRemasterCompletedFrame();
	assert(completed && completed->originalSceneContribution == 0.4f && completed->representativeEmitters == 2);
	RemasterProfile base = profile, local = base, disk = base;
	local.areaSceneSettings["room_061"] = {0.5f, 1}; disk.areaSceneSettings["overworld"] = {0.7f, 8};
	RemasterProfileMergeResult result;
	assert(S9xRemasterMergeProfiles(base, local, disk, {}, false, result, errors));
	assert(result.conflicts.empty() && result.saved.areaSceneSettings.at("room_061").originalSceneContribution == 0.5f &&
		result.saved.areaSceneSettings.at("overworld").representativeEmitters == 8);
	disk.areaSceneSettings["room_061"] = {0.6f, 3};
	assert(S9xRemasterMergeProfiles(base, local, disk, {}, false, result, errors) && result.conflicts.size() == 1);
	std::cout << "PASS area scene defaults, isolation, shared overworld, strict persistence, replay/live publication and save merging\n";
}
