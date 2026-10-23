#ifndef REMASTER_SURFACE_MESH_MATERIAL_H
#define REMASTER_SURFACE_MESH_MATERIAL_H

#include "profile.h"
#include "surface_mesh.h"

namespace RemasterSurfaceMesh
{
// Solidity is a material/placement decision, never a shading-normal decision.
// Unclassified elevated BG details remain finite even when they touch a wall.
// Missing structural semantics must not close their underpasses.
inline void classify(Sample &sample, RemasterSourceType source, unsigned sourceIndex,
	RemasterSurfaceClass surfaceClass, const std::string &assetGroup, uint32_t instanceId,
	RemasterTileRole role = RemasterTileRole::Unknown, bool occurrenceRole = false)
{
	// Explicit occurrence semantics override structural material defaults, including
	// Unknown: unresolved artwork must never inherit a wall shell from its hash.
	if (occurrenceRole || role != RemasterTileRole::Unknown)
	{
		surfaceClass = role == RemasterTileRole::Wall || role == RemasterTileRole::WallCorner ? RemasterSurfaceClass::WallFace :
			role == RemasterTileRole::Floor ? RemasterSurfaceClass::Floor : RemasterSurfaceClass::Unclassified;
	}
	const bool object = source == RemasterSourceType::Object;
	const bool wall = surfaceClass == RemasterSurfaceClass::WallFace ||
		(surfaceClass == RemasterSurfaceClass::WallTop && assetGroup != "stair_rails");
	const bool floor = surfaceClass == RemasterSurfaceClass::Floor;
	sample.sheet = !object && (wall || floor);
	sample.solidWall = !object && wall;
	// OAM slots are frame-local identities; never weld unrelated sprites or BG.
	sample.domain = object ? (uint64_t(1) << 48) | (uint64_t(sourceIndex) + 1) :
		wall ? 2 : floor ? 1 : surfaceClass == RemasterSurfaceClass::Unclassified ? 10 + sourceIndex :
		(uint64_t(2) << 48) | instanceId;
}
}

#endif
