// Explicit nondefault coverage survives saves, unrelated edits and captures.
#include "../runtime/frame.h"
#include <cstdlib>
#include <iostream>
#include <memory>

static void require(bool value, const char *message)
{
	if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

int main(int argc, char **argv)
{
	require(argc == 2, "usage: profile_occlusion_persistence_test temporary-profile-path");
	RemasterProfile profile;
	profile.gameTitle = "Occlusion persistence";
	profile.romSha256 = std::string(64, 'a');
	const RemasterTileContentId zero{1, 1, 4}, mixed{2, 1, 4}, unrelated{3, 1, 4};
	for (auto id : {zero, mixed, unrelated})
	{
		auto &a = profile.assets[id]; a.tileId = id;
		a.hasHeight = true; a.height.fill(7);
	}
	profile.assets[zero].hasOcclusion = profile.assets[mixed].hasOcclusion = true;
	profile.assets[zero].occlusion.fill(0);
	for (unsigned i = 0; i < 64; ++i) profile.assets[mixed].occlusion[i] = i % 3 == 0 ? 0 : i % 3 == 1 ? 125 : 255;
	profile.assets[mixed].tileRole = RemasterTileRole::Doorway;
	profile.assets[mixed].hasDoorframeMask = true;
	profile.assets[mixed].doorframeMask = 0; // Explicit empty is authored, not absent.
	const auto expectedZero = profile.assets[zero].occlusion;
	const auto expectedMixed = profile.assets[mixed].occlusion;
	std::vector<RemasterProfileDiagnostic> errors;
	auto check = [&](const RemasterProfile &p) {
		require(p.assets.at(zero).hasOcclusion && p.assets.at(zero).occlusion == expectedZero, "zero coverage lost");
		require(p.assets.at(mixed).hasOcclusion && p.assets.at(mixed).occlusion == expectedMixed, "mixed coverage lost");
		require(p.assets.at(mixed).hasDoorframeMask && p.assets.at(mixed).doorframeMask == 0, "explicit empty mask lost");
	};
	RemasterProfile loaded;
	require(S9xRemasterWriteProfile(profile, argv[1], errors) && S9xRemasterLoadProfile(argv[1], loaded, errors), "profile write/read failed");
	check(loaded);
	loaded.assets[unrelated].height.fill(31);
	loaded.assets[zero].height[0] = 6; // Geometry edit must not clear coverage.
	require(S9xRemasterWriteProfile(loaded, argv[1], errors) && S9xRemasterLoadProfile(argv[1], profile, errors), "edited profile write/read failed");
	check(profile);
	require(profile.assets.at(unrelated).height[0] == 31 && profile.assets.at(zero).height[0] == 6, "height edit lost");
	std::string text;
	require(S9xRemasterSerializeProfile(profile, text, errors) && S9xRemasterParseProfile(text, loaded, errors), "profile roundtrip failed");
	check(loaded);
	auto frame = std::make_unique<RemasterFrame>();
	frame->width = 3; frame->height = 1;
	frame->mainPixels.resize(3); frame->subPixels.resize(3); frame->originalRgb555.resize(3);
	for (auto id : {zero, mixed, unrelated}) { RemasterFrameTileInstance instance; instance.tileId = id; frame->tileInstances.push_back(instance); }
	S9xRemasterApplyProfileToFrame(loaded, *frame);
	std::vector<uint8_t> bytes;
	auto replay = std::make_unique<RemasterFrame>();
	require(S9xSerializeRemasterFrame(*frame, bytes) && S9xDeserializeRemasterFrame(bytes, *replay), "capture roundtrip failed");
	unsigned checked = 0;
	for (const auto &a : replay->assetMetadata)
	{
		if (a.tileId == zero) { require(a.hasOcclusion && a.occlusion == expectedZero && a.height[0] == 6, "capture zero coverage/height lost"); ++checked; }
		if (a.tileId == mixed) { require(a.hasOcclusion && a.occlusion == expectedMixed && a.hasDoorframeMask && a.doorframeMask == 0, "capture mixed coverage/mask lost"); ++checked; }
	}
	require(checked == 2, "capture missing authored metadata");
	require(std::remove(argv[1]) == 0, "temporary profile cleanup failed");
	std::cout << "Explicit zero/mixed occlusion: write/read, unrelated height edits, profile/capture persistence PASS\n";
}
