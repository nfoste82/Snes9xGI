#include "../remaster/emitter_groups.h"
#include <cstdio>

// Write a candidate profile with existing combination links expanded through
// observed animation tracks. Emission/geometry metadata is never copied.
int main(int argc, char **argv)
{
	if (argc != 4) {
		std::fprintf(stderr, "Usage: expand_emitter_animation_links capture.s9xrmf input.toml output.toml\n"); return 1;
	}
	RemasterFrame frame; RemasterProfile profile; std::vector<RemasterProfileDiagnostic> diagnostics;
	if (std::string(argv[2]) == argv[3] || std::ifstream(argv[3]).good()) {
		std::fprintf(stderr, "Output must be a new candidate file\n"); return 1;
	}
	if (!S9xReadRemasterFrame(argv[1], frame) || !S9xRemasterLoadProfile(argv[2], profile, diagnostics)) return 1;
	const auto original = profile.assets;
	unsigned added = 0;
	for (const auto &entry : original) for (const auto &peer : entry.second.combineEmitters) {
		const auto expanded = S9xRemasterEmitterSelectionAnimationVariants(frame, {entry.first, peer});
		for (const auto &tile : expanded) {
			auto &asset = profile.assets[tile]; asset.tileId = tile;
			for (const auto &other : expanded) if (!(tile == other) &&
				std::find(asset.combineEmitters.begin(), asset.combineEmitters.end(), other) == asset.combineEmitters.end()) {
				asset.combineEmitters.push_back(other); added++;
			}
		}
	}
	std::string text;
	if (!S9xRemasterSerializeProfile(profile, text, diagnostics)) return 1;
	std::ofstream output(argv[3]); output << text;
	if (!output) return 1;
	std::printf("Added %u animation combination links; wrote %s\n", added, argv[3]);
}
