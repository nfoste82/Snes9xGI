#ifndef _REMASTER_TILE_TYPE_OVERLAY_H_
#define _REMASTER_TILE_TYPE_OVERLAY_H_

#include "frame.h"

// Show authored metadata, not a runtime geometry/support classification. An
// unresolved placement can still have a perfectly valid authored tile type.
inline std::vector<RemasterTileRole> S9xRemasterTileTypeOverlayRoles(const RemasterFrame &frame)
{
	std::map<RemasterTileContentId, RemasterTileRole> roles;
	for (const auto &metadata : frame.assetMetadata) roles[metadata.tileId] = metadata.tileRole;
	std::vector<RemasterTileRole> result;
	result.reserve(frame.tileInstances.size());
	for (const auto &instance : frame.tileInstances)
	{
		const auto found = roles.find(instance.tileId);
		result.push_back(found == roles.end() ? RemasterTileRole::Unknown : found->second);
	}
	return result;
}

inline void S9xRemasterTintTileTypePixel(const RemasterFrame &frame,
	const std::vector<RemasterTileRole> &roles, size_t index,
	unsigned &red, unsigned &green, unsigned &blue)
{
	if (index >= frame.mainPixels.size()) return;
	const auto &main = frame.mainPixels[index];
	const auto &pixel = !main.instanceId && index < frame.subPixels.size() ? frame.subPixels[index] : main;
	if (!pixel.instanceId || pixel.instanceId > roles.size() || pixel.instanceId > frame.tileInstances.size()) return;
	const auto &instance = frame.tileInstances[pixel.instanceId - 1];
	// BG3 is the HUD; backdrop and unsupported pixels are not decoded tiles.
	if (instance.source != RemasterSourceType::Object &&
		(instance.source != RemasterSourceType::Background || instance.sourceIndex == 2)) return;
	const uint32_t color = S9xRemasterTileRoleColor(roles[pixel.instanceId - 1]);
	red = (red + 2 * ((color >> 16) & 255)) / 3;
	green = (green + 2 * ((color >> 8) & 255)) / 3;
	blue = (blue + 2 * (color & 255)) / 3;
}

#endif
