#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>

uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = 0;

int main()
{
	for (unsigned main = 0; main < 32; ++main)
	for (unsigned sub = 0; sub < 32; ++sub)
	for (unsigned forced = 0; forced < 32; ++forced)
	for (unsigned view = 0; view < 3; ++view)
	{
		const auto m = S9xRemasterInspectionLayers(main, sub, forced, view, false);
		const auto s = S9xRemasterInspectionLayers(main, sub, forced, view, true);
		assert(m == (view ? ((main | sub) & ~forced & (1u << (view - 1))) : (main & ~forced)));
		assert(s == (view ? 0 : (sub & ~forced)));
	}
	auto &state = S9xRemasterState();
	for (uint8_t view : {0, 1, 2, 0})
	{
		state.requestedLayerView.store(view);
		S9xRemasterBeginFrame(64, 8, 8, 8);
		assert(state.activeLayerView == view);
		state.requestedLayerView.store((view + 1) % 3);
		assert(state.activeLayerView == view); // no mid-frame UI changes
	}
	std::cout << "PASS layer masks: all enable/forced combinations, subscreen-only BG exposure, OBJ exclusion and frame-boundary latching\n";
}
