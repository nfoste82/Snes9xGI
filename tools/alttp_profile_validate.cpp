/* Validate a generated ALTTP profile with the same parser used by Snes9x. */
#include "../remaster/profile.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
struct LayerCounts
{
	size_t materials = 0;
	size_t groups = 0;
	size_t rules = 0;
	size_t assets = 0;
	size_t selectors = 0;
	size_t occlusion = 0;
	size_t height = 0;
	size_t normals = 0;
	size_t emission = 0;

	bool operator== (const LayerCounts &other) const
	{
		return materials == other.materials && groups == other.groups && rules == other.rules &&
			assets == other.assets && selectors == other.selectors && occlusion == other.occlusion &&
			height == other.height && normals == other.normals && emission == other.emission;
	}
};

LayerCounts CountLayers (const RemasterProfile &profile)
{
	LayerCounts counts;
	counts.materials = profile.materials.size();
	counts.groups = profile.assetGroups.size();
	counts.rules = profile.rules.size();
	counts.assets = profile.assets.size();
	for (const auto &entry : profile.assets)
	{
		const RemasterAssetMetadata &asset = entry.second;
		counts.selectors += asset.hasMaterialSelectors;
		counts.occlusion += asset.hasOcclusion;
		counts.height += asset.hasHeight;
		counts.normals += asset.hasNormals;
		counts.emission += asset.hasEmission;
	}
	return counts;
}

void PrintDiagnostics (const char *stage, const std::vector<RemasterProfileDiagnostic> &diagnostics)
{
	std::cerr << stage << " failed\n";
	for (const RemasterProfileDiagnostic &diagnostic : diagnostics)
	{
		if (diagnostic.line)
			std::cerr << "  line " << diagnostic.line << ": ";
		else
			std::cerr << "  ";
		std::cerr << diagnostic.message << '\n';
	}
}
} // namespace

int main (int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " PROFILE.toml\n";
		return 2;
	}

	RemasterProfile loaded;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterLoadProfile(argv[1], loaded, diagnostics))
	{
		PrintDiagnostics("profile parse", diagnostics);
		return 1;
	}
	const LayerCounts before = CountLayers(loaded);

	std::string serialized;
	if (!S9xRemasterSerializeProfile(loaded, serialized, diagnostics))
	{
		PrintDiagnostics("profile serialization", diagnostics);
		return 1;
	}

	RemasterProfile roundTrip;
	if (!S9xRemasterParseProfile(serialized, roundTrip, diagnostics))
	{
		PrintDiagnostics("serialized profile parse", diagnostics);
		return 1;
	}
	if (!(before == CountLayers(roundTrip)))
	{
		std::cerr << "round-trip changed asset or layer counts\n";
		return 1;
	}

	std::string serializedAgain;
	if (!S9xRemasterSerializeProfile(roundTrip, serializedAgain, diagnostics))
	{
		PrintDiagnostics("round-trip serialization", diagnostics);
		return 1;
	}
	if (serialized != serializedAgain)
	{
		std::cerr << "round-trip changed canonical profile content\n";
		return 1;
	}

	std::cout << "Valid profile: schema " << loaded.schemaVersion << ", " << before.materials <<
		" materials, " << before.groups << " groups, " << before.rules << " rules, " <<
		before.assets << " assets\n";
	std::cout << "Asset layers: " << before.selectors << " materials, " << before.occlusion <<
		" occlusion, " << before.height << " height, " << before.normals << " normals, " <<
		before.emission << " emission\n";
	return 0;
}
