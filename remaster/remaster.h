/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REMASTER_H_
#define _REMASTER_H_

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

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

static const uint32_t REMASTER_OWNER_UNSUPPORTED = 0xffffffffu;
static const uint32_t REMASTER_OWNER_FORCED_BLANK = 0xfffffffeu;

extern uint32_t *S9xRemasterCurrentOwners;
extern uint32_t S9xRemasterCurrentOwner;

struct RemasterState
{
	std::atomic<RemasterDebugMode> requestedDebugMode { RemasterDebugMode::Original };
	RemasterDebugMode activeDebugMode = RemasterDebugMode::Original;
	std::vector<uint32_t> mainOwners;
	std::vector<uint32_t> subOwners;
	bool currentSubscreen = false;
};

inline RemasterState &S9xRemasterState (void)
{
	static RemasterState state;
	return state;
}

inline bool S9xRemasterEnabled (void)
{
	return S9xRemasterState().activeDebugMode != RemasterDebugMode::Original;
}

inline RemasterDebugMode S9xRemasterGetDebugMode (void)
{
	return S9xRemasterState().activeDebugMode;
}

inline RemasterDebugMode S9xRemasterCycleDebugMode (void)
{
	RemasterState &state = S9xRemasterState();
	int next = (static_cast<int>(state.requestedDebugMode.load(std::memory_order_relaxed)) + 1) % 3;
	RemasterDebugMode mode = static_cast<RemasterDebugMode>(next);
	state.requestedDebugMode.store(mode, std::memory_order_relaxed);
	return mode;
}

inline void S9xRemasterBeginFrame (size_t pixelCount)
{
	RemasterState &state = S9xRemasterState();
	state.activeDebugMode = state.requestedDebugMode.load(std::memory_order_relaxed);
	if (!S9xRemasterEnabled())
	{
		S9xRemasterCurrentOwners = nullptr;
		return;
	}

	state.mainOwners.assign(pixelCount, REMASTER_OWNER_UNSUPPORTED);
	state.subOwners.assign(pixelCount, REMASTER_OWNER_UNSUPPORTED);
	S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;
	S9xRemasterCurrentOwners = nullptr;
	state.currentSubscreen = false;
}

inline void S9xRemasterClearSpan (size_t offset, size_t width, uint32_t owner = REMASTER_OWNER_UNSUPPORTED)
{
	if (!S9xRemasterEnabled())
		return;

	RemasterState &state = S9xRemasterState();
	if (offset >= state.mainOwners.size())
		return;

	width = width < state.mainOwners.size() - offset ? width : state.mainOwners.size() - offset;
	std::fill_n(state.mainOwners.begin() + offset, width, owner);
	std::fill_n(state.subOwners.begin() + offset, width, owner);
}

inline void S9xRemasterSetSubscreen (bool sub)
{
	RemasterState &state = S9xRemasterState();
	state.currentSubscreen = sub;
	S9xRemasterCurrentOwners = S9xRemasterEnabled() ? (sub ? state.subOwners.data() : state.mainOwners.data()) : nullptr;
}

inline uint32_t S9xRemasterOwner (RemasterSourceType source, uint8_t index, uint16_t tile)
{
	return (static_cast<uint32_t>(source) << 24) | (static_cast<uint32_t>(index) << 16) | tile;
}

inline void S9xRemasterSetDraw (RemasterSourceType source, uint8_t index, uint16_t tile)
{
	if (S9xRemasterEnabled())
		S9xRemasterCurrentOwner = S9xRemasterOwner(source, index, tile);
}

inline void S9xRemasterSetUnsupportedDraw (void)
{
	if (S9xRemasterEnabled())
		S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;
}

inline void S9xRemasterWriteOwner (size_t offset)
{
	if (S9xRemasterCurrentOwners)
		S9xRemasterCurrentOwners[offset] = S9xRemasterCurrentOwner;
}

inline const uint32_t *S9xRemasterMainOwners (void)
{
	const RemasterState &state = S9xRemasterState();
	return state.mainOwners.empty() ? nullptr : state.mainOwners.data();
}

inline const uint32_t *S9xRemasterSubOwners (void)
{
	const RemasterState &state = S9xRemasterState();
	return state.subOwners.empty() ? nullptr : state.subOwners.data();
}

#endif
