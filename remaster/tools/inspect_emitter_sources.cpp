#include "../runtime/emitter_patches.h"
#include <cstdio>
#include <map>

// Inspect authored identities/links in a capture without changing its profile.
int main(int argc, char **argv)
{
	if (argc != 2) return 1;
	RemasterFrame frame;
	if (!S9xReadRemasterFrame(argv[1], frame)) return 1;
	std::map<RemasterTileContentId, unsigned> counts;
	for (const auto &placement : S9xRemasterEmitterPlacements(frame)) {
		const auto *metadata = S9xRemasterFrameMetadataForTile(frame, placement.tileId);
		if (metadata && metadata->hasEmission) counts[placement.tileId]++;
	}
	for (const auto &entry : counts) {
		const auto *m = S9xRemasterFrameMetadataForTile(frame, entry.first);
		unsigned emitting = 0;
		for (unsigned p = 0; p < 64; p++) if (m->emissionRgba[p*4+3] &&
			(m->emissionRgba[p*4] || m->emissionRgba[p*4+1] || m->emissionRgba[p*4+2])) emitting++;
		std::printf("%016llx placements=%u emitting_pixels=%u depth=%.2f omni=%u peers=",
			static_cast<unsigned long long>(entry.first.hash), entry.second, emitting, m->emissionDepth, m->emissionOmnidirectional);
		for (const auto &peer : m->combineEmitters) std::printf("%016llx,", static_cast<unsigned long long>(peer.hash));
		std::puts("");
	}
	for (const auto &group : frame.assetGroups) if (group.name.compare(0,18,"capture_animation_") == 0) {
		std::printf("%s: ", group.name.c_str());
		for (const auto &tile : group.tileIds) std::printf("%016llx,", static_cast<unsigned long long>(tile.hash));
		std::puts("");
	}
}
