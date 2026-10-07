#include "profile.h"
#include <cassert>

int main ()
{
	RemasterProfile profile;
	profile.schemaVersion = 19;
	profile.gameTitle = "Semantics test";
	profile.romSha256 = std::string(64, '1');
	RemasterTileContentId id;
	assert(RemasterProfileParsing::ParseTileId("v1:4bpp:0123456789abcdef", id));
	auto &asset = profile.assets[id];
	asset.tileId = id;
	asset.tileRole = RemasterTileRole::WallCorner;
	asset.hasTileSemantics = true;
	asset.tileSemanticNormals = { "north", "east" };
	asset.tileSemanticCorner = "inside";
	asset.tileSemanticNote = "Quoted \"art\" and \\ evidence\nsecond line\twith tab";
	asset.tileSemanticContextDependent = true;
	asset.hasOcclusion = true; // Default coverage must not discard annotations.
	std::string text;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	assert(S9xRemasterSerializeProfile(profile, text, diagnostics));
	RemasterProfile decoded;
	assert(S9xRemasterParseProfile(text, decoded, diagnostics));
	assert(decoded.schemaVersion == 20);
	const auto &result = decoded.assets.at(id);
	assert(result.hasTileSemantics && result.tileSemanticNormals == asset.tileSemanticNormals);
	assert(result.tileSemanticCorner == "inside" && result.tileSemanticContextDependent);
	assert(result.tileSemanticNote == asset.tileSemanticNote);
	asset.tileSemanticNormals = { "north", "north_east" };
	assert(!S9xRemasterSerializeProfile(profile, text, diagnostics));
	asset.tileSemanticNormals = { "north", "north" };
	assert(!S9xRemasterSerializeProfile(profile, text, diagnostics));
	asset.tileRole = RemasterTileRole::Unknown;
	asset.tileSemanticNormals.clear();
	asset.tileSemanticCorner.clear();
	asset.tileSemanticNote.clear();
	asset.tileSemanticContextDependent = false;
	assert(S9xRemasterSerializeProfile(profile, text, diagnostics));
	assert(S9xRemasterParseProfile(text, decoded, diagnostics));
	assert(decoded.assets.at(id).hasTileSemantics); // Explicit unknown is retained.
	text.replace(text.find("schema_version = 20"), 19, "schema_version = 19");
	assert(!S9xRemasterParseProfile(text, decoded, diagnostics));
}
