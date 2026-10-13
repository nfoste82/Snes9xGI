#include "frame.h"
#include "emission_appearance.h"
#include <cstdio>
#include <cstdlib>

static void require (bool value, const char *message)
{
	if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

int main ()
{
	uint8_t orange[] = {204, 102, 26, 25};
	require(S9xRemasterEmissionPreview(orange) == std::array<uint8_t, 3>{204, 102, 26},
		"standard preview preserves authored orange");
	for (unsigned intensity = 1; intensity <= 255; intensity++) {
		orange[3] = intensity;
		const auto color = S9xRemasterEmissionPreview(orange);
		require(color[0] > color[1] && color[1] > color[2], "orange preview never clips to white");
		require(std::abs(2 * int(color[1]) - int(color[0])) <= 1, "preview preserves orange ratios");
	}
	orange[3] = 0;
	require(S9xRemasterEmissionPreview(orange) == std::array<uint8_t, 3>{}, "disabled preview stays black");
	RemasterProfile profile;
	profile.romSha256 = std::string(64, 'a');
	profile.gameTitle = "Emission direction fixture";
	RemasterTileContentId tile { 123, 1, 4 };
	profile.assets[tile].tileId = tile;
	profile.assets[tile].emissionOmnidirectional = true;
	std::string text;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	require(S9xRemasterSerializeProfile(profile, text, diagnostics), "serialize omni-only metadata");
	RemasterProfile decodedProfile;
	require(S9xRemasterParseProfile(text, decodedProfile, diagnostics), "parse omni-only metadata");
	require(decodedProfile.schemaVersion == 19 && decodedProfile.assets.at(tile).emissionOmnidirectional,
		"profile preserves independent emission direction");
	std::string oldText = text;
	oldText.replace(oldText.find("schema_version = 19"), 19, "schema_version = 18");
	require(!S9xRemasterParseProfile(oldText, decodedProfile, diagnostics), "old schema rejects new option");
	std::string invalid = text;
	invalid.replace(invalid.find("emission_omnidirectional = true"), 30, "emission_omnidirectional = 2");
	require(!S9xRemasterParseProfile(invalid, decodedProfile, diagnostics), "invalid boolean rejected");

	RemasterFrame frame;
	frame.width = frame.height = 1;
	frame.originalRgb555.resize(1);
	frame.mainPixels.resize(1);
	frame.subPixels.resize(1);
	RemasterFrameAsset asset; asset.tileId = tile; frame.assets.push_back(asset);
	S9xRemasterApplyProfileToFrame(profile, frame);
	require(frame.assetMetadata[0].emissionOmnidirectional, "profile application preserves direction");
	std::vector<uint8_t> bytes;
	require(S9xSerializeRemasterFrame(frame, bytes), "serialize v27 frame");
	RemasterFrame decoded;
	require(S9xDeserializeRemasterFrame(bytes, decoded), "decode v27 frame");
	require(decoded.assetMetadata[0].emissionOmnidirectional, "frame preserves direction");
	frame.assetMetadata[0].emissionOmnidirectional = false;
	std::vector<uint8_t> planar;
	require(S9xSerializeRemasterFrame(frame, planar), "serialize planar frame");
	const auto difference = std::mismatch(bytes.begin(), bytes.end(), planar.begin());
	require(difference.first != bytes.end(), "direction occupies a distinct byte");
	const size_t offset = difference.first - bytes.begin();
	bytes[offset] = 2;
	require(!S9xDeserializeRemasterFrame(bytes, decoded), "invalid frame direction rejected");
	planar.erase(planar.begin() + offset, planar.begin() + offset + 2); // v27 direction and v31 normal transform
	planar.erase(planar.end() - 4, planar.end()); // v30 empty palette-artwork count
	planar[8] = 26;
	require(S9xDeserializeRemasterFrame(planar, decoded), "legacy v26 frame accepted");
	require(!decoded.assetMetadata[0].emissionOmnidirectional, "legacy frame retains directional default");
	std::puts("Emission direction checks passed");
}
