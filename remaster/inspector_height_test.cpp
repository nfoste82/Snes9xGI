#include "inspector_height.h"
#include <cstdlib>
#include <iostream>

static void check (bool value, const char *message)
{
	if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

int main ()
{
	RemasterFrame frame;
	RemasterTileContentId tile;
	tile.hash = 123;
	tile.hashVersion = 1;
	tile.bitDepth = 4;
	RemasterFrameAssetMetadata captured;
	captured.tileId = tile;
	captured.hasHeight = true;
	for (size_t p = 0; p < 64; ++p) captured.height[p] = p % 8;
	frame.assetMetadata.push_back(captured);
	RemasterFrameTileInstance lower, upper;
	lower.tileId = upper.tileId = tile;
	upper.heightOffset = 32;
	frame.tileInstances = { lower, upper };
	frame.mainPixels.resize(1); frame.subPixels.resize(1);
	frame.mainPixels[0].instanceId = 1;
	frame.subPixels[0].instanceId = 2;
	RemasterHeightSampling sampling;
	const auto *authored = S9xRemasterInspectorAuthoredHeight(nullptr, frame, tile, sampling);
	check(authored && *authored == captured.height, "capture-local authored samples");
	const auto effective = S9xRemasterInspectorEffectiveHeight(&frame.tileInstances[1], authored);
	for (size_t p = 0; p < 64; ++p)
		check((*authored)[p] == p % 8 && effective.at(p) == 32 + int(p % 8), "authored 0..7 / effective 32..39");
	std::vector<uint32_t> occurrences;
	check(S9xRemasterInspectorHeightInstance(frame, tile, 0, occurrences) == 0 && occurrences.size() == 2,
		"multiple occurrences must remain ambiguous, not highest plane");
	check(S9xRemasterInspectorHeightInstance(frame, tile, 1, occurrences) == 1, "picked lower occurrence");
	check(S9xRemasterInspectorHeightInstance(frame, tile, 2, occurrences) == 2, "explicit upper occurrence");
	for (bool h : {false, true}) for (bool v : {false, true})
	{
		auto instance = upper;
		instance.hFlip = h; instance.vFlip = v;
		instance.hasPlacementHeight = true;
		for (size_t p = 0; p < 64; ++p) instance.placementHeight[p] = 63 - p;
		const auto overridden = S9xRemasterInspectorEffectiveHeight(&instance, authored);
		check(overridden.placementOverride, "placement override source");
		for (size_t p = 0; p < 64; ++p)
		{
			const size_t canonical = S9xRemasterInspectorCanonicalPixel(instance, p);
			check(canonical == (v ? 7 - p / 8 : p / 8) * 8 + (h ? 7 - p % 8 : p % 8), "flip canonical index");
			check(overridden.at(canonical) == 32 + int(63 - canonical), "canonical placement override, base once");
			check((*authored)[canonical] == canonical % 8, "override never modifies authoring");
		}
		instance.heightOffset = -40;
		check(S9xRemasterInspectorEffectiveHeight(&instance, authored).at(63) == -40, "signed negative effective height");
		check(S9xRemasterInspectorEffectiveHeight(&instance, nullptr).available, "override without authored height");
	}
	check(!S9xRemasterInspectorEffectiveHeight(nullptr, authored).available, "detached animation has no effective occurrence");
	check(!S9xRemasterInspectorEffectiveHeight(&lower, nullptr).available, "no local metadata or base is missing");
	lower.heightOffset = -10;
	const auto baseOnly = S9xRemasterInspectorEffectiveHeight(&lower, nullptr);
	check(baseOnly.available && !baseOnly.local && baseOnly.at(0) == -10, "base-only runtime fallback is explicit");
	RemasterTileContentId detached = tile; detached.hash++;
	check(S9xRemasterInspectorHeightInstance(frame, detached, 2, occurrences) == 0 && occurrences.empty(), "animation absent in representative frame");
	frame.tileInstances.pop_back();
	check(S9xRemasterInspectorHeightInstance(frame, tile, 0, occurrences) == 1, "unique occurrence is unambiguous");
	frame.tileInstances.push_back(upper);
	frame.subPixels[0].instanceId = 0;
	check(S9xRemasterInspectorHeightInstance(frame, tile, 2, occurrences) == 1,
		"unused draw context is not a selectable occurrence");

	RemasterProfile profile;
	profile.schemaVersion = 20;
	profile.gameTitle = "Inspector height separation fixture";
	profile.romSha256 = std::string(64, '0');
	check(!S9xRemasterInspectorAuthoredHeight(&profile, frame, tile, sampling), "loaded missing profile layer never falls back to capture");
	auto &asset = profile.assets[tile]; asset.tileId = tile;
	asset.height.fill(99); // stale non-authored storage must not leak on reset/first paint
	S9xRemasterInspectorInitializeAuthoredHeight(asset);
	check(asset.height[1] == 0, "new authored layer initialized deliberately, never capture or placement");
	asset.height = captured.height;
	asset.hasTileSemantics = true;
	asset.tileSemanticNote = "Keep unrelated schema20 metadata";
	std::string before, saved;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	check(S9xRemasterSerializeProfile(profile, before, diagnostics), "serialize undo snapshot");
	S9xRemasterInspectorInitializeAuthoredHeight(asset);
	check(asset.height == captured.height, "existing local samples survive brush initialization");
	asset.height[3] = 9; // selected-pixel local edit
	check(S9xRemasterSerializeProfile(profile, saved, diagnostics), "serialize local edit");
	RemasterProfile reloaded;
	check(S9xRemasterParseProfile(saved, reloaded, diagnostics), "parse saved edit");
	check(reloaded.assets.at(tile).height[3] == 9 && reloaded.assets.at(tile).height[7] == 7,
		"save only local bytes, not placement-offset samples");
	check(reloaded.schemaVersion == 20 && reloaded.assets.at(tile).tileSemanticNote == asset.tileSemanticNote,
		"height edits preserve schema20 semantic annotations");
	// Animation/layer copy uses the authored source (as the native action does),
	// even when the source occurrence supplies a different effective local map.
	auto &variant = reloaded.assets[detached]; variant.tileId = detached;
	variant.hasHeight = true;
	variant.height = reloaded.assets.at(tile).height;
	variant.heightSampling = reloaded.assets.at(tile).heightSampling;
	check(variant.height[3] == 9 && variant.height[7] == 7,
		"detached animation copy remains authored-local, not effective");
	frame.tileInstances[0].hasPlacementHeight = true;
	frame.tileInstances[0].placementHeight.fill(6);
	frame.tileInstances[0].heightOffset = -32;
	S9xRemasterApplyProfileToFrame(reloaded, frame);
	authored = S9xRemasterInspectorAuthoredHeight(&reloaded, frame, tile, sampling);
	check(authored && (*authored)[3] == 9 && S9xRemasterFrameMetadataForTile(frame, tile)->height[3] == 9,
		"profile refresh/extraction stores authored local values, not adjusted instance geometry");
	const auto refreshed = S9xRemasterInspectorEffectiveHeight(&frame.tileInstances[0], authored);
	check(refreshed.placementOverride && refreshed.at(3) == -26, "profile refresh preserves independent placement override/base");
	check(S9xRemasterParseProfile(before, reloaded, diagnostics), "undo local snapshot");
	check(reloaded.assets.at(tile).height == captured.height, "undo restores authored map, no placement data");
	S9xRemasterApplyProfileToFrame(reloaded, frame);
	check(frame.assetMetadata.front().height == captured.height && frame.tileInstances[0].heightOffset == -32 &&
		frame.tileInstances[0].placementHeight[3] == 6, "undo/refresh restores authored local map without changing placement geometry");
	std::cout << "inspector height separation checks passed\n";
}
