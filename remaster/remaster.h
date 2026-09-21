/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REMASTER_H_
#define _REMASTER_H_

#include "profile.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

static const uint32_t REMASTER_OWNER_UNSUPPORTED = 0xffffffffu;
static const uint32_t REMASTER_OWNER_FORCED_BLANK = 0xfffffffeu;

extern uint32_t *S9xRemasterCurrentOwners;
extern uint32_t S9xRemasterCurrentOwner;

struct RemasterState
{
	struct ObservedTile
	{
		struct ProfileOutcome
		{
			std::string assetGroup;
			std::string material;
			uint64_t observations = 0;
		};

		uint64_t hash = 0;
		uint64_t observations = 0;
		uint64_t unmatchedProfileObservations = 0;
		uint64_t ambiguousProfileObservations = 0;
		std::set<size_t> conflictingRuleLines;
		uint32_t sourceMask = 0;
		uint16_t tileNumber = 0;
		uint16_t vramAddress = 0;
		uint8_t bitDepth = 0;
		uint8_t sourceIndex = 0;
		uint8_t palette = 0;
		RemasterSourceType source = RemasterSourceType::Backdrop;
		std::map<std::string, ProfileOutcome> profileOutcomes;
		uint8_t indices[64] = {};
	};

	std::atomic<RemasterDebugMode> requestedDebugMode { RemasterDebugMode::Original };
	RemasterDebugMode activeDebugMode = RemasterDebugMode::Original;
	std::vector<uint32_t> mainOwners;
	std::vector<uint32_t> subOwners;
	std::mutex inventoryMutex;
	RemasterProfile requestedProfile;
	RemasterProfile activeProfile;
	bool profilePending = false;
	bool captureHasProfile = false;
	std::string requestedInventoryPath;
	std::string activeInventoryPath;
	std::map<uint64_t, ObservedTile> observedTiles;
	uint32_t hashCollisions = 0;
	uint64_t drawContexts = 0;
	uint64_t tileCacheVisits = 0;
	RemasterSourceType currentSource = RemasterSourceType::Backdrop;
	uint16_t currentTile = 0;
	uint8_t currentSourceIndex = 0;
	bool inventoryActive = false;
	bool currentDrawSupported = false;
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

inline bool S9xRemasterObserving (void)
{
	const RemasterState &state = S9xRemasterState();
	return S9xRemasterEnabled() || state.inventoryActive;
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
	{
		std::lock_guard<std::mutex> lock(state.inventoryMutex);
		if (state.profilePending)
		{
			state.activeProfile = std::move(state.requestedProfile);
			state.profilePending = false;
		}
		if (!state.requestedInventoryPath.empty())
		{
			state.activeInventoryPath.swap(state.requestedInventoryPath);
			state.observedTiles.clear();
			state.hashCollisions = 0;
			state.drawContexts = 0;
			state.tileCacheVisits = 0;
			state.captureHasProfile = !state.activeProfile.rules.empty();
			state.inventoryActive = true;
		}
	}
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
	if (S9xRemasterObserving())
	{
		RemasterState &state = S9xRemasterState();
		state.currentSource = source;
		state.currentSourceIndex = index;
		state.currentTile = tile;
		state.currentDrawSupported = true;
		if (state.inventoryActive)
			state.drawContexts++;
		S9xRemasterCurrentOwner = S9xRemasterOwner(source, index, tile);
	}
}

inline void S9xRemasterSetInventorySource (RemasterSourceType source, uint8_t index)
{
	RemasterState &state = S9xRemasterState();
	if (state.inventoryActive)
	{
		state.currentSource = source;
		state.currentSourceIndex = index;
		state.currentDrawSupported = true;
	}
}

inline void S9xRemasterSetUnsupportedDraw (void)
{
	if (S9xRemasterObserving())
	{
		S9xRemasterState().currentDrawSupported = false;
		S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;
	}
}

inline uint64_t S9xRemasterHashTile (uint8_t bitDepth, const uint8_t *indices)
{
	// FNV-1a over an explicitly versioned payload: version, kind, depth, width, height, indices.
	const uint8_t header[] = { 1, 1, bitDepth, 8, 8 };
	uint64_t hash = UINT64_C(14695981039346656037);
	for (uint8_t value : header)
	{
		hash ^= value;
		hash *= UINT64_C(1099511628211);
	}
	for (size_t i = 0; i < 64; i++)
	{
		hash ^= indices[i];
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

inline void S9xRemasterObserveTile (const uint8_t *indices, uint8_t bitDepth, uint16_t vramAddress, uint16_t tileWord)
{
	RemasterState &state = S9xRemasterState();
	if (state.inventoryActive)
		state.tileCacheVisits++;
	if (!state.inventoryActive || !state.currentDrawSupported ||
		(state.currentSource != RemasterSourceType::Background && state.currentSource != RemasterSourceType::Object))
		return;

	uint64_t hash = S9xRemasterHashTile(bitDepth, indices);
	RemasterProfileMatch profileMatch;
	if (!state.activeProfile.rules.empty())
	{
		RemasterProfileMatchContext context;
		context.tileId = { hash, 1, bitDepth };
		context.source = state.currentSource;
		context.sourceIndex = state.currentSourceIndex;
		context.palette = (tileWord >> 10) & 7;
		profileMatch = S9xRemasterMatchProfile(state.activeProfile, context);
	}
	auto recordProfileMatch = [&profileMatch] (RemasterState::ObservedTile &observed) {
		if (profileMatch.status == RemasterProfileMatchStatus::Ambiguous)
		{
			observed.ambiguousProfileObservations++;
			observed.conflictingRuleLines.insert(profileMatch.conflictingRuleLines.begin(),
				profileMatch.conflictingRuleLines.end());
			return;
		}
		if (profileMatch.status != RemasterProfileMatchStatus::Matched || !profileMatch.material)
		{
			if (S9xRemasterState().captureHasProfile)
				observed.unmatchedProfileObservations++;
			return;
		}
		std::string group = profileMatch.assetGroup ? profileMatch.assetGroup->name : std::string();
		std::string key = profileMatch.material->name + "\n" + group;
		RemasterState::ObservedTile::ProfileOutcome &outcome = observed.profileOutcomes[key];
		outcome.material = profileMatch.material->name;
		outcome.assetGroup = group;
		outcome.observations++;
	};
	auto found = state.observedTiles.find(hash);
	if (found != state.observedTiles.end())
	{
		if (found->second.bitDepth != bitDepth || !std::equal(indices, indices + 64, found->second.indices))
		{
			state.hashCollisions++;
			return;
		}
		found->second.observations++;
		found->second.sourceMask |= 1u << static_cast<uint8_t>(state.currentSource);
		recordProfileMatch(found->second);
		return;
	}

	RemasterState::ObservedTile observed;
	observed.hash = hash;
	observed.observations = 1;
	observed.sourceMask = 1u << static_cast<uint8_t>(state.currentSource);
	observed.tileNumber = tileWord & 0x3ff;
	observed.vramAddress = vramAddress;
	observed.bitDepth = bitDepth;
	observed.sourceIndex = state.currentSourceIndex;
	observed.palette = (tileWord >> 10) & 7;
	observed.source = state.currentSource;
	recordProfileMatch(observed);
	std::copy(indices, indices + 64, observed.indices);
	state.observedTiles.emplace(hash, observed);
}

inline void S9xRemasterRequestTileInventory (const std::string &path)
{
	RemasterState &state = S9xRemasterState();
	std::lock_guard<std::mutex> lock(state.inventoryMutex);
	state.requestedInventoryPath = path;
}

inline void S9xRemasterSetProfile (RemasterProfile profile)
{
	RemasterState &state = S9xRemasterState();
	std::lock_guard<std::mutex> lock(state.inventoryMutex);
	state.requestedProfile = std::move(profile);
	state.profilePending = true;
}

inline void S9xRemasterClearProfile (void)
{
	S9xRemasterSetProfile(RemasterProfile());
}

inline bool S9xRemasterEndFrame (void)
{
	RemasterState &state = S9xRemasterState();
	if (!state.inventoryActive)
		return false;

	std::ofstream output(state.activeInventoryPath, std::ios::out | std::ios::trunc);
	if (!output)
	{
		state.inventoryActive = false;
		return false;
	}

	output << "{\n  \"schema_version\": 1,\n  \"hash_version\": 1,\n"
		<< "  \"hash_algorithm\": \"fnv1a64\",\n  \"hash_collisions\": " << state.hashCollisions
		<< ",\n  \"draw_contexts\": " << state.drawContexts
		<< ",\n  \"tile_cache_visits\": " << state.tileCacheVisits
		<< ",\n  \"profile_loaded\": " << (state.captureHasProfile ? "true" : "false")
		<< ",\n  \"assets\": [\n";
	bool firstAsset = true;
	for (const auto &entry : state.observedTiles)
	{
		const RemasterState::ObservedTile &tile = entry.second;
		if (!firstAsset)
			output << ",\n";
		firstAsset = false;
		output << "    {\n      \"id\": \"v1:" << static_cast<unsigned>(tile.bitDepth) << "bpp:"
			<< std::hex << std::setfill('0') << std::setw(16) << tile.hash << std::dec << "\",\n"
			<< "      \"observations\": " << tile.observations << ",\n"
			<< "      \"source_mask\": " << tile.sourceMask << ",\n"
			<< "      \"example\": { \"source\": \""
			<< (tile.source == RemasterSourceType::Background ? "background" : "object")
			<< "\", \"source_index\": " << static_cast<unsigned>(tile.sourceIndex)
			<< ", \"tile_number\": " << tile.tileNumber
			<< ", \"palette\": " << static_cast<unsigned>(tile.palette)
			<< ", \"vram_address\": " << tile.vramAddress << " },\n"
			<< "      \"profile_matches\": [";
		bool firstMatch = true;
		for (const auto &matchEntry : tile.profileOutcomes)
		{
			const RemasterState::ObservedTile::ProfileOutcome &match = matchEntry.second;
			if (!firstMatch)
				output << ", ";
			firstMatch = false;
			output << "{ \"material\": \"" << match.material << "\"";
			if (!match.assetGroup.empty())
				output << ", \"asset_group\": \"" << match.assetGroup << "\"";
			output << ", \"observations\": " << match.observations << " }";
		}
		output << "],\n"
			<< "      \"profile_unmatched_observations\": " << tile.unmatchedProfileObservations << ",\n"
			<< "      \"profile_ambiguous_observations\": " << tile.ambiguousProfileObservations << ",\n"
			<< "      \"profile_conflicting_rule_lines\": [";
		bool firstLine = true;
		for (size_t line : tile.conflictingRuleLines)
		{
			if (!firstLine)
				output << ", ";
			firstLine = false;
			output << line;
		}
		output << "],\n"
			<< "      \"indices\": [\n";
		for (size_t y = 0; y < 8; y++)
		{
			output << "        [";
			for (size_t x = 0; x < 8; x++)
			{
				if (x)
					output << ", ";
				output << static_cast<unsigned>(tile.indices[y * 8 + x]);
			}
			output << "]" << (y == 7 ? "\n" : ",\n");
		}
		output << "      ]\n    }";
	}
	output << "\n  ]\n}\n";
	state.inventoryActive = false;
	return output.good();
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
