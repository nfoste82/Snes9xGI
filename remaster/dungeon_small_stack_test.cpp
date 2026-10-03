// Run from the repository root. Checks stay enabled with -DNDEBUG.
// c++ -std=c++17 -O2 -DNDEBUG -pthread remaster/dungeon_small_stack_test.cpp -o /path/test
// Also compile/run with -O0 -g. Optional complete fixture arguments use the
// runtime adapter's layout: profile snapshot attr linkX linkY plane
// [stairX stairY highNorth changesPlane]...
#include "remaster.h"
#include <pthread.h>
#include <iostream>
#include <stdexcept>

uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;

static void require(bool ok, const char *message)
{
	if (!ok) throw std::runtime_error(message);
}

static std::vector<uint8_t> readBytes(const char *path)
{
	std::ifstream input(path, std::ios::binary);
	require(bool(input), "cannot open fixture");
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct Run { int argc; char **argv; size_t stack; std::string error; };

static void exercise(Run &run)
{
#ifdef __APPLE__
	const size_t actualStack = pthread_get_stacksize_np(pthread_self());
	// Darwin reports the allocation including a small pthread overhead (12 KiB
	// on this arm64 host). The configured usable stack is checked below.
	if (actualStack < run.stack || actualStack > run.stack + 16 * 1024)
		throw std::runtime_error("worker stack exceeds crash budget: requested=" +
			std::to_string(run.stack) + " actual=" + std::to_string(actualStack));
#endif
	auto profile = std::make_unique<RemasterProfile>();
	std::vector<RemasterProfileDiagnostic> errors;
	require(S9xRemasterLoadProfile(run.argv[1], *profile, errors), "root profile failed to parse");
	auto tiles = std::make_unique<std::array<RemasterDungeonFloorContext::Placement, 4096>>();
	auto map = std::make_unique<RemasterDungeonHeightMap>();
	std::vector<uint8_t> attributes(8192, 1), maps(16384), graphics(65536);
	RemasterDungeonFloorContext context;
	context.verifiedAlttpRom = context.indoors = true;
	context.roomIndex = 7; context.linkX = 80; context.linkY = 160;
	context.collisionAttributes = attributes.data(); context.placements = tiles->data();
	using Surface = RemasterDungeonFloorContext::Surface;
	// Nontrivial authored geometry: two floors related by four wall ramps.
	for (int y : {12, 19}) for (int yy = y; yy < y + 3; yy++) for (int x = 8; x < 18; x++)
	{
		attributes[yy * 64 + x] = 0;
		auto &tile = (*tiles)[yy * 64 + x]; tile.surface = Surface::Floor; tile.hasHeight = true;
	}
	for (int y = 15; y < 19; y++) for (int x = 8; x < 18; x++)
	{
		auto &tile = (*tiles)[y * 64 + x]; tile.surface = Surface::Wall; tile.hasHeight = true;
		for (int yy = 0; yy < 8; yy++) for (int xx = 0; xx < 8; xx++) tile.height[yy * 8 + xx] = 7 - yy;
	}
	require(S9xRemasterBuildDungeonHeightMap(context, 50, *map), "synthetic solve failed");
	require(map->offsets[13 * 64 + 10] == 32 && map->offsets[20 * 64 + 10] == 0,
		"authored ramp relationship changed");
	RemasterFrame applied;
	applied.width = applied.height = 8; applied.mainPixels.resize(64); applied.subPixels.resize(64);
	RemasterFrameTileInstance instance;
	instance.source = RemasterSourceType::Background; instance.sourceIndex = 1;
	applied.tileInstances.push_back(instance);
	for (auto &pixel : applied.mainPixels) pixel.instanceId = 1;
	context.backgroundScrollX = 80; context.backgroundScrollY = 104;
	S9xRemasterApplyDungeonHeightMap(applied, context, *map);
	require(applied.tileInstances[0].heightOffset == 32, "geometry application failed");
	// Exercise the legacy wall-observation/border path as well, not only the
	// hasRoomGeometry fast path. It must retain the same solved floor base.
	map->hasRoomGeometry = false; applied.tileInstances[0].heightOffset = 0;
	S9xRemasterApplyDungeonHeightMap(applied, context, *map);
	require(applied.tileInstances[0].heightOffset == 32, "legacy application failed");
	context.backgroundScrollX = context.backgroundScrollY = 0;
	context.linkY = 152; // complete collision body fits the lower synthetic floor
	context.placements = nullptr;
	if (run.argc > 2)
	{
		auto raw = readBytes(run.argv[2]), attr = readBytes(run.argv[3]);
		const size_t vramOffset = 14 + 224 * (30 + 256 * 6);
		require(raw.size() == vramOffset + 65536 + 512 && !std::memcmp(raw.data(), "ALTPPD1\0", 8), "invalid complete snapshot");
		require(attr.size() == 8202 && !std::memcmp(attr.data(), "ALTPAT1\0", 8), "invalid complete attributes");
		auto word = [&] (size_t i) { return raw[i] | (raw[i + 1] << 8); };
		context.roomIndex = word(8);
		require((attr[8] | (attr[9] << 8)) == context.roomIndex, "fixture room mismatch");
		context.linkX = std::stoi(run.argv[4]); context.linkY = std::stoi(run.argv[5]); context.linkPlane = std::stoi(run.argv[6]);
		context.graphicsBase = word(30) * 2;
		require((word(32) & 3) == 3, "fixture map is incomplete");
		std::copy_n(raw.data() + vramOffset, 65536, graphics.begin());
		std::copy_n(attr.data() + 10, 8192, attributes.begin());
		for (int layer = 0; layer < 2; layer++) for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++)
		{
			int address = (word(18 + layer * 10) + (y / 32) * 2048 + (x / 32) * 1024 + (y % 32) * 32 + x % 32) & 32767;
			int target = (layer ? 0 : 8192) + (y * 64 + x) * 2;
			maps[target] = graphics[address * 2]; maps[target + 1] = graphics[address * 2 + 1];
		}
		for (int i = 7; i < run.argc; i += 4)
		{
			require(context.stairCount < 32, "too many stairs");
			auto &s = context.stairs[context.stairCount++];
			s.x = std::stoi(run.argv[i]); s.y = std::stoi(run.argv[i + 1]);
			s.highIsNorth = std::stoi(run.argv[i + 2]); s.changesPlane = std::stoi(run.argv[i + 3]);
		}
	}
	context.constructedMaps = maps.data(); context.graphics = graphics.data();
	// Exact supplied (normally root) profile metadata, no generated substitutions.
	S9xRemasterDungeonPlacements(context, *profile, *tiles);
	context.placements = tiles->data();
	require(S9xRemasterBuildDungeonHeightMap(context, profile->upperFloorHeight, *map), "profile placement solve failed");
	require(map->hasRoomGeometry && map->requiredPlaneCells[context.linkPlane] > 0, "empty profile solve");
	context.placements = nullptr; // EndFrame must compile its own placements.
	S9xRemasterSetProfile(std::move(*profile));
	S9xRemasterSetLiveFramesEnabled(true);
	auto &state = S9xRemasterState();
	state.runtimeDungeonHeightKey = 0; state.dungeonSnapshotValid = false;
	const auto epoch = state.runtimeDungeonHeightEpoch;
	uint16_t screen[64] = {};
	RemasterFrame completed;
	for (int iteration = 0; iteration < 3; iteration++)
	{
		S9xRemasterBeginFrame(64, 8, 8, 8);
		state.tileInstances.push_back(instance);
		std::fill(state.mainInstanceIds.begin(), state.mainInstanceIds.end(), 1);
		S9xRemasterEndFrame(screen, 8, 8, 8, &completed, &context);
		require(completed.width == 8 && state.runtimeDungeonHeightValid, "EndFrame did not solve");
		require(state.runtimeDungeonHeightMap.offsets == map->offsets &&
			state.runtimeDungeonHeightMap.planeOffsets == map->planeOffsets, "caller/direct solve mismatch");
		require(state.runtimeDungeonHeightEpoch == epoch + 1, "unchanged frame rebuilt geometry");
	}
}

static void *worker(void *argument)
{
	auto &run = *static_cast<Run *>(argument);
	try { exercise(run); } catch (const std::exception &e) { run.error = e.what(); }
	return nullptr;
}

int main(int argc, char **argv)
{
	if (!(argc == 2 || (argc >= 7 && (argc - 7) % 4 == 0)))
	{
		std::cerr << "usage: test profile [snapshot attr linkX linkY plane [stairX stairY highNorth changesPlane]...]\n";
		return 2;
	}
	for (size_t stack : {size_t(512 * 1024), size_t(544 * 1024)})
	{
		Run run{argc, argv, stack, {}};
		pthread_attr_t attributes;
		if (pthread_attr_init(&attributes)) return 1;
		if (pthread_attr_setstacksize(&attributes, stack)) return 1;
		size_t configuredStack = 0;
		if (pthread_attr_getstacksize(&attributes, &configuredStack) || configuredStack != stack) return 1;
		pthread_t thread;
		const int created = pthread_create(&thread, &attributes, worker, &run);
		pthread_attr_destroy(&attributes);
		if (created || pthread_join(thread, nullptr)) return 1;
		if (!run.error.empty()) { std::cerr << run.error << '\n'; return 1; }
		std::cout << "PASS stack=" << stack << " profile=" << argv[1]
			<< (argc > 2 ? " complete fixture" : " synthetic constructed room") << '\n';
	}
}
