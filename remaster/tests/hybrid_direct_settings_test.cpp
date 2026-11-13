#include "../runtime/frame.h"
#include <cassert>
#include <iostream>

int main()
{
	RemasterProfile profile;
	profile.schemaVersion = 40;
	profile.gameTitle = "Hybrid settings regression";
	profile.romSha256 = std::string(64,'0');
	std::vector<RemasterProfileDiagnostic> diagnostics;
	std::string text;
	assert(S9xRemasterSerializeProfile(profile,text,diagnostics));
	RemasterProfile decoded;
	assert(S9xRemasterParseProfile(text,decoded,diagnostics));
	assert(!decoded.hybridDirect.enabled);
	const auto before = S9xRemasterGetSceneSettings(profile);
	profile.hybridDirect = {true,0.01f,16};
	assert(S9xRemasterSerializeProfile(profile,text,diagnostics));
	assert(text.find("schema_version = 41") == 0);
	assert(S9xRemasterParseProfile(text,decoded,diagnostics));
	assert(decoded.hybridDirect == profile.hybridDirect);
	const auto changed = S9xRemasterGetSceneSettings(profile);
	assert(!S9xRemasterSceneSettingsEqual(before,changed));
	S9xRemasterApplySceneSettings(profile,before);
	assert(S9xRemasterSceneSettingsEqual(before,S9xRemasterGetSceneSettings(profile)));
	S9xRemasterApplySceneSettings(profile,changed);
	assert(profile.hybridDirect.enabled);
	for (const char *bad : {"hybrid_direct = 1", "hybrid_direct_cutoff = nan",
		"hybrid_direct_cutoff = 0.001", "hybrid_direct_cutoff = 0.11",
		"hybrid_direct_tail_samples = 0", "hybrid_direct_tail_samples = 33"}) {
		const std::string input = "schema_version = 41\n[game]\ntitle = \"test\"\nrom_sha256 = \"" +
			profile.romSha256 + "\"\n[lighting_space]\n" + bad + "\n";
		assert(!S9xRemasterParseProfile(input,decoded,diagnostics));
	}
	RemasterFrame frame;
	frame.width = frame.height = 1;
	frame.originalRgb555.resize(1); frame.mainPixels.resize(1); frame.subPixels.resize(1);
	frame.hybridDirect = profile.hybridDirect;
	std::vector<uint8_t> bytes;
	assert(S9xSerializeRemasterFrame(frame,bytes));
	RemasterFrame restored;
	assert(S9xDeserializeRemasterFrame(bytes,restored));
	assert(restored.hybridDirect == frame.hybridDirect);
	frame.schemaVersion = 43;
	assert(S9xSerializeRemasterFrame(frame,bytes));
	assert(S9xDeserializeRemasterFrame(bytes,restored));
	assert(!restored.hybridDirect.enabled);
	frame.schemaVersion = REMASTER_FRAME_SCHEMA_VERSION;
	frame.hybridDirect.tailSamples = 33;
	assert(!S9xSerializeRemasterFrame(frame,bytes));
	std::cout << "PASS hybrid profile/frame roundtrip, defaults, strict ranges, scene-settings Undo/Redo\n";
}
