#ifndef REMASTER_EMITTER_GROUPS_H
#define REMASTER_EMITTER_GROUPS_H
#include "frame.h"
#include <set>

// Visible tile footprints, not emitting-pixel bounds: gaps in the artwork do
// not break adjacency. Partial/clipped tiles still supply their true origin.
struct RemasterEmitterPlacement
{
	bool sub = false;
	uint32_t instance = 0;
	int x = 0, y = 0;
	RemasterTileContentId tileId;
	RemasterSourceType source = RemasterSourceType::Backdrop;
	uint8_t sourceIndex = 0;
};

inline std::vector<RemasterEmitterPlacement> S9xRemasterEmitterPlacements(const RemasterFrame &frame,bool channelDonors=false,
	const std::vector<RemasterFramePixel> *selectedEmission=nullptr)
{
	// A supplied raster is the exact production emission donor raster, not a
	// cached visible winner. The fallback keeps tools/source-only callers intact.
	std::unique_ptr<RemasterFrameChannelResolver> resolver;
	if(channelDonors && !selectedEmission)resolver=std::make_unique<RemasterFrameChannelResolver>(frame);
	std::map<std::pair<bool,uint32_t>, RemasterEmitterPlacement> placements;
	std::set<std::pair<bool,uint32_t>> inconsistent;
	for (size_t i = 0; i < frame.mainPixels.size(); i++) {
		const auto &main = frame.mainPixels[i];
		bool sub = (!main.instanceId || main.instanceId > frame.tileInstances.size()) &&
			uint8_t(main.owner >> 24) == uint8_t(RemasterSourceType::Backdrop);
		if (sub && i >= frame.subPixels.size()) continue;
		const auto p = channelDonors ? selectedEmission ? (*selectedEmission)[i] : resolver->select(frame,i,!frame.scenePixels.empty()).donors[size_t(RemasterMetadataChannel::Emission)] : sub ? frame.subPixels[i] : main;
		if(channelDonors)sub=i<frame.subPixels.size() && p.instanceId==frame.subPixels[i].instanceId && p.owner==frame.subPixels[i].owner;
		if (!p.instanceId || p.instanceId > frame.tileInstances.size() || p.tilePixel >= 64) continue;
		const auto &instance = frame.tileInstances[p.instanceId-1];
		int localX = p.tilePixel%8, localY = p.tilePixel/8;
		if (instance.hFlip) localX = 7-localX;
		if (instance.vFlip) localY = 7-localY;
		RemasterEmitterPlacement placement;
		placement.sub = sub; placement.instance = p.instanceId;
		placement.x = int(i%frame.width)-localX; placement.y = int(i/frame.width)-localY;
		placement.tileId = instance.tileId; placement.source = instance.source; placement.sourceIndex = instance.sourceIndex;
		auto key = std::make_pair(sub,p.instanceId);
		auto found = placements.find(key);
		if (found != placements.end() && (found->second.x != placement.x || found->second.y != placement.y)) inconsistent.insert(key);
		else placements[key] = placement;
	}
	std::vector<RemasterEmitterPlacement> result;
	for (const auto &entry : placements) if (!inconsistent.count(entry.first)) result.push_back(entry.second);
	return result;
}

inline bool S9xRemasterEmitterAdjacent(const RemasterEmitterPlacement &a, const RemasterEmitterPlacement &b)
{
	return a.sub == b.sub && a.source == b.source &&
		(a.source != RemasterSourceType::Background || a.sourceIndex == b.sourceIndex) &&
		((a.x == b.x && std::abs(a.y-b.y) == 8) || (a.y == b.y && std::abs(a.x-b.x) == 8));
}

// At least one connected occurrence must contain every selected type. Merely
// finding disconnected pairs elsewhere on screen is insufficient.
inline bool S9xRemasterEmitterSelectionConnected(const RemasterFrame &frame,
	const std::vector<RemasterTileContentId> &selected)
{
	if (selected.size() < 2) return false;
	auto placements = S9xRemasterEmitterPlacements(frame);
	std::vector<bool> visited(placements.size(),false);
	for (size_t seed = 0; seed < placements.size(); seed++) {
		if (visited[seed] || std::find(selected.begin(),selected.end(),placements[seed].tileId) == selected.end()) continue;
		std::vector<size_t> pending = {seed}; visited[seed] = true;
		std::set<RemasterTileContentId> types;
		for (size_t cursor = 0; cursor < pending.size(); cursor++) {
			size_t a = pending[cursor]; types.insert(placements[a].tileId);
			for (size_t b = 0; b < placements.size(); b++) if (!visited[b] &&
				std::find(selected.begin(),selected.end(),placements[b].tileId) != selected.end() &&
				S9xRemasterEmitterAdjacent(placements[a],placements[b])) { visited[b] = true; pending.push_back(b); }
		}
		if (types.size() == selected.size()) return true;
	}
	return false;
}

// Expand each selected identity independently: different parts of a fixture may
// animate asynchronously. Author all cross-frame peers, then runtime adjacency
// decides which currently present placements actually join.
inline std::vector<RemasterTileContentId> S9xRemasterEmitterSelectionAnimationVariants(
	const RemasterFrame &frame, const std::vector<RemasterTileContentId> &selected)
{
	std::set<RemasterTileContentId> expanded(selected.begin(), selected.end());
	for (const auto &tile : selected) {
		std::string groupName;
		for (const auto &instance : frame.tileInstances) if (instance.tileId == tile) {
			groupName = instance.assetGroup; break;
		}
		const auto variants = S9xRemasterFrameAnimationVariants(frame, tile, groupName);
		expanded.insert(variants.begin(), variants.end());
	}
	return {expanded.begin(), expanded.end()};
}
#endif
