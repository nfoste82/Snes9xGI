#ifndef S9X_REMASTER_INSPECTOR_HEIGHT_H
#define S9X_REMASTER_INSPECTOR_HEIGHT_H

#include "frame.h"

// Tile-local contrast is independent of the scene height-view multiplier.
// These display bounds never alter authored samples or placement bases.
struct RemasterInspectorHeightPreviewRange
{
	int minimum = 0, maximum = 255;
	uint8_t grayscale(float sample) const
	{
		const float value = (sample - minimum) / float(std::max(1, maximum - minimum));
		return static_cast<uint8_t>(std::lround(std::max(0.0f, std::min(1.0f, value)) * 255.0f));
	}
};

inline RemasterInspectorHeightPreviewRange S9xRemasterInspectorLocalPreviewRange(
	const std::array<int16_t, 64> &height)
{
	const auto bounds = std::minmax_element(height.begin(), height.end());
	if (*bounds.first != *bounds.second) return {int(*bounds.first), int(*bounds.second)};
	// Flat data stays flat, including authored zero. Its exact value is reported
	// separately; avoid pretending a constant layer has a gradient.
	return {std::min(0, int(*bounds.first)), std::max(255, int(*bounds.second))};
}

inline void S9xRemasterInspectorInitializeAuthoredHeight (RemasterAssetMetadata &asset)
{
	if (!asset.hasHeight) asset.height.fill(0);
	asset.hasHeight = true;
}

// Inspector authoring never reads instance geometry. A loaded profile is
// authoritative, including an absent/reset layer; capture fallback is local
// asset data copied by ApplyProfileToFrame, not an effective placement array.
inline const std::array<int16_t, 64> *S9xRemasterInspectorAuthoredHeight (
	const RemasterProfile *profile, const RemasterFrame &frame, const RemasterTileContentId &tile,
	RemasterHeightSampling &sampling)
{
	sampling = RemasterHeightSampling::Nearest;
	if (profile)
	{
		const auto found = profile->assets.find(tile);
		if (found == profile->assets.end() || !found->second.hasHeight) return nullptr;
		sampling = found->second.heightSampling;
		return &found->second.height;
	}
	const auto *captured = S9xRemasterFrameMetadataForTile(frame, tile);
	if (!captured || !captured->hasHeight) return nullptr;
	sampling = captured->heightSampling;
	return &captured->height;
}

inline size_t S9xRemasterInspectorCanonicalPixel (const RemasterFrameTileInstance &instance, size_t displayed)
{
	const size_t x = displayed % 8, y = displayed / 8;
	return (instance.vFlip ? 7 - y : y) * 8 + (instance.hFlip ? 7 - x : x);
}

// IDs are one-based packet instances, not hash occurrences or gameplay planes.
// Never select the highest base (or even the first instance) on ambiguity.
inline uint32_t S9xRemasterInspectorHeightInstance (const RemasterFrame &frame,
	const RemasterTileContentId &tile, uint32_t chosen, std::vector<uint32_t> &occurrences)
{
	occurrences.clear();
	std::set<uint32_t> referenced;
	for (const auto *pixels : { &frame.mainPixels, &frame.subPixels })
		for (const auto &pixel : *pixels)
			if (pixel.instanceId && pixel.instanceId <= frame.tileInstances.size()) referenced.insert(pixel.instanceId);
	for (size_t i = 0; i < frame.tileInstances.size(); ++i)
		if (referenced.count(static_cast<uint32_t>(i + 1)) && frame.tileInstances[i].tileId == tile)
			occurrences.push_back(static_cast<uint32_t>(i + 1));
	if (std::find(occurrences.begin(), occurrences.end(), chosen) != occurrences.end()) return chosen;
	return occurrences.size() == 1 ? occurrences.front() : 0;
}

struct RemasterInspectorEffectiveHeight
{
	const std::array<int16_t, 64> *local = nullptr;
	int32_t base = 0;
	bool available = false;
	bool placementOverride = false;
	// base-only is the runtime's zero-local fallback, not authored zero data.
	int32_t at (size_t canonical) const { return base + (local ? (*local)[canonical] : 0); }
};

inline RemasterInspectorEffectiveHeight S9xRemasterInspectorEffectiveHeight (
	const RemasterFrameTileInstance *instance, const std::array<int16_t, 64> *authored)
{
	RemasterInspectorEffectiveHeight result;
	if (!instance) return result;
	result.base = instance->heightOffset;
	result.placementOverride = instance->hasPlacementHeight;
	result.local = instance->hasPlacementHeight ? &instance->placementHeight : authored;
	result.available = result.local || result.base != 0;
	return result;
}

#endif
