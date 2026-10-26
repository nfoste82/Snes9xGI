// Runtime-only room diagnostics. Deliberately not part of S9XRMF/ALTPHM.
#ifndef _REMASTER_ROOM_DIAGNOSTICS_H_
#define _REMASTER_ROOM_DIAGNOSTICS_H_
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>

struct RemasterRoomDiagnostic
{
	// Stable within the pinned room state, NOT an elevation/color index.
	// Minimum room cell in the support component + plane*4096 + BG index*8192.
	int32_t floorId = -1;
	int32_t floorBase = 255; // existing signed-base unknown sentinel
	int32_t lowerFloorId = -1;
	int32_t lowerFloorBase = 255;
	int16_t lowerFloorDistance = -1; // TILE edges: 1 at the foot; -1 unknown, -2 conflict
	uint16_t room = 0xffff;
	uint16_t cell = 0xffff;
	uint8_t plane = 0xff;
	uint8_t background = 0xff;
	// Screen/map orientation, already H/V transformed. Increasing HEIGHT,
	// not wall tangent/lighting normal. N,S,W,E,NW,NE,SW,SE = bits 0..7.
	uint8_t uphillDirections = 0;
	bool wall = false;
	bool heightConflict = false; // independent of structural path ambiguity
	bool axisUnknown = false;
	bool planeObserved = false; // scope, not visual upper/lower ownership
	bool semanticFloorSupport = false; // exact zero-local transition floor; NOT movement ownership
	enum class WallSource : uint8_t { None, AuthoredRole, RomAssembly };
	WallSource wallSource = WallSource::None;
	uint16_t wallTemplateOffset = 0; // diagnostic ownership; never a solver role
};

struct RemasterRoomDiagnostics
{
	std::array<RemasterRoomDiagnostic,8192> cells;
};

struct RemasterRoomPixelDiagnostic
{
	std::array<RemasterRoomDiagnostic,2> planes;
	uint8_t visiblePlane = 0xff; // tie/unknown remains explicit; never merge planes
};

struct RemasterRoomDiagnosticContact { int from, to; uint8_t downhill; };

// Consume independently evidenced structural contacts, NOT height equalities.
// A floor component can be identified while its base is unknown/conflicting.
// No searching for nearest floors or height/rise division. All branches
// (including both corner faces) must identify the SAME floor and tile distance.
inline void S9xRemasterResolveRoomDiagnosticDistances(RemasterRoomDiagnostics &out,
	const std::vector<RemasterRoomDiagnosticContact> &contacts,
	const std::array<uint8_t,8192> &requiredDownhill)
{
	std::vector<std::vector<int>> reverse(8192), dependencies(8192);
	std::array<int,8192> pending = {};
	std::array<uint8_t,8192> covered = {};
	for (const auto &e : contacts) {
		dependencies[e.from].push_back(e.to); reverse[e.to].push_back(e.from);
		pending[e.from]++; covered[e.from] |= e.downhill;
	}
	std::vector<int> queue;
	for (int i=0;i<8192;i++) {
		auto &d=out.cells[i];
		if (!d.wall && d.floorId>=0) {
			d.lowerFloorId=d.floorId; d.lowerFloorBase=d.floorBase;
			d.lowerFloorDistance=0; queue.push_back(i);
		} else if (d.wall && d.lowerFloorDistance==-2) queue.push_back(i);
	}
	for (size_t at=0;at<queue.size();at++) for (int i : reverse[queue[at]]) {
		if (--pending[i] || !requiredDownhill[i] || covered[i]!=requiredDownhill[i]) continue;
		auto &d=out.cells[i];
		if (d.lowerFloorDistance==-2) continue;
		bool conflict=false; int distance=-1, id=-1, base=255;
		for (int j : dependencies[i]) {
			const auto &s=out.cells[j];
			if (s.lowerFloorDistance==-2) { conflict=true; continue; }
			if (distance>=0 && (distance!=s.lowerFloorDistance+1 || id!=s.lowerFloorId)) conflict=true;
			distance=s.lowerFloorDistance+1; id=s.lowerFloorId; base=s.lowerFloorBase;
		}
		d.lowerFloorDistance=conflict ? -2 : static_cast<int16_t>(distance);
		if (!conflict) { d.lowerFloorId=id; d.lowerFloorBase=base; }
		queue.push_back(i);
	}
}
#endif
