#include "../runtime/held_lamp.h"
#include "../runtime/profile_merge.h"
#include <cassert>

int main()
{
	RemasterProfile profile;
	profile.gameTitle = "Lamp test";
	profile.romSha256 = std::string(64, 'a');
	assert(profile.linkLampBrightness == 200);
	std::vector<RemasterProfileDiagnostic> diagnostics;
	std::string text;
	assert(S9xRemasterSerializeProfile(profile, text, diagnostics));
	assert(text.find("link_lamp_brightness") == std::string::npos);
	RemasterProfile legacy;
	assert(S9xRemasterParseProfile(text, legacy, diagnostics));
	assert(legacy.linkLampBrightness == 200);
	const auto original = S9xRemasterGetSceneSettings(legacy);
	for (float brightness : {0.0f, 75.5f, 200.0f, 1600.0f})
	{
		profile.schemaVersion = 32;
		profile.linkLampBrightness = brightness;
		assert(S9xRemasterSerializeProfile(profile, text, diagnostics));
		RemasterProfile restored;
		assert(S9xRemasterParseProfile(text, restored, diagnostics));
		assert(restored.linkLampBrightness == brightness);
		auto settings = S9xRemasterGetSceneSettings(restored);
		S9xRemasterApplySceneSettings(legacy, settings);
		assert(S9xRemasterSceneSettingsEqual(settings, S9xRemasterGetSceneSettings(legacy)));
		S9xRemasterApplySceneSettings(legacy, original);
		assert(legacy.linkLampBrightness == 200);

		RemasterHeldLampState state;
		state.verifiedRom = state.darkWithLantern = true;
		state.facing = 2;
		RemasterFrame frame;
		RemasterFrameLight lamp;
		assert(S9xRemasterHeldLampLight(state, RemasterHeldLampSettings{}, lamp));
		frame.lights.push_back(lamp);
		frame.heldLampLightIndex = 0;
		frame.lights.push_back(lamp); // unrelated light must not change
		S9xRemasterApplyProfileToFrame(restored, frame);
		assert(frame.lights[0].intensity == brightness);
		assert(frame.lights[1].intensity == 200);
		S9xRemasterApplyProfileToFrame(legacy, frame);
		assert(frame.lights[0].intensity == 200); // zero can be undone while paused
		frame.heldLampLightIndex = SIZE_MAX; // loaded capture has no inferred identity
		S9xRemasterApplyProfileToFrame(restored, frame);
		assert(frame.lights[0].intensity == 200);
	}
	for (const std::string value : {"-1", "1601", "nan", "inf", "oops"})
	{
		const auto start = text.find("link_lamp_brightness = ");
		auto invalid = text;
		invalid.replace(start, invalid.find('\n', start) - start, "link_lamp_brightness = " + value);
		RemasterProfile rejected;
		assert(!S9xRemasterParseProfile(invalid, rejected, diagnostics));
	}
	text.replace(0, text.find('\n'), "schema_version = 31");
	assert(!S9xRemasterParseProfile(text, legacy, diagnostics));
	profile.linkLampBrightness = 50;
	RemasterProfile disk = legacy;
	disk.reflectanceBoost = 2;
	RemasterProfileMergeResult merged;
	assert(S9xRemasterMergeProfiles(legacy, profile, disk, {}, false, merged, diagnostics));
	assert(merged.conflicts.empty());
	assert(merged.saved.linkLampBrightness == 50 && merged.saved.reflectanceBoost == 2);
}
