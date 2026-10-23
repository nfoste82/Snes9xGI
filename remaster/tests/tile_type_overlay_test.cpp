#include "../runtime/tile_type_overlay.h"
#include "../../port.h"
#include "../../macosx/mac-render.h"
#include <cassert>
#include <set>

int main()
{
	std::set<uint32_t> colors;
	for (unsigned role = 0; role < static_cast<unsigned>(RemasterTileRole::Count); role++)
		assert(colors.insert(S9xRemasterTileRoleColor(static_cast<RemasterTileRole>(role))).second);
	assert(S9xRemasterTileRoleColor(static_cast<RemasterTileRole>(255)) ==
		S9xRemasterTileRoleColor(RemasterTileRole::Unknown));

	RemasterFrame frame;
	frame.mainPixels.resize(1);
	frame.subPixels.resize(1);
	RemasterFrameTileInstance instance;
	instance.source = RemasterSourceType::Background;
	instance.tileId = {123, 1, 4};
	frame.tileInstances.push_back(instance);
	frame.mainPixels[0].instanceId = 1;
	RemasterFrameAssetMetadata metadata;
	metadata.tileId = instance.tileId;
	metadata.tileRole = RemasterTileRole::Wall;
	frame.assetMetadata.push_back(metadata);
	auto tint = [&]() {
		unsigned r = 90, g = 120, b = 150;
		S9xRemasterTintTileTypePixel(frame, S9xRemasterTileTypeOverlayRoles(frame), 0, r, g, b);
		return std::array<unsigned, 3>{r, g, b};
	};
	const auto wall = tint();
	const uint32_t color = S9xRemasterTileRoleColor(RemasterTileRole::Wall);
	assert(wall == (std::array<unsigned, 3>{(90 + 2 * (color >> 16)) / 3,
		(120 + 2 * ((color >> 8) & 255)) / 3, (150 + 2 * (color & 255)) / 3}));
	// Support failure must not hide assigned metadata types.
	for (auto status : {RemasterOccurrenceGeometryStatus::Unresolved, RemasterOccurrenceGeometryStatus::Conflict,
		RemasterOccurrenceGeometryStatus::Resolved})
	{
		frame.tileInstances[0].geometryStatus = status;
		frame.tileInstances[0].placementTileRole = RemasterTileRole::Floor;
		assert(tint() == wall);
	}
	frame.tileInstances[0].source = RemasterSourceType::Object;
	assert(tint() == wall);
	frame.assetMetadata.clear();
	const auto unknown = tint();
	metadata.tileRole = RemasterTileRole::Unknown;
	frame.assetMetadata.push_back(metadata);
	assert(tint() == unknown && unknown != wall);
	frame.tileInstances[0].source = RemasterSourceType::Background;
	frame.tileInstances[0].sourceIndex = 2;
	const std::array<unsigned, 3> original = {90, 120, 150};
	assert(tint() == original); // HUD preserved.
	frame.tileInstances[0].sourceIndex = 0;
	frame.mainPixels[0].instanceId = 0;
	frame.subPixels[0].instanceId = 1;
	assert(tint() == unknown); // Subscreen ownership fallback.
	frame.mainPixels[0].instanceId = 99;
	assert(tint() == original); // Invalid ownership is safe.
	frame.mainPixels[0].instanceId = frame.subPixels[0].instanceId = 0;
	assert(tint() == original);

	static_assert(static_cast<unsigned>(RemasterLightingView::Height) == 5, "Preserve shader IDs");
	static_assert(static_cast<unsigned>(RemasterLightingView::Normal) == 7, "Preserve shader IDs");
	auto view = RemasterLightingView::Composite;
	std::set<unsigned> views;
	do {
		assert(static_cast<unsigned>(view) != 4);
		assert(views.insert(static_cast<unsigned>(view)).second);
		view = NextRemasterLightingView(view);
	} while (view != RemasterLightingView::Composite);
	assert(views.size() == static_cast<unsigned>(RemasterLightingView::Count) - 1);
	assert(NextRemasterLightingView(RemasterLightingView::DirectContribution) == RemasterLightingView::Height);
	assert(views.count(static_cast<unsigned>(RemasterLightingView::MetadataTypes)) == 1);
	assert(NextRemasterLightingView(RemasterLightingView::Mesh) == RemasterLightingView::MetadataTypes);
	assert(NextRemasterLightingView(RemasterLightingView::MetadataTypes) == RemasterLightingView::Composite);
}
