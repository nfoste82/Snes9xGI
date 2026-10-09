/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REMASTER_TYPES_H_
#define _REMASTER_TYPES_H_

#include <cstdint>

enum class RemasterDebugMode : uint8_t
{
	Original,
	Overlay,
	SurfaceIds,
	TileTypes,
	FloorWalls,
	Count
};

enum class RemasterTileRole : uint8_t
{
	Unknown, Floor, Wall, WallCorner, Obstacle, StairTop, StairBottom, Other, Door, Count
};

inline const char *S9xRemasterTileRoleName (RemasterTileRole role)
{
	static const char *names[] = {"unknown", "floor", "wall", "wall_corner", "obstacle", "stair_top", "stair_bottom", "other", "door"};
	return static_cast<unsigned>(role) < static_cast<unsigned>(RemasterTileRole::Count) ? names[static_cast<unsigned>(role)] : "unknown";
}

inline uint32_t S9xRemasterTileRoleColor (RemasterTileRole role)
{
	// RGB colors; unknown deliberately remains bright red.
	static const uint32_t colors[] = {0xff3030, 0x40d878, 0x408cff, 0xb060ff, 0xffa030, 0x30dddd, 0xff70bd, 0xaaaaaa, 0xe8ce60};
	return colors[static_cast<unsigned>(role) < static_cast<unsigned>(RemasterTileRole::Count) ? static_cast<unsigned>(role) : 0];
}

enum class RemasterSourceType : uint8_t
{
	Backdrop = 1,
	Background = 2,
	Object = 3
};

// Masks are canonical, row-major: bit (y * 8 + x), with bit 0 at top left.
// Transform only when sampling a placed tile; editor and capture tilePixel are canonical.
inline bool S9xRemasterDoorframePixel (uint64_t mask, unsigned x, unsigned y, bool hFlip = false, bool vFlip = false)
{
	if (x >= 8 || y >= 8) return false;
	return (mask >> ((vFlip ? 7 - y : y) * 8 + (hFlip ? 7 - x : x))) & 1;
}

#endif
