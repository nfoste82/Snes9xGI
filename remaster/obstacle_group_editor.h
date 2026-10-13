#ifndef REMASTER_OBSTACLE_GROUP_EDITOR_H
#define REMASTER_OBSTACLE_GROUP_EDITOR_H

#include "frame.h"

// Occurrence selection is deliberately separate from identity selection: one
// repeated middle tile can occupy several distinct members of a footprint.
struct RemasterObstacleEditorCell
{
	RemasterTileContentId tileId;
	int x = 0, y = 0;
	uint8_t background = 0;
	bool hflip = false, vflip = false;
};

inline bool S9xRemasterObstacleEditorCellAt(const RemasterFrame &frame,
	uint32_t x, uint32_t y, RemasterObstacleEditorCell &cell)
{
	bool sub = false;
	const auto *instance = S9xRemasterFrameVisibleInstanceAt(frame, x, y, &sub);
	if (!instance || instance->source != RemasterSourceType::Background || instance->tileId.bitDepth != 4)
		return false;
	const auto &pixels = sub ? frame.subPixels : frame.mainPixels;
	const auto pixel = pixels[size_t(y) * frame.width + x].tilePixel;
	if (pixel >= 64) return false;
	const int dx = instance->hFlip ? 7 - pixel % 8 : pixel % 8;
	const int dy = instance->vFlip ? 7 - pixel / 8 : pixel / 8;
	cell = {instance->tileId, int(x) - dx, int(y) - dy, instance->sourceIndex,
		instance->hFlip, instance->vFlip};
	return true;
}

inline bool S9xRemasterObstacleEditorBuild(const std::vector<RemasterObstacleEditorCell> &cells,
	const std::vector<RemasterObstacleGroup> &existing, const std::string &name,
	RemasterObstacleGroup &result, std::string &error)
{
	if (cells.empty()) { error = "Select the individual tiles of one obstacle in the scene."; return false; }
	int left = cells.front().x, top = cells.front().y;
	for (const auto &cell : cells) { left = std::min(left, cell.x); top = std::min(top, cell.y); }
	RemasterObstacleGroup candidate; candidate.name = name;
	std::set<std::pair<int,int>> positions;
	for (const auto &cell : cells)
	{
		if (cell.background != cells.front().background || (cell.x-left)%8 || (cell.y-top)%8)
		{ error = "Select aligned 8×8 tiles from one background layer."; return false; }
		const int x = (cell.x-left)/8, y = (cell.y-top)/8;
		if (!positions.emplace(x,y).second) { error = "The selection contains duplicate positions."; return false; }
		for (const auto &group : existing) for (const auto &member : group.members)
			if (member.tileId == cell.tileId)
			{ error = "A selected tile already belongs to obstacle group '" + group.name + "'."; return false; }
		candidate.members.push_back({cell.tileId,x,y,0,cell.hflip,cell.vflip,false});
	}
	std::sort(candidate.members.begin(),candidate.members.end(),[](const auto &a,const auto &b)
		{ return std::tie(a.y,a.x)<std::tie(b.y,b.x); });
	// Editable initial contacts on the selected footprint's south boundary.
	// Z starts at zero: no vertical geometry is guessed from the artwork.
	for (auto &member : candidate.members)
		member.supportContact = !positions.count({member.x,member.y+1});
	auto groups = existing; groups.push_back(candidate);
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterValidateObstacleGroups(groups,diagnostics))
	{ error = diagnostics.empty() ? "Invalid obstacle group." : diagnostics.front().message; return false; }
	result = std::move(candidate); return true;
}

#endif
