#ifndef S9X_REMASTER_OVERWORLD_OFFSETS_H
#define S9X_REMASTER_OVERWORLD_OFFSETS_H
#include "profile.h"
#include <unordered_map>

// Full 16-bit pixel world, independent BGs and worlds. No tile identity in key.
inline uint64_t S9xRemasterOverworldOffsetKey(const RemasterContextLocation &at) {
	return uint64_t(at.area) << 32 | uint64_t(at.background) << 26 | uint64_t(at.y) << 13 | at.x;
}
inline int S9xRemasterCalculatedOverworldOffset(const RemasterCalculatedOverworldHeights *layer,
	const RemasterContextLocation &at) {
	using Layer = RemasterCalculatedOverworldHeights;
	if (!layer || at.area<320 || at.area>321 || at.background!=1 || at.x>=Layer::Width || at.y>=Layer::Width)
		return Layer::Missing;
	return layer->values[(at.area-320)*Layer::Cells + at.y*Layer::Width + at.x];
}
struct RemasterResolvedOverworldOffset {
	int value;
	uint8_t status; // 1 = default zero, 2 = authored, 3 = calculated
};
inline RemasterResolvedOverworldOffset S9xRemasterOverworldFallback(
	const RemasterCalculatedOverworldHeights *layer, const RemasterContextLocation &at) {
	const int calculated = S9xRemasterCalculatedOverworldOffset(layer, at);
	return calculated == RemasterCalculatedOverworldHeights::Missing ? RemasterResolvedOverworldOffset{0,1}
		: RemasterResolvedOverworldOffset{calculated,3};
}
inline RemasterResolvedOverworldOffset S9xRemasterResolveOverworldOffset(const RemasterProfile &p,
	const RemasterContextLocation &at) {
	const auto e = p.overworldOffsets.find(at);
	return e != p.overworldOffsets.end() ? RemasterResolvedOverworldOffset{e->second,2}
		: S9xRemasterOverworldFallback(p.calculatedOverworldHeights.get(), at);
}
struct RemasterOverworldOffsetLookup {
	std::unordered_map<uint64_t, int16_t> entries;
	std::shared_ptr<const RemasterCalculatedOverworldHeights> calculated;
	void compile(const RemasterProfile &p) {
		calculated = p.calculatedOverworldHeights;
		entries.clear(); entries.reserve(p.overworldOffsets.size());
		for (const auto &e:p.overworldOffsets) entries.emplace(S9xRemasterOverworldOffsetKey(e.first), e.second);
	}
	RemasterResolvedOverworldOffset resolve(const RemasterContextLocation &at) const {
		if (at.area<320 || at.area>321 || at.background>1 || at.x>8191 || at.y>8191) return {0,0};
		const auto e=entries.find(S9xRemasterOverworldOffsetKey(at));
		return e!=entries.end() ? RemasterResolvedOverworldOffset{e->second,2}
			: S9xRemasterOverworldFallback(calculated.get(), at);
	}
	int32_t get(const RemasterContextLocation &at) const { return resolve(at).value; }
};
inline RemasterOverworldOffsetLookup &S9xRemasterOverworldLookup() {
	static RemasterOverworldOffsetLookup lookup; return lookup;
}
#endif
