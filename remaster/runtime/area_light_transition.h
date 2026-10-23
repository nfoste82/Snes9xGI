#ifndef _REMASTER_AREA_LIGHT_TRANSITION_H_
#define _REMASTER_AREA_LIGHT_TRANSITION_H_

#include "profile.h"
#include <algorithm>

// Emulation-thread state only. Presentation drops must not lengthen the fade.
struct RemasterAreaLightTransition
{
	RemasterAreaContext area;
	RemasterDirectionalLight light;
	uint32_t frame = 0, elapsed = 20;
	uint16_t scrollX = 0, scrollY = 0;
	float startBrightness = 0;
	bool initialized = false;

	RemasterDirectionalLight update(RemasterAreaContext destination,
		RemasterDirectionalLight target, uint32_t emulatedFrame,
		bool preparingScroll, uint16_t x, uint16_t y, bool scrollPhaseReady = true)
	{
		const uint32_t delta = emulatedFrame - frame;
		const bool moved = x != scrollX || y != scrollY;
		scrollX = x; scrollY = y; frame = emulatedFrame;
		if (!initialized || !destination.valid() || delta > 120)
		{
			initialized = destination.valid(); area = destination; light = target; elapsed = 20;
			return light;
		}
		const bool changed = destination.id != area.id;
		if (changed)
		{
			// $A0 already names the destination while module 7/2 loads it.
			// Keep the outgoing light until the actual BG camera starts moving.
			if (preparingScroll && (!scrollPhaseReady || !moved)) return light;
			startBrightness = light.enabled ? light.brightness : 0;
			area = destination; elapsed = 0;
		}
		else if (elapsed >= 20)
		{
			// Live authoring within the same area remains immediate.
			light = target;
			return light;
		}
		elapsed = std::min<uint32_t>(20, elapsed + (changed ? 1 : delta));
		const float endBrightness = target.enabled ? target.brightness : 0;
		// Keep the outgoing direction/color during a fade to a disabled light.
		if (target.enabled) light = target;
		light.brightness = startBrightness + (endBrightness - startBrightness) * (float(elapsed) / 20);
		light.enabled = target.enabled || light.brightness > 0;
		if (elapsed == 20) light = target;
		return light;
	}
};

#endif
