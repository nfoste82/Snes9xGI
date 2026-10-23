#ifndef _REMASTER_LAYER_VIEW_H_
#define _REMASTER_LAYER_VIEW_H_

#include <cstdint>

// One-based SNES BG labels; OBJ is deliberately absent in isolated views.
inline uint8_t S9xRemasterInspectionLayers(uint8_t main, uint8_t sub, uint8_t forced,
	uint8_t view, bool subscreen)
{
	if (view == 1 || view == 2)
		return subscreen ? 0 : uint8_t((main | sub) & ~forced & (1u << (view - 1)));
	return uint8_t((subscreen ? sub : main) & ~forced);
}

#endif
