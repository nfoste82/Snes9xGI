/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REMASTER_H_
#define _REMASTER_H_

#include "frame.h"
#include "profile.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

static const uint32_t REMASTER_OWNER_UNSUPPORTED = 0xffffffffu;
static const uint32_t REMASTER_OWNER_FORCED_BLANK = 0xfffffffeu;

extern uint32_t *S9xRemasterCurrentOwners;
extern uint32_t S9xRemasterCurrentOwner;

struct RemasterState
{
	struct PerformanceMetrics
	{
		double emulationFramePeriodMs = 0.0;
		double emulationFrameMs = 0.0;
		double pacingWaitMs = 0.0;
		double lightingFieldMs = 0.0;
		double lightingFieldWaitMs = 0.0;
		double lightingMeshMs = 0.0;
		uint32_t directEmitterCount = 0;
		uint32_t directSampleCount = 0;
		double lightingPreparationMs = 0.0;
		double directEncodeMs = 0.0;
		double indirectEncodeMs = 0.0;
		double accumulationEncodeMs = 0.0;
		double compositeEncodeMs = 0.0;
		double presentationQueueMs = 0.0;
		double drawableMs = 0.0;
		double gpuFrameMs = 0.0;
		double gpuDirectMs = 0.0;
		double gpuIndirectMs = 0.0;
		bool gpuStageTimingsAvailable = false;
		double presentedFps = 0.0;
		uint64_t droppedPresentations = 0;
	};

	struct ProfileMatchKey
	{
		RemasterTileContentId tileId;
		RemasterSourceType source = RemasterSourceType::Backdrop;
		uint8_t sourceIndex = 0;
		uint8_t palette = 0;

		bool operator< (const ProfileMatchKey &other) const
		{
			return std::tie(tileId, source, sourceIndex, palette) <
				std::tie(other.tileId, other.source, other.sourceIndex, other.palette);
		}
	};

	struct ObservedTile
	{
		struct Context
		{
			RemasterSourceType source = RemasterSourceType::Backdrop;
			uint16_t tileNumber = 0;
			uint16_t vramAddress = 0;
			uint64_t observations = 0;
			uint8_t sourceIndex = 0;
			uint8_t palette = 0;
		};

		struct ProfileOutcome
		{
			std::string assetGroup;
			std::string material;
			uint64_t observations = 0;
		};

		struct VisibleCell
		{
			uint16_t x = 0;
			uint16_t y = 0;
			uint16_t pixels = 0;
		};

		uint64_t hash = 0;
		uint64_t observations = 0;
		uint64_t visiblePixels = 0;
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
		std::map<uint64_t, Context> contexts;
		std::map<uint32_t, VisibleCell> visibleCells;
		std::map<std::string, ProfileOutcome> profileOutcomes;
		uint16_t visibleMinX = 0;
		uint16_t visibleMinY = 0;
		uint16_t visibleMaxX = 0;
		uint16_t visibleMaxY = 0;
		uint8_t indices[64] = {};
	};

	std::atomic<RemasterDebugMode> requestedDebugMode { RemasterDebugMode::Original };
	std::atomic<bool> requestedLiveFrames { false };
	std::atomic<bool> performanceMetricsEnabled { false };
	std::chrono::steady_clock::time_point performanceFrameStarted;
	std::chrono::steady_clock::time_point performanceLastFrameStarted;
	double performanceLastPresentedTime = 0.0;
	PerformanceMetrics performanceMetrics;
	std::mutex performanceMetricsMutex;
	RemasterDebugMode activeDebugMode = RemasterDebugMode::Original;
	std::vector<uint32_t> mainOwners;
	std::vector<uint32_t> subOwners;
	std::vector<uint64_t> mainTileHashes;
	std::vector<uint64_t> subTileHashes;
	std::vector<uint32_t> mainInstanceIds;
	std::vector<uint32_t> subInstanceIds;
	std::vector<uint8_t> mainTilePixels;
	std::vector<uint8_t> subTilePixels;
	RemasterFrame completedFrame;
	std::mutex inventoryMutex;
	RemasterProfile requestedProfile;
	RemasterProfile activeProfile;
	std::vector<RemasterDungeonHeightMap> activeDungeonHeightMaps;
	std::vector<RemasterDungeonHeightMap> requestedDungeonHeightMaps;
	RemasterDungeonHeightMap runtimeDungeonHeightMap;
	RemasterDungeonHeightMap previousRuntimeDungeonHeightMap;
	uint16_t runtimeDungeonObservedRoom = 0xffff;
	uint64_t runtimeDungeonHeightKey = 0;
	uint64_t runtimeDungeonObservedInputKey = 0;
	uint64_t runtimeDungeonHeightEpoch = 0;
	bool runtimeDungeonHeightValid = false;
	bool dungeonHeightMapsPending = false;
	bool profilePending = false;
	RemasterSceneSettings requestedSceneSettings;
	bool sceneSettingsPending = false;
	bool captureHasProfile = false;
	std::string requestedInventoryPath;
	std::string activeInventoryPath;
	std::string requestedFramePath;
	std::string activeFramePath;
	RemasterAnimationCapture animationCapture;
	uint32_t requestedFrameCount = 1;
	uint32_t activeFrameCount = 0;
	std::map<uint64_t, ObservedTile> observedTiles;
	std::map<ProfileMatchKey, RemasterProfileMatch> profileMatchCache;
	std::vector<RemasterFrameTileInstance> tileInstances;
	uint32_t hashCollisions = 0;
	uint64_t drawContexts = 0;
	uint64_t tileCacheVisits = 0;
	uint64_t currentTileHash = 0;
	size_t capturePitch = 0;
	size_t captureWidth = 0;
	size_t captureHeight = 0;
	RemasterSourceType currentSource = RemasterSourceType::Backdrop;
	uint16_t currentTile = 0;
	uint8_t currentSourceIndex = 0;
	uint8_t currentPpuPriority = 0;
	bool inventoryActive = false;
	bool frameCaptureActive = false;
	bool liveFramesActive = false;
	bool completedFrameAvailable = false;
	bool currentTileHashValid = false;
	bool currentDrawSupported = false;
	bool currentSubscreen = false;
	uint8_t currentTilePixel = 0xff;
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

inline uint64_t S9xRemasterDungeonHeightInputKey (const RemasterDungeonFloorContext &context,
	uint8_t floorRise)
{
	uint64_t key = UINT64_C(1469598103934665603);
	auto mix = [&key] (uint8_t value) { key = (key ^ value) * UINT64_C(1099511628211); };
	mix(context.roomIndex & 255); mix(context.roomIndex >> 8);
	mix(context.collisionMode); mix(floorRise);
	if (context.collisionAttributes)
		for (size_t i = 0; i < 8192; i++) mix(context.collisionAttributes[i]);
	for (uint8_t i = 0; i < context.stairCount; i++)
	{
		const auto &stair = context.stairs[i];
		mix(stair.x); mix(stair.y); mix(stair.table); mix(stair.tableEntry);
		mix(stair.highIsNorth); mix(stair.changesPlane);
	}
	return key;
}

inline bool S9xRemasterDungeonHeightInputsPublishable (const RemasterDungeonFloorContext &context,
	uint16_t publishedRoom)
{
	return context.collisionAttributes && context.collisionMode == 0 && context.stairCount != 0 &&
		(!context.submodule || context.roomIndex == publishedRoom);
}

inline bool S9xRemasterObserving (void)
{
	const RemasterState &state = S9xRemasterState();
	return S9xRemasterEnabled() || state.inventoryActive || state.frameCaptureActive || state.liveFramesActive;
}

inline bool S9xRemasterFramePacketActive (void)
{
	const RemasterState &state = S9xRemasterState();
	return state.frameCaptureActive || state.liveFramesActive;
}

inline void S9xRemasterSetPerformanceMetricsEnabled (bool enabled)
{
	RemasterState &state = S9xRemasterState();
	state.performanceMetricsEnabled.store(enabled, std::memory_order_relaxed);
	{
		std::lock_guard<std::mutex> lock(state.performanceMetricsMutex);
		state.performanceMetrics = {};
		state.performanceFrameStarted = {};
		state.performanceLastFrameStarted = {};
		state.performanceLastPresentedTime = 0.0;
	}
}

inline bool S9xRemasterPerformanceMetricsEnabled (void)
{
	return S9xRemasterState().performanceMetricsEnabled.load(std::memory_order_relaxed);
}

inline RemasterState::PerformanceMetrics S9xRemasterGetPerformanceMetrics (void)
{
	RemasterState &state = S9xRemasterState();
	std::lock_guard<std::mutex> lock(state.performanceMetricsMutex);
	return state.performanceMetrics;
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

inline void S9xRemasterBeginFrame (size_t pixelCount, size_t pitch = 0, size_t width = 0, size_t height = 0)
{
	RemasterState &state = S9xRemasterState();
	state.activeDebugMode = state.requestedDebugMode.load(std::memory_order_relaxed);
	state.liveFramesActive = state.requestedLiveFrames.load(std::memory_order_relaxed);
	state.completedFrameAvailable = false;
	{
		std::lock_guard<std::mutex> lock(state.inventoryMutex);
		if (state.profilePending)
		{
			state.activeProfile = std::move(state.requestedProfile);
			state.profileMatchCache.clear();
			state.profilePending = false;
		}
		if (state.dungeonHeightMapsPending)
		{
			state.activeDungeonHeightMaps = std::move(state.requestedDungeonHeightMaps);
			state.dungeonHeightMapsPending = false;
		}
		if (state.sceneSettingsPending)
		{
			S9xRemasterApplySceneSettings(state.activeProfile, state.requestedSceneSettings);
			state.sceneSettingsPending = false;
		}
		if (!state.requestedInventoryPath.empty())
		{
			state.activeInventoryPath = std::move(state.requestedInventoryPath);
			state.requestedInventoryPath.clear();
			state.observedTiles.clear();
			state.hashCollisions = 0;
			state.drawContexts = 0;
			state.tileCacheVisits = 0;
			state.capturePitch = pitch;
			state.captureWidth = width;
			state.captureHeight = height;
			state.captureHasProfile = !state.activeProfile.rules.empty();
			state.inventoryActive = true;
			state.mainTileHashes.assign(pixelCount, 0);
			state.subTileHashes.assign(pixelCount, 0);
		}
		if (!state.requestedFramePath.empty())
		{
			state.activeFramePath = std::move(state.requestedFramePath);
			state.requestedFramePath.clear();
			state.frameCaptureActive = true;
			state.activeFrameCount = state.requestedFrameCount;
			state.animationCapture = RemasterAnimationCapture();
			state.capturePitch = pitch;
			state.captureWidth = width;
			state.captureHeight = height;
			state.captureHasProfile = !state.activeProfile.rules.empty();
			state.observedTiles.clear();
			state.hashCollisions = 0;
			state.drawContexts = 0;
			state.tileCacheVisits = 0;
		}
	}
	if (state.frameCaptureActive)
	{
		state.observedTiles.clear();
		state.hashCollisions = 0;
		state.drawContexts = 0;
		state.tileCacheVisits = 0;
	}
	if (state.liveFramesActive)
	{
		state.capturePitch = pitch;
		state.captureWidth = width;
		state.captureHeight = height;
		state.captureHasProfile = !state.activeProfile.rules.empty();
		state.observedTiles.clear();
		state.hashCollisions = 0;
		state.drawContexts = 0;
		state.tileCacheVisits = 0;
	}
	if (S9xRemasterFramePacketActive())
	{
		state.tileInstances.clear();
		state.mainInstanceIds.assign(pixelCount, 0);
		state.subInstanceIds.assign(pixelCount, 0);
		state.mainTilePixels.assign(pixelCount, 0xff);
		state.subTilePixels.assign(pixelCount, 0xff);
	}
	state.currentTileHashValid = false;
	state.currentSubscreen = false;
	if (!S9xRemasterEnabled() && !S9xRemasterFramePacketActive())
	{
		S9xRemasterCurrentOwners = nullptr;
		return;
	}

	state.mainOwners.assign(pixelCount, REMASTER_OWNER_UNSUPPORTED);
	state.subOwners.assign(pixelCount, REMASTER_OWNER_UNSUPPORTED);
	S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;
	S9xRemasterCurrentOwners = nullptr;
}

inline void S9xRemasterClearSpan (size_t offset, size_t width, uint32_t owner = REMASTER_OWNER_UNSUPPORTED)
{
	RemasterState &state = S9xRemasterState();
	if (!S9xRemasterEnabled() && !state.inventoryActive && !S9xRemasterFramePacketActive())
		return;

	if ((S9xRemasterEnabled() || S9xRemasterFramePacketActive()) && offset < state.mainOwners.size())
	{
		const size_t ownerWidth = width < state.mainOwners.size() - offset ? width : state.mainOwners.size() - offset;
		std::fill_n(state.mainOwners.begin() + offset, ownerWidth, owner);
		std::fill_n(state.subOwners.begin() + offset, ownerWidth, owner);
	}
	if (S9xRemasterFramePacketActive() && offset < state.mainInstanceIds.size())
	{
		const size_t instanceWidth = width < state.mainInstanceIds.size() - offset ? width : state.mainInstanceIds.size() - offset;
		std::fill_n(state.mainInstanceIds.begin() + offset, instanceWidth, 0);
		std::fill_n(state.subInstanceIds.begin() + offset, instanceWidth, 0);
		std::fill_n(state.mainTilePixels.begin() + offset, instanceWidth, 0xff);
		std::fill_n(state.subTilePixels.begin() + offset, instanceWidth, 0xff);
	}
	if (state.inventoryActive && offset < state.mainTileHashes.size())
	{
		const size_t hashWidth = width < state.mainTileHashes.size() - offset ? width : state.mainTileHashes.size() - offset;
		std::fill_n(state.mainTileHashes.begin() + offset, hashWidth, UINT64_C(0));
		std::fill_n(state.subTileHashes.begin() + offset, hashWidth, UINT64_C(0));
	}
}

inline void S9xRemasterSetSubscreen (bool sub)
{
	RemasterState &state = S9xRemasterState();
	state.currentSubscreen = sub;
	S9xRemasterCurrentOwners = (S9xRemasterEnabled() || S9xRemasterFramePacketActive()) ?
		(sub ? state.subOwners.data() : state.mainOwners.data()) : nullptr;
}

inline uint32_t S9xRemasterOwner (RemasterSourceType source, uint8_t index, uint16_t tile)
{
	return (static_cast<uint32_t>(source) << 24) | (static_cast<uint32_t>(index) << 16) | tile;
}

inline void S9xRemasterSetDraw (RemasterSourceType source, uint8_t index, uint16_t tile,
	uint8_t ppuPriority = 0)
{
	if (S9xRemasterObserving())
	{
		RemasterState &state = S9xRemasterState();
		state.currentSource = source;
		state.currentSourceIndex = index;
		state.currentPpuPriority = ppuPriority;
		state.currentTile = tile;
		state.currentDrawSupported = true;
		state.currentTileHashValid = false;
		if (state.inventoryActive)
			state.drawContexts++;
		S9xRemasterCurrentOwner = S9xRemasterOwner(source, index, tile);
	}
}

inline void S9xRemasterSetInventorySource (RemasterSourceType source, uint8_t index)
{
	RemasterState &state = S9xRemasterState();
	if (state.inventoryActive || S9xRemasterFramePacketActive())
	{
		state.currentSource = source;
		state.currentSourceIndex = index;
		state.currentPpuPriority = 0;
		state.currentDrawSupported = true;
		state.currentTileHashValid = false;
	}
}

inline void S9xRemasterSetUnsupportedDraw (void)
{
	if (S9xRemasterObserving())
	{
		S9xRemasterState().currentDrawSupported = false;
		S9xRemasterState().currentTileHashValid = false;
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
	if ((!state.inventoryActive && !S9xRemasterFramePacketActive()) || !state.currentDrawSupported ||
		(state.currentSource != RemasterSourceType::Background && state.currentSource != RemasterSourceType::Object))
		return;

	uint64_t hash = S9xRemasterHashTile(bitDepth, indices);
	state.currentTileHash = hash;
	state.currentTileHashValid = true;
	state.currentTilePixel = 0xff;
	const uint8_t palette = (tileWord >> 10) & 7;
	const uint64_t contextKey = (static_cast<uint64_t>(state.currentSource) << 56) |
		(static_cast<uint64_t>(state.currentSourceIndex) << 48) |
		(static_cast<uint64_t>(palette) << 40) |
		(static_cast<uint64_t>(tileWord & 0x3ff) << 16) | vramAddress;
	auto recordContext = [&] (RemasterState::ObservedTile &observed) {
		RemasterState::ObservedTile::Context &context = observed.contexts[contextKey];
		context.source = state.currentSource;
		context.sourceIndex = state.currentSourceIndex;
		context.tileNumber = tileWord & 0x3ff;
		context.palette = palette;
		context.vramAddress = vramAddress;
		context.observations++;
	};
	RemasterProfileMatch profileMatch;
	if (!state.activeProfile.rules.empty())
	{
		const RemasterState::ProfileMatchKey key = {
			{ hash, 1, bitDepth }, state.currentSource, state.currentSourceIndex, palette
		};
		auto cached = state.profileMatchCache.find(key);
		if (cached == state.profileMatchCache.end())
		{
			RemasterProfileMatchContext context;
			context.tileId = key.tileId;
			context.source = key.source;
			context.sourceIndex = key.sourceIndex;
			context.palette = key.palette;
			cached = state.profileMatchCache.emplace(key,
				S9xRemasterMatchProfile(state.activeProfile, context)).first;
		}
		profileMatch = cached->second;
	}
	if (S9xRemasterFramePacketActive())
	{
		RemasterFrameTileInstance instance;
		instance.tileId = { hash, 1, bitDepth };
		instance.source = state.currentSource;
		instance.sourceIndex = state.currentSourceIndex;
		instance.tileNumber = tileWord & 0x3ff;
		instance.palette = palette;
		instance.ppuPriority = state.currentPpuPriority;
		instance.hFlip = (tileWord & 0x4000) != 0;
		instance.vFlip = (tileWord & 0x8000) != 0;
		instance.vramAddress = vramAddress;
		instance.matchStatus = profileMatch.status;
		if (profileMatch.rule)
			instance.ruleLine = static_cast<uint32_t>(profileMatch.rule->line);
		if (profileMatch.assetGroup)
			instance.assetGroup = profileMatch.assetGroup->name;
		if (profileMatch.material)
			instance.material = profileMatch.material->name;
		state.tileInstances.push_back(instance);
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
		if (state.inventoryActive)
		{
			found->second.observations++;
			found->second.sourceMask |= 1u << static_cast<uint8_t>(state.currentSource);
			recordContext(found->second);
			recordProfileMatch(found->second);
		}
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
	observed.palette = palette;
	observed.source = state.currentSource;
	if (state.inventoryActive)
	{
		recordContext(observed);
		recordProfileMatch(observed);
	}
	std::copy(indices, indices + 64, observed.indices);
	state.observedTiles.emplace(hash, observed);
}

inline void S9xRemasterRequestTileInventory (const std::string &path)
{
	RemasterState &state = S9xRemasterState();
	std::lock_guard<std::mutex> lock(state.inventoryMutex);
	state.requestedInventoryPath = path;
}

inline void S9xRemasterRequestFrameCapture (const std::string &path, uint32_t frameCount = 1)
{
	RemasterState &state = S9xRemasterState();
	std::lock_guard<std::mutex> lock(state.inventoryMutex);
	state.requestedFramePath = path;
	state.requestedFrameCount = std::max<uint32_t>(1, frameCount);
}

inline void S9xRemasterSetLiveFramesEnabled (bool enabled)
{
	S9xRemasterState().requestedLiveFrames.store(enabled, std::memory_order_relaxed);
}

inline const RemasterFrame *S9xRemasterCompletedFrame (void)
{
	const RemasterState &state = S9xRemasterState();
	return state.completedFrameAvailable ? &state.completedFrame : nullptr;
}

inline void S9xRemasterSetProfile (RemasterProfile profile)
{
	RemasterState &state = S9xRemasterState();
	std::lock_guard<std::mutex> lock(state.inventoryMutex);
	state.requestedProfile = std::move(profile);
	state.profilePending = true;
	state.sceneSettingsPending = false;
}

inline void S9xRemasterSetDungeonHeightMaps (std::vector<RemasterDungeonHeightMap> maps)
{
	RemasterState &state = S9xRemasterState();
	std::lock_guard<std::mutex> lock(state.inventoryMutex);
	state.requestedDungeonHeightMaps = std::move(maps);
	state.dungeonHeightMapsPending = true;
}

inline void S9xRemasterSetSceneSettings (const RemasterSceneSettings &settings)
{
	RemasterState &state = S9xRemasterState();
	std::lock_guard<std::mutex> lock(state.inventoryMutex);
	state.requestedSceneSettings = settings;
	state.sceneSettingsPending = true;
}

inline void S9xRemasterClearProfile (void)
{
	S9xRemasterSetProfile(RemasterProfile());
	S9xRemasterSetDungeonHeightMaps({});
}

enum RemasterCaptureResult
{
	RemasterCaptureNone = 0,
	RemasterCaptureInventory = 1,
	RemasterCaptureFrame = 2
};

inline bool S9xRemasterFinalizeFrame (RemasterFrame &frame, const uint16_t *screen, size_t screenPitch,
	size_t screenWidth, size_t screenHeight)
{
	const RemasterState &state = S9xRemasterState();
	if (!S9xRemasterFramePacketActive() || !screen || !screenWidth || !screenHeight || screenPitch < screenWidth ||
		screenWidth > state.capturePitch || screenWidth > UINT32_MAX || screenHeight > UINT32_MAX ||
		screenHeight > SIZE_MAX / screenPitch)
		return false;

	const size_t lastSourceOffset = (screenHeight - 1) * screenPitch + screenWidth - 1;
	if (lastSourceOffset >= state.mainOwners.size() || lastSourceOffset >= state.subOwners.size() ||
		lastSourceOffset >= state.mainInstanceIds.size() || lastSourceOffset >= state.subInstanceIds.size() ||
		lastSourceOffset >= state.mainTilePixels.size() || lastSourceOffset >= state.subTilePixels.size())
		return false;

	RemasterFrame result;
	result.width = static_cast<uint32_t>(screenWidth);
	result.height = static_cast<uint32_t>(screenHeight);
	result.profileRomSha256 = state.activeProfile.romSha256;
	result.lightingCoordinateScale = state.activeProfile.lightingCoordinateScale;
	result.cameraDirection = state.activeProfile.cameraDirection;
	result.indirectBounceCount = state.activeProfile.indirectBounceCount;
	result.indirectRoughness = state.activeProfile.indirectRoughness;
	result.reflectanceBoost = state.activeProfile.reflectanceBoost;
	result.originalSceneContribution = state.activeProfile.originalSceneContribution;
	result.heightPreviewMultiplier = state.activeProfile.heightPreviewMultiplier;
	result.samplesPerFrame = state.activeProfile.samplesPerFrame;
	result.sampleAccumulation = state.activeProfile.sampleAccumulation;
	const size_t pixelCount = screenWidth * screenHeight;
	result.originalRgb555.reserve(pixelCount);
	result.mainPixels.reserve(pixelCount);
	result.subPixels.reserve(pixelCount);
	for (size_t y = 0; y < screenHeight; y++)
	{
		for (size_t x = 0; x < screenWidth; x++)
		{
			const size_t sourceOffset = y * screenPitch + x;
			result.originalRgb555.push_back(screen[sourceOffset]);
			RemasterFramePixel mainPixel;
			mainPixel.owner = state.mainOwners[sourceOffset];
			mainPixel.instanceId = state.mainInstanceIds[sourceOffset];
			mainPixel.tilePixel = state.mainTilePixels[sourceOffset];
			result.mainPixels.push_back(mainPixel);
			RemasterFramePixel subPixel;
			subPixel.owner = state.subOwners[sourceOffset];
			subPixel.instanceId = state.subInstanceIds[sourceOffset];
			subPixel.tilePixel = state.subTilePixels[sourceOffset];
			result.subPixels.push_back(subPixel);
		}
	}
	for (const auto &entry : state.observedTiles)
	{
		RemasterFrameAsset asset;
		asset.tileId = { entry.second.hash, 1, entry.second.bitDepth };
		std::copy(entry.second.indices, entry.second.indices + 64, asset.indices);
		result.assets.push_back(asset);
	}
	result.tileInstances = state.tileInstances;
	S9xRemasterApplyProfileToFrame(state.activeProfile, result);
	struct ArtworkAccumulator
	{
		std::array<uint32_t, 64> red = {};
		std::array<uint32_t, 64> green = {};
		std::array<uint32_t, 64> blue = {};
		std::array<uint32_t, 64> count = {};
	};
	std::map<RemasterTileContentId, ArtworkAccumulator> artwork;
	if (state.frameCaptureActive)
	{
		for (size_t offset = 0; offset < result.mainPixels.size(); offset++)
		{
			const RemasterFramePixel &pixel = result.mainPixels[offset];
			if (!pixel.instanceId || pixel.instanceId > result.tileInstances.size() || pixel.tilePixel >= 64)
				continue;
			ArtworkAccumulator &samples = artwork[result.tileInstances[pixel.instanceId - 1].tileId];
			const uint16_t color = result.originalRgb555[offset];
			samples.red[pixel.tilePixel] += (color >> 10) & 31;
			samples.green[pixel.tilePixel] += (color >> 5) & 31;
			samples.blue[pixel.tilePixel] += color & 31;
			samples.count[pixel.tilePixel]++;
		}
		for (const auto &entry : artwork)
		{
			RemasterFrameArtworkColors colors;
			colors.tileId = entry.first;
			for (size_t pixel = 0; pixel < 64; pixel++)
			{
				const uint32_t count = entry.second.count[pixel];
				if (!count)
					continue;
				colors.rgb555[pixel] = static_cast<uint16_t>(((entry.second.red[pixel] / count) << 10) |
					((entry.second.green[pixel] / count) << 5) | (entry.second.blue[pixel] / count));
				colors.visiblePixels |= UINT64_C(1) << pixel;
			}
			result.artworkColors.push_back(colors);
		}
	}
	frame = std::move(result);
	return true;
}

inline uint8_t S9xRemasterEndFrame (const uint16_t *screen = nullptr, size_t screenPitch = 0,
	size_t screenWidth = 0, size_t screenHeight = 0, RemasterFrame *completedFrame = nullptr,
	const RemasterDungeonFloorContext *dungeonFloor = nullptr)
{
	RemasterState &state = S9xRemasterState();
	uint8_t result = RemasterCaptureNone;
	if (completedFrame)
		*completedFrame = RemasterFrame();
	if (state.inventoryActive)
	{
		for (size_t y = 0; y < state.captureHeight; y++)
		{
			for (size_t x = 0; x < state.captureWidth; x++)
			{
				const size_t offset = y * state.capturePitch + x;
				if (offset >= state.mainTileHashes.size() || !state.mainTileHashes[offset])
					continue;
				auto found = state.observedTiles.find(state.mainTileHashes[offset]);
				if (found == state.observedTiles.end())
					continue;
				RemasterState::ObservedTile &tile = found->second;
				const uint16_t cellX = static_cast<uint16_t>(x / 8);
				const uint16_t cellY = static_cast<uint16_t>(y / 8);
				RemasterState::ObservedTile::VisibleCell &cell = tile.visibleCells[
					(static_cast<uint32_t>(cellY) << 16) | cellX];
				cell.x = cellX;
				cell.y = cellY;
				cell.pixels++;
				if (!tile.visiblePixels)
				{
					tile.visibleMinX = tile.visibleMaxX = static_cast<uint16_t>(x);
					tile.visibleMinY = tile.visibleMaxY = static_cast<uint16_t>(y);
				}
				else
				{
					tile.visibleMinX = std::min(tile.visibleMinX, static_cast<uint16_t>(x));
					tile.visibleMinY = std::min(tile.visibleMinY, static_cast<uint16_t>(y));
					tile.visibleMaxX = std::max(tile.visibleMaxX, static_cast<uint16_t>(x));
					tile.visibleMaxY = std::max(tile.visibleMaxY, static_cast<uint16_t>(y));
				}
				tile.visiblePixels++;
			}
		}

		const std::string temporaryPath = state.activeInventoryPath + ".tmp";
		std::ofstream output(temporaryPath, std::ios::out | std::ios::trunc);
		if (output)
		{

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
			<< "      \"contexts\": [";
		bool firstContext = true;
		for (const auto &contextEntry : tile.contexts)
		{
			const RemasterState::ObservedTile::Context &context = contextEntry.second;
			if (!firstContext)
				output << ", ";
			firstContext = false;
			output << "{ \"source\": \""
				<< (context.source == RemasterSourceType::Background ? "background" : "object")
				<< "\", \"source_index\": " << static_cast<unsigned>(context.sourceIndex)
				<< ", \"tile_number\": " << context.tileNumber
				<< ", \"palette\": " << static_cast<unsigned>(context.palette)
				<< ", \"vram_address\": " << context.vramAddress
				<< ", \"observations\": " << context.observations << " }";
		}
		output << "],\n"
			<< "      \"visible_pixels\": " << tile.visiblePixels << ",\n"
			<< "      \"visible_bounds\": ";
		if (tile.visiblePixels)
			output << "{ \"min_x\": " << tile.visibleMinX << ", \"min_y\": " << tile.visibleMinY
				<< ", \"max_x\": " << tile.visibleMaxX << ", \"max_y\": " << tile.visibleMaxY << " }";
		else
			output << "null";
		output << ",\n"
			<< "      \"visible_cells\": [";
		bool firstCell = true;
		for (const auto &cellEntry : tile.visibleCells)
		{
			const RemasterState::ObservedTile::VisibleCell &cell = cellEntry.second;
			if (!firstCell)
				output << ", ";
			firstCell = false;
			output << "{ \"x\": " << cell.x << ", \"y\": " << cell.y
				<< ", \"pixels\": " << cell.pixels << " }";
		}
		output << "],\n"
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
			output.close();
			const bool wroteOutput = output.good() && std::rename(temporaryPath.c_str(), state.activeInventoryPath.c_str()) == 0;
			if (wroteOutput)
				result |= RemasterCaptureInventory;
			else
				std::remove(temporaryPath.c_str());
		}
		state.inventoryActive = false;
	}

	RemasterFrame frame;
	const bool frameFinalized = S9xRemasterFinalizeFrame(frame, screen, screenPitch, screenWidth, screenHeight);
	if (frameFinalized)
	{
		RemasterDungeonHeightApplyDiagnostics dungeonHeightDiagnostics;
		bool runtimeInputsPublishable = false;
		const RemasterDungeonHeightMap *appliedDungeonHeightMap = nullptr;
		if (dungeonFloor)
		{
			const bool roomChanged = state.runtimeDungeonObservedRoom != dungeonFloor->roomIndex;
			if (roomChanged)
			{
				if (state.runtimeDungeonHeightValid &&
					state.runtimeDungeonHeightMap.roomIndex == dungeonFloor->previousRoomIndex)
					state.previousRuntimeDungeonHeightMap = state.runtimeDungeonHeightMap;
				state.runtimeDungeonObservedRoom = dungeonFloor->roomIndex;
			}
			const RemasterDungeonHeightMap *reviewMap = nullptr;
			for (const RemasterDungeonHeightMap &map : state.activeDungeonHeightMaps)
				if (map.roomIndex == dungeonFloor->roomIndex)
				{
					reviewMap = &map;
					break;
				}
			runtimeInputsPublishable = S9xRemasterDungeonHeightInputsPublishable(*dungeonFloor,
				state.runtimeDungeonHeightValid ? state.runtimeDungeonHeightMap.roomIndex : 0xffff);
			state.runtimeDungeonObservedInputKey = S9xRemasterDungeonHeightInputKey(*dungeonFloor,
				state.activeProfile.upperFloorHeight);
			if (!reviewMap && runtimeInputsPublishable)
			{
				const uint64_t key = state.runtimeDungeonObservedInputKey;
				if (key != state.runtimeDungeonHeightKey)
				{
					RemasterDungeonHeightMap candidate;
					if (S9xRemasterBuildDungeonHeightMap(*dungeonFloor,
						state.activeProfile.upperFloorHeight, candidate) &&
						candidate.roomIndex == dungeonFloor->roomIndex)
					{
						state.runtimeDungeonHeightMap = std::move(candidate);
						state.runtimeDungeonHeightKey = key;
						state.runtimeDungeonHeightEpoch++;
						state.runtimeDungeonHeightValid = true;
					}
				}
			}
			if (!reviewMap && state.runtimeDungeonHeightValid &&
				state.runtimeDungeonHeightMap.roomIndex == dungeonFloor->roomIndex)
				reviewMap = &state.runtimeDungeonHeightMap;
			const RemasterDungeonHeightMap *sourceRoomMap = nullptr;
			for (const RemasterDungeonHeightMap &map : state.activeDungeonHeightMaps)
				if (map.roomIndex == dungeonFloor->previousRoomIndex) sourceRoomMap = &map;
			if (!sourceRoomMap && state.previousRuntimeDungeonHeightMap.roomIndex == dungeonFloor->previousRoomIndex)
				sourceRoomMap = &state.previousRuntimeDungeonHeightMap;
			if (reviewMap && reviewMap->roomIndex == dungeonFloor->roomIndex)
			{
				appliedDungeonHeightMap = reviewMap;
				S9xRemasterApplyDungeonHeightMap(frame, *dungeonFloor, *reviewMap, sourceRoomMap,
					&dungeonHeightDiagnostics);
			}
			else
				S9xRemasterApplyDungeonFloorHeight(frame, *dungeonFloor,
					state.activeProfile.upperFloorHeight, nullptr);
		}
		if (dungeonFloor)
		{
			const char *tracePath = std::getenv("S9X_REMASTER_DUNGEON_HEIGHT_TRACE");
			if (tracePath && *tracePath)
			{
				std::ofstream trace(tracePath, std::ios::app);
				if (trace)
				{
					const RemasterDungeonHeightMap &map = appliedDungeonHeightMap ?
						*appliedDungeonHeightMap : state.runtimeDungeonHeightMap;
					trace << "{\"room\":" << dungeonFloor->roomIndex
						<< ",\"previous_room\":" << dungeonFloor->previousRoomIndex
						<< ",\"submodule\":" << unsigned(dungeonFloor->submodule)
						<< ",\"transition_flags\":" << unsigned(dungeonFloor->roomTransitionFlags)
						<< ",\"scroll\":[" << dungeonFloor->backgroundScrollX << ',' << dungeonFloor->backgroundScrollY << ']'
						<< ",\"link\":[" << dungeonFloor->linkX << ',' << dungeonFloor->linkY << ','
						<< unsigned(dungeonFloor->linkPlane) << ']'
						<< ",\"collision_mode\":" << unsigned(dungeonFloor->collisionMode)
						<< ",\"collision_pointer_present\":" << (dungeonFloor->collisionAttributes ? "true" : "false")
						<< ",\"room_inputs_publishable\":" << (runtimeInputsPublishable ? "true" : "false")
						<< ",\"rise\":" << unsigned(state.activeProfile.upperFloorHeight)
						<< ",\"observed_input_key\":" << state.runtimeDungeonObservedInputKey
						<< ",\"published_map_key\":" << state.runtimeDungeonHeightKey
						<< ",\"map_epoch\":" << state.runtimeDungeonHeightEpoch
						<< ",\"map_valid\":" << (state.runtimeDungeonHeightValid ? "true" : "false")
						<< ",\"map_room\":" << map.roomIndex
						<< ",\"published_runtime_map_room\":" << state.runtimeDungeonHeightMap.roomIndex
						<< ",\"geometry_source\":\"" << (appliedDungeonHeightMap &&
							appliedDungeonHeightMap != &state.runtimeDungeonHeightMap ? "compiled_profile_map" :
							appliedDungeonHeightMap ? "runtime_complete_tables" : "legacy_fallback") << "\""
						<< ",\"components\":" << map.componentCount
						<< ",\"constraints\":" << map.constraintCount
						<< ",\"conflicting_components\":" << map.conflictingComponentCount
						<< ",\"solved_plane_cells\":[" << map.solvedPlaneCells[0] << ',' << map.solvedPlaneCells[1] << ']'
						<< ",\"applied\":{\"destination_pixels\":" << dungeonHeightDiagnostics.destinationPixels
						<< ",\"source_pixels\":" << dungeonHeightDiagnostics.sourcePixels
						<< ",\"destination_raised\":" << dungeonHeightDiagnostics.destinationRaisedPixels
						<< ",\"source_raised\":" << dungeonHeightDiagnostics.sourceRaisedPixels
						<< ",\"destination_unknown\":" << dungeonHeightDiagnostics.destinationUnknownPixels
						<< ",\"source_unknown\":" << dungeonHeightDiagnostics.sourceUnknownPixels << "}"
						<< ",\"stairs\":[";
					for (uint8_t i = 0; i < dungeonFloor->stairCount; i++)
					{
						if (i) trace << ',';
						const auto &stair = dungeonFloor->stairs[i];
						trace << "{\"xy\":[" << unsigned(stair.x) << ',' << unsigned(stair.y)
							<< "],\"table\":" << unsigned(stair.table)
							<< ",\"entry\":" << unsigned(stair.tableEntry)
							<< ",\"raw_high_side\":\"" << (stair.highIsNorth ? "north" : "south")
							<< "\",\"transformed_high_side\":\"" << (stair.highIsNorth ? "north" : "south")
							<< "\",\"orientation_transform\":\"none_runtime_table_is_displayed_handler\""
							<< ",\"changes_plane\":" << (stair.changesPlane ? "true" : "false") << '}';
					}
					trace << "]}\n";
				}
			}
		}
		S9xRemasterAlignGeneratedSpriteParts(frame, dungeonFloor);
		if (state.frameCaptureActive)
		{
			S9xRemasterAddAnimationCaptureFrame(state.animationCapture, frame);
			if (state.animationCapture.frameCount >= state.activeFrameCount)
			{
				RemasterFrame captured = S9xRemasterFinishAnimationCapture(std::move(state.animationCapture));
				if (S9xWriteRemasterFrame(captured, state.activeFramePath))
					result |= RemasterCaptureFrame;
				state.frameCaptureActive = false;
			}
		}
		if (completedFrame)
			*completedFrame = frame;
		if (state.liveFramesActive)
		{
			state.completedFrame = std::move(frame);
			state.completedFrameAvailable = true;
		}
	}
	else if (state.frameCaptureActive)
	{
		state.frameCaptureActive = false;
		state.animationCapture = RemasterAnimationCapture();
	}
	return result;
}

inline void S9xRemasterSetTilePixel (uint8_t tilePixel)
{
	S9xRemasterState().currentTilePixel = tilePixel;
}

inline void S9xRemasterWriteOwner (size_t offset)
{
	if (S9xRemasterCurrentOwners)
		S9xRemasterCurrentOwners[offset] = S9xRemasterCurrentOwner;

	RemasterState &state = S9xRemasterState();
	if (state.inventoryActive && offset < state.mainTileHashes.size())
		(state.currentSubscreen ? state.subTileHashes : state.mainTileHashes)[offset] =
			state.currentTileHashValid ? state.currentTileHash : UINT64_C(0);
	if (S9xRemasterFramePacketActive() && offset < state.mainInstanceIds.size())
	{
		(state.currentSubscreen ? state.subInstanceIds : state.mainInstanceIds)[offset] =
			state.currentTileHashValid ? static_cast<uint32_t>(state.tileInstances.size()) : 0;
		(state.currentSubscreen ? state.subTilePixels : state.mainTilePixels)[offset] =
			state.currentTileHashValid ? state.currentTilePixel : 0xff;
	}
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
