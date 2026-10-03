// Exercise the exact cached runtime solver against a complete linked fixture.
#include "../remaster/remaster.h"
#include <iostream>
#include <stdexcept>

static std::vector<uint8_t> read(const char *path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in) throw std::runtime_error("cannot open input");
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
static uint64_t digest(const std::vector<uint8_t> &bytes)
{
	uint64_t value = UINT64_C(14695981039346656037);
	for (uint8_t b : bytes) value = (value ^ b) * UINT64_C(1099511628211);
	return value;
}
int main(int argc, char **argv)
{
	try {
		if (argc < 7 || (argc - 7) % 4) throw std::runtime_error("usage: snapshot attr profile linkX linkY plane [stairX stairY highNorth changesPlane]...");
		auto raw = read(argv[1]), attr = read(argv[2]);
		const size_t vramOffset = 14 + 224 * (30 + 256 * 6);
		if (raw.size() != vramOffset + 65536 + 512 || std::memcmp(raw.data(), "ALTPPD1\0", 8) ||
			attr.size() != 8202 || std::memcmp(attr.data(), "ALTPAT1\0", 8)) throw std::runtime_error("invalid complete snapshot/attributes");
		auto word = [&] (size_t i) { return raw[i] | (raw[i + 1] << 8); };
		RemasterDungeonFloorContext context;
		context.verifiedAlttpRom = context.indoors = true; context.roomIndex = word(8);
		if ((attr[8] | (attr[9] << 8)) != context.roomIndex) throw std::runtime_error("attribute room mismatch");
		context.linkX = std::stoi(argv[4]); context.linkY = std::stoi(argv[5]); context.linkPlane = std::stoi(argv[6]);
		context.collisionAttributes = attr.data() + 10; context.graphics = raw.data() + vramOffset;
		context.graphicsBase = word(30) * 2;
		if ((word(32) & 3) != 3) throw std::runtime_error("not a complete 64x64 map");
		std::array<uint8_t, 16384> maps = {};
		for (int layer = 0; layer < 2; layer++) for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++)
		{
			int address = (word(18 + layer * 10) + (y / 32) * 2048 + (x / 32) * 1024 + (y % 32) * 32 + x % 32) & 32767;
			int target = (layer ? 0 : 8192) + (y * 64 + x) * 2;
			maps[target] = context.graphics[address * 2]; maps[target + 1] = context.graphics[address * 2 + 1];
		}
		context.constructedMaps = maps.data();
		for (int i = 7; i < argc; i += 4)
		{
			if (context.stairCount == 32) throw std::runtime_error("too many stairs");
			auto &stair = context.stairs[context.stairCount++];
			stair.x = std::stoi(argv[i]); stair.y = std::stoi(argv[i + 1]);
			stair.highIsNorth = std::stoi(argv[i + 2]); stair.changesPlane = std::stoi(argv[i + 3]);
		}
		RemasterProfile profile; std::vector<RemasterProfileDiagnostic> errors;
		if (!S9xRemasterLoadProfile(argv[3], profile, errors)) throw std::runtime_error(errors.front().message);
		std::array<RemasterDungeonFloorContext::Placement, 4096> tiles = {};
		S9xRemasterDungeonPlacements(context, profile, tiles); context.placements = tiles.data();
		// Include final tile words in unresolved-structure diagnostics.
		std::array<uint16_t, 4096> tileWords = {};
		for (int i = 0; i < 4096; i++) tileWords[i] = maps[i * 2] | (maps[i * 2 + 1] << 8);
		RemasterDungeonHeightMap map;
		if (!S9xRemasterBuildDungeonHeightMap(context, profile.upperFloorHeight, map)) throw std::runtime_error("unsupported room");
		std::cout << "{\"format\":\"alttp-runtime-room-heights-v1\",\"room_id\":" << context.roomIndex
			<< ",\"bg2_words_fnv1a64\":\"" << std::hex << digest(std::vector<uint8_t>(maps.begin(), maps.begin() + 8192)) << "\"" << std::dec
			<< ",\"link_xy\":[" << context.linkX << ',' << context.linkY << "],\"plane\":" << int(context.linkPlane)
			<< ",\"snapshot_fnv1a64\":\"" << std::hex << digest(raw) << "\",\"attributes_fnv1a64\":\"" << digest(attr)
			<< "\",\"profile_fnv1a64\":\"" << digest(read(argv[3])) << "\"" << std::dec
			<< ",\"structural_conflicts\":" << map.structuralConflicts << ",\"solved_wall_cells\":" << map.solvedWallCells
			<< ",\"unknown_wall_cells\":" << map.unknownWallCells << ",\"cells_plane_x_y_base\":[";
		bool first = true;
		for (int p = 0; p < 2; p++) for (int i = 0; i < 4096; i++) if (map.planeOffsets[p][i] != 255)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << '[' << p << ',' << i % 64 << ',' << i / 64 << ',' << int(map.planeOffsets[p][i]) << ']';
		}
		std::cout << "],\"structural_placements\":["; first = true;
		for (int i = 0; i < 4096; i++) if (map.offsets[i] != 255 &&
			(tiles[i].surface == RemasterDungeonFloorContext::Surface::Wall || tiles[i].surface == RemasterDungeonFloorContext::Surface::Obstacle))
		{
			if (!first) std::cout << ','; first = false;
			std::cout << "{\"x\":" << i % 64 << ",\"y\":" << i / 64 << ",\"floor_base\":" << int(map.offsets[i])
				<< ",\"kind\":\"" << (tiles[i].surface == RemasterDungeonFloorContext::Surface::Wall ? "authored_wall" : "authored_obstacle") << "\",\"planes\":[";
			bool firstPlane = true;
			for (int p = 0; p < 2; p++) if ((tiles[i].surface == RemasterDungeonFloorContext::Surface::Wall ?
				map.structuralPlaneOffsets[p][i] : map.planeOffsets[p][i]) == map.offsets[i])
			{ if (!firstPlane) std::cout << ','; firstPlane = false; std::cout << p; }
			std::cout << "]}";
		}
		std::cout << "],\"relative_anchor_provenance\":["; first = true;
		for (const auto &anchor : map.relativeAnchors)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << "{\"node\":" << anchor.node << ",\"relative_minimum\":" << anchor.minimum
				<< ",\"support_nodes\":" << anchor.supportNodes << ",\"convention\":\""
				<< (anchor.isolatedFlat ? "isolated_authored_flat_relative_zero" : "lowest_established_support_relative_zero") << "\"}";
		}
		std::cout << "],\"geometry_component_issues\":["; first = true;
		for (const auto &issue : map.geometryGroupIssues)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << "{\"start_node\":" << issue.start << ",\"nodes\":" << issue.nodes << ",\"equality_mismatches\":" << issue.mismatches
				<< ",\"negative_base_nodes\":" << issue.negative << ",\"overflow_nodes\":" << issue.overflow
				<< ",\"isolated_flat\":" << (issue.isolatedFlat ? "true" : "false") << '}';
		}
		std::cout << "],\"constraint_issues\":["; first = true;
		for (const auto &issue : map.geometryConflicts)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << '[' << issue.node << ',' << issue.other << ',' << issue.expected << ',' << issue.actual << ']';
		}
		std::cout << "],\"issue_placements\":["; first = true;
		for (const auto &issue : map.geometryIssuePlacements)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << '[' << issue[0] << ',' << issue[1] << ',' << issue[2] << ',' << issue[3] << ']';
		}
		std::cout << "],\"unresolved_wall_placements\":["; first = true;
		for (int i = 0; i < 4096; i++) if (tiles[i].surface == RemasterDungeonFloorContext::Surface::Wall && map.offsets[i] == 255)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << "{\"xy\":[" << i % 64 << ',' << i / 64 << "],\"local_min\":" << int(*std::min_element(tiles[i].height.begin(), tiles[i].height.end()))
				<< ",\"local_max\":" << int(*std::max_element(tiles[i].height.begin(), tiles[i].height.end()))
				<< ",\"tile_word\":" << tileWords[i] << ",\"neighbors\":[";
			bool firstNeighbor = true;
			for (const auto &d : {std::array<int, 2>{0,-1}, {0,1}, {-1,0}, {1,0}})
			{
				int x = i % 64 + d[0], y = i / 64 + d[1];
				if (x < 0 || x >= 64 || y < 0 || y >= 64) continue;
				if (!firstNeighbor) std::cout << ','; firstNeighbor = false;
				int j = y * 64 + x;
				std::cout << "{\"xy\":[" << x << ',' << y << "],\"role\":" << int(tiles[j].surface)
					<< ",\"has_height\":" << (tiles[j].hasHeight ? "true" : "false")
					<< ",\"base\":" << int(map.offsets[j]) << ",\"attributes\":[" << int(context.collisionAttributes[j])
					<< ',' << int(context.collisionAttributes[4096 + j]) << "]}";
			}
			std::cout << "]}";
		}
		std::cout << "]}\n";
	} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
