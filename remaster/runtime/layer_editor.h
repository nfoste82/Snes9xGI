#ifndef SNES9X_REMASTER_LAYER_EDITOR_H
#define SNES9X_REMASTER_LAYER_EDITOR_H

#include "profile.h"

enum RemasterEditorLayer
{
	RemasterEditorArtwork, RemasterEditorMaterial, RemasterEditorOcclusion,
	RemasterEditorHeight, RemasterEditorNormal, RemasterEditorEmission,
	RemasterEditorMetadata, RemasterEditorDoorframe, RemasterEditorObstacleGrouping
};

// Offset each selected authored layer once, retaining its own relief/sampling.
// A named-context edit must never fall through to the shared asset geometry.
inline bool S9xRemasterStepSelectedTileHeights(RemasterProfile &profile,
	const std::vector<RemasterTileContentId> &tiles, int delta,
	unsigned geometryFacing = 0, const std::string &contextName = {})
{
	if (delta != -8 && delta != -1 && delta != 1 && delta != 8) return false;
	bool changed = false;
	const std::set<RemasterTileContentId> unique(tiles.begin(), tiles.end());
	for (const auto &tile : unique)
	{
		RemasterGeometryLayers *geometry = nullptr;
		auto asset = profile.assets.find(tile);
		if (!contextName.empty())
		{
			auto contexts = profile.tileContexts.find(tile);
			if (contexts == profile.tileContexts.end()) continue;
			auto context = contexts->second.find(contextName);
			if (context == contexts->second.end()) continue;
			geometry = &context->second;
		}
		else if (asset != profile.assets.end())
			geometry = &S9xRemasterFlipGeometryForEditing(asset->second, geometryFacing);
		auto height = geometry && geometry->hasHeight ? geometry->height : std::array<int16_t, 64>{};
		bool tileChanged = false;
		for (auto &sample : height)
		{
			const auto adjusted = static_cast<int16_t>(std::max(-32768, std::min(32767, int(sample) + delta)));
			tileChanged |= sample != adjusted;
			sample = adjusted;
		}
		if (!tileChanged) continue;
		if (!geometry)
		{
			auto &metadata = profile.assets[tile]; metadata.tileId = tile;
			geometry = &S9xRemasterFlipGeometryForEditing(metadata, geometryFacing);
		}
		if (!geometry->hasHeight) geometry->heightSampling = RemasterHeightSampling::Nearest;
		geometry->height = height;
		geometry->hasHeight = true;
		changed = true;
	}
	if (changed) profile.schemaVersion = std::max<uint32_t>(profile.schemaVersion, 2);
	return changed;
}

// Translate within one tile, without wrapping or reflecting vector components.
// Newly exposed samples use the layer's neutral/default pixel.
template<class T, size_t N>
inline void S9xRemasterShiftLayerPixels(std::array<T, N> &values, int dx, int dy,
	const std::array<T, N / 64> &fill = {})
{
	static_assert(N % 64 == 0, "8x8 pixel layer required");
	const auto before = values;
	for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++)
		for (size_t c = 0; c < N / 64; c++)
			values[(y * 8 + x) * (N / 64) + c] =
				x-dx >= 0 && x-dx < 8 && y-dy >= 0 && y-dy < 8 ?
				before[((y-dy) * 8 + x-dx) * (N / 64) + c] : fill[c];
}

inline bool S9xRemasterShiftMetadataLayer(RemasterAssetMetadata &metadata, int layer, int dx, int dy)
{
	if (!((dx == 0 && (dy == -1 || dy == 1)) || (dy == 0 && (dx == -1 || dx == 1)))) return false;
	switch (layer)
	{
		case RemasterEditorMaterial:
			if (!metadata.hasMaterialSelectors) return false;
			S9xRemasterShiftLayerPixels(metadata.materialSelectors, dx, dy); break;
		case RemasterEditorOcclusion:
			if (!metadata.hasOcclusion) return false;
			S9xRemasterShiftLayerPixels(metadata.occlusion, dx, dy, std::array<uint8_t, 1>{{255}}); break;
		case RemasterEditorHeight:
			if (!metadata.hasHeight) return false;
			S9xRemasterShiftLayerPixels(metadata.height, dx, dy); break;
		case RemasterEditorNormal:
			if (!metadata.hasNormals) return false;
			S9xRemasterShiftLayerPixels(metadata.normalXyz, dx, dy, std::array<uint8_t, 3>{{128,128,255}}); break;
		case RemasterEditorEmission:
			if (!metadata.hasEmission) return false;
			{
				std::array<uint8_t,64> mask={}; for(unsigned p=0;p<64;p++) mask[p]=(metadata.emissionColorMask>>p)&1;
				S9xRemasterShiftLayerPixels(mask,dx,dy); metadata.emissionColorMask=0;
				for(unsigned p=0;p<64;p++) metadata.emissionColorMask |= uint64_t(mask[p])<<p;
			}
			S9xRemasterShiftLayerPixels(metadata.emissionRgba, dx, dy); break;
		case RemasterEditorDoorframe:
		{
			if (!metadata.hasDoorframeMask) return false;
			const auto before = metadata.doorframeMask; metadata.doorframeMask = 0;
			for (int y=0;y<8;y++) for (int x=0;x<8;x++)
				if (x+dx>=0 && x+dx<8 && y+dy>=0 && y+dy<8 && (before & (UINT64_C(1) << (y*8+x))))
					metadata.doorframeMask |= UINT64_C(1) << ((y+dy)*8+x+dx);
			break;
		}
		default: return false;
	}
	return true;
}

// Mirror pixel locations only; vector/RGBA components remain intact.
template<class T, size_t N>
inline void S9xRemasterFlipLayerPixels(std::array<T, N> &values, bool horizontal)
{
	static_assert(N % 64 == 0, "8x8 pixel layer required");
	const auto before = values;
	for (size_t y = 0; y < 8; y++) for (size_t x = 0; x < 8; x++)
		for (size_t c = 0; c < N / 64; c++)
			values[(y * 8 + x) * (N / 64) + c] = before[((horizontal ? y : 7 - y) * 8 + (horizontal ? 7 - x : x)) * (N / 64) + c];
}

// Reflect both the sample positions and their directions. With byte/127.5 - 1
// encoding, 255 - component exactly negates the decoded component.
inline void S9xRemasterFlipNormalLayer(std::array<uint8_t, 192> &values, bool horizontal)
{
	S9xRemasterFlipLayerPixels(values, horizontal);
	for (size_t p = 0; p < 64; p++)
		values[p * 3 + (horizontal ? 0 : 1)] = 255 - values[p * 3 + (horizontal ? 0 : 1)];
}

// Quarter turns in screen coordinates (+X right, +Y down).
template<class T, size_t N>
inline void S9xRemasterRotateLayerPixels(std::array<T, N> &values, bool clockwise)
{
	static_assert(N % 64 == 0, "8x8 pixel layer required");
	const auto before = values;
	for (size_t y = 0; y < 8; y++) for (size_t x = 0; x < 8; x++)
		for (size_t c = 0; c < N / 64; c++)
			values[(y * 8 + x) * (N / 64) + c] =
				before[(clockwise ? (7 - x) * 8 + y : x * 8 + 7 - y) * (N / 64) + c];
}

inline void S9xRemasterRotateNormalLayer(std::array<uint8_t, 192> &values, bool clockwise,
	unsigned previewFlip = 0)
{
	// Flip-specific samples are canonical, but their vectors are gameplay-space.
	const bool mirrored = bool(previewFlip & 1) != bool(previewFlip & 2);
	S9xRemasterRotateLayerPixels(values, clockwise != mirrored);
	for (size_t p = 0; p < 64; p++)
	{
		const uint8_t x = values[p * 3], y = values[p * 3 + 1];
		values[p * 3] = clockwise ? 255 - y : y;
		values[p * 3 + 1] = clockwise ? x : 255 - x;
	}
}

inline bool S9xRemasterRotateMetadataLayer(RemasterAssetMetadata &metadata, int layer, bool clockwise)
{
	switch (layer)
	{
		case RemasterEditorMetadata:
			if (!metadata.doorwaySupport) return false;
			{
				const unsigned clockwiseMap[] = {0,3,4,2,1}, counterclockwiseMap[] = {0,4,3,1,2};
				const auto d=static_cast<unsigned>(metadata.doorwaySupportDirection);
				if(d<1 || d>4)return false;
				metadata.doorwaySupportDirection=static_cast<RemasterSupportDirection>((clockwise?clockwiseMap:counterclockwiseMap)[d]);
			}
			break;
		case RemasterEditorMaterial:
			if (!metadata.hasMaterialSelectors) return false;
			S9xRemasterRotateLayerPixels(metadata.materialSelectors, clockwise); break;
		case RemasterEditorOcclusion:
			if (!metadata.hasOcclusion) return false;
			S9xRemasterRotateLayerPixels(metadata.occlusion, clockwise); break;
		case RemasterEditorHeight:
			if (!metadata.hasHeight) return false;
			S9xRemasterRotateLayerPixels(metadata.height, clockwise); break;
		case RemasterEditorNormal:
			if (!metadata.hasNormals) return false;
			S9xRemasterRotateNormalLayer(metadata.normalXyz, clockwise); break;
		case RemasterEditorEmission:
		case RemasterEditorDoorframe:
		{
			if (layer == RemasterEditorEmission ? !metadata.hasEmission : !metadata.hasDoorframeMask) return false;
			auto &bits = layer == RemasterEditorEmission ? metadata.emissionColorMask : metadata.doorframeMask;
			std::array<uint8_t, 64> mask = {};
			for (unsigned p = 0; p < 64; p++) mask[p] = (bits >> p) & 1;
			S9xRemasterRotateLayerPixels(mask, clockwise);
			bits = 0;
			for (unsigned p = 0; p < 64; p++) bits |= uint64_t(mask[p]) << p;
			if (layer == RemasterEditorEmission) S9xRemasterRotateLayerPixels(metadata.emissionRgba, clockwise);
			break;
		}
		default: return false;
	}
	return true;
}

inline bool S9xRemasterFlipMetadataLayer(RemasterAssetMetadata &metadata, int layer, bool horizontal)
{
	switch (layer)
	{
		case RemasterEditorMetadata:
			if (!metadata.doorwaySupport) return false;
			metadata.doorwaySupportDirection=S9xRemasterTransformSupportDirection(metadata.doorwaySupportDirection,horizontal,!horizontal);
			break;
		case RemasterEditorMaterial:
			if (!metadata.hasMaterialSelectors) return false;
			S9xRemasterFlipLayerPixels(metadata.materialSelectors, horizontal); break;
		case RemasterEditorOcclusion:
			if (!metadata.hasOcclusion) return false;
			S9xRemasterFlipLayerPixels(metadata.occlusion, horizontal); break;
		case RemasterEditorHeight:
			if (!metadata.hasHeight) return false;
			S9xRemasterFlipLayerPixels(metadata.height, horizontal); break;
		case RemasterEditorNormal:
			if (!metadata.hasNormals) return false;
			S9xRemasterFlipNormalLayer(metadata.normalXyz, horizontal); break;
		case RemasterEditorEmission:
			if (!metadata.hasEmission) return false;
			{
				std::array<uint8_t,64> mask={}; for(unsigned p=0;p<64;p++) mask[p]=(metadata.emissionColorMask>>p)&1;
				S9xRemasterFlipLayerPixels(mask,horizontal); metadata.emissionColorMask=0;
				for(unsigned p=0;p<64;p++) metadata.emissionColorMask |= uint64_t(mask[p])<<p;
			}
			S9xRemasterFlipLayerPixels(metadata.emissionRgba, horizontal); break;
		case RemasterEditorDoorframe:
		{
			if (!metadata.hasDoorframeMask) return false;
			const uint64_t before = metadata.doorframeMask;
			metadata.doorframeMask = 0;
			for (unsigned y = 0; y < 8; y++) for (unsigned x = 0; x < 8; x++)
				if (before & (UINT64_C(1) << (y * 8 + x)))
					metadata.doorframeMask |= UINT64_C(1) << ((horizontal ? y : 7 - y) * 8 + (horizontal ? 7 - x : x));
			break;
		}
		default: return false;
	}
	return true;
}
#endif
