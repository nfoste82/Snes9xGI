#include "../runtime/frame.h"
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

	// Appending roles must not change existing capture role IDs.
	static_assert(static_cast<unsigned>(RemasterTileRole::Door) == 8, "Keep existing role IDs stable");
	asset.tileRole = RemasterTileRole::Particle;
	asset.tileSemanticNote = "Floating particle";
	assert(std::string(S9xRemasterTileRoleName(asset.tileRole)) == "particle");
	assert(S9xRemasterTileRoleColor(asset.tileRole) != S9xRemasterTileRoleColor(RemasterTileRole::Unknown));
	assert(S9xRemasterSerializeProfile(profile, text, diagnostics));
	assert(text.find("tile_role = \"particle\"") != std::string::npos);
	assert(S9xRemasterParseProfile(text, decoded, diagnostics));
	assert(decoded.assets.at(id).tileRole == RemasterTileRole::Particle);
	assert(decoded.assets.at(id).tileSemanticNote == asset.tileSemanticNote);

	RemasterFrame frame, captured;
	frame.width = frame.height = 1;
	frame.originalRgb555.resize(1);
	frame.mainPixels.resize(1);
	frame.subPixels.resize(1);
	RemasterFrameAssetMetadata metadata;
	metadata.tileId = id;
	metadata.tileRole = RemasterTileRole::Particle;
	frame.assetMetadata.push_back(metadata);
	std::vector<uint8_t> bytes;
	assert(S9xSerializeRemasterFrame(frame, bytes));
	assert(S9xDeserializeRemasterFrame(bytes, captured));
	assert(captured.assetMetadata.at(0).tileRole == RemasterTileRole::Particle);
}
