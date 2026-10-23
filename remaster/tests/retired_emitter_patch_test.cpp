#include "../runtime/hidden_emitter_samples.h"
#include <cassert>

int main()
{
	RemasterProfile profile;
	profile.schemaVersion = 15;
	profile.gameTitle = "Retired emitter patch compatibility";
	profile.romSha256 = std::string(64, '0');
	std::string text;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	assert(S9xRemasterSerializeProfile(profile, text, diagnostics));
	assert(text.find("emitter_patch_size") == std::string::npos);
	const auto insertion = text.find("[lighting_space]\n") + std::string("[lighting_space]\n").size();
	for (unsigned size = 1; size <= 5; size++) {
		auto legacy = text;
		legacy.insert(insertion, "emitter_patch_size = " + std::to_string(size) + "\n");
		RemasterProfile decoded;
		assert(S9xRemasterParseProfile(legacy, decoded, diagnostics));
		assert(S9xRemasterSceneSettingsEqual(S9xRemasterGetSceneSettings(profile), S9xRemasterGetSceneSettings(decoded)));
		std::string saved;
		assert(S9xRemasterSerializeProfile(decoded, saved, diagnostics));
		assert(saved == text);
	}

	RemasterFrame frame;
	frame.width = frame.height = 1;
	frame.mainPixels.resize(1); frame.subPixels.resize(1); frame.originalRgb555.resize(1);
	frame.profileRomSha256 = profile.romSha256;
	std::vector<uint8_t> bytes;
	assert(S9xSerializeRemasterFrame(frame, bytes));
	// Header through reflectance boost, immediately before representative count.
	const size_t retiredOffset = 8 + 4 + 4 + 4 + 4 + frame.profileRomSha256.size() + 32;
	assert(bytes[retiredOffset] == 1);
	for (unsigned size = 1; size <= 5; size++) {
		bytes[retiredOffset] = size;
		RemasterFrame decoded;
		assert(S9xDeserializeRemasterFrame(bytes, decoded));
		assert(decoded.representativeEmitters == 0);
		std::vector<uint8_t> saved;
		assert(S9xSerializeRemasterFrame(decoded, saved));
		assert(saved[retiredOffset] == 1);
	}

	// Hidden sources remain per-pixel with representatives disabled, even when
	// a caller requests the clustered GPU-compatible sample layout.
	frame = RemasterFrame{};
	for (unsigned y = 0; y < 8; y++) for (unsigned x = 0; x < 8; x++) {
		RemasterFrameHiddenEmitter e;
		e.x = x + 0.5f; e.y = y + 0.5f; e.z = 0;
		e.depth = 2; e.normal = {{0, 0, 1}}; e.emission = {{255, 128, 0, 25}};
		frame.hiddenEmitters.push_back(e);
	}
	const auto pixels = S9xRemasterHiddenDirectSamples(frame, false, true);
	assert(pixels.size() == 256);
	assert(S9xRemasterHiddenDirectSamples(frame, true, true).size() == 64);
	frame.representativeEmitters = 3;
	const auto representatives = S9xRemasterHiddenDirectSamples(frame, false, true);
	assert(representatives.size() == 12);
	for (unsigned c = 0; c < 3; c++) {
		float before = 0, after = 0;
		for (const auto &s : pixels) before += s.radiance[c];
		for (const auto &s : representatives) after += s.radiance[c];
		assert(std::fabs(before - after) < 0.1f);
	}
}
