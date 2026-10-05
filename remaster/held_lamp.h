#ifndef REMASTER_HELD_LAMP_H
#define REMASTER_HELD_LAMP_H

#include "frame.h"
#include <cmath>

// Verified USA ALTTP adapter input, copied at the completed-frame boundary.
// Do not consult live WRAM during replay or infer lamp state from painted colors.
struct RemasterHeldLampState
{
	bool verifiedRom = false;
	bool darkWithLantern = false; // WRAM $0458; passive held lamp, not Y-item flame
	uint8_t submodule = 0;
	uint8_t facing = 0xff; // WRAM $002f: north/south/west/east = 0/2/4/6
	float linkScreenX = 0; // WRAM $0022 minus scene BG scroll, signed source pixels
	float linkScreenY = 0; // WRAM $0020 minus scene BG scroll
	float floorHeight = 0; // resolved actor floor in physical lighting units
};

// Authorable physical sphere, not an influence radius or a painted light cone.
// Defaults model a warm small flame at hand height, outside Link's silhouette.
struct RemasterHeldLampSettings
{
	float forwardOffset = 12;
	float lateralOffset = 0;
	float height = 12;
	float radius = 2;
	float intensity = 200; // authored emission calibration: 25 = 1x
	std::array<float, 3> color = {{1.0f, 0.72f, 0.38f}}; // sRGB
};

inline bool S9xRemasterHeldLampLight(const RemasterHeldLampState &state,
	const RemasterHeldLampSettings &settings, RemasterFrameLight &light)
{
	if (!state.verifiedRom || !state.darkWithLantern || state.submodule == 20 ||
		state.facing > 6 || (state.facing & 1)) return false;
	for (float v : {state.linkScreenX, state.linkScreenY, state.floorHeight,
		settings.forwardOffset, settings.lateralOffset, settings.height, settings.radius,
		settings.intensity, settings.color[0], settings.color[1], settings.color[2]})
		if (!std::isfinite(v)) return false;
	if (settings.radius <= 0 || settings.intensity <= 0 || settings.height < 0) return false;
	for (float c : settings.color) if (c < 0 || c > 1) return false;
	const float dx[] = {0, 0, -1, 1}, dy[] = {-1, 1, 0, 0};
	const unsigned direction = state.facing / 2;
	light.x = state.linkScreenX + 8 + dx[direction] * settings.forwardOffset - dy[direction] * settings.lateralOffset;
	light.y = state.linkScreenY + 12 + dy[direction] * settings.forwardOffset + dx[direction] * settings.lateralOffset;
	light.z = state.floorHeight + settings.height;
	light.radius = settings.radius;
	light.intensity = settings.intensity;
	light.red = settings.color[0]; light.green = settings.color[1]; light.blue = settings.color[2];
	return true;
}

// Preserve HUD/dialogue ink, not the rectangular region behind it. Transparent BG3
// pixels expose room BG2 pixels, which must still receive lighting at the top.
// Stock ownership is intentional: the optional lamp-free scene can differ.
// ALTTP's ordinary dark-room main plane already contains the complete BG2 room
// under subscreen BG1 color math. Recover it from winning pre-math colors when
// the independent pass was discarded by a partial-flush/transition boundary.
inline bool S9xRemasterRecoverLampScene(RemasterFrame &frame, const uint16_t *colors,
	size_t count, size_t pitch, const RemasterHeldLampState &lamp)
{
	if (!lamp.verifiedRom || !lamp.darkWithLantern || !colors || pitch < frame.width ||
		!frame.width || !frame.height || count / pitch < frame.height ||
		frame.mainPixels.size() != size_t(frame.width) * frame.height) return false;
	for (const auto &p : frame.mainPixels)
	{
		const auto source = static_cast<RemasterSourceType>(p.owner >> 24);
		const unsigned layer = (p.owner >> 16) & 255u;
		if (source != RemasterSourceType::Object && source != RemasterSourceType::Backdrop &&
			!(source == RemasterSourceType::Background && (layer == 1 || layer == 2))) return false;
	}
	frame.scenePixels = frame.mainPixels;
	frame.sceneRgb555.resize(frame.mainPixels.size());
	for (size_t y = 0; y < frame.height; y++)
		std::copy_n(colors + y * pitch, frame.width, frame.sceneRgb555.begin() + y * frame.width);
	return true;
}

inline bool S9xRemasterHeldLampHudPixel(const RemasterFrame &frame, size_t pixel)
{
	if (frame.profileRomSha256 != "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb" ||
		frame.width != 256 || pixel >= size_t(frame.width) * frame.height ||
		pixel >= frame.mainPixels.size()) return false;
	const auto &p = frame.mainPixels[pixel];
	if (static_cast<RemasterSourceType>(p.owner >> 24) == RemasterSourceType::Background &&
		((p.owner >> 16) & 255u) == 2) return true; // BG3 HUD/dialogue, even unknown hashes
	if (!p.instanceId || p.instanceId > frame.tileInstances.size()) return false;
	const auto &instance = frame.tileInstances[p.instanceId - 1];
	const auto *metadata = S9xRemasterFrameMetadataForTile(frame, instance.tileId);
	const std::string &material = metadata && metadata->hasMaterialSelectors && p.tilePixel < 64 &&
		!metadata->materialSelectors[p.tilePixel].empty() ? metadata->materialSelectors[p.tilePixel] : instance.material;
	for (const auto &m : frame.materials)
		if (m.name == material) return m.surfaceClass == RemasterSurfaceClass::UserInterface;
	return false;
}

#endif
