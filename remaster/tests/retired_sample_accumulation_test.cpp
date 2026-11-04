// Retired controls must remain readable without reintroducing rendering state.
#include "../runtime/frame.h"
#include <cassert>

int main()
{
	RemasterProfile profile;
	profile.schemaVersion = 15;
	profile.gameTitle = "Retired accumulation compatibility";
	profile.romSha256 = std::string(64, '0');
	std::string text;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	assert(S9xRemasterSerializeProfile(profile, text, diagnostics));
	assert(text.find("sample_accumulation") == std::string::npos);
	const auto insertion = text.find("[lighting_space]\n") + std::string("[lighting_space]\n").size();
	for (const char *value : {"true", "false"})
	{
		auto old = text;
		old.insert(insertion, std::string("sample_accumulation = ") + value + "\n");
		RemasterProfile decoded;
		assert(S9xRemasterParseProfile(old, decoded, diagnostics));
		assert(S9xRemasterSceneSettingsEqual(S9xRemasterGetSceneSettings(profile), S9xRemasterGetSceneSettings(decoded)));
		std::string saved;
		assert(S9xRemasterSerializeProfile(decoded, saved, diagnostics) && saved == text);
	}
	for (const char *value : {"2", "\"true\"", "TRUE"})
	{
		auto invalid = text;
		invalid.insert(insertion, std::string("sample_accumulation = ") + value + "\n");
		RemasterProfile decoded;
		assert(!S9xRemasterParseProfile(invalid, decoded, diagnostics));
	}

	RemasterFrame frame;
	frame.width = frame.height = 1;
	frame.mainPixels.resize(1); frame.subPixels.resize(1); frame.originalRgb555.resize(1);
	frame.profileRomSha256 = profile.romSha256;
	std::vector<uint8_t> bytes;
	assert(S9xSerializeRemasterFrame(frame, bytes));
	// Independent wire offset: magic/schema/dimensions/string, then scale,
	// bounces, roughness, original contribution, connections and reserved bool.
	const size_t slot = 8 + 4 + 4 + 4 + 4 + frame.profileRomSha256.size() + 4 + 1 + 4 + 4 + 1;
	assert(bytes[slot] == 1);
	for (uint8_t value : {0, 1})
	{
		bytes[slot] = value;
		RemasterFrame decoded;
		assert(S9xDeserializeRemasterFrame(bytes, decoded));
		std::vector<uint8_t> saved;
		assert(S9xSerializeRemasterFrame(decoded, saved) && saved == bytes);
	}
	bytes[slot] = 2;
	RemasterFrame decoded;
	assert(!S9xDeserializeRemasterFrame(bytes, decoded));
}
