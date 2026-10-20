#ifndef SNES9X_REMASTER_LAYER_EDITOR_H
#define SNES9X_REMASTER_LAYER_EDITOR_H

#include "profile.h"

enum RemasterEditorLayer
{
	RemasterEditorArtwork, RemasterEditorMaterial, RemasterEditorOcclusion,
	RemasterEditorHeight, RemasterEditorNormal, RemasterEditorEmission,
	RemasterEditorMetadata, RemasterEditorDoorframe
};

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

inline bool S9xRemasterFlipMetadataLayer(RemasterAssetMetadata &metadata, int layer, bool horizontal)
{
	switch (layer)
	{
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
