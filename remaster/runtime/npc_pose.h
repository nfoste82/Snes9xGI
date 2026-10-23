#ifndef SNES9X_REMASTER_NPC_POSE_H
#define SNES9X_REMASTER_NPC_POSE_H

#include "frame.h"

// Draw-only reference poses have local artwork/identity, not gameplay elevation.
struct RemasterNpcPosePart
{
	RemasterFrameAsset asset;
	std::array<uint16_t, 64> rgb555 = {};
	int x = 0, y = 0;
	uint16_t address = 0;
	uint8_t slot = 0;
	bool hFlip = false, vFlip = false;
};

enum class RemasterNpcDebugView { Artwork, Normals, Height, Occlusion, Emission, Material };

struct RemasterNpcPoseBounds
{
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	void include(int x, int y)
	{
		if (x1 <= x0 || y1 <= y0) { x0 = x; y0 = y; x1 = x + 8; y1 = y + 8; }
		else { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x + 8); y1 = std::max(y1, y + 8); }
	}
};

inline bool S9xRemasterNpcCanAuthorLayer(const RemasterProfile &profile,
	const RemasterTileContentId &tile, bool normals, bool overwrite)
{
	const auto found = profile.assets.find(tile);
	if (found != profile.assets.end() && S9xRemasterHasGeometryVariants(found->second)) return false;
	return overwrite || found == profile.assets.end() ||
		!(normals ? found->second.hasNormals : found->second.hasHeight);
}

inline std::array<uint8_t, 192> S9xRemasterNpcUniformNormals(const std::array<uint8_t, 3> &direction)
{
	std::array<uint8_t, 192> normals;
	for (unsigned p = 0; p < 64; p++)
		std::copy(direction.begin(), direction.end(), normals.begin() + p * 3);
	return normals;
}

inline std::array<int16_t, 64> S9xRemasterNpcHeightRamp(int bottomPixelY, int tileY, bool vFlip)
{
	std::array<int16_t, 64> height;
	for (unsigned p = 0; p < 64; p++)
	{
		const int displayY = tileY + (vFlip ? 7 - int(p / 8) : int(p / 8));
		height[p] = static_cast<int16_t>(std::max(0, std::min(32767, bottomPixelY - displayY)));
	}
	return height;
}

inline std::array<uint8_t, 3> S9xRemasterOcclusionDebugColor(uint8_t value)
{
	const float coverage = value / 255.0f;
	return {{value, uint8_t(std::lround((0.35f - coverage * 0.30f) * 255.0f)),
		uint8_t(std::lround((0.05f - coverage * 0.05f) * 255.0f))}};
}

inline std::array<uint8_t, 3> S9xRemasterNpcDebugColor(const RemasterFrame &frame, size_t at,
	RemasterNpcDebugView view, double heightMin = 0, double heightMax = 255)
{
	const auto &pixel = frame.mainPixels[at];
	const auto &instance = frame.tileInstances[pixel.instanceId - 1];
	const auto *sourceMetadata = S9xRemasterFrameMetadataForTile(frame, instance.tileId);
	const auto resolved = sourceMetadata ? S9xRemasterResolveInstanceGeometry(*sourceMetadata, instance) : RemasterFrameAssetMetadata{};
	const auto *metadata = sourceMetadata ? &resolved : nullptr;
	const size_t p = pixel.tilePixel;
	if (view == RemasterNpcDebugView::Normals)
	{
		float nx = 0, ny = 0, nz = 1;
		if (metadata && metadata->hasNormals)
		{
			nx = metadata->normalXyz[p * 3] / 127.5f - 1;
			ny = metadata->normalXyz[p * 3 + 1] / 127.5f - 1;
			nz = metadata->normalXyz[p * 3 + 2] / 127.5f - 1;
		}
		else if (metadata && metadata->hasHeight)
		{
			const size_t x = p % 8, y = p / 8;
			nx = -(metadata->height[y * 8 + std::min<size_t>(7, x + 1)] - metadata->height[y * 8 + (x ? x - 1 : x)]) / 255.0f * frame.lightingCoordinateScale;
			ny = -(metadata->height[std::min<size_t>(7, y + 1) * 8 + x] - metadata->height[(y ? y - 1 : y) * 8 + x]) / 255.0f * frame.lightingCoordinateScale;
			nz = 2;
		}
		S9xRemasterTransformNormalForTileInstance(instance, nx, ny, nz,
			metadata && metadata->hasNormals && metadata->normalIgnoreTileTransform);
		const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
		if (length < 0.0001f) { nx = ny = 0; nz = 1; }
		else { nx /= length; ny /= length; nz /= length; }
		return {{uint8_t(std::lround((nx * 0.5f + 0.5f) * 255)), uint8_t(std::lround((ny * 0.5f + 0.5f) * 255)), uint8_t(std::lround((nz * 0.5f + 0.5f) * 255))}};
	}
	if (view == RemasterNpcDebugView::Height)
	{
		if (!metadata || !metadata->hasHeight) return {{160, 0, 180}};
		const double range = heightMax > heightMin ? heightMax - heightMin : 255;
		const uint8_t value = static_cast<uint8_t>(std::lround(std::max(0.0, std::min(1.0,
			(metadata->height[p] - heightMin) / range)) * 255));
		return {{value, value, value}};
	}
	if (view == RemasterNpcDebugView::Occlusion)
	{
		const uint8_t value = metadata && metadata->hasOcclusion ? metadata->occlusion[p] : 255;
		return S9xRemasterOcclusionDebugColor(value);
	}
	if (view == RemasterNpcDebugView::Emission)
	{
		if (!metadata || !metadata->hasEmission) return {{0, 0, 0}};
		const auto rgba = S9xRemasterEmissionSample(*metadata,p,instance.hasPaletteArtwork ? instance.paletteRgb555[p] : frame.originalRgb555[at]);
		return {{uint8_t(rgba[0]*rgba[3]/255),uint8_t(rgba[1]*rgba[3]/255),uint8_t(rgba[2]*rgba[3]/255)}};
	}
	if (view == RemasterNpcDebugView::Material)
	{
		const std::string name = metadata && metadata->hasMaterialSelectors && !metadata->materialSelectors[p].empty() ? metadata->materialSelectors[p] : instance.material;
		if (name.empty()) return {{160, 0, 180}};
		uint32_t hash = 2166136261u;
		for (uint8_t byte : name) hash = (hash ^ byte) * 16777619u;
		return {{uint8_t(64 + (hash & 127)), uint8_t(64 + ((hash >> 8) & 127)), uint8_t(64 + ((hash >> 16) & 127))}};
	}
	const uint16_t rgb = frame.originalRgb555[at];
	return {{uint8_t(((rgb >> 10) & 31) * 255 / 31), uint8_t(((rgb >> 5) & 31) * 255 / 31), uint8_t((rgb & 31) * 255 / 31)}};
}

inline bool S9xRemasterBuildNpcPoseFrame(const std::vector<RemasterNpcPosePart> &parts,
	const std::string &romHash, RemasterFrame &result, const RemasterNpcPoseBounds *canvas = nullptr)
{
	if (parts.empty() || parts.size() > 512 || romHash.size() != 64) return false;
	int x0 = parts.front().x, y0 = parts.front().y, x1 = x0 + 8, y1 = y0 + 8;
	for (const auto &part : parts)
	{
		if (part.x < -256 || part.x > 256 || part.y < -256 || part.y > 256 || part.slot >= 128 ||
			part.asset.tileId.bitDepth != 4 || part.asset.tileId.hashVersion != 1) return false;
		for (uint8_t ink : part.asset.indices) if (ink > 15) return false;
		if (part.asset.tileId.hash != S9xRemasterHashTile(4, part.asset.indices)) return false;
		x0 = std::min(x0, part.x); y0 = std::min(y0, part.y);
		x1 = std::max(x1, part.x + 8); y1 = std::max(y1, part.y + 8);
	}
	if (x1 - x0 > 256 || y1 - y0 > 256) return false;
	if (canvas)
	{
		// Preserve absolute draw coordinates, not per-frame centering. The shared
		// union can be larger than any individual pose, but remains bounded.
		if (canvas->x0 > x0 || canvas->y0 > y0 || canvas->x1 < x1 || canvas->y1 < y1 ||
			canvas->x1 - canvas->x0 > 520 || canvas->y1 - canvas->y0 > 520) return false;
		x0 = canvas->x0; y0 = canvas->y0; x1 = canvas->x1; y1 = canvas->y1;
	}
	RemasterFrame frame;
	frame.width = x1 - x0 + 8; frame.height = y1 - y0 + 8;
	frame.profileRomSha256 = romHash;
	frame.originalRgb555.resize(frame.width * frame.height);
	frame.mainPixels.resize(frame.width * frame.height);
	frame.subPixels.resize(frame.width * frame.height);
	std::set<RemasterTileContentId> identities;
	for (const auto &part : parts)
	{
		if (identities.insert(part.asset.tileId).second) frame.assets.push_back(part.asset);
		RemasterFrameTileInstance instance;
		instance.tileId = part.asset.tileId;
		instance.source = RemasterSourceType::Object;
		instance.sourceIndex = part.slot;
		instance.vramAddress = part.address * 2; // reference VRAM words -> Snes9x bytes
		instance.hFlip = part.hFlip; instance.vFlip = part.vFlip;
		instance.hasPaletteArtwork = true;
		instance.paletteRgb555 = part.rgb555;
		frame.tileInstances.push_back(instance);
	}
	// Earlier OAM slots win. Transparent foreground pixels leave rear parts pickable.
	for (size_t n = parts.size(); n-- > 0;)
	{
		const auto &part = parts[n];
		for (unsigned p = 0; p < 64; p++)
		{
			if (!part.asset.indices[p]) continue;
			const unsigned x = part.x - x0 + 4 + (part.hFlip ? 7 - p % 8 : p % 8);
			const unsigned y = part.y - y0 + 4 + (part.vFlip ? 7 - p / 8 : p / 8);
			auto &pixel = frame.mainPixels[y * frame.width + x];
			pixel.instanceId = static_cast<uint32_t>(n + 1);
			pixel.tilePixel = static_cast<uint8_t>(p);
			pixel.owner = 0x03000000u | part.slot;
			frame.originalRgb555[y * frame.width + x] = part.rgb555[p];
		}
	}
	result = std::move(frame);
	return true;
}

#endif
