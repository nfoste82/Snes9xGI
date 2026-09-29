/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REMASTER_FRAME_H_
#define _REMASTER_FRAME_H_

#include "profile.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

static const uint32_t REMASTER_FRAME_SCHEMA_VERSION = 19;

struct RemasterFramePixel
{
	uint32_t owner = 0xffffffffu;
	uint32_t instanceId = 0;
	uint8_t tilePixel = 0xff;
};

struct RemasterFrameAsset
{
	RemasterTileContentId tileId;
	uint8_t indices[64] = {};
};

struct RemasterFrameArtworkColors
{
	RemasterTileContentId tileId;
	std::array<uint16_t, 64> rgb555 = {};
	uint64_t visiblePixels = 0;
};

struct RemasterFrameAssetGroup
{
	std::string name;
	std::vector<RemasterTileContentId> tileIds;
};

struct RemasterFrameAssetMetadata
{
	RemasterTileContentId tileId;
	std::array<std::string, 64> materialSelectors;
	std::array<uint8_t, 64> occlusion = {};
	std::array<uint8_t, 64> height = {};
	std::array<uint8_t, 192> normalXyz = {};
	std::array<uint8_t, 256> emissionRgba = {};
	bool hasMaterialSelectors = false;
	bool hasOcclusion = false;
	bool hasHeight = false;
	bool hasNormals = false;
	bool hasEmission = false;
	bool directLightingOppositeFacing = false;
	RemasterHeightSampling heightSampling = RemasterHeightSampling::Nearest;
};

struct RemasterFrameMaterial
{
	std::string name;
	RemasterSurfaceClass surfaceClass = RemasterSurfaceClass::Unclassified;
	std::array<float, 3> diffuseReflectance = {{ 0.0f, 0.0f, 0.0f }};
	float roughness = 0.8f;
	float metalness = 0.0f;
	float specularLevel = 0.25f;
	float zMin = 0.0f;
	float zMax = 0.0f;
	bool receivesGi = true;
	bool castsShadow = false;
	bool hasDiffuseReflectance = false;
};

struct RemasterFrameTileInstance
{
	RemasterTileContentId tileId;
	RemasterSourceType source = RemasterSourceType::Backdrop;
	uint32_t ruleLine = 0;
	uint16_t tileNumber = 0;
	uint16_t vramAddress = 0;
	uint8_t sourceIndex = 0;
	uint8_t palette = 0;
	uint8_t ppuPriority = 0;
	uint8_t heightOffset = 0;
	int8_t normalYaw = 0;
	bool hFlip = false;
	bool vFlip = false;
	RemasterProfileMatchStatus matchStatus = RemasterProfileMatchStatus::NoMatch;
	std::string assetGroup;
	std::string material;
};

inline void S9xRemasterTransformNormalForTileInstance (const RemasterFrameTileInstance &instance,
	float &x, float &y, float &z)
{
	if (instance.hFlip)
		x = -x;
	if (instance.vFlip)
		y = -y;
	if (instance.normalYaw)
	{
		const float previousX = x;
		const float turn = 0.5f * instance.normalYaw;
		x = 0.8660254f * x + turn * z;
		z = 0.8660254f * z - turn * previousX;
	}
}

struct RemasterFrameLight
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float radius = 0.0f;
	float red = 0.0f;
	float green = 0.0f;
	float blue = 0.0f;
	float intensity = 0.0f;
};

struct RemasterFrame
{
	uint32_t schemaVersion = REMASTER_FRAME_SCHEMA_VERSION;
	uint32_t width = 0;
	uint32_t height = 0;
	std::string profileRomSha256;
	float lightingCoordinateScale = 16.0f;
	std::array<float, 3> cameraDirection = {{ 0.0f, 0.0f, -1.0f }};
	uint8_t indirectBounceCount = 0;
	float indirectRoughness = 1.0f;
	float reflectanceBoost = 0.0f;
	float originalSceneContribution = 0.65f;
	uint8_t heightPreviewMultiplier = 8;
	uint8_t samplesPerFrame = 1;
	bool sampleAccumulation = true;
	std::vector<uint16_t> originalRgb555;
	std::vector<RemasterFramePixel> mainPixels;
	std::vector<RemasterFramePixel> subPixels;
	std::vector<RemasterFrameAsset> assets;
	std::vector<RemasterFrameArtworkColors> artworkColors;
	std::vector<RemasterFrameAssetGroup> assetGroups;
	std::vector<RemasterFrameAssetMetadata> assetMetadata;
	std::vector<RemasterFrameMaterial> materials;
	std::vector<RemasterFrameTileInstance> tileInstances;
	std::vector<RemasterFrameLight> lights;
};

struct RemasterDungeonFloorContext
{
	struct Stair
	{
		uint8_t x = 0;
		uint8_t y = 0;
		bool highIsNorth = false;
		bool changesPlane = false;
	};
	bool verifiedAlttpRom = false;
	bool indoors = false;
	uint8_t collisionMode = 0;
	uint8_t linkFacing = 0xff;
	uint8_t linkPlane = 0;
	uint16_t linkX = 0;
	uint16_t linkY = 0;
	uint16_t roomIndex = 0xffff;
	uint16_t backgroundScrollX = 0;
	uint16_t backgroundScrollY = 0;
	const uint8_t *collisionAttributes = nullptr;
	std::array<Stair, 32> stairs = {};
	uint8_t stairCount = 0;
};

struct RemasterDungeonHeightMap
{
	uint16_t roomIndex = 0xffff;
	std::array<uint8_t, 64 * 64> offsets = {};
};

inline bool S9xRemasterReadDungeonHeightMap (const std::string &path,
	RemasterDungeonHeightMap &result)
{
	std::ifstream input(path, std::ios::binary);
	std::array<uint8_t, 8 + 2 + 64 * 64> bytes = {};
	if (!input.read(reinterpret_cast<char *>(bytes.data()), bytes.size()) || input.get() != EOF ||
		std::memcmp(bytes.data(), "ALTPHM1\0", 8) != 0)
		return false;
	const uint16_t room = static_cast<uint16_t>(bytes[8] | (bytes[9] << 8));
	if (room >= 320)
		return false;
	result.roomIndex = room;
	std::copy(bytes.begin() + 10, bytes.end(), result.offsets.begin());
	return true;
}

inline bool S9xRemasterBuildDungeonHeightMap (const RemasterDungeonFloorContext &context,
	uint8_t floorRise, RemasterDungeonHeightMap &result)
{
	result.roomIndex = context.roomIndex;
	result.offsets.fill(255);
	if (!context.verifiedAlttpRom || !context.indoors || !floorRise ||
		!context.collisionAttributes || context.collisionMode != 0)
		return false;

	std::array<int16_t, 2 * 64 * 64> components;
	components.fill(-1);
	std::vector<std::vector<uint16_t>> cells;
	std::array<uint8_t, 2 * 497 * 488> visited = {};
	auto walkable = [&] (int plane, int x, int y, int direction) {
		static const int sx[4][3] = {{ 0, 8, 15 }, { 8, 0, 15 }, { 0, 0, 0 }, { 15, 15, 15 }};
		static const int sy[4][3] = {{ 8, 8, 8 }, { 24, 24, 24 }, { 8, 16, 23 }, { 8, 16, 23 }};
		for (int i = 0; i < 3; i++)
		{
			const uint8_t value = context.collisionAttributes[plane * 4096 +
				((y + sy[direction][i]) / 8) * 64 + (x + sx[direction][i]) / 8];
			if (value != 0 && !(value >= 0x80 && value <= 0x8f &&
				(value & 1) == (direction >= 2))) return false;
		}
		return true;
	};
	std::vector<uint32_t> seeds;
	seeds.push_back((context.linkPlane * 488 + (context.linkY & 511)) * 497 + (context.linkX & 511));
	for (uint8_t i = 0; i < context.stairCount; i++)
	{
		const auto &stair = context.stairs[i];
		const int x = stair.x * 8 + 8, north = stair.y * 8 - 24, south = stair.y * 8 + 16;
		for (int plane = 0; plane < 2; plane++)
			for (int y : { north, south })
				if (x >= 0 && x <= 496 && y >= 0 && y <= 487)
					seeds.push_back((plane * 488 + y) * 497 + x);
	}
	for (uint32_t seed : seeds)
	{
		if (seed >= visited.size() || visited[seed]) continue;
		const int16_t id = static_cast<int16_t>(cells.size());
		cells.push_back({});
		std::vector<uint32_t> queue(1, seed);
		visited[seed] = 1;
		for (size_t cursor = 0; cursor < queue.size(); cursor++)
		{
			const uint32_t node = queue[cursor];
			const int x = node % 497, row = node / 497, plane = row / 488, y = row % 488;
			for (int px : { x, x + 15 }) for (int py : { y + 8, y + 23 })
			{
				const int index = (py / 8) * 64 + px / 8;
				if (context.collisionAttributes[plane * 4096 + index] == 0 &&
					components[plane * 4096 + index] < 0)
				{
					components[plane * 4096 + index] = id;
					cells[id].push_back(static_cast<uint16_t>(index));
				}
			}
			static const int dx[4] = { 0, 0, -1, 1 }, dy[4] = { -1, 1, 0, 0 };
			for (int direction = 0; direction < 4; direction++)
			{
				const int nx = x + dx[direction], ny = y + dy[direction];
				if (nx < 0 || nx > 496 || ny < 0 || ny > 487 || !walkable(plane, nx, ny, direction)) continue;
				const uint8_t center = context.collisionAttributes[plane * 4096 +
					((y + 16) / 8) * 64 + (x + 8) / 8];
				if (center >= 0x80 && center <= 0x8f && (center & 1) != (direction >= 2)) continue;
				const uint32_t next = (plane * 488 + ny) * 497 + nx;
				if (!visited[next])
				{
					visited[next] = 1;
					queue.push_back(next);
				}
			}
		}
	}

	struct Constraint { int16_t low, high; };
	std::vector<Constraint> constraints;
	for (uint8_t i = 0; i < context.stairCount; i++)
	{
		const RemasterDungeonFloorContext::Stair &stair = context.stairs[i];
		if (stair.x > 60 || stair.y == 0 || stair.y > 59)
			continue;
		const int northCell = (stair.y - 1) * 64 + stair.x + 1;
		const int southCell = (stair.y + 4) * 64 + stair.x + 1;
		if (stair.changesPlane)
		{
			const int highCell = stair.highIsNorth ? northCell : southCell;
			const int lowCell = stair.highIsNorth ? southCell : northCell;
			const int16_t high = components[highCell];
			const int16_t low = components[4096 + lowCell];
			if (high >= 0 && low >= 0 && high != low)
				constraints.push_back({ low, high });
		}
		else
			for (int plane = 0; plane < 2; plane++)
			{
				const int16_t north = components[plane * 4096 + northCell];
				const int16_t south = components[plane * 4096 + southCell];
				if (north >= 0 && south >= 0 && north != south)
					constraints.push_back(stair.highIsNorth ? Constraint{ south, north } :
						Constraint{ north, south });
			}
	}
	if (constraints.empty())
		return false;

	std::vector<int16_t> levels(cells.size(), INT16_MIN);
	std::vector<bool> conflict(cells.size(), false);
	for (const Constraint &start : constraints)
	{
		if (levels[start.low] != INT16_MIN)
			continue;
		levels[start.low] = 0;
		std::vector<int16_t> queue(1, start.low), group(1, start.low);
		for (size_t cursor = 0; cursor < queue.size(); cursor++)
			for (const Constraint &edge : constraints)
			{
				int16_t next = -1, proposed = 0;
				if (edge.low == queue[cursor])
				{
					next = edge.high;
					proposed = levels[queue[cursor]] + 1;
				}
				else if (edge.high == queue[cursor])
				{
					next = edge.low;
					proposed = levels[queue[cursor]] - 1;
				}
				if (next < 0) continue;
				if (levels[next] == INT16_MIN)
				{
					levels[next] = proposed;
					queue.push_back(next);
					group.push_back(next);
				}
				else if (levels[next] != proposed)
					for (int16_t item : group) conflict[item] = true;
			}
		int16_t base = INT16_MAX;
		for (int16_t item : group) base = std::min(base, levels[item]);
		for (int16_t item : group) levels[item] -= base;
	}
	bool solved = false;
	for (size_t component = 0; component < cells.size(); component++)
		if (levels[component] != INT16_MIN && !conflict[component])
			for (uint16_t index : cells[component])
			{
				const int value = std::min(254, levels[component] * floorRise);
				uint8_t &output = result.offsets[index];
				if (output == 255 || output == value)
				{
					output = static_cast<uint8_t>(value);
					solved = true;
				}
				else
					output = 255;
			}
	return solved;
}

inline void S9xRemasterApplyDungeonHeightMap (RemasterFrame &frame,
	const RemasterDungeonFloorContext &context, const RemasterDungeonHeightMap &map)
{
	if (map.roomIndex != context.roomIndex)
		return;
	const size_t originalCount = frame.tileInstances.size();
	std::vector<uint8_t> firstOffset(originalCount, 255);
	std::set<std::string> wallFaceMaterials;
	for (const RemasterFrameMaterial &material : frame.materials)
		if (material.surfaceClass == RemasterSurfaceClass::WallFace)
			wallFaceMaterials.insert(material.name);
	std::array<uint8_t, 64 * 64> placementOffsets = map.offsets;
	struct WallCell
	{
		int8_t dx[2] = {};
		int8_t dy[2] = {};
		uint8_t directions = 0;
		uint8_t localMinimum = 0;
		bool conflict = false;
	};
	std::array<WallCell, 64 * 64> walls = {};
	std::vector<const RemasterFrameAssetMetadata *> instanceMetadata(originalCount, nullptr);
	for (size_t instanceId = 0; instanceId < originalCount; instanceId++)
		for (const RemasterFrameAssetMetadata &metadata : frame.assetMetadata)
			if (metadata.tileId == frame.tileInstances[instanceId].tileId && metadata.hasHeight)
			{
				instanceMetadata[instanceId] = &metadata;
				break;
			}
	auto observeWalls = [&] (const std::vector<RemasterFramePixel> &pixels) {
		if (pixels.size() != static_cast<size_t>(frame.width) * frame.height)
			return;
		for (uint32_t y = 0; y < frame.height; y++)
			for (uint32_t x = 0; x < frame.width; x++)
			{
				const RemasterFramePixel &pixel = pixels[static_cast<size_t>(y) * frame.width + x];
				if (!pixel.instanceId || pixel.instanceId > originalCount) continue;
				const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
				if (instance.source != RemasterSourceType::Background || instance.sourceIndex != 1 ||
					!wallFaceMaterials.count(instance.material)) continue;
				const RemasterFrameAssetMetadata *metadata = instanceMetadata[pixel.instanceId - 1];
				if (!metadata) continue;
				int north = 0, south = 0, west = 0, east = 0;
				uint8_t minimum = 255;
				for (int i = 0; i < 8; i++)
				{
					north += metadata->height[i];
					south += metadata->height[56 + i];
					west += metadata->height[i * 8];
					east += metadata->height[i * 8 + 7];
				}
				for (uint8_t value : metadata->height) minimum = std::min(minimum, value);
				WallCell candidate;
				auto direction = [&] (int dx, int dy) {
					if (instance.hFlip) dx = -dx;
					if (instance.vFlip) dy = -dy;
					candidate.dx[candidate.directions] = static_cast<int8_t>(dx);
					candidate.dy[candidate.directions++] = static_cast<int8_t>(dy);
				};
				if (east < west) direction(1, 0); else if (west < east) direction(-1, 0);
				if (south < north) direction(0, 1); else if (north < south) direction(0, -1);
				if (candidate.directions == 2)
				{
					bool planar = true;
					for (int yy = 0; yy < 8; yy++)
						for (int xx = 0; xx < 8; xx++)
						{
							const int expected = metadata->height[yy * 8] + metadata->height[xx] - metadata->height[0];
							if (std::abs(static_cast<int>(metadata->height[yy * 8 + xx]) - expected) > 1)
								planar = false;
						}
					if (planar)
					{
						candidate.dx[0] += candidate.dx[1];
						candidate.dy[0] += candidate.dy[1];
						candidate.directions = 1;
					}
				}
				if (!candidate.directions) continue;
				candidate.localMinimum = minimum;
				const uint32_t roomX = (context.backgroundScrollX + x) & 511;
				const uint32_t roomY = (context.backgroundScrollY + y) & 511;
				WallCell &cell = walls[(roomY / 8) * 64 + roomX / 8];
				if (!cell.directions)
					cell = candidate;
				else if (cell.directions != candidate.directions || cell.localMinimum != candidate.localMinimum ||
					cell.dx[0] != candidate.dx[0] || cell.dy[0] != candidate.dy[0] ||
					(cell.directions == 2 && (cell.dx[1] != candidate.dx[1] || cell.dy[1] != candidate.dy[1])))
					cell.conflict = true;
			}
	};
	observeWalls(frame.mainPixels);
	observeWalls(frame.subPixels);
	for (int index = 0; index < 64 * 64; index++)
	{
		const WallCell &wall = walls[index];
		if (!wall.directions || wall.conflict) continue;
		placementOffsets[index] = 255;
		const int x = index % 64, y = index / 64;
		int proposed = -1;
		bool conflict = false;
		for (uint8_t direction = 0; direction < wall.directions; direction++)
		{
			int xx = x, yy = y;
			for (int steps = 1; steps < 64; steps++)
			{
				xx += wall.dx[direction]; yy += wall.dy[direction];
				if (xx < 0 || xx >= 64 || yy < 0 || yy >= 64) break;
				const int target = yy * 64 + xx;
				if (map.offsets[target] != 255)
				{
					const int value = static_cast<int>(map.offsets[target]) + (steps - 1) * 8 - wall.localMinimum;
					if (value < 0 || value > 254 || (proposed >= 0 && proposed != value)) conflict = true;
					else proposed = value;
					break;
				}
				if (!walls[target].directions || walls[target].conflict) break;
				bool continuation = false;
				for (uint8_t other = 0; other < walls[target].directions; other++)
					if (walls[target].dx[other] == wall.dx[direction] &&
						walls[target].dy[other] == wall.dy[direction]) continuation = true;
				if (!continuation) break;
			}
		}
		if (!conflict && proposed >= 0) placementOffsets[index] = static_cast<uint8_t>(proposed);
	}
	// Completed collision tables mark axis-aligned door thresholds as 80..8f.
	// Carry a floor base through the doorway only when both gameplay planes and
	// every solved sample on its movement axis agree.
	for (int index = 0; index < 64 * 64; index++)
	{
		int axis = -1;
		for (int plane = 0; plane < 2; plane++)
		{
			const uint8_t attribute = context.collisionAttributes ?
				context.collisionAttributes[plane * 4096 + index] : 0;
			if (attribute < 0x80 || attribute > 0x8f) continue;
			const int candidate = (attribute & 1) ? 0 : 1; // W/E doors use X; N/S use Y.
			if (axis >= 0 && axis != candidate) axis = -2;
			else if (axis != -2) axis = candidate;
		}
		if (axis < 0) continue;
		const int x = index % 64, y = index / 64;
		int proposed = -1;
		bool conflict = false;
		for (int sign : {-1, 1})
			for (int distance = 1; distance <= 3; distance++)
			{
				const int xx = x + (axis == 0 ? sign * distance : 0);
				const int yy = y + (axis == 1 ? sign * distance : 0);
				if (xx < 0 || xx >= 64 || yy < 0 || yy >= 64) break;
				const uint8_t value = map.offsets[yy * 64 + xx];
				if (value == 255) continue;
				if (proposed >= 0 && proposed != value) conflict = true;
				else proposed = value;
				break;
			}
		if (!conflict && proposed >= 0) placementOffsets[index] = static_cast<uint8_t>(proposed);
	}
	auto offsetAt = [&] (const RemasterFramePixel &pixel, uint32_t x, uint32_t y) -> uint8_t {
		if (!pixel.instanceId || pixel.instanceId > originalCount)
			return 0;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		if (instance.source != RemasterSourceType::Background || instance.sourceIndex != 1 ||
			instance.assetGroup == "stair_treads" || instance.assetGroup == "stair_rails")
			return 0;
		const uint32_t roomX = (context.backgroundScrollX + x) & 511;
		const uint32_t roomY = (context.backgroundScrollY + y) & 511;
		const size_t cell = (roomY / 8) * 64 + roomX / 8;
		if (wallFaceMaterials.count(instance.material) && !walls[cell].directions)
			return 0;
		const uint8_t value = placementOffsets[cell];
		return value == 255 ? 0 : value;
	};
	auto visit = [&] (std::vector<RemasterFramePixel> &pixels, bool assign,
		std::map<std::pair<uint32_t, uint8_t>, uint32_t> &copies) {
		if (pixels.size() != static_cast<size_t>(frame.width) * frame.height)
			return;
		for (uint32_t y = 0; y < frame.height; y++)
			for (uint32_t x = 0; x < frame.width; x++)
			{
			RemasterFramePixel &pixel = pixels[static_cast<size_t>(y) * frame.width + x];
			if (!pixel.instanceId || pixel.instanceId > originalCount)
				continue;
			const uint32_t originalId = pixel.instanceId;
			const uint8_t offset = offsetAt(pixel, x, y);
			uint8_t &first = firstOffset[originalId - 1];
			if (!assign)
			{
				if (first == 255)
					first = offset;
				continue;
			}
			if (offset == first)
				continue;
			const auto key = std::make_pair(originalId, offset);
			auto found = copies.find(key);
			if (found == copies.end())
			{
				RemasterFrameTileInstance copy = frame.tileInstances[originalId - 1];
				copy.heightOffset = static_cast<uint8_t>(std::min(255,
					static_cast<int>(copy.heightOffset) + offset));
				frame.tileInstances.push_back(copy);
				found = copies.emplace(key, static_cast<uint32_t>(frame.tileInstances.size())).first;
			}
			pixel.instanceId = found->second;
			}
	};
	std::map<std::pair<uint32_t, uint8_t>, uint32_t> copies;
	visit(frame.mainPixels, false, copies);
	visit(frame.subPixels, false, copies);
	visit(frame.mainPixels, true, copies);
	visit(frame.subPixels, true, copies);
	for (size_t i = 0; i < originalCount; i++)
		if (firstOffset[i] != 255)
			frame.tileInstances[i].heightOffset = static_cast<uint8_t>(std::min(255,
					static_cast<int>(frame.tileInstances[i].heightOffset) + firstOffset[i]));

	// Ground-supported OAM art follows the solved cell beneath the assembled
	// visible object. This is spatial rather than priority-based: ALTTP reuses
	// both artwork and OAM priorities on different visual floors.
	struct Bounds { int x0 = 10000, y0 = 10000, x1 = -1, y1 = -1; };
	std::vector<Bounds> objectBounds(frame.tileInstances.size());
	for (uint32_t y = 0; y < frame.height; y++)
		for (uint32_t x = 0; x < frame.width; x++)
		{
			const RemasterFramePixel &pixel = frame.mainPixels[static_cast<size_t>(y) * frame.width + x];
			if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size()) continue;
			const size_t id = pixel.instanceId - 1;
			if (frame.tileInstances[id].source != RemasterSourceType::Object) continue;
			Bounds &bounds = objectBounds[id];
			bounds.x0 = std::min(bounds.x0, static_cast<int>(x));
			bounds.y0 = std::min(bounds.y0, static_cast<int>(y));
			bounds.x1 = std::max(bounds.x1, static_cast<int>(x));
			bounds.y1 = std::max(bounds.y1, static_cast<int>(y));
		}
	Bounds slots[128];
	for (size_t i = 0; i < objectBounds.size(); i++)
		if (objectBounds[i].x1 >= 0 && frame.tileInstances[i].sourceIndex < 128)
		{
			Bounds &slot = slots[frame.tileInstances[i].sourceIndex];
			slot.x0 = std::min(slot.x0, objectBounds[i].x0);
			slot.y0 = std::min(slot.y0, objectBounds[i].y0);
			slot.x1 = std::max(slot.x1, objectBounds[i].x1);
			slot.y1 = std::max(slot.y1, objectBounds[i].y1);
		}
	for (size_t i = 0; i < objectBounds.size(); i++)
		if (objectBounds[i].x1 >= 0 && frame.tileInstances[i].sourceIndex < 128)
		{
			const Bounds &bounds = slots[frame.tileInstances[i].sourceIndex];
			const uint32_t roomX = (context.backgroundScrollX + (bounds.x0 + bounds.x1) / 2) & 511;
			const uint32_t roomY = (context.backgroundScrollY + bounds.y1) & 511;
			const uint8_t offset = map.offsets[(roomY / 8) * 64 + roomX / 8];
			if (offset != 255)
				frame.tileInstances[i].heightOffset = static_cast<uint8_t>(std::min(255,
					static_cast<int>(frame.tileInstances[i].heightOffset) + offset));
		}
}

inline bool S9xRemasterUpperFloorAt (uint16_t room, uint16_t x, uint16_t y)
{
	// Room-local BG2 coordinates, anchored by the game's room ID and PPU
	// scroll. The room masks follow the upper floor through its surrounding wall
	// faces; the lower room and the stair tiles keep their base heights.
	if (room == 0x55)
	{
		// The southwest platform is L-shaped: a northern walkway reaches the
		// right edge, then a narrower western landing descends to the stairs.
		// The lower floor east of that landing and below the stairs stays at 0.
		return (y >= 320 && y < 384 && x >= 80 && x < 256) ||
			(y >= 384 && y < 464 && x >= 72 && x < 176);
	}
	if (room == 0x60)
	{
		// The raised entry and eastward rocky walkway occupy the room's
		// southeast quadrant. The stairwell ends before the lower south floor.
		return x >= 336 && y >= 48 && y < 192 &&
			(y < 112 || x < 440);
	}
	if (room == 0x61)
	{
		if (x >= 256 || y < 48 || y >= 168)
			return false;
		if (y < 80)
			return x >= 80 - static_cast<int>(y - 48); // North walkway behind the inner wall.
		const int left = std::max(16, 48 - static_cast<int>(y - 80));
		if (y >= 160 && x >= 72 && x < 112)
			return false; // The wall beside the stair opening descends to the lower floor.
		return x >= left && x < 128; // Landing and wall face; east floor stays lower.
	}
	return false;
}

inline void S9xRemasterApplyDungeonFloorHeight (RemasterFrame &frame,
	const RemasterDungeonFloorContext &context, uint8_t upperFloorHeight,
	const RemasterDungeonHeightMap *reviewMap = nullptr)
{
	if (!context.verifiedAlttpRom || !context.indoors || !upperFloorHeight)
		return;
	if (reviewMap && reviewMap->roomIndex == context.roomIndex)
	{
		S9xRemasterApplyDungeonHeightMap(frame, context, *reviewMap);
		return;
	}
	if (context.roomIndex != 0x55 && context.roomIndex != 0x60 && context.roomIndex != 0x61)
		return;
	const size_t originalCount = frame.tileInstances.size();
	std::vector<uint8_t> region(originalCount, 0);
	auto upper = [&] (const RemasterFramePixel &pixel, uint32_t x, uint32_t y) {
		if (!pixel.instanceId || pixel.instanceId > originalCount)
			return false;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		if (instance.source != RemasterSourceType::Background || instance.sourceIndex != 1 ||
			instance.assetGroup == "stair_treads" || instance.assetGroup == "stair_rails" ||
			instance.tileId.hash == UINT64_C(0xc40518b112d0814e))
			return false;
		return S9xRemasterUpperFloorAt(context.roomIndex,
			static_cast<uint16_t>((context.backgroundScrollX + x) & 511),
			static_cast<uint16_t>((context.backgroundScrollY + y) & 511));
	};
	auto classify = [&] (const std::vector<RemasterFramePixel> &pixels) {
		if (pixels.size() != static_cast<size_t>(frame.width) * frame.height)
			return;
		for (uint32_t y = 0; y < frame.height; y++)
			for (uint32_t x = 0; x < frame.width; x++)
			{
				const RemasterFramePixel &pixel = pixels[static_cast<size_t>(y) * frame.width + x];
				if (pixel.instanceId && pixel.instanceId <= originalCount)
					region[pixel.instanceId - 1] |= upper(pixel, x, y) ? 2 : 1;
			}
	};
	classify(frame.mainPixels);
	classify(frame.subPixels);
	std::vector<uint32_t> raisedCopy(originalCount, 0);
	for (size_t i = 0; i < originalCount; i++)
	{
		if (region[i] == 2)
			frame.tileInstances[i].heightOffset = static_cast<uint8_t>(std::min(255,
				static_cast<int>(frame.tileInstances[i].heightOffset) + upperFloorHeight));
		else if (region[i] == 3)
		{
			RemasterFrameTileInstance raised = frame.tileInstances[i];
			raised.heightOffset = static_cast<uint8_t>(std::min(255,
				static_cast<int>(raised.heightOffset) + upperFloorHeight));
			frame.tileInstances.push_back(raised);
			raisedCopy[i] = static_cast<uint32_t>(frame.tileInstances.size());
		}
	}
	auto split = [&] (std::vector<RemasterFramePixel> &pixels) {
		if (pixels.size() != static_cast<size_t>(frame.width) * frame.height)
			return;
		for (uint32_t y = 0; y < frame.height; y++)
			for (uint32_t x = 0; x < frame.width; x++)
			{
				RemasterFramePixel &pixel = pixels[static_cast<size_t>(y) * frame.width + x];
				if (pixel.instanceId && pixel.instanceId <= originalCount &&
					raisedCopy[pixel.instanceId - 1] && upper(pixel, x, y))
					pixel.instanceId = raisedCopy[pixel.instanceId - 1];
			}
	};
	split(frame.mainPixels);
	split(frame.subPixels);
}

inline void S9xRemasterAlignGeneratedSpriteParts (RemasterFrame &frame,
	const RemasterDungeonFloorContext *context = nullptr)
{
	// The ROM atlas gives each 8x8 silhouette a local height ramp. Align only
	// those exact generated ramps to the bottom of their assembled OAM figure.
	std::map<RemasterTileContentId, const RemasterFrameAsset *> art;
	std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> metadata;
	for (const RemasterFrameAsset &asset : frame.assets)
		art.emplace(asset.tileId, &asset);
	for (const RemasterFrameAssetMetadata &asset : frame.assetMetadata)
		metadata.emplace(asset.tileId, &asset);
	std::vector<bool> generated(frame.tileInstances.size(), false);
	for (size_t i = 0; i < frame.tileInstances.size(); i++)
	{
		const RemasterFrameTileInstance &instance = frame.tileInstances[i];
		if (instance.source != RemasterSourceType::Object)
			continue;
		const auto a = art.find(instance.tileId);
		const auto m = metadata.find(instance.tileId);
		if (a == art.end() || m == metadata.end() || !m->second->hasHeight || !m->second->hasOcclusion)
			continue;
		unsigned visible = 0;
		bool exactRamp = true;
		for (size_t p = 0; p < 64; p++)
		{
			const bool ink = a->second->indices[p] != 0;
			visible += ink;
			if (m->second->height[p] != (ink ? 13 - p / 8 : 0) ||
				m->second->occlusion[p] != (ink ? 255 : 0))
			{
				exactRamp = false;
				break;
			}
		}
		generated[i] = exactRamp && visible >= 4;
		if (generated[i] && context && context->verifiedAlttpRom &&
			instance.vramAddress >= 0x8000 && instance.vramAddress < 0x8400)
		{
			if (context->linkFacing == 4)
				frame.tileInstances[i].normalYaw = -1;
			else if (context->linkFacing == 6)
				frame.tileInstances[i].normalYaw = 1;
			else
				frame.tileInstances[i].normalYaw = 0;
		}
	}
	if (std::find(generated.begin(), generated.end(), true) == generated.end())
		return;
	struct Bounds { int x0 = 10000, y0 = 10000, x1 = -1, y1 = -1; };
	std::vector<Bounds> parts(frame.tileInstances.size());
	std::vector<int> partTileBottom(frame.tileInstances.size(), -10000);
	Bounds slots[128];
	int slotTileBottom[128];
	for (int i = 0; i < 128; i++) slotTileBottom[i] = -10000;
	uint8_t slotPriority[128] = {};
	for (uint32_t y = 0; y < frame.height; y++)
		for (uint32_t x = 0; x < frame.width; x++)
		{
			const RemasterFramePixel &pixel = frame.mainPixels[static_cast<size_t>(y) * frame.width + x];
			if (!pixel.instanceId || pixel.instanceId > parts.size() || pixel.tilePixel >= 64 ||
				!generated[pixel.instanceId - 1])
				continue;
			const size_t id = pixel.instanceId - 1;
			const uint8_t slot = frame.tileInstances[id].sourceIndex;
			if (slot >= 128)
				continue;
			Bounds &part = parts[id], &whole = slots[slot];
			part.x0 = std::min(part.x0, static_cast<int>(x));
			part.y0 = std::min(part.y0, static_cast<int>(y));
			part.x1 = std::max(part.x1, static_cast<int>(x));
			part.y1 = std::max(part.y1, static_cast<int>(y));
			whole.x0 = std::min(whole.x0, static_cast<int>(x));
			whole.y0 = std::min(whole.y0, static_cast<int>(y));
			whole.x1 = std::max(whole.x1, static_cast<int>(x));
			whole.y1 = std::max(whole.y1, static_cast<int>(y));
			const int localY = frame.tileInstances[id].vFlip ?
				7 - pixel.tilePixel / 8 : pixel.tilePixel / 8;
			const int tileBottom = static_cast<int>(y) - localY + 7;
			partTileBottom[id] = std::max(partTileBottom[id], tileBottom);
			slotTileBottom[slot] = std::max(slotTileBottom[slot], tileBottom);
			slotPriority[slot] = frame.tileInstances[id].ppuPriority;
		}
	uint8_t parent[128];
	for (int i = 0; i < 128; i++) parent[i] = static_cast<uint8_t>(i);
	auto root = [&parent] (int id) {
		while (parent[id] != id) id = parent[id];
		return id;
	};
	for (int i = 0; i < 128; i++)
		for (int j = i + 1; j < std::min(i + 3, 128); j++)
		{
			if (slots[i].x1 < 0 || slots[j].x1 < 0 || slotPriority[i] != slotPriority[j])
				continue;
			const int overlap = std::min(slots[i].x1, slots[j].x1) - std::max(slots[i].x0, slots[j].x0) + 1;
			const int gap = std::max(slots[i].y0, slots[j].y0) - std::min(slots[i].y1, slots[j].y1) - 1;
			if (overlap >= 4 && gap <= 8)
				parent[root(j)] = static_cast<uint8_t>(root(i));
		}
	int bottom[128];
	for (int i = 0; i < 128; i++) bottom[i] = -1;
	for (int i = 0; i < 128; i++)
		if (slots[i].y1 >= 0)
			bottom[root(i)] = std::max(bottom[root(i)], slotTileBottom[i]);
	for (size_t i = 0; i < parts.size(); i++)
	{
		if (parts[i].y1 < 0 || !generated[i]) continue;
		RemasterFrameTileInstance &instance = frame.tileInstances[i];
		const int rise = bottom[root(instance.sourceIndex)] - partTileBottom[i];
		instance.heightOffset = static_cast<uint8_t>(std::min(255, static_cast<int>(instance.heightOffset) + rise));
	}
}

inline void S9xRemasterApplyProfileToFrame (const RemasterProfile &profile, RemasterFrame &frame)
{
	frame.profileRomSha256 = profile.romSha256;
	frame.lightingCoordinateScale = profile.lightingCoordinateScale;
	frame.cameraDirection = profile.cameraDirection;
	frame.indirectBounceCount = profile.indirectBounceCount;
	frame.indirectRoughness = profile.indirectRoughness;
	frame.reflectanceBoost = profile.reflectanceBoost;
	frame.originalSceneContribution = profile.originalSceneContribution;
	frame.heightPreviewMultiplier = profile.heightPreviewMultiplier;
	frame.samplesPerFrame = profile.samplesPerFrame;
	frame.sampleAccumulation = profile.sampleAccumulation;

	frame.assetGroups.clear();
	for (const auto &entry : profile.assetGroups)
	{
		RemasterFrameAssetGroup group;
		group.name = entry.second.name;
		group.tileIds = entry.second.tileIds;
		frame.assetGroups.push_back(group);
	}

	// A frame only needs metadata for artwork it contains. Keeping the entire
	// profile here makes every live frame proportional to the game's full atlas.
	std::set<RemasterTileContentId> frameTileIds;
	for (const RemasterFrameAsset &asset : frame.assets)
		frameTileIds.insert(asset.tileId);
	for (const RemasterFrameTileInstance &instance : frame.tileInstances)
		frameTileIds.insert(instance.tileId);
	frame.assetMetadata.clear();
	for (const RemasterTileContentId &tileId : frameTileIds)
	{
		const auto entry = profile.assets.find(tileId);
		if (entry == profile.assets.end())
			continue;
		const RemasterAssetMetadata &source = entry->second;
		RemasterFrameAssetMetadata metadata;
		metadata.tileId = source.tileId;
		metadata.materialSelectors = source.materialSelectors;
		metadata.occlusion = source.occlusion;
		metadata.height = source.height;
		metadata.normalXyz = source.normalXyz;
		metadata.emissionRgba = source.emissionRgba;
		metadata.hasMaterialSelectors = source.hasMaterialSelectors;
		metadata.hasOcclusion = source.hasOcclusion;
		metadata.hasHeight = source.hasHeight;
		metadata.hasNormals = source.hasNormals;
		metadata.hasEmission = source.hasEmission;
		metadata.directLightingOppositeFacing = source.directLightingOppositeFacing;
		metadata.heightSampling = source.heightSampling;
		frame.assetMetadata.push_back(metadata);
	}

	frame.materials.clear();
	for (const auto &entry : profile.materials)
	{
		const RemasterMaterial &source = entry.second;
		RemasterFrameMaterial material;
		material.name = source.name;
		material.surfaceClass = source.surfaceClass;
		material.diffuseReflectance = source.diffuseReflectance;
		material.roughness = source.roughness;
		material.metalness = source.metalness;
		material.specularLevel = source.specularLevel;
		material.zMin = source.zMin;
		material.zMax = source.zMax;
		material.receivesGi = source.receivesGi;
		material.castsShadow = source.castsShadow;
		material.hasDiffuseReflectance = source.hasDiffuseReflectance;
		frame.materials.push_back(material);
	}

	for (RemasterFrameTileInstance &instance : frame.tileInstances)
	{
		RemasterProfileMatchContext context;
		context.tileId = instance.tileId;
		context.source = instance.source;
		context.sourceIndex = instance.sourceIndex;
		context.palette = instance.palette;
		const RemasterProfileMatch match = S9xRemasterMatchProfile(profile, context);
		instance.matchStatus = match.status;
		instance.ruleLine = match.rule ? static_cast<uint32_t>(match.rule->line) : 0;
		instance.assetGroup = match.assetGroup ? match.assetGroup->name : std::string();
		instance.material = match.material ? match.material->name : std::string();
	}
}

struct RemasterAnimationTrackKey
{
	RemasterSourceType source = RemasterSourceType::Backdrop;
	uint8_t sourceIndex = 0;
	uint16_t tileNumber = 0;
	uint16_t cellX = 0;
	uint16_t cellY = 0;

	bool operator< (const RemasterAnimationTrackKey &other) const
	{
		return std::tie(source, sourceIndex, tileNumber, cellX, cellY) <
			std::tie(other.source, other.sourceIndex, other.tileNumber, other.cellX, other.cellY);
	}
};

struct RemasterAnimationCapture
{
	RemasterFrame representativeFrame;
	std::map<RemasterAnimationTrackKey, std::vector<RemasterTileContentId>> tracks;
	uint32_t frameCount = 0;
};

inline bool S9xRemasterFrameHasPixelData (const RemasterFrame &, uint32_t, uint32_t);

inline void S9xRemasterAddAnimationCaptureFrame (RemasterAnimationCapture &capture, const RemasterFrame &frame)
{
	if (!S9xRemasterFrameHasPixelData(frame, frame.width, frame.height))
		return;
	if (!capture.frameCount)
		capture.representativeFrame = frame;
	else if (frame.width != capture.representativeFrame.width || frame.height != capture.representativeFrame.height)
		return;

	std::set<RemasterTileContentId> capturedAssets;
	for (const RemasterFrameAsset &asset : capture.representativeFrame.assets)
		capturedAssets.insert(asset.tileId);
	for (const RemasterFrameAsset &asset : frame.assets)
		if (capturedAssets.insert(asset.tileId).second)
			capture.representativeFrame.assets.push_back(asset);
	std::set<RemasterTileContentId> capturedArtworkColors;
	for (const RemasterFrameArtworkColors &artwork : capture.representativeFrame.artworkColors)
		capturedArtworkColors.insert(artwork.tileId);
	for (const RemasterFrameArtworkColors &artwork : frame.artworkColors)
		if (capturedArtworkColors.insert(artwork.tileId).second)
			capture.representativeFrame.artworkColors.push_back(artwork);
	std::set<RemasterTileContentId> capturedMetadata;
	for (const RemasterFrameAssetMetadata &metadata : capture.representativeFrame.assetMetadata)
		capturedMetadata.insert(metadata.tileId);
	for (const RemasterFrameAssetMetadata &metadata : frame.assetMetadata)
		if (capturedMetadata.insert(metadata.tileId).second)
			capture.representativeFrame.assetMetadata.push_back(metadata);

	std::vector<uint32_t> minimumX(frame.tileInstances.size(), UINT32_MAX);
	std::vector<uint32_t> minimumY(frame.tileInstances.size(), UINT32_MAX);
	for (uint32_t y = 0; y < frame.height; y++)
		for (uint32_t x = 0; x < frame.width; x++)
		{
			const RemasterFramePixel &pixel = frame.mainPixels[static_cast<size_t>(y) * frame.width + x];
			if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size())
				continue;
			const size_t instance = pixel.instanceId - 1;
			minimumX[instance] = std::min(minimumX[instance], x);
			minimumY[instance] = std::min(minimumY[instance], y);
		}
	for (size_t i = 0; i < frame.tileInstances.size(); i++)
	{
		if (minimumX[i] == UINT32_MAX || minimumY[i] == UINT32_MAX)
			continue;
		const RemasterFrameTileInstance &instance = frame.tileInstances[i];
		const RemasterAnimationTrackKey key = { instance.source, instance.sourceIndex, instance.tileNumber,
			static_cast<uint16_t>(minimumX[i] / 8), static_cast<uint16_t>(minimumY[i] / 8) };
		std::vector<RemasterTileContentId> &variants = capture.tracks[key];
		if (std::find(variants.begin(), variants.end(), instance.tileId) == variants.end())
			variants.push_back(instance.tileId);
	}
	capture.frameCount++;
}

inline RemasterFrame S9xRemasterFinishAnimationCapture (RemasterAnimationCapture capture)
{
	RemasterFrame &frame = capture.representativeFrame;
	std::set<std::vector<RemasterTileContentId>> emittedGroups;
	uint32_t groupNumber = 1;
	for (const auto &entry : capture.tracks)
	{
		const std::vector<RemasterTileContentId> &variants = entry.second;
		if (variants.size() < 2 || !emittedGroups.insert(variants).second)
			continue;
		for (RemasterFrameAssetGroup &authoredGroup : frame.assetGroups)
			for (const RemasterTileContentId &variant : variants)
				authoredGroup.tileIds.erase(std::remove(authoredGroup.tileIds.begin(),
					authoredGroup.tileIds.end(), variant), authoredGroup.tileIds.end());
		RemasterFrameAssetGroup group;
		group.name = "capture_animation_" + std::to_string(groupNumber++);
		group.tileIds = variants;
		frame.assetGroups.push_back(group);
		for (RemasterFrameTileInstance &instance : frame.tileInstances)
			if (std::find(variants.begin(), variants.end(), instance.tileId) != variants.end())
				instance.assetGroup = group.name;
	}
	return std::move(frame);
}

inline bool S9xRemasterFrameHasPixelData (const RemasterFrame &frame, uint32_t width, uint32_t height)
{
	if (!width || !height || frame.width != width || frame.height != height ||
		width > SIZE_MAX / height)
		return false;
	const size_t pixelCount = static_cast<size_t>(width) * height;
	return frame.originalRgb555.size() == pixelCount && frame.mainPixels.size() == pixelCount &&
		frame.subPixels.size() == pixelCount;
}

inline void S9xRemasterInferLegacyTileInstanceFlips (RemasterFrame &frame)
{
	if (frame.schemaVersion < 5 || frame.schemaVersion >= 8 || frame.tileInstances.empty())
		return;
	struct Evidence
	{
		std::array<int64_t, 8> rowScreenX;
		std::array<int8_t, 8> rowTileX;
		std::array<int64_t, 8> columnScreenY;
		std::array<int8_t, 8> columnTileY;
		bool hKnown = false;
		bool vKnown = false;
		Evidence ()
		{
			rowScreenX.fill(-1);
			rowTileX.fill(-1);
			columnScreenY.fill(-1);
			columnTileY.fill(-1);
		}
	};
	std::vector<Evidence> evidence(frame.tileInstances.size());
	auto collect = [&] (const std::vector<RemasterFramePixel> &pixels) {
		for (uint32_t y = 0; y < frame.height; y++)
			for (uint32_t x = 0; x < frame.width; x++)
			{
				const RemasterFramePixel &pixel = pixels[static_cast<size_t>(y) * frame.width + x];
				if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size() || pixel.tilePixel >= 64)
					continue;
				const size_t instanceIndex = pixel.instanceId - 1;
				Evidence &item = evidence[instanceIndex];
				const int8_t tileX = pixel.tilePixel % 8;
				const int8_t tileY = pixel.tilePixel / 8;
				if (!item.hKnown && item.rowTileX[tileY] >= 0 && item.rowTileX[tileY] != tileX)
				{
					frame.tileInstances[instanceIndex].hFlip =
						(static_cast<int64_t>(x) - item.rowScreenX[tileY]) * (tileX - item.rowTileX[tileY]) < 0;
					item.hKnown = true;
				}
				else if (item.rowTileX[tileY] < 0)
				{
					item.rowScreenX[tileY] = x;
					item.rowTileX[tileY] = tileX;
				}
				if (!item.vKnown && item.columnTileY[tileX] >= 0 && item.columnTileY[tileX] != tileY)
				{
					frame.tileInstances[instanceIndex].vFlip =
						(static_cast<int64_t>(y) - item.columnScreenY[tileX]) * (tileY - item.columnTileY[tileX]) < 0;
					item.vKnown = true;
				}
				else if (item.columnTileY[tileX] < 0)
				{
					item.columnScreenY[tileX] = y;
					item.columnTileY[tileX] = tileY;
				}
			}
	};
	collect(frame.mainPixels);
	collect(frame.subPixels);
}

namespace RemasterFrameSerialization
{
	inline bool ValidTileId (const RemasterTileContentId &tileId)
	{
		return tileId.hashVersion == 1 &&
			(tileId.bitDepth == 2 || tileId.bitDepth == 4 || tileId.bitDepth == 8);
	}

	struct Reader
	{
		const std::vector<uint8_t> &bytes;
		size_t offset = 0;

		explicit Reader (const std::vector<uint8_t> &source) : bytes(source) {}

		bool ReadU8 (uint8_t &value)
		{
			if (offset >= bytes.size())
				return false;
			value = bytes[offset++];
			return true;
		}

		bool ReadU16 (uint16_t &value)
		{
			uint8_t low = 0;
			uint8_t high = 0;
			if (!ReadU8(low) || !ReadU8(high))
				return false;
			value = static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
			return true;
		}

		bool ReadU32 (uint32_t &value)
		{
			value = 0;
			for (unsigned shift = 0; shift < 32; shift += 8)
			{
				uint8_t byte = 0;
				if (!ReadU8(byte))
					return false;
				value |= static_cast<uint32_t>(byte) << shift;
			}
			return true;
		}

		bool ReadU64 (uint64_t &value)
		{
			value = 0;
			for (unsigned shift = 0; shift < 64; shift += 8)
			{
				uint8_t byte = 0;
				if (!ReadU8(byte))
					return false;
				value |= static_cast<uint64_t>(byte) << shift;
			}
			return true;
		}

		bool ReadFloat (float &value)
		{
			uint32_t bits = 0;
			if (!ReadU32(bits))
				return false;
			std::memcpy(&value, &bits, sizeof(value));
			return true;
		}

		bool ReadString (std::string &value)
		{
			uint32_t size = 0;
			if (!ReadU32(size) || size > bytes.size() - offset)
				return false;
			value.assign(reinterpret_cast<const char *>(bytes.data() + offset), size);
			offset += size;
			return true;
		}

		bool ReadTileId (RemasterTileContentId &tileId)
		{
			return ReadU8(tileId.hashVersion) && ReadU8(tileId.bitDepth) && ReadU64(tileId.hash);
		}
	};

	inline void U8 (std::vector<uint8_t> &bytes, uint8_t value)
	{
		bytes.push_back(value);
	}

	inline void U16 (std::vector<uint8_t> &bytes, uint16_t value)
	{
		U8(bytes, static_cast<uint8_t>(value));
		U8(bytes, static_cast<uint8_t>(value >> 8));
	}

	inline void U32 (std::vector<uint8_t> &bytes, uint32_t value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8)
			U8(bytes, static_cast<uint8_t>(value >> shift));
	}

	inline void U64 (std::vector<uint8_t> &bytes, uint64_t value)
	{
		for (unsigned shift = 0; shift < 64; shift += 8)
			U8(bytes, static_cast<uint8_t>(value >> shift));
	}

	inline void Float (std::vector<uint8_t> &bytes, float value)
	{
		uint32_t bits = 0;
		static_assert(sizeof(bits) == sizeof(value), "32-bit float required");
		std::memcpy(&bits, &value, sizeof(bits));
		U32(bytes, bits);
	}

	inline bool Size (std::vector<uint8_t> &bytes, size_t value)
	{
		if (value > UINT32_MAX)
			return false;
		U32(bytes, static_cast<uint32_t>(value));
		return true;
	}

	inline bool String (std::vector<uint8_t> &bytes, const std::string &value)
	{
		if (!Size(bytes, value.size()))
			return false;
		bytes.insert(bytes.end(), value.begin(), value.end());
		return true;
	}

	inline void TileId (std::vector<uint8_t> &bytes, const RemasterTileContentId &tileId)
	{
		U8(bytes, tileId.hashVersion);
		U8(bytes, tileId.bitDepth);
		U64(bytes, tileId.hash);
	}
}

inline bool S9xDeserializeRemasterFrame (const std::vector<uint8_t> &bytes, RemasterFrame &frame)
{
	static const uint8_t magic[] = { 'S', '9', 'X', 'R', 'M', 'F', 0, 1 };
	if (bytes.size() < sizeof(magic) || !std::equal(magic, magic + sizeof(magic), bytes.begin()))
		return false;

	RemasterFrameSerialization::Reader input(bytes);
	input.offset = sizeof(magic);
	RemasterFrame result;
	uint32_t assetCount = 0;
	uint32_t artworkColorCount = 0;
	uint32_t groupCount = 0;
	uint32_t metadataCount = 0;
	uint32_t materialCount = 0;
	uint32_t instanceCount = 0;
	uint32_t lightCount = 0;
	uint8_t sampleAccumulation = 1;
	if (!input.ReadU32(result.schemaVersion) || result.schemaVersion < 1 ||
		result.schemaVersion > REMASTER_FRAME_SCHEMA_VERSION ||
		!input.ReadU32(result.width) || !input.ReadU32(result.height) || !result.width || !result.height ||
		result.width > UINT32_MAX / result.height || !input.ReadString(result.profileRomSha256) ||
		(result.schemaVersion >= 7 && !input.ReadFloat(result.lightingCoordinateScale)) ||
		(result.schemaVersion >= 9 && !input.ReadU8(result.indirectBounceCount)) ||
		(result.schemaVersion >= 11 && !input.ReadFloat(result.indirectRoughness)) ||
		(result.schemaVersion >= 12 && (!input.ReadFloat(result.originalSceneContribution) ||
			!input.ReadU8(result.samplesPerFrame) || !input.ReadU8(sampleAccumulation))) ||
		(result.schemaVersion >= 14 && (!input.ReadFloat(result.cameraDirection[0]) ||
			!input.ReadFloat(result.cameraDirection[1]) || !input.ReadFloat(result.cameraDirection[2]))) ||
		(result.schemaVersion >= 15 && !input.ReadU8(result.heightPreviewMultiplier)) ||
		(result.schemaVersion >= 17 && !input.ReadFloat(result.reflectanceBoost)) ||
		!input.ReadU32(assetCount) ||
		(result.schemaVersion >= 13 && !input.ReadU32(artworkColorCount)) ||
		(result.schemaVersion >= 2 && !input.ReadU32(groupCount)) ||
		(result.schemaVersion >= 3 && !input.ReadU32(metadataCount)) || !input.ReadU32(materialCount) ||
		!input.ReadU32(instanceCount) || !input.ReadU32(lightCount))
		return false;
	if (!std::isfinite(result.lightingCoordinateScale) || result.lightingCoordinateScale <= 0.0f)
		return false;
	if (result.indirectBounceCount > 16)
		return false;
	if (!std::isfinite(result.indirectRoughness) || result.indirectRoughness < 0.0f || result.indirectRoughness > 1.0f)
		return false;
	if (!std::isfinite(result.reflectanceBoost) || result.reflectanceBoost < 0.0f || result.reflectanceBoost > 8.0f)
		return false;
	const float cameraLengthSquared = result.cameraDirection[0] * result.cameraDirection[0] +
		result.cameraDirection[1] * result.cameraDirection[1] + result.cameraDirection[2] * result.cameraDirection[2];
	if (!std::isfinite(result.cameraDirection[0]) || !std::isfinite(result.cameraDirection[1]) ||
		!std::isfinite(result.cameraDirection[2]) || !std::isfinite(cameraLengthSquared) || cameraLengthSquared <= 0.0f)
		return false;
	if (!std::isfinite(result.originalSceneContribution) || result.originalSceneContribution < 0.0f ||
		result.originalSceneContribution > 1.0f || result.samplesPerFrame < 1 || result.samplesPerFrame > 128 || sampleAccumulation > 1)
		return false;
	if (result.heightPreviewMultiplier < 1 || result.heightPreviewMultiplier > 20)
		return false;
	result.sampleAccumulation = sampleAccumulation != 0;
	const size_t pixelCount = static_cast<size_t>(result.width) * result.height;
	if (pixelCount > bytes.size() / 2 || assetCount > bytes.size() / 74 ||
		artworkColorCount > bytes.size() / 146 || groupCount > bytes.size() / 4 ||
		metadataCount > bytes.size() / 12 || metadataCount > 16384 ||
		materialCount > bytes.size() || instanceCount > bytes.size() / 25 || lightCount > bytes.size() / 32)
		return false;

	result.originalRgb555.resize(pixelCount);
	result.mainPixels.resize(pixelCount);
	result.subPixels.resize(pixelCount);
	for (uint16_t &color : result.originalRgb555)
		if (!input.ReadU16(color))
			return false;
	for (RemasterFramePixel &pixel : result.mainPixels)
		if (!input.ReadU32(pixel.owner) || !input.ReadU32(pixel.instanceId) ||
			(result.schemaVersion >= 5 && !input.ReadU8(pixel.tilePixel)))
			return false;
	for (RemasterFramePixel &pixel : result.subPixels)
		if (!input.ReadU32(pixel.owner) || !input.ReadU32(pixel.instanceId) ||
			(result.schemaVersion >= 5 && !input.ReadU8(pixel.tilePixel)))
			return false;

	result.assets.resize(assetCount);
	for (RemasterFrameAsset &asset : result.assets)
	{
		if (!input.ReadTileId(asset.tileId) || !RemasterFrameSerialization::ValidTileId(asset.tileId) ||
			input.offset + 64 > bytes.size())
			return false;
		std::copy(bytes.begin() + input.offset, bytes.begin() + input.offset + 64, asset.indices);
		input.offset += 64;
	}
	result.artworkColors.resize(artworkColorCount);
	std::set<RemasterTileContentId> artworkColorIds;
	for (RemasterFrameArtworkColors &artwork : result.artworkColors)
	{
		if (!input.ReadTileId(artwork.tileId) || !RemasterFrameSerialization::ValidTileId(artwork.tileId) ||
			!input.ReadU64(artwork.visiblePixels) || !artworkColorIds.insert(artwork.tileId).second)
			return false;
		for (uint16_t &color : artwork.rgb555)
			if (!input.ReadU16(color))
				return false;
	}
	result.assetGroups.resize(groupCount);
	for (RemasterFrameAssetGroup &group : result.assetGroups)
	{
		uint32_t tileCount = 0;
		if (!input.ReadString(group.name) || !input.ReadU32(tileCount) || tileCount > bytes.size() / 10)
			return false;
		group.tileIds.resize(tileCount);
		for (RemasterTileContentId &tileId : group.tileIds)
			if (!input.ReadTileId(tileId) || !RemasterFrameSerialization::ValidTileId(tileId))
				return false;
	}
	result.assetMetadata.resize(metadataCount);
	std::set<RemasterTileContentId> metadataIds;
	for (RemasterFrameAssetMetadata &metadata : result.assetMetadata)
	{
		uint8_t hasMaterials = 0;
		uint8_t hasOcclusion = 0;
		uint8_t hasHeight = 0;
		uint8_t heightSampling = 0;
		uint8_t hasEmission = 0;
		uint8_t hasNormals = 0;
		uint8_t directLightingOppositeFacing = 0;
		if (!input.ReadTileId(metadata.tileId) || !RemasterFrameSerialization::ValidTileId(metadata.tileId) ||
			!input.ReadU8(hasMaterials) || !input.ReadU8(hasOcclusion) ||
			hasMaterials > 1 || hasOcclusion > 1 ||
			(result.schemaVersion >= 4 && (!input.ReadU8(hasHeight) || !input.ReadU8(heightSampling) ||
				hasHeight > 1 || heightSampling > static_cast<uint8_t>(RemasterHeightSampling::Linear))) ||
			(result.schemaVersion >= 6 && (!input.ReadU8(hasEmission) || hasEmission > 1)) ||
			(result.schemaVersion >= 8 && (!input.ReadU8(hasNormals) || hasNormals > 1)) ||
			(result.schemaVersion >= 10 && (!input.ReadU8(directLightingOppositeFacing) || directLightingOppositeFacing > 1)))
			return false;
		if (!metadataIds.insert(metadata.tileId).second)
			return false;
		metadata.hasMaterialSelectors = hasMaterials != 0;
		metadata.hasOcclusion = hasOcclusion != 0;
		metadata.hasHeight = hasHeight != 0;
		metadata.hasEmission = hasEmission != 0;
		metadata.hasNormals = hasNormals != 0;
		metadata.directLightingOppositeFacing = directLightingOppositeFacing != 0;
		metadata.heightSampling = static_cast<RemasterHeightSampling>(heightSampling);
		if (metadata.hasMaterialSelectors)
			for (std::string &name : metadata.materialSelectors)
				if (!input.ReadString(name))
					return false;
		if (metadata.hasOcclusion)
		{
			if (input.offset + metadata.occlusion.size() > bytes.size())
				return false;
			std::copy(bytes.begin() + input.offset, bytes.begin() + input.offset + metadata.occlusion.size(),
				metadata.occlusion.begin());
			input.offset += metadata.occlusion.size();
		}
		if (metadata.hasHeight)
		{
			if (input.offset + metadata.height.size() > bytes.size())
				return false;
			std::copy(bytes.begin() + input.offset, bytes.begin() + input.offset + metadata.height.size(),
				metadata.height.begin());
			input.offset += metadata.height.size();
		}
		if (metadata.hasNormals)
		{
			if (input.offset + metadata.normalXyz.size() > bytes.size())
				return false;
			std::copy(bytes.begin() + input.offset, bytes.begin() + input.offset + metadata.normalXyz.size(),
				metadata.normalXyz.begin());
			input.offset += metadata.normalXyz.size();
		}
		if (metadata.hasEmission)
		{
			if (input.offset + metadata.emissionRgba.size() > bytes.size())
				return false;
			std::copy(bytes.begin() + input.offset, bytes.begin() + input.offset + metadata.emissionRgba.size(),
				metadata.emissionRgba.begin());
			input.offset += metadata.emissionRgba.size();
		}
	}
	result.materials.resize(materialCount);
	for (RemasterFrameMaterial &material : result.materials)
	{
		uint8_t surfaceClass = 0;
		uint8_t receivesGi = 0;
		uint8_t castsShadow = 0;
		uint8_t hasDiffuseReflectance = 0;
		if (!input.ReadString(material.name) || !input.ReadU8(surfaceClass) ||
			surfaceClass > static_cast<uint8_t>(RemasterSurfaceClass::UserInterface) ||
			!input.ReadFloat(material.roughness) || !input.ReadFloat(material.metalness) ||
			!input.ReadFloat(material.specularLevel) || !input.ReadFloat(material.zMin) ||
			!input.ReadFloat(material.zMax) || !input.ReadU8(receivesGi) || !input.ReadU8(castsShadow) ||
			receivesGi > 1 || castsShadow > 1)
			return false;
		material.surfaceClass = static_cast<RemasterSurfaceClass>(surfaceClass);
		material.receivesGi = receivesGi != 0;
		material.castsShadow = castsShadow != 0;
		if (result.schemaVersion >= 16)
		{
			if (!input.ReadU8(hasDiffuseReflectance) || hasDiffuseReflectance > 1)
				return false;
			material.hasDiffuseReflectance = hasDiffuseReflectance != 0;
			if (material.hasDiffuseReflectance)
			{
				for (float &component : material.diffuseReflectance)
					if (!input.ReadFloat(component) || !std::isfinite(component) || component < 0.0f || component > 1.0f)
						return false;
			}
		}
	}
	result.tileInstances.resize(instanceCount);
	for (RemasterFrameTileInstance &instance : result.tileInstances)
	{
		uint8_t source = 0;
		uint8_t matchStatus = 0;
		uint8_t hFlip = 0;
		uint8_t vFlip = 0;
		uint8_t normalYaw = 0;
		if (!input.ReadTileId(instance.tileId) || !RemasterFrameSerialization::ValidTileId(instance.tileId) ||
			!input.ReadU8(source) ||
			source < static_cast<uint8_t>(RemasterSourceType::Backdrop) ||
			source > static_cast<uint8_t>(RemasterSourceType::Object) ||
			!input.ReadU8(instance.sourceIndex) || !input.ReadU16(instance.tileNumber) ||
			!input.ReadU8(instance.palette) || !input.ReadU16(instance.vramAddress) ||
			(result.schemaVersion >= 8 && (!input.ReadU8(hFlip) || !input.ReadU8(vFlip) || hFlip > 1 || vFlip > 1)) ||
			!input.ReadU8(matchStatus) ||
			matchStatus > static_cast<uint8_t>(RemasterProfileMatchStatus::Ambiguous) ||
			!input.ReadU32(instance.ruleLine) || !input.ReadString(instance.assetGroup) ||
			!input.ReadString(instance.material) ||
			(result.schemaVersion >= 18 && (!input.ReadU8(instance.ppuPriority) ||
				instance.ppuPriority > 3 || !input.ReadU8(instance.heightOffset))) ||
			(result.schemaVersion >= 19 && (!input.ReadU8(normalYaw) ||
				(normalYaw != 0 && normalYaw != 1 && normalYaw != 255))))
			return false;
		instance.source = static_cast<RemasterSourceType>(source);
		instance.hFlip = hFlip != 0;
		instance.vFlip = vFlip != 0;
		instance.normalYaw = normalYaw == 255 ? -1 : static_cast<int8_t>(normalYaw);
		instance.matchStatus = static_cast<RemasterProfileMatchStatus>(matchStatus);
	}
	result.lights.resize(lightCount);
	for (RemasterFrameLight &light : result.lights)
		if (!input.ReadFloat(light.x) || !input.ReadFloat(light.y) || !input.ReadFloat(light.z) ||
			!input.ReadFloat(light.radius) || !input.ReadFloat(light.red) || !input.ReadFloat(light.green) ||
			!input.ReadFloat(light.blue) || !input.ReadFloat(light.intensity))
			return false;
	if (input.offset != bytes.size())
		return false;
	for (const RemasterFramePixel &pixel : result.mainPixels)
		if (pixel.instanceId > result.tileInstances.size() ||
			(result.schemaVersion >= 5 && pixel.instanceId && pixel.tilePixel >= 64))
			return false;
	for (const RemasterFramePixel &pixel : result.subPixels)
		if (pixel.instanceId > result.tileInstances.size() ||
			(result.schemaVersion >= 5 && pixel.instanceId && pixel.tilePixel >= 64))
			return false;
	S9xRemasterInferLegacyTileInstanceFlips(result);
	frame = std::move(result);
	return true;
}

inline bool S9xReadRemasterFrame (const std::string &path, RemasterFrame &frame)
{
	std::ifstream input(path, std::ios::binary);
	if (!input)
		return false;
	std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	return (input.good() || input.eof()) && S9xDeserializeRemasterFrame(bytes, frame);
}

inline const RemasterFrameTileInstance *S9xRemasterFrameInstanceAt (
	const RemasterFrame &frame, uint32_t x, uint32_t y, bool subscreen = false)
{
	if (x >= frame.width || y >= frame.height)
		return nullptr;
	const std::vector<RemasterFramePixel> &pixels = subscreen ? frame.subPixels : frame.mainPixels;
	const size_t offset = static_cast<size_t>(y) * frame.width + x;
	if (offset >= pixels.size() || pixels[offset].instanceId == 0 ||
		pixels[offset].instanceId > frame.tileInstances.size())
		return nullptr;
	return &frame.tileInstances[pixels[offset].instanceId - 1];
}

inline std::vector<uint32_t> S9xRemasterFrameOccurrences (
	const RemasterFrame &frame, const RemasterTileContentId &tileId, bool subscreen = false)
{
	const std::vector<RemasterFramePixel> &pixels = subscreen ? frame.subPixels : frame.mainPixels;
	std::vector<uint32_t> occurrences;
	for (size_t offset = 0; offset < pixels.size(); offset++)
	{
		const uint32_t instanceId = pixels[offset].instanceId;
		if (instanceId && instanceId <= frame.tileInstances.size() &&
			frame.tileInstances[instanceId - 1].tileId == tileId)
			occurrences.push_back(static_cast<uint32_t>(offset));
	}
	return occurrences;
}

inline const RemasterFrameAsset *S9xRemasterFrameAssetForTile (
	const RemasterFrame &frame, const RemasterTileContentId &tileId)
{
	for (const RemasterFrameAsset &asset : frame.assets)
		if (asset.tileId == tileId)
			return &asset;
	return nullptr;
}

inline const RemasterFrameArtworkColors *S9xRemasterFrameArtworkColorsForTile (
	const RemasterFrame &frame, const RemasterTileContentId &tileId)
{
	for (const RemasterFrameArtworkColors &artwork : frame.artworkColors)
		if (artwork.tileId == tileId)
			return &artwork;
	return nullptr;
}

inline const RemasterFrameAssetMetadata *S9xRemasterFrameMetadataForTile (
	const RemasterFrame &frame, const RemasterTileContentId &tileId)
{
	for (const RemasterFrameAssetMetadata &metadata : frame.assetMetadata)
		if (metadata.tileId == tileId)
			return &metadata;
	return nullptr;
}

inline std::pair<int, int> S9xRemasterFrameHeightRange (const RemasterFrame &frame)
{
	std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> byTile;
	for (const RemasterFrameAssetMetadata &metadata : frame.assetMetadata)
		byTile.emplace(metadata.tileId, &metadata);
	std::vector<const RemasterFrameAssetMetadata *> byInstance(frame.tileInstances.size(), nullptr);
	for (size_t i = 0; i < frame.tileInstances.size(); i++)
	{
		const auto found = byTile.find(frame.tileInstances[i].tileId);
		if (found != byTile.end())
			byInstance[i] = found->second;
	}
	int minimum = 255, maximum = 0;
	bool foundHeight = false;
	for (const RemasterFramePixel &pixel : frame.mainPixels)
	{
		if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size() || pixel.tilePixel >= 64)
			continue;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		const RemasterFrameAssetMetadata *metadata = byInstance[pixel.instanceId - 1];
		if ((!metadata || !metadata->hasHeight) && !instance.heightOffset)
			continue;
		const int height = std::min(255, static_cast<int>(instance.heightOffset) +
			(metadata && metadata->hasHeight ? metadata->height[pixel.tilePixel] : 0));
		minimum = std::min(minimum, height);
		maximum = std::max(maximum, height);
		foundHeight = true;
	}
	return foundHeight ? std::make_pair(minimum, std::max(maximum, minimum + 1)) :
		std::make_pair(0, 1);
}

inline bool S9xRemasterHeightPreviewRangeValid (int minimum, int maximum)
{
	return minimum >= 0 && minimum <= 255 && maximum >= 1 && maximum <= 256 &&
		maximum > minimum;
}

inline const RemasterFrameMaterial *S9xRemasterFrameMaterialForName (
	const RemasterFrame &frame, const std::string &name)
{
	for (const RemasterFrameMaterial &material : frame.materials)
		if (material.name == name)
			return &material;
	return nullptr;
}

inline const RemasterFrameMaterial *S9xRemasterFrameMaterialForPixel (
	const RemasterFrame &frame, const RemasterFramePixel &pixel)
{
	if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size())
		return nullptr;
	const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
	const std::string *name = nullptr;
	if (pixel.tilePixel < 64)
	{
		const RemasterFrameAssetMetadata *metadata = S9xRemasterFrameMetadataForTile(frame, instance.tileId);
		if (metadata && metadata->hasMaterialSelectors && !metadata->materialSelectors[pixel.tilePixel].empty())
			name = &metadata->materialSelectors[pixel.tilePixel];
	}
	if (!name && !instance.material.empty())
		name = &instance.material;
	return name ? S9xRemasterFrameMaterialForName(frame, *name) : nullptr;
}

inline std::vector<RemasterFrameLight> S9xRemasterFrameEmissionLights (const RemasterFrame &frame)
{
	struct Accumulator
	{
		float x = 0.0f;
		float y = 0.0f;
		float red = 0.0f;
		float green = 0.0f;
		float blue = 0.0f;
		float z = 0.0f;
		float weight = 0.0f;
		bool emissive = false;
	};
	std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> metadataByTile;
	for (const RemasterFrameAssetMetadata &metadata : frame.assetMetadata)
		metadataByTile.emplace(metadata.tileId, &metadata);
	std::vector<Accumulator> accumulators(frame.tileInstances.size());
	for (size_t offset = 0; offset < frame.mainPixels.size(); offset++)
	{
		const RemasterFramePixel &pixel = frame.mainPixels[offset];
		if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size())
			continue;
		Accumulator &accumulator = accumulators[pixel.instanceId - 1];
		if (pixel.tilePixel >= 64)
			continue;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		const auto metadataEntry = metadataByTile.find(instance.tileId);
		const RemasterFrameAssetMetadata *metadata = metadataEntry == metadataByTile.end() ? nullptr : metadataEntry->second;
		if (!metadata || !metadata->hasEmission)
			continue;
		const size_t emissionOffset = pixel.tilePixel * 4;
		const float intensity = metadata->emissionRgba[emissionOffset + 3] / 25.0f;
		if (intensity <= 0.0f)
			continue;
		accumulator.x += (static_cast<float>(offset % frame.width) + 0.5f) * intensity;
		accumulator.y += (static_cast<float>(offset / frame.width) + 0.5f) * intensity;
		// Eight standard-intensity pixels produce a 1x light; every pixel remains additive.
		// Match the shader's emission decode before summing differently colored pixels.
		accumulator.red += std::pow(metadata->emissionRgba[emissionOffset] / 255.0f, 2.2f) * intensity / 8.0f;
		accumulator.green += std::pow(metadata->emissionRgba[emissionOffset + 1] / 255.0f, 2.2f) * intensity / 8.0f;
		accumulator.blue += std::pow(metadata->emissionRgba[emissionOffset + 2] / 255.0f, 2.2f) * intensity / 8.0f;
		accumulator.z += (metadata->hasHeight ? metadata->height[pixel.tilePixel] / 255.0f : 0.0f) * intensity;
		accumulator.weight += intensity;
		accumulator.emissive = true;
	}
	std::vector<RemasterFrameLight> lights;
	for (const Accumulator &accumulator : accumulators)
	{
		if (!accumulator.emissive || accumulator.weight <= 0.0f)
			continue;
		RemasterFrameLight light;
		light.x = accumulator.x / accumulator.weight;
		light.y = accumulator.y / accumulator.weight;
		const float emitterHeight = accumulator.z / accumulator.weight * frame.lightingCoordinateScale;
		light.z = std::max(emitterHeight, frame.lightingCoordinateScale / 255.0f);
		light.radius = 96.0f;
		light.red = accumulator.red;
		light.green = accumulator.green;
		light.blue = accumulator.blue;
		light.intensity = 12.0f;
		lights.push_back(light);
	}
	return lights;
}

inline std::vector<RemasterTileContentId> S9xRemasterFrameAssetGroupVariants (
	const RemasterFrame &frame, const std::string &groupName)
{
	if (groupName.empty())
		return {};
	for (const RemasterFrameAssetGroup &group : frame.assetGroups)
		if (group.name == groupName)
			return group.tileIds;

	std::vector<RemasterTileContentId> variants;
	for (const RemasterFrameTileInstance &instance : frame.tileInstances)
	{
		if (instance.assetGroup != groupName ||
			std::find(variants.begin(), variants.end(), instance.tileId) != variants.end())
			continue;
		variants.push_back(instance.tileId);
	}
	return variants;
}

inline bool S9xSerializeRemasterFrame (const RemasterFrame &frame, std::vector<uint8_t> &bytes)
{
	if (frame.schemaVersion != REMASTER_FRAME_SCHEMA_VERSION ||
		frame.width == 0 || frame.height == 0 ||
		frame.width > UINT32_MAX / frame.height)
		return false;
	const size_t pixelCount = static_cast<size_t>(frame.width) * frame.height;
	if (frame.originalRgb555.size() != pixelCount || frame.mainPixels.size() != pixelCount ||
		frame.subPixels.size() != pixelCount)
		return false;
	for (const RemasterFrameAsset &asset : frame.assets)
		if (!RemasterFrameSerialization::ValidTileId(asset.tileId))
			return false;
	std::set<RemasterTileContentId> artworkColorIds;
	for (const RemasterFrameArtworkColors &artwork : frame.artworkColors)
		if (!RemasterFrameSerialization::ValidTileId(artwork.tileId) ||
			!artworkColorIds.insert(artwork.tileId).second)
			return false;
	for (const RemasterFrameAssetGroup &group : frame.assetGroups)
		for (const RemasterTileContentId &tileId : group.tileIds)
			if (!RemasterFrameSerialization::ValidTileId(tileId))
				return false;
	if (frame.assetMetadata.size() > 16384)
		return false;
	std::set<RemasterTileContentId> metadataIds;
	for (const RemasterFrameAssetMetadata &metadata : frame.assetMetadata)
	{
		if (!RemasterFrameSerialization::ValidTileId(metadata.tileId))
			return false;
		if (!metadataIds.insert(metadata.tileId).second)
			return false;
	}
	for (const RemasterFrameTileInstance &instance : frame.tileInstances)
		if (!RemasterFrameSerialization::ValidTileId(instance.tileId) || instance.ppuPriority > 3 ||
			instance.normalYaw < -1 || instance.normalYaw > 1)
			return false;
	for (const RemasterFrameMaterial &material : frame.materials)
		if (material.hasDiffuseReflectance)
			for (float component : material.diffuseReflectance)
				if (!std::isfinite(component) || component < 0.0f || component > 1.0f)
					return false;

	bytes.clear();
	const uint8_t magic[] = { 'S', '9', 'X', 'R', 'M', 'F', 0, 1 };
	bytes.insert(bytes.end(), magic, magic + sizeof(magic));
	RemasterFrameSerialization::U32(bytes, frame.schemaVersion);
	RemasterFrameSerialization::U32(bytes, frame.width);
	RemasterFrameSerialization::U32(bytes, frame.height);
	if (!RemasterFrameSerialization::String(bytes, frame.profileRomSha256) ||
		(frame.schemaVersion >= 7 && (!std::isfinite(frame.lightingCoordinateScale) || frame.lightingCoordinateScale <= 0.0f)))
		return false;
	if (frame.schemaVersion >= 7)
		RemasterFrameSerialization::Float(bytes, frame.lightingCoordinateScale);
	if (frame.schemaVersion >= 9)
	{
		if (frame.indirectBounceCount > 16)
			return false;
		RemasterFrameSerialization::U8(bytes, frame.indirectBounceCount);
	}
	if (frame.schemaVersion >= 11)
	{
		if (!std::isfinite(frame.indirectRoughness) || frame.indirectRoughness < 0.0f || frame.indirectRoughness > 1.0f)
			return false;
		RemasterFrameSerialization::Float(bytes, frame.indirectRoughness);
	}
	if (frame.schemaVersion >= 12)
	{
		if (!std::isfinite(frame.originalSceneContribution) || frame.originalSceneContribution < 0.0f ||
			frame.originalSceneContribution > 1.0f || frame.samplesPerFrame < 1 || frame.samplesPerFrame > 128)
			return false;
		RemasterFrameSerialization::Float(bytes, frame.originalSceneContribution);
		RemasterFrameSerialization::U8(bytes, frame.samplesPerFrame);
		RemasterFrameSerialization::U8(bytes, frame.sampleAccumulation ? 1 : 0);
	}
	if (frame.schemaVersion >= 14)
	{
		const float cameraLengthSquared = frame.cameraDirection[0] * frame.cameraDirection[0] +
			frame.cameraDirection[1] * frame.cameraDirection[1] + frame.cameraDirection[2] * frame.cameraDirection[2];
		if (!std::isfinite(frame.cameraDirection[0]) || !std::isfinite(frame.cameraDirection[1]) ||
			!std::isfinite(frame.cameraDirection[2]) || !std::isfinite(cameraLengthSquared) || cameraLengthSquared <= 0.0f)
			return false;
		for (float component : frame.cameraDirection)
			RemasterFrameSerialization::Float(bytes, component);
	}
	if (frame.schemaVersion >= 15)
	{
		if (frame.heightPreviewMultiplier < 1 || frame.heightPreviewMultiplier > 20)
			return false;
		RemasterFrameSerialization::U8(bytes, frame.heightPreviewMultiplier);
	}
	if (frame.schemaVersion >= 17)
	{
		if (!std::isfinite(frame.reflectanceBoost) || frame.reflectanceBoost < 0.0f || frame.reflectanceBoost > 8.0f)
			return false;
		RemasterFrameSerialization::Float(bytes, frame.reflectanceBoost);
	}
	if (!RemasterFrameSerialization::Size(bytes, frame.assets.size()) ||
		!RemasterFrameSerialization::Size(bytes, frame.artworkColors.size()) ||
		!RemasterFrameSerialization::Size(bytes, frame.assetGroups.size()) ||
		!RemasterFrameSerialization::Size(bytes, frame.assetMetadata.size()) ||
		!RemasterFrameSerialization::Size(bytes, frame.materials.size()) ||
		!RemasterFrameSerialization::Size(bytes, frame.tileInstances.size()) ||
		!RemasterFrameSerialization::Size(bytes, frame.lights.size()))
		return false;
	for (uint16_t color : frame.originalRgb555)
		RemasterFrameSerialization::U16(bytes, color);
	for (const RemasterFramePixel &pixel : frame.mainPixels)
	{
		RemasterFrameSerialization::U32(bytes, pixel.owner);
		RemasterFrameSerialization::U32(bytes, pixel.instanceId);
		RemasterFrameSerialization::U8(bytes, pixel.tilePixel);
	}
	for (const RemasterFramePixel &pixel : frame.subPixels)
	{
		RemasterFrameSerialization::U32(bytes, pixel.owner);
		RemasterFrameSerialization::U32(bytes, pixel.instanceId);
		RemasterFrameSerialization::U8(bytes, pixel.tilePixel);
	}
	for (const RemasterFrameAsset &asset : frame.assets)
	{
		RemasterFrameSerialization::TileId(bytes, asset.tileId);
		bytes.insert(bytes.end(), asset.indices, asset.indices + 64);
	}
	for (const RemasterFrameArtworkColors &artwork : frame.artworkColors)
	{
		RemasterFrameSerialization::TileId(bytes, artwork.tileId);
		RemasterFrameSerialization::U64(bytes, artwork.visiblePixels);
		for (uint16_t color : artwork.rgb555)
			RemasterFrameSerialization::U16(bytes, color);
	}
	for (const RemasterFrameAssetGroup &group : frame.assetGroups)
	{
		if (!RemasterFrameSerialization::String(bytes, group.name) ||
			!RemasterFrameSerialization::Size(bytes, group.tileIds.size()))
			return false;
		for (const RemasterTileContentId &tileId : group.tileIds)
			RemasterFrameSerialization::TileId(bytes, tileId);
	}
	for (const RemasterFrameAssetMetadata &metadata : frame.assetMetadata)
	{
		RemasterFrameSerialization::TileId(bytes, metadata.tileId);
		RemasterFrameSerialization::U8(bytes, metadata.hasMaterialSelectors ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, metadata.hasOcclusion ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, metadata.hasHeight ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(metadata.heightSampling));
		RemasterFrameSerialization::U8(bytes, metadata.hasEmission ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, metadata.hasNormals ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, metadata.directLightingOppositeFacing ? 1 : 0);
		if (metadata.hasMaterialSelectors)
			for (const std::string &name : metadata.materialSelectors)
				if (!RemasterFrameSerialization::String(bytes, name))
					return false;
		if (metadata.hasOcclusion)
			bytes.insert(bytes.end(), metadata.occlusion.begin(), metadata.occlusion.end());
		if (metadata.hasHeight)
			bytes.insert(bytes.end(), metadata.height.begin(), metadata.height.end());
		if (metadata.hasNormals)
			bytes.insert(bytes.end(), metadata.normalXyz.begin(), metadata.normalXyz.end());
		if (metadata.hasEmission)
			bytes.insert(bytes.end(), metadata.emissionRgba.begin(), metadata.emissionRgba.end());
	}
	for (const RemasterFrameMaterial &material : frame.materials)
	{
		if (!RemasterFrameSerialization::String(bytes, material.name))
			return false;
		RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(material.surfaceClass));
		RemasterFrameSerialization::Float(bytes, material.roughness);
		RemasterFrameSerialization::Float(bytes, material.metalness);
		RemasterFrameSerialization::Float(bytes, material.specularLevel);
		RemasterFrameSerialization::Float(bytes, material.zMin);
		RemasterFrameSerialization::Float(bytes, material.zMax);
		RemasterFrameSerialization::U8(bytes, material.receivesGi ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, material.castsShadow ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, material.hasDiffuseReflectance ? 1 : 0);
		if (material.hasDiffuseReflectance)
			for (float component : material.diffuseReflectance)
				RemasterFrameSerialization::Float(bytes, component);
	}
	for (const RemasterFrameTileInstance &instance : frame.tileInstances)
	{
		RemasterFrameSerialization::TileId(bytes, instance.tileId);
		RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(instance.source));
		RemasterFrameSerialization::U8(bytes, instance.sourceIndex);
		RemasterFrameSerialization::U16(bytes, instance.tileNumber);
		RemasterFrameSerialization::U8(bytes, instance.palette);
		RemasterFrameSerialization::U16(bytes, instance.vramAddress);
		RemasterFrameSerialization::U8(bytes, instance.hFlip ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, instance.vFlip ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(instance.matchStatus));
		RemasterFrameSerialization::U32(bytes, instance.ruleLine);
		if (!RemasterFrameSerialization::String(bytes, instance.assetGroup) ||
			!RemasterFrameSerialization::String(bytes, instance.material))
			return false;
		RemasterFrameSerialization::U8(bytes, instance.ppuPriority);
		RemasterFrameSerialization::U8(bytes, instance.heightOffset);
		RemasterFrameSerialization::U8(bytes, instance.normalYaw < 0 ? 255 :
			static_cast<uint8_t>(instance.normalYaw));
	}
	for (const RemasterFrameLight &light : frame.lights)
	{
		RemasterFrameSerialization::Float(bytes, light.x);
		RemasterFrameSerialization::Float(bytes, light.y);
		RemasterFrameSerialization::Float(bytes, light.z);
		RemasterFrameSerialization::Float(bytes, light.radius);
		RemasterFrameSerialization::Float(bytes, light.red);
		RemasterFrameSerialization::Float(bytes, light.green);
		RemasterFrameSerialization::Float(bytes, light.blue);
		RemasterFrameSerialization::Float(bytes, light.intensity);
	}
	return true;
}

inline bool S9xWriteRemasterFrame (const RemasterFrame &frame, const std::string &path)
{
	std::vector<uint8_t> bytes;
	if (!S9xSerializeRemasterFrame(frame, bytes))
		return false;
	const std::string temporaryPath = path + ".tmp";
	std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
	if (!output)
		return false;
	output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	output.close();
	const bool wrote = output.good() && std::rename(temporaryPath.c_str(), path.c_str()) == 0;
	if (!wrote)
		std::remove(temporaryPath.c_str());
	return wrote;
}

#endif
