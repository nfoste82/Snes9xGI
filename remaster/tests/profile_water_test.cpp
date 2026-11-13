#include "../runtime/profile.h"
#include <iostream>
#include <stdexcept>

int main()
{
	try
	{
		RemasterProfile profile;
		profile.gameTitle = "Water metadata";
		profile.romSha256 = std::string(64, 'a');
		auto &asset = profile.assets[RemasterTileContentId{123, 1, 4}];
		asset.tileId = RemasterTileContentId{123, 1, 4};
		asset.tileRole = RemasterTileRole::Floor;
		asset.hasTileSemantics = true;
		asset.tileSemanticWater = true;
		std::vector<RemasterProfileDiagnostic> errors;
		std::string text;
		if (!S9xRemasterSerializeProfile(profile, text, errors) || text.find("tile_semantic_water = true") == std::string::npos)
		{
			for (const auto &error : errors) std::cerr << error.line << ": " << error.message << '\n';
			throw std::runtime_error("water floor must serialize and validate");
		}
		asset.tileSemanticWater = false;
		if (!S9xRemasterSerializeProfile(profile, text, errors) || text.find("tile_semantic_water = false") == std::string::npos)
			throw std::runtime_error("water false must serialize and validate");
		asset.tileSemanticWater = true;
		asset.tileRole = RemasterTileRole::Other;
		if (S9xRemasterSerializeProfile(profile, text, errors))
			throw std::runtime_error("nonfloor water must be rejected");
		std::cout << "Floor water profile roundtrip passed\n";
		return 0;
	}
	catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
