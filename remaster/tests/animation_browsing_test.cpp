#include "../runtime/frame.h"

#include <cstdio>
#include <cstdlib>

static void require (bool condition, const char *message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

int main (int argc, char **argv)
{
	const RemasterTileContentId first { 1, 1, 4 }, second { 2, 1, 4 }, fixed { 3, 1, 4 };
	RemasterFrame frame;
	frame.assetGroups = { { "capture_animation_1", { first, second } }, { "stone", { fixed } } };
	require(S9xRemasterFrameAnimationVariants(frame, first, "") ==
		(std::vector<RemasterTileContentId> { first, second }), "animation lookup without instance group");
	require(S9xRemasterFrameAnimationVariants(frame, second, "stone") ==
		(std::vector<RemasterTileContentId> { first, second }), "animation takes precedence over semantic group");
	require(S9xRemasterFrameAnimationVariants(frame, fixed, "") ==
		(std::vector<RemasterTileContentId> { fixed }), "static tile remains singleton");
	require(S9xRemasterFrameAnimationVariants(frame, fixed, "stone") ==
		(std::vector<RemasterTileContentId> { fixed }), "authored group fallback");

	RemasterProfile profile;
	RemasterFrameTileInstance instance;
	instance.tileId = first;
	instance.assetGroup = "capture_animation_1";
	frame.tileInstances.push_back(instance);
	for (int refresh = 0; refresh < 3; refresh++)
	{
		S9xRemasterApplyProfileToFrame(profile, frame);
		require(frame.assetGroups.size() == 1, "profile refresh retains exactly one observed track");
		require(frame.tileInstances[0].assetGroup == "capture_animation_1", "profile refresh retains instance track");
	}

	// A click capture contains only the currently displayed phase. Refreshing
	// and editing it must never turn the full profile into a visible-only subset.
	std::vector<RemasterProfileDiagnostic> diagnostics;
	profile.gameTitle = "Animation preservation fixture";
	profile.romSha256.assign(64, '0');
	for (const auto &tile : { first, second, fixed })
	{
		auto &metadata = profile.assets[tile];
		metadata.tileId = tile;
		metadata.hasHeight = metadata.hasNormals = metadata.hasEmission = true;
		metadata.height.fill(static_cast<uint8_t>(18 + tile.hash));
		metadata.normalXyz.fill(128);
		metadata.emissionRgba.fill(static_cast<uint8_t>(35 + tile.hash));
		metadata.emissionDepth = float(tile.hash);
		metadata.emissionOmnidirectional = true;
	}
	std::string beforeCapture;
	require(S9xRemasterSerializeProfile(profile, beforeCapture, diagnostics), "serialize all authored phases");
	RemasterFrame singleFrame;
	singleFrame.tileInstances.push_back(instance);
	for (int refresh = 0; refresh < 3; refresh++)
	{
		S9xRemasterApplyProfileToFrame(profile, singleFrame);
		require(singleFrame.assetMetadata.size() == 1, "single frame only needs visible phase metadata");
		std::string afterCapture;
		require(S9xRemasterSerializeProfile(profile, afterCapture, diagnostics) && afterCapture == beforeCapture,
			"capture/profile refresh preserves unseen authored phases byte-for-byte");
	}
	profile.assets[first].height[0] = 42;
	std::string saved;
	RemasterProfile reloaded;
	require(S9xRemasterSerializeProfile(profile, saved, diagnostics) &&
		S9xRemasterParseProfile(saved, reloaded, diagnostics), "save/reload after single-frame edit");
	require(reloaded.assets[first].height[0] == 42, "visible edit survives save/reload");
	for (const auto &tile : { second, fixed })
	{
		const auto &original = profile.assets.at(tile);
		const auto &retained = reloaded.assets.at(tile);
		require(retained.hasHeight && retained.height == original.height && retained.hasNormals &&
			retained.normalXyz == original.normalXyz && retained.hasEmission &&
			retained.emissionRgba == original.emissionRgba && retained.emissionDepth == original.emissionDepth &&
			retained.emissionOmnidirectional, "unseen phase layers and emission options survive save/reload");
	}

	if (argc == 3)
	{
		require(S9xReadRemasterFrame(argv[1], frame), "read room061_007 capture");
		std::vector<RemasterProfileDiagnostic> diagnostics;
		require(S9xRemasterLoadProfile(argv[2], profile, diagnostics), "read captured profile");
		for (int refresh = 0; refresh < 3; refresh++)
		{
			for (uint64_t hash : { UINT64_C(0x125926cbfb8f3f5c), UINT64_C(0xe9f8444b3af50ba3),
				UINT64_C(0x73caad274b5bb270), UINT64_C(0xfd86d3ed217c0d03) })
			{
				const RemasterTileContentId tile { hash, 1, 4 };
				const size_t expected = hash == UINT64_C(0xfd86d3ed217c0d03) ? 1 : 3;
				const auto variants = S9xRemasterFrameAnimationVariants(frame, tile, "");
				require(variants.size() == expected, "actual torch frame count");
				for (const auto &variant : variants)
				{
					require(S9xRemasterFrameAssetForTile(frame, variant) != nullptr, "variant artwork is decoded");
					require(S9xRemasterFrameArtworkColorsForTile(frame, variant) != nullptr, "variant artwork colors retained");
				}
			}
			S9xRemasterApplyProfileToFrame(profile, frame);
		}
	}
	else
		require(argc == 1, "usage: animation-browsing-test [room061_007.s9xrmf profile.toml]");
	std::puts("Animation browsing checks passed");
}
