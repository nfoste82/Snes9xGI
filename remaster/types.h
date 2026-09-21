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
	SurfaceIds
};

enum class RemasterSourceType : uint8_t
{
	Backdrop = 1,
	Background = 2,
	Object = 3
};

#endif
