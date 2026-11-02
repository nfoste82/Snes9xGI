#ifndef S9X_REMASTER_OVERWORLD_OFFSETS_H
#define S9X_REMASTER_OVERWORLD_OFFSETS_H
#include "profile.h"
#include <unordered_map>

// Full 16-bit pixel world, independent BGs and worlds. No tile identity in key.
inline uint64_t S9xRemasterOverworldOffsetKey(const RemasterContextLocation &at) {
	return uint64_t(at.area) << 32 | uint64_t(at.background) << 26 | uint64_t(at.y) << 13 | at.x;
}
struct RemasterOverworldOffsetLookup {
	std::unordered_map<uint64_t, int16_t> entries;
	void compile(const RemasterProfile &p) {
		entries.clear(); entries.reserve(p.overworldOffsets.size());
		for (const auto &e:p.overworldOffsets) entries.emplace(S9xRemasterOverworldOffsetKey(e.first), e.second);
	}
	int32_t get(const RemasterContextLocation &at) const {
		if (at.area<320 || at.area>321 || at.background>1 || at.x>8191 || at.y>8191) return 0;
		const auto e=entries.find(S9xRemasterOverworldOffsetKey(at));
		return e==entries.end()?0:e->second;
	}
};
inline RemasterOverworldOffsetLookup &S9xRemasterOverworldLookup() {
	static RemasterOverworldOffsetLookup lookup; return lookup;
}
#endif
