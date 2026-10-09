/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REMASTER_H_
#define _REMASTER_H_

#include "frame.h"
#include "capture_path.h"
#include "profile.h"
#include "held_lamp.h"
#include "offscreen_emitters.h"
#include "diagonal_assembly.h"

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
		uint32_t directSphereCount = 0;
		uint32_t directSphereSampleCount = 0;
		uint32_t directBatchSize = 0;
		uint32_t directDepthEmitterCount = 0;
		uint64_t directEmitterSignature = 0;
		double directSourceBuildMs = 0.0;
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
	std::atomic<bool> requestedInspectableFrames { false };
	std::atomic<bool> requestedRoomDiagnostics { false };
	std::atomic<bool> performanceMetricsEnabled { false };
	std::chrono::steady_clock::time_point performanceFrameStarted;
	std::chrono::steady_clock::time_point performanceLastFrameStarted;
	double performanceLastPresentedTime = 0.0;
	PerformanceMetrics performanceMetrics;
	std::mutex performanceMetricsMutex;
	RemasterDebugMode activeDebugMode = RemasterDebugMode::Original;
	std::vector<uint32_t> mainOwners;
	bool scenePass = false;
	std::vector<uint16_t> sceneRgb;
	std::vector<uint16_t> mainPreMathRgb;
	std::vector<uint8_t> sceneDepth;
	std::vector<RemasterFramePixel> scenePixels;
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
	RemasterDungeonHeightMap runtimeDungeonBg1HeightMap;
	RemasterDungeonHeightMap previousRuntimeDungeonBg1HeightMap;
	uint16_t runtimeDungeonObservedRoom = 0xffff;
	uint64_t runtimeDungeonHeightKey = 0;
	uint64_t runtimeDungeonObservedInputKey = 0;
	uint64_t runtimeDungeonHeightEpoch = 0;
	bool runtimeDungeonHeightValid = false;
	// Exact evidence key above advances on artwork changes; this key identifies
	// the last actual solve. Retained classification is heap-backed on small stacks.
	uint64_t runtimeDungeonSolveGeometryKey = 0;
	uint64_t dungeonGeometryReuseCount = 0;
	uint64_t dungeonClassifiedChangedCells = 0;
	std::unique_ptr<std::array<RemasterDungeonFloorContext::Placement,4096>> dungeonRetainedBg2;
	std::unique_ptr<std::array<RemasterDungeonFloorContext::Placement,4096>> dungeonRetainedBg1;
	// Bounded exact-evidence animation states. Unlike equivalent reuse, this can
	// retain genuinely different solved geometry without merging those states.
	struct DungeonSolvedArtworkState {
		uint64_t inputKey=0;
		std::array<uint8_t,65536> graphics;
		RemasterDungeonHeightMap bg1,bg2;
		std::array<RemasterDungeonFloorContext::Placement,4096> placements1,placements2;
	};
	std::vector<std::unique_ptr<DungeonSolvedArtworkState>> dungeonArtworkStates;
	uint64_t dungeonArtworkStateHits=0;
	RemasterDungeonFloorContext::EntrySupport dungeonObservedEntrySupport;
	struct TransitionSupport {
		bool valid=false, pending=false, doorObserved=false;
		uint16_t room=0xffff,destination=0xffff,x=0,y=0;
		uint8_t plane=0,rise=0;
		int base=0,cell=-1;
		uint64_t state=0,profile=0,rom=0,session=0,event=0;
		unsigned age=0,frames=0;
	};
	TransitionSupport transitionSupport;
	uint64_t dungeonSession=1, dungeonTransitionSequence=0;
	RemasterDungeonFloorContext::EntrySupport runtimeEntrySupport;
	RemasterDungeonFloorContext::EntrySupport runtimeBg1EntrySupport;
	uint16_t runtimeEntryRoom=0xffff;
	uint64_t dungeonGeometryProfileKey = 0;
	bool dungeonGeometryProfileDirty = true;
	std::array<uint8_t, 16384> dungeonObservedMaps = {};
	std::array<uint8_t, 8192> dungeonObservedAttributes = {};
	std::array<uint8_t, 65536> dungeonObservedGraphics = {};
	std::array<uint8_t,12876> dungeonObservedTemplates = {};
	std::array<RemasterDungeonFloorContext::Stair, 32> dungeonObservedStairs = {};
	uint8_t dungeonObservedStairCount = 255;
	uint16_t dungeonObservedGraphicsBase = 0xffff;
	uint64_t dungeonObservedRomIdentity = 0;
	uint16_t dungeonObservedBg1GraphicsBase = 0xffff;
	const uint8_t *dungeonObservedTemplateData = nullptr;
	uint8_t dungeonObservedRise = 255;
	uint8_t dungeonObservedPlane = 255;
	uint8_t dungeonObservedCollisionMode = 255;
	uint64_t dungeonTraceEventKey = 0;
	bool dungeonSnapshotValid = false;
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
	struct DungeonCaptureEvidence
	{
		RemasterDungeonFloorContext context;
		std::array<uint8_t,16384> maps;
		std::array<uint8_t,8192> attributes;
		std::array<uint8_t,65536> graphics;
		std::array<uint8_t,12876> templates;
		RemasterProfile profile;
		std::unique_ptr<RemasterDungeonHeightMap> map;
		uint64_t key=0,epoch=0;
		bool publishable=false;
	};
	std::unique_ptr<DungeonCaptureEvidence> animationDungeonEvidence;
	uint32_t requestedFrameCount = 1;
	uint32_t activeFrameCount = 0;
	std::string lastWrittenCapturePath;
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

inline void S9xRemasterInvalidateDungeonContinuity()
{
	auto &s=S9xRemasterState();s.dungeonSession++;
	s.transitionSupport={};s.runtimeEntrySupport={};s.runtimeBg1EntrySupport={};s.runtimeEntryRoom=0xffff;
	s.dungeonSnapshotValid=false;s.runtimeDungeonHeightValid=false;
	s.dungeonRetainedBg2.reset();s.dungeonRetainedBg1.reset();s.runtimeDungeonSolveGeometryKey=0;
	s.dungeonArtworkStates.clear();
	s.runtimeDungeonHeightKey=0;s.runtimeDungeonObservedRoom=0xffff;
	s.previousRuntimeDungeonHeightMap.roomIndex=0xffff;
	s.previousRuntimeDungeonBg1HeightMap.roomIndex=0xffff;
	s.runtimeDungeonBg1HeightMap.roomIndex=0xffff;
}

// Observe ordinary door scrolls only. Module07/02 constructs the next room;
// stairs/falls/warps have distinct handlers and cannot imply a zero height delta.
inline void S9xRemasterObserveDungeonTransition(const RemasterDungeonFloorContext &c)
{
	auto &s=S9xRemasterState();auto &t=s.transitionSupport;
	if(!c.verifiedAlttpRom || !c.indoors || c.module!=7 || !c.romIdentity ||
		s.dungeonGeometryProfileDirty || (t.valid && (t.rom!=c.romIdentity || t.session!=s.dungeonSession || t.profile!=s.dungeonGeometryProfileKey || t.rise!=s.activeProfile.upperFloorHeight))) {
		t={};s.runtimeEntrySupport={};s.runtimeBg1EntrySupport={};s.runtimeEntryRoom=0xffff;return;
	}
	if(t.valid && !t.pending) {
		if(++t.age>8)t={};
		else if(c.submodule==2) {
			const int delta=int(c.roomIndex)-int(t.room);
			const bool adjacent=((delta==1 || delta==-1) && (c.roomIndex>>4)==(t.room>>4)) || delta==16 || delta==-16;
			if(c.roomIndex==t.room || (adjacent && c.previousRoomIndex==t.room)) {
				t.pending=true;t.doorObserved=true;t.destination=c.roomIndex==t.room?0xffff:c.roomIndex;
				t.event=++s.dungeonTransitionSequence;t.frames=0;
			} else t={};
		} else if(c.submodule || c.roomIndex!=t.room)t={};
	}
	if(t.pending) {
		if(++t.frames>240 || (c.submodule && c.submodule!=2 && c.submodule!=5) ||
			(c.roomIndex!=t.room && c.previousRoomIndex!=t.room && c.previousRoomIndex!=t.destination)) {t={};return;}
		if(c.roomIndex!=t.room) {
			const int delta=int(c.roomIndex)-int(t.room);
			if(!(((delta==1 || delta==-1) && (c.roomIndex>>4)==(t.room>>4)) || delta==16 || delta==-16)){t={};return;}
			if(t.destination!=0xffff && t.destination!=c.roomIndex){t={};return;}
			t.destination=c.roomIndex;
		}
	}
}

inline void S9xRemasterRecordGroundedDungeonSupport(const RemasterDungeonFloorContext &c,
	const RemasterDungeonHeightMap *m,bool currentInputs)
{
	auto &s=S9xRemasterState();auto &t=s.transitionSupport;
	if(t.pending)return;
	if(!currentInputs || !c.groundedEvidence || c.submodule || c.module!=7 || !c.romIdentity ||
		s.runtimeDungeonObservedInputKey!=s.runtimeDungeonHeightKey ||
		!m || !m->hasRoomGeometry || m->roomIndex!=c.roomIndex || !m->roomDiagnostics || !c.collisionAttributes) {
		if(!c.groundedEvidence || c.module!=7)t={};return;
	}
	const int x=c.linkX&511,y=c.linkY&511,p=c.linkPlane&1;
	if(x>496 || y>487){t={};return;}
	int base=INT_MIN,id=-1,cell=-1;
	for(int yy=(y+8)/8;yy<=(y+23)/8;yy++)for(int xx=x/8;xx<=(x+15)/8;xx++) {
		const int i=yy*64+xx;const auto &d=m->roomDiagnostics->cells[p*4096+i];
		const int b=m->planeOffsets[p][i];
		if(!m->producerZeroFloor[i] || c.collisionAttributes[p*4096+i]!=0 || b==255 || d.floorId<0 || d.heightConflict ||
			(base!=INT_MIN && (base!=b || id!=d.floorId))) return;
		base=b;id=d.floorId;cell=p*4096+i;
	}
	t={};t.valid=true;t.room=c.roomIndex;t.x=c.linkX;t.y=c.linkY;t.plane=p;t.cell=cell;t.base=base;
	t.state=s.runtimeDungeonHeightKey;t.profile=s.dungeonGeometryProfileKey;t.rom=c.romIdentity;t.session=s.dungeonSession;
	t.rise=s.activeProfile.upperFloorHeight;
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
	for(int b=0;b<8;b++)mix(uint8_t(context.romIdentity>>(b*8)));
	mix(context.collisionMode); mix(floorRise);
	mix(context.linkPlane & 1);
	mix(context.placementBackgroundIndex);
	for(uint64_t v:{uint64_t(uint32_t(context.entrySupport.cell)),uint64_t(uint32_t(context.entrySupport.base)),
		uint64_t(context.entrySupport.predecessorRoom),context.entrySupport.predecessorState,context.entrySupport.transitionEvidence,context.entrySupport.destinationState})
		for(int b=0;b<8;b++)mix(uint8_t(v>>(8*b)));
	mix(context.dungeonTemplateData != nullptr);
	if(context.dungeonTemplateData) {
		for(int i=0;i<12876;i++)mix(context.dungeonTemplateData[i]);
	}
	if (context.constructedMaps)
		for (size_t i = 0; i < 16384; i++) mix(context.constructedMaps[i]);
	if (context.graphics && context.constructedMaps)
	{
		std::array<bool, 1024> used = {};
		for (int i = 0; i < 8192; i++)
		{
			const int tile = (context.constructedMaps[i * 2] | (context.constructedMaps[i * 2 + 1] << 8)) & 1023;
			if(i==4096)used.fill(false);
			if (used[tile]) continue;
			used[tile] = true;
			const int base=i>=4096 && context.bg1GraphicsBase!=0xffff?context.bg1GraphicsBase:context.graphicsBase;
			for (int b = 0; b < 32; b++) mix(context.graphics[static_cast<uint16_t>(base + tile * 32 + b)]);
		}
	}
	mix(context.graphicsBase & 255); mix(context.graphicsBase >> 8);
	mix(context.bg1GraphicsBase & 255); mix(context.bg1GraphicsBase >> 8);
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

// Artwork identities are application/provenance inputs, not interchangeable
// geometry labels. Require exact activated metadata as well as final classified
// records. Normals/occlusion may constrain future ownership rules even when the
// current classifier happens to produce the same mask. Emission/material/artwork
// fields remain live frame inputs and are deliberately NOT reused here.
inline bool S9xRemasterDungeonArtworkGeometryEquivalent(const RemasterProfile &profile,
	uint64_t a,uint64_t b)
{
	if(a==b)return true;
	const RemasterTileContentId ai={a,1,4},bi={b,1,4};
	const auto x=profile.assets.find(ai),y=profile.assets.find(bi);
	if((x==profile.assets.end())!=(y==profile.assets.end()))return false;
	if(x!=profile.assets.end()) {
		auto fields=[](const RemasterAssetMetadata &t){return std::tie(
			t.tileRole,t.hasTileSemantics,t.tileSemanticContextDependent,t.tileSemanticNormals,t.tileSemanticCorner,
			t.hasHeight,t.height,t.hasNormals,t.normalXyz,t.hasOcclusion,t.occlusion,t.hasDoorframeMask,t.doorframeMask);};
		if(fields(x->second)!=fields(y->second))return false;
	}
	for(const auto &g:profile.assetGroups) {
		const auto &ids=g.second.tileIds;
		if((std::find(ids.begin(),ids.end(),ai)!=ids.end())!=(std::find(ids.begin(),ids.end(),bi)!=ids.end()))return false;
	}
	return true;
}

// Exact classifier dependency proof for artwork-only changes. The classifier
// reads decoded identities only through profile metadata/group membership; ROM
// assembly ownership reads final words, not palette-index pixels. Those complete
// dependencies have already been byte-compared by the caller. Preserve EVERY
// retained classified record, refreshing only its producer identity after ALL
// changed usages prove equal activated metadata. Rejection mutates nothing.
inline uint64_t S9xRemasterHashTile(uint8_t bitDepth,const uint8_t *indices);
inline bool S9xRemasterRefreshDungeonArtworkGeometry(const RemasterDungeonFloorContext &c,
	const RemasterProfile &profile,RemasterState &s)
{
#ifdef ALTTP_TEST_CACHE_STAGES
	auto &stages=S9xRemasterDungeonCacheStages();RemasterDungeonStageTimer stage(stages.proofMs,stages.proofs);
#endif
	if(!c.constructedMaps || !c.graphics || !s.dungeonRetainedBg1 || !s.dungeonRetainedBg2)return false;
	std::map<uint16_t,uint64_t> hashes;
	std::map<std::pair<uint64_t,uint64_t>,bool> equal;
	auto fresh=std::make_unique<std::array<std::array<uint64_t,4096>,2>>();
	uint64_t changed=0;
	for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++) {
		const int offset=(bg?0:8192)+i*2;
		const int word=c.constructedMaps[offset]|c.constructedMaps[offset+1]<<8;
		const uint16_t address=uint16_t((!bg && c.bg1GraphicsBase!=0xffff?c.bg1GraphicsBase:c.graphicsBase)+(word&1023)*32);
		auto found=hashes.find(address);
		if(found==hashes.end()) {
			uint8_t pixels[64];
			for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
				int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(address+b/2*16+y*2+b%2)]>>(7-x))&1)<<b;
				pixels[y*8+x]=v;
			}
			found=hashes.emplace(address,S9xRemasterHashTile(4,pixels)).first;
		}
		const auto old=bg?(*s.dungeonRetainedBg2)[i].tileHash:(*s.dungeonRetainedBg1)[i].tileHash;
		const auto hash=found->second;(*fresh)[bg][i]=hash;
		if(old==hash)continue;
		changed++;
		const auto pair=std::make_pair(old,hash);auto test=equal.find(pair);
		if(test==equal.end())test=equal.emplace(pair,S9xRemasterDungeonArtworkGeometryEquivalent(profile,old,hash)).first;
		if(!test->second)return false;
	}
	for(int i=0;i<4096;i++) {
		(*s.dungeonRetainedBg1)[i].tileHash=s.runtimeDungeonBg1HeightMap.producerHashes[i]=(*fresh)[0][i];
		(*s.dungeonRetainedBg2)[i].tileHash=s.runtimeDungeonHeightMap.producerHashes[i]=(*fresh)[1][i];
	}
	s.dungeonClassifiedChangedCells=changed;
	return true;
}

inline std::unique_ptr<RemasterState::DungeonCaptureEvidence> S9xRemasterPinDungeonCaptureEvidence(
	const RemasterDungeonFloorContext *c,const RemasterProfile &profile,
	const RemasterDungeonHeightMap *map,uint64_t key,uint64_t epoch,bool publishable)
{
	if(!c || !c->verifiedAlttpRom || !c->indoors)return nullptr;
	auto evidence=std::make_unique<RemasterState::DungeonCaptureEvidence>();
	evidence->context=*c;evidence->context.placements=nullptr;
	evidence->context.supportRelations=nullptr; // solve-local pointer, never retain
	evidence->context.wallSupportRelations=nullptr;
	auto &p=evidence->context;
	if(c->constructedMaps){std::copy_n(c->constructedMaps,16384,evidence->maps.begin());p.constructedMaps=evidence->maps.data();}
	if(c->collisionAttributes){std::copy_n(c->collisionAttributes,8192,evidence->attributes.begin());p.collisionAttributes=evidence->attributes.data();}
	if(c->graphics){std::copy_n(c->graphics,65536,evidence->graphics.begin());p.graphics=evidence->graphics.data();}
	if(c->dungeonTemplateData){std::copy_n(c->dungeonTemplateData,12876,evidence->templates.begin());p.dungeonTemplateData=evidence->templates.data();}
	evidence->profile=profile;if(map)evidence->map=std::make_unique<RemasterDungeonHeightMap>(*map);
	evidence->key=key;evidence->epoch=epoch;evidence->publishable=publishable;
	return evidence;
}

inline void S9xRemasterWriteDungeonCaptureEvidence (const std::string &path,
	const RemasterDungeonFloorContext &c, const RemasterProfile &profile,
	const RemasterDungeonHeightMap *map, uint64_t key, uint64_t epoch, bool publishable)
{
	// Independent sidecars preserve complete live inputs; the viewport packet
	// alone cannot reproduce a solve. Never write to the loaded source profile.
	if (!c.verifiedAlttpRom || !c.indoors) return;
	std::string text; std::vector<RemasterProfileDiagnostic> errors;
	if (!S9xRemasterSerializeProfile(profile, text, errors)) return;
	uint64_t digest = UINT64_C(14695981039346656037);
	for (unsigned char b : text) digest = (digest ^ b) * UINT64_C(1099511628211);
	std::ofstream pinned(path + ".profile.toml", std::ios::binary); pinned << text;
	std::ofstream input(path + ".dungeon-input.bin", std::ios::binary);
	bool complete = c.constructedMaps && c.collisionAttributes && c.graphics;
	if (complete) {
		input.write("ALTPDI1\0", 8);
		input.write(reinterpret_cast<const char *>(c.constructedMaps), 16384);
		input.write(reinterpret_cast<const char *>(c.collisionAttributes), 8192);
		input.write(reinterpret_cast<const char *>(c.graphics), 65536);
	}
	// Optional verified template evidence is separate: old ALTPDI1 consumers
	// remain byte-compatible, and no ROM graphics are copied into the sidecar.
	if(c.dungeonTemplateData) {
		std::ofstream templates(path+".dungeon-templates.bin",std::ios::binary);
		templates.write("ALTPDT1\0",8);
		templates.write(reinterpret_cast<const char *>(c.dungeonTemplateData),12876);
	}
	if(c.hasTerrainActors) {
		// Additive sidecar: ALTPDI1 and schema30 remain byte-compatible. The
		// evidence context is value-owned with the FIRST completed capture frame.
		std::vector<uint8_t> bytes={'A','L','T','P','D','A','1',0};
		RemasterFrameSerialization::U16(bytes,c.actorScrollX);RemasterFrameSerialization::U16(bytes,c.actorScrollY);
		RemasterFrameSerialization::U16(bytes,uint16_t(c.linkZ));
		for(const auto &a:c.terrainActors){RemasterFrameSerialization::U16(bytes,a.x);RemasterFrameSerialization::U16(bytes,a.y);for(uint8_t b:{a.state,a.kind,a.z,a.plane})bytes.push_back(b);}
		for(const auto &o:c.terrainOam){RemasterFrameSerialization::U16(bytes,o.x);for(uint8_t b:{o.y,o.name,o.flags,o.size})bytes.push_back(b);}
		std::ofstream actors(path+".terrain-input.bin",std::ios::binary);actors.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());
	}
	std::ofstream out(path + ".dungeon.json");
	out << "{\"format\":\"alttp-live-height-evidence-v1\",\"rom_sha256\":\"" << profile.romSha256
		<< "\",\"profile_serialized_fnv1a64\":\"" << std::hex << digest << std::dec
		<< "\",\"room\":" << c.roomIndex << ",\"previous_room\":" << c.previousRoomIndex
		<< ",\"submodule\":" << int(c.submodule) << ",\"collision_mode\":" << int(c.collisionMode)
		<< ",\"link_xy\":[" << c.linkX << ',' << c.linkY << "],\"plane\":" << int(c.linkPlane)
		<< ",\"scroll\":[" << c.backgroundScrollX << ',' << c.backgroundScrollY << ']'
		<< ",\"graphics_base\":" << c.graphicsBase << ",\"bg1_graphics_base\":" << c.bg1GraphicsBase << ",\"rise\":" << int(profile.upperFloorHeight)
		<< ",\"ppu_map_base\":[" << c.ppuMapBase[0] << ',' << c.ppuMapBase[1]
		<< "],\"ppu_map_size\":[" << int(c.ppuMapSize[0]) << ',' << int(c.ppuMapSize[1]) << ']'
		<< ",\"complete_inputs\":" << (complete ? "true" : "false")
		<< ",\"publishable_inputs\":" << (publishable ? "true" : "false")
		<< ",\"cache_key\":" << key << ",\"epoch\":" << epoch << ",\"stairs\":[";
	for (int i=0;i<c.stairCount;i++) {
		if(i) out << ','; const auto &s=c.stairs[i];
		out << '[' << int(s.x) << ',' << int(s.y) << ',' << s.highIsNorth << ',' << s.changesPlane << ']';
	}
	out << "],\"applied_map_room\":" << (map ? map->roomIndex : 65535);
	out << ",\"entry_support\":{\"cell\":" << c.entrySupport.cell << ",\"base\":" << c.entrySupport.base
		<< ",\"predecessor_room\":" << c.entrySupport.predecessorRoom << ",\"predecessor_state\":" << c.entrySupport.predecessorState
		<< ",\"transition_evidence\":" << c.entrySupport.transitionEvidence << ",\"destination_state\":" << c.entrySupport.destinationState << '}';
	const auto &session=S9xRemasterState();
	out << ",\"continuity_session\":" << session.dungeonSession << ",\"rom_identity\":" << c.romIdentity
		<< ",\"grounded_evidence\":" << (c.groundedEvidence?"true":"false");
	if(map) {
		out << ",\"configured_stair_fallbacks\":" << map->configuredStairFallbacks
			<< ",\"persistent_support_required\":["<<map->persistentSupportRequired[0]<<','<<map->persistentSupportRequired[1]<<']'
			<< ",\"persistent_support_missing\":["<<map->persistentSupportMissing[0]<<','<<map->persistentSupportMissing[1]<<']'
			<< ",\"entry_support_consumed\":" << (map->entrySupportConsumed?"true":"false");
		out << ",\"required_planes\":[" << map->requiredPlaneCells[0] << ',' << map->requiredPlaneCells[1]
			<< "],\"missing_planes\":[" << map->unknownPlaneCells[0] << ',' << map->unknownPlaneCells[1]
			<< "],\"solved_walls\":" << map->solvedWallCells << ",\"unknown_walls\":" << map->unknownWallCells
			<< ",\"structural_conflicts\":" << map->structuralConflicts << ",\"plane_bases\":[";
		for(int p=0;p<2;p++){if(p)out<<',';out<<'[';for(int i=0;i<4096;i++){if(i)out<<',';out<<int(map->planeOffsets[p][i]);}out<<']';}
		out << "],\"visible_bases\":[";
		for(int i=0;i<4096;i++){if(i)out<<',';out<<int(map->offsets[i]);}out<<']';
		out << ",\"candidate_floor_cells\":[" << map->candidateFloorCells[0] << ',' << map->candidateFloorCells[1]
			<< "],\"unobserved_floor_cells\":[" << map->unobservedFloorCells[0] << ',' << map->unobservedFloorCells[1]
			<< "],\"disconnected_plane_candidates\":" << (map->disconnectedPlaneCandidates?"true":"false")
			<< ",\"plane_relationship_unresolved\":" << (map->planeRelationshipUnresolved?"true":"false");
		out << ",\"geometry_component_issues\":[";
		for(size_t i=0;i<map->geometryGroupIssues.size();i++){
			if(i)out<<',';const auto &g=map->geometryGroupIssues[i];
			out << "{\"nodes\":" << g.nodes << ",\"mismatches\":" << g.mismatches
				<< ",\"negative\":" << g.negative << ",\"overflow\":" << g.overflow
				<< ",\"isolated_flat\":" << (g.isolatedFlat?"true":"false") << '}';
		} out << ']';
	}
	out << "}\n";
}

inline bool S9xRemasterDungeonHeightInputsPublishable (const RemasterDungeonFloorContext &context,
	uint16_t publishedRoom)
{
	return context.collisionAttributes && context.collisionMode == 0 &&
		context.verifiedAlttpRom && context.indoors &&
		context.module == 7 &&
		(context.stairCount != 0 || context.constructedMaps) &&
		(context.constructedMaps ? !context.submodule : (!context.submodule || context.roomIndex == publishedRoom));
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
	int next = (static_cast<int>(state.requestedDebugMode.load(std::memory_order_relaxed)) + 1) % static_cast<int>(RemasterDebugMode::Count);
	RemasterDebugMode mode = static_cast<RemasterDebugMode>(next);
	state.requestedDebugMode.store(mode, std::memory_order_relaxed);
	state.requestedRoomDiagnostics.store(mode == RemasterDebugMode::FloorWalls, std::memory_order_relaxed);
	return mode;
}

inline void S9xRemasterBeginFrame (size_t pixelCount, size_t pitch = 0, size_t width = 0, size_t height = 0,
	bool capturePreMath = false)
{
	RemasterState &state = S9xRemasterState();
	state.activeDebugMode = state.requestedDebugMode.load(std::memory_order_relaxed);
	state.liveFramesActive = state.requestedLiveFrames.load(std::memory_order_relaxed) ||
		state.requestedRoomDiagnostics.load(std::memory_order_relaxed) ||
		state.requestedInspectableFrames.load(std::memory_order_relaxed) ||
		state.activeDebugMode == RemasterDebugMode::TileTypes ||
		state.activeDebugMode == RemasterDebugMode::FloorWalls;
	state.completedFrameAvailable = false;
	state.scenePass = false;
	state.sceneRgb.clear();
	state.sceneDepth.clear();
	state.scenePixels.clear();
	state.mainPreMathRgb.clear();
	{
		std::lock_guard<std::mutex> lock(state.inventoryMutex);
		if (state.profilePending)
		{
			state.activeProfile = std::move(state.requestedProfile);
			state.dungeonGeometryProfileDirty = true;
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
			state.dungeonGeometryProfileDirty = true;
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
			state.animationDungeonEvidence.reset();
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
		if (capturePreMath) state.mainPreMathRgb.assign(pixelCount, 0);
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

inline void S9xRemasterDungeonPlacements (const RemasterDungeonFloorContext &context,
	const RemasterProfile &profile, std::array<RemasterDungeonFloorContext::Placement, 4096> &output)
{
#ifdef ALTTP_TEST_CACHE_STAGES
	auto &stages=S9xRemasterDungeonCacheStages();RemasterDungeonStageTimer stage(stages.classifyMs,stages.classifications);
#endif
	using Surface = RemasterDungeonFloorContext::Surface;
	std::map<RemasterTileContentId, Surface> surfaces;
	for (const auto &group : profile.assetGroups)
	{
		Surface role = Surface::Unknown;
		if (group.first == "wall_faces") role = Surface::Wall;
		else if (group.first == "dungeon_floor" || group.first == "wall_tops") role = Surface::Floor;
		else if (group.first == "obstacles") role = Surface::Obstacle;
		else if (group.first == "stair_treads" || group.first == "stair_rails") role = Surface::Stair;
		if (role == Surface::Unknown) continue;
		auto priority = [] (Surface s) {
			return s == Surface::Obstacle ? 4 : s == Surface::Stair ? 3 : s == Surface::Wall ? 2 : s == Surface::Floor ? 1 : 0;
		};
		for (const auto &id : group.second.tileIds)
			if (!surfaces.count(id) || priority(role) > priority(surfaces[id])) surfaces[id] = role;
	}
	std::map<uint16_t, RemasterTileContentId> hashes;
	for (int i = 0; i < 4096; i++)
	{
		const uint16_t word = context.constructedMaps[i * 2] | (context.constructedMaps[i * 2 + 1] << 8);
		const uint16_t address = static_cast<uint16_t>(context.graphicsBase + (word & 1023) * 32);
		if (!hashes.count(address))
		{
			uint8_t pixels[64];
			for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++)
			{
				uint8_t index = 0;
				for (int plane = 0; plane < 4; plane++)
					index |= ((context.graphics[static_cast<uint16_t>(address + (plane / 2) * 16 + y * 2 + plane % 2)] >> (7 - x)) & 1) << plane;
				pixels[y * 8 + x] = index;
			}
			hashes[address] = {S9xRemasterHashTile(4, pixels), 1, 4};
		}
		auto &placed = output[i];
		placed = RemasterDungeonFloorContext::Placement{};
		const auto id = hashes[address];
		placed.tileHash = id.hash;
		if (surfaces.count(id)) placed.surface = surfaces[id];
		auto asset = profile.assets.find(id);
		// Workbench roles are authored semantics too. Generated groups can lag
		// live edits; ignoring the explicit role left the lower wall's middle
		// and foot Unknown despite their authored ramps being in the packet.
		if (asset != profile.assets.end())
		{
			placed.semanticBoundaryKnown=asset->second.hasTileSemantics || asset->second.tileRole!=RemasterTileRole::Unknown ||
				placed.surface!=Surface::Unknown;
			placed.semanticSoutheastWall=asset->second.hasTileSemantics && !asset->second.tileSemanticContextDependent &&
				asset->second.tileRole==RemasterTileRole::Wall && asset->second.tileSemanticNormals==std::vector<std::string>{"south_east"};
			const auto role = asset->second.tileRole;
			if (role == RemasterTileRole::Wall || role == RemasterTileRole::WallCorner) {
				if(!placed.semanticSoutheastWall)placed.surface = Surface::Wall;
				else if(asset->second.hasNormals && asset->second.hasHeight &&
					*std::min_element(asset->second.height.begin(),asset->second.height.end())==0)placed.surface=Surface::Wall;
			}
			else if (role == RemasterTileRole::Obstacle) placed.surface = Surface::Obstacle;
			else if (role == RemasterTileRole::Floor) placed.surface = Surface::Floor;
			else if (role == RemasterTileRole::StairTop || role == RemasterTileRole::StairBottom) placed.surface = Surface::Stair;
			// A stale floor/wall group must
			// not promote the new role into a solved structural classification.
			else if (role == RemasterTileRole::Door) placed.surface = Surface::Door;
			if (role == RemasterTileRole::Door) {
				placed.hasDoorframeMask=asset->second.hasDoorframeMask;
				placed.doorframeMask=asset->second.doorframeMask;
			}
			placed.diagnosticAuthoredFloor=role==RemasterTileRole::Floor;
		}
		if (asset != profile.assets.end() && asset->second.hasHeight)
		{ placed.hasHeight = true; placed.height = asset->second.height; }
		// Supporting axis evidence only; this cannot classify a wall. Reviewed
		// east/south references descend TOWARD +X/+Y face normals: uphill is
		// opposite. Require consistent significant XY on every non-cap sample;
		// defaults, rounded sign changes and relief roughness remain unknown.
		if(asset!=profile.assets.end() && asset->second.hasNormals) {
			int sx=0,sy=0,samples=0;bool consistent=true;
			for(int pixel=0;pixel<64;pixel++) {
				const auto *n=asset->second.normalXyz.data()+pixel*3;
				if(n[0]==128 && n[1]==128 && n[2]==255)continue;
				const int x=int(n[0])-128,y=int(n[1])-128;
				const int nx=std::abs(x)>=16?(x>0?-1:1):0,ny=std::abs(y)>=16?(y>0?-1:1):0;
				if(!nx && !ny){consistent=false;continue;}
				if(samples && (sx!=nx || sy!=ny))consistent=false;
				sx=nx;sy=ny;samples++;
			}
			static const int dx[]={0,0,-1,1,-1,1,-1,1},dy[]={-1,1,0,0,-1,-1,1,1};
			if(consistent && samples>=8)for(int k=0;k<8;k++)if(dx[k]==sx && dy[k]==sy)placed.diagnosticNormalUphill=1<<k;
		}
		if (placed.surface == Surface::Wall && placed.hasHeight && asset->second.hasNormals &&
			*std::max_element(placed.height.begin(),placed.height.end()) > 0)
			for (int pixel=0; pixel<64; pixel++)
				if (placed.height[pixel] == 0 && asset->second.normalXyz[pixel*3] == 128 &&
					asset->second.normalXyz[pixel*3+1] == 128 && asset->second.normalXyz[pixel*3+2] == 255)
					placed.groundedFloorMask |= UINT64_C(1) << pixel;
		// A wall-top group is not permission to turn a ramp or relief into a
		// plateau. Only authored flat regions participate in flat continuation.
		if (placed.surface == Surface::Floor && placed.hasHeight &&
			!std::all_of(placed.height.begin(), placed.height.end(), [&] (uint8_t h) { return h == placed.height[0]; }))
			placed.surface = Surface::Unknown;
		placed.hflip = (word & 0x4000) != 0; placed.vflip = (word & 0x8000) != 0;
		// The dungeon floor pattern is also painted outside playable space.
		// A solid usage on a completed plane is not a supporting plateau:
		// promoting its generated zero relief to an isolated-flat datum can
		// ground adjacent wall caps without support evidence.
		// Keep explicit wall-top assemblies separate; collision excludes this
		// pattern usage from support, it does not classify it as a wall.
		const auto pattern = profile.assetGroups.find("dungeon_floor");
		const auto tops = profile.assetGroups.find("wall_tops");
		if (placed.surface == Surface::Floor && context.collisionAttributes &&
			pattern != profile.assetGroups.end() &&
			std::find(pattern->second.tileIds.begin(), pattern->second.tileIds.end(), id) != pattern->second.tileIds.end() &&
			(tops == profile.assetGroups.end() || std::find(tops->second.tileIds.begin(), tops->second.tileIds.end(), id) == tops->second.tileIds.end()))
			for (int p = 0; p < 2; p++)
				if (context.collisionAttributes[p*4096+i] == 1 || context.collisionAttributes[p*4096+i] == 2)
					placed.structuralFloorPlaneMask &= ~(1 << p);
		if(placed.surface==Surface::Floor && placed.hasHeight &&
			std::all_of(placed.height.begin(),placed.height.end(),[](uint8_t h){return h==0;})) placed.floorPlaneMask|=1;
		const uint16_t lowerWord=context.constructedMaps[8192+i*2]|context.constructedMaps[8193+i*2]<<8;
		const uint16_t lowerAddress=static_cast<uint16_t>((context.bg1GraphicsBase==0xffff?context.graphicsBase:context.bg1GraphicsBase)+(lowerWord&1023)*32);
		if(!hashes.count(lowerAddress)) {
			uint8_t pixels[64];
			for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
				int index=0;for(int b=0;b<4;b++)index|=((context.graphics[static_cast<uint16_t>(lowerAddress+(b/2)*16+y*2+b%2)]>>(7-x))&1)<<b;
				pixels[y*8+x]=index;
			}
			hashes[lowerAddress]={S9xRemasterHashTile(4,pixels),1,4};
		}
		const auto lowerId=hashes[lowerAddress];const auto lowerAsset=profile.assets.find(lowerId);
		if(surfaces.count(lowerId) && surfaces[lowerId]==Surface::Floor && lowerAsset!=profile.assets.end() &&
			lowerAsset->second.hasHeight && std::all_of(lowerAsset->second.height.begin(),lowerAsset->second.height.end(),[](uint8_t h){return h==0;}))
			placed.floorPlaneMask|=2;
		placed.distinctLowerUsage=(lowerWord&0xc3ff)!=(word&0xc3ff);
	}
	// Statue handlers subtype0 38 and subtype2 1d/21/26 emit 2x3;
	// freestanding column handlers subtype0 39/3d emit 2x4. Do not include
	// the distinct wall-column dispatches. Final words prove usage, not shape.
	if(context.verifiedAlttpRom && context.dungeonTemplateData) {
		auto owners=std::make_unique<std::array<int,4096>>();owners->fill(-1);
		for(int off:{0xe26,0xed2,0xf5e,0x84a,0x85a})for(int flip=0;flip<4;flip++) {
		const int h=off==0x84a || off==0x85a?4:3;
		for(int y=0;y<=64-h;y++)for(int x=0;x<=62;x++) {
			bool exact=true,varied=false;int first=-1;
			for(int xx=0;exact && xx<2;xx++)for(int yy=0;exact && yy<h;yy++) {
				const int sx=flip&1?1-xx:xx,sy=flip&2?h-1-yy:yy;
				const auto *src=context.dungeonTemplateData+off+(sx*h+sy)*2;
				const int expected=(src[0]|src[1]<<8)^((flip&1?0x4000:0)|(flip&2?0x8000:0));
				const int i=(y+yy)*64+x+xx,word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
				const auto &t=output[i];
				if(first<0)first=expected;else varied|=first!=expected;
				if((word&0xdfff)!=(expected&0xdfff) || !t.hasHeight ||
					(t.surface!=Surface::Unknown && t.surface!=Surface::Obstacle) || t.diagnosticAuthoredFloor)exact=false;
				if(exact) {
					const auto a=profile.assets.find({t.tileHash,1,4});
					if(a!=profile.assets.end() && a->second.tileRole!=RemasterTileRole::Unknown &&
						a->second.tileRole!=RemasterTileRole::Obstacle)exact=false;
				}
			}
			if(!exact || !varied)continue;
			const int owner=(y*64+x)*8+h;
			for(int xx=0;xx<2;xx++)for(int yy=0;yy<h;yy++) {
				auto &o=(*owners)[y*64+x+yy*64+xx];if(o==-1)o=owner;else if(o!=owner)o=-2;
			}
		}
		}
		for(int i=0;i<4096;i++)if((*owners)[i]>=0) {
			const int owner=(*owners)[i],origin=owner/8,h=owner%8;bool unique=true;
			for(int xx=0;xx<2;xx++)for(int yy=0;yy<h;yy++)unique&=(*owners)[origin+yy*64+xx]==owner;
			if(unique)output[i].surface=Surface::Obstacle;
		}
	}
	// Pot handlers use the same two reusable ROM templates (light/dark). The
	// complete indexed 70..7f footprint distinguishes liftables from identical
	// fake/decorative art. RoomDraw_SinglePot installs the fixed 0d0e/0d1e floor
	// replacement. This support evidence does not manufacture local metadata.
	if(context.verifiedAlttpRom && context.dungeonTemplateData && context.collisionAttributes && context.placementBackgroundIndex==1) {
		// This is the handler's persistent FLOOR support, not replacement asset
		// relief. RoomDraw_16x16Single restores the explicit four floor words.
		// No height array is manufactured for those potentially unseen assets.
		for(int off:{0xe82,0xe92})for(int flip=0;flip<4;flip++)for(int y=0;y<63;y++)for(int x=0;x<63;x++) {
			bool exact=true;
			for(int xx=0;exact && xx<2;xx++)for(int yy=0;exact && yy<2;yy++) {
				const int sx=flip&1?1-xx:xx,sy=flip&2?1-yy:yy,n=off+2*(sx*2+sy),i=(y+yy)*64+x+xx;
				const int expected=(context.dungeonTemplateData[n]|context.dungeonTemplateData[n+1]<<8)^((flip&1?0x4000:0)|(flip&2?0x8000:0));
				const auto &t=output[i];exact &= (context.constructedMaps[2*i]|context.constructedMaps[2*i+1]<<8)==expected && t.hasHeight &&
					(t.surface==Surface::Unknown || t.surface==Surface::Obstacle) && !t.diagnosticAuthoredFloor;
			}
			if(!exact)continue;
			for(int p=0;p<2;p++) {
				const int attr=context.collisionAttributes[p*4096+y*64+x];bool indexed=attr>=0x70 && attr<=0x7f;
				for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++)indexed &= context.collisionAttributes[p*4096+(y+yy)*64+x+xx]==attr;
				if(indexed)for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
					auto &t=output[(y+yy)*64+x+xx];t.surface=Surface::Obstacle;
					t.removableSupportPlanes|=1<<p;
				}
			}
		}
	}
	// Ordinary door dispatch writes twelve column-major template words.
	// Match complete final identity/flips (priority-only writes are harmless).
	// These are ROM source-template offsets, not asset/room elevation lists.
	// Split/lower/copy operations are deliberately not accepted here.
	if(context.verifiedAlttpRom && context.dungeonTemplateData) {
		static const uint16_t starts[]={0x2716,0x29f0,0x2c6a,0x2e6a};
		static const uint16_t ends[]={0x2926,0x2be8,0x2e32,0x3032};
		for(int d=0;d<4;d++) for(int off=starts[d];off<=ends[d];off+=24) for(int split=0;split<2;split++) {
			if(split && context.bg1GraphicsBase!=0xffff && context.bg1GraphicsBase!=context.graphicsBase)continue;
			const int w=d<2?4:3,h=d<2?3:4;
			for(int y=0;y<=64-h;y++)for(int x=0;x<=64-w;x++) {
				bool exact=true,frame=false;
				for(int xx=0;exact && xx<w;xx++)for(int yy=0;exact && yy<h;yy++) {
					const int i=(y+yy)*64+x+xx,n=off+2*(xx*h+yy);
					const int expected=context.dungeonTemplateData[n]|context.dungeonTemplateData[n+1]<<8;
					// High-range dispatch retains the outer band on BG2 and
					// writes the two inner bands on BG1. Remapped types select
					// the actual source template; no input type/hash whitelist.
					const int bg=split ? ((d==0?yy==0:d==1?yy==h-1:d==2?xx==0:xx==w-1)?1:0) : context.placementBackgroundIndex;
					const int offset=bg==context.placementBackgroundIndex?0:8192;
					const int word=context.constructedMaps[offset+2*i]|context.constructedMaps[offset+2*i+1]<<8;
					exact=(word&0xdfff)==(expected&0xdfff);
					frame|=output[i].surface==Surface::Door && output[i].hasDoorframeMask && output[i].doorframeMask;
				}
				// Ordinary axis thresholds prove a real doorway rather than the
				// closed-wall template (which can match many sliding origins).
				bool threshold=false;
				if(context.collisionAttributes)for(int xx=0;xx<w;xx++)for(int yy=0;yy<h;yy++) {
					const int a=context.collisionAttributes[(y+yy)*64+x+xx];
					threshold |= a>=0x80 && a<=0x8f && (a&1)==(d>=2);
				}
				if(!exact || (!frame && !threshold))continue;
				for(int xx=0;xx<w;xx++)for(int yy=0;yy<h;yy++) {
					const int bg=split ? ((d==0?yy==0:d==1?yy==h-1:d==2?xx==0:xx==w-1)?1:0) : context.placementBackgroundIndex;
					if(bg!=context.placementBackgroundIndex)continue;
					auto &t=output[(y+yy)*64+x+xx];
					// The full outer jamb/lintel band has no opening pixels. Its
					// assembly part coordinate and complete authored unit ramp prove
					// a region even when legacy asset role/mask activation is absent.
					// Interior mixed parts still require their user-authored masks.
					const bool framePart=d<2?(xx==0 || xx==w-1 || (d==0?yy==0:yy==h-1)):
						(yy==0 || yy==h-1 || (d==2?xx==0:xx==w-1));
					if(threshold && framePart && t.hasHeight && !t.hasDoorframeMask &&
						(t.surface==Surface::Unknown || t.surface==Surface::Door)) {
						bool ramp=true;uint64_t mask=UINT64_MAX;const auto asset=profile.assets.find({t.tileHash,1,4});
						if(t.diagnosticAuthoredFloor || (asset!=profile.assets.end() && asset->second.tileRole!=RemasterTileRole::Unknown && asset->second.tileRole!=RemasterTileRole::Door))ramp=false;
						// Exact transparent/excluded pixels are NOT frame geometry.
						// A mixed opaque part still needs its authored frame mask.
						if(asset!=profile.assets.end() && asset->second.hasOcclusion) {
							mask=0;for(int k=0;k<64;k++) {
								const int o=asset->second.occlusion[k];if(o==255)mask|=UINT64_C(1)<<k;
								else if(o!=0 || t.height[k]!=0)ramp=false;
							}
						}
						int minimum=255;for(int k=0;k<64;k++)if(mask&(UINT64_C(1)<<k))minimum=std::min(minimum,int(t.height[k]));
						for(int py=0;py<8;py++)for(int px=0;px<8;px++)
							if(mask&(UINT64_C(1)<<(py*8+px)))ramp &= t.height[py*8+px]==minimum+7-(d<2?py:px);
						if(ramp && mask){t.surface=Surface::Door;t.hasDoorframeMask=true;t.doorframeMask=mask;}
					}
					if(t.surface!=Surface::Door)continue;
					if(t.doorTemplateOffset && (t.doorAssembly!=y*64+x+1 || t.doorDirection!=d)) {t.doorDirection=0xfe;continue;}
					t.doorTemplateOffset=off;t.doorAssembly=y*64+x+1;
					t.splitDoorPart=split!=0;
					if(t.doorDirection!=0xfe)t.doorDirection=d;
				}
			}
		}
	}
	// Handler01/02 (north/south) and61/62 (west/east) emit 2x4/4x2
	// perimeter templates from the ROM predefined table. An intact exact
	// final-word face footprint is placement evidence, not a hash-wide wall rule.
	// Require an existing wall seed and authored relief on every member;
	// explicit floor/obstacle/stair roles or overwritten words reject it.
	if (context.verifiedAlttpRom && context.dungeonTemplateData)
	{
		auto authoredWallSeeds=std::make_unique<std::array<bool,4096>>();
		for(int i=0;i<4096;i++)(*authoredWallSeeds)[i]=output[i].surface==Surface::Wall;
		// Footprint probes only read inputs and accumulate acceptance evidence.
		// Once rejected, remaining members/seed votes cannot affect publication.
		// Stop the probe, but preserve candidate order and all accepted writes.
		for (int kind=0; kind<4; kind++)
		{
			const int offsets[]={0x2e8,0x2f8,0x2c8,0x2d8};
			const int w=kind<2?2:4, h=kind<2?4:2;
			// The fourth row/column is floor trim routinely overwritten by doors
			// and objects. Recognize only the three surviving face rows/columns;
			// never classify or propagate through the overwritten trim.
			const int faceW=kind<2?2:3, faceH=kind<2?3:2;
			const int startX=kind==3?1:0, startY=kind==1?1:0;
			for(int y=0;y<=64-h;y++) for(int x=0;x<=64-w;x++)
			{
				bool exact=true, seed=false;
				for(int yy=startY;exact && yy<startY+faceH;yy++) for(int xx=startX;exact && xx<startX+faceW;xx++)
				{
					const int i=(y+yy)*64+x+xx, n=kind<2?xx*4+yy:yy*4+xx;
					const auto *src=context.dungeonTemplateData+offsets[kind]+n*2;
					const int expected=src[0]|src[1]<<8;
					const int word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
					const auto &t=output[i];
					if((word&0xdfff)!=(expected&0xdfff) || !t.hasHeight || t.diagnosticAuthoredFloor ||
						(t.surface!=Surface::Wall && t.surface!=Surface::Unknown))exact=false;
					seed|=t.surface==Surface::Wall;
				}
				// A complete surviving face may have no internally classified
				// asset (east/west upper bands). An immediately adjoining classified
				// wall foot can witness ownership, but only with all eight authored
				// seam samples compatible and the full ordered ROM face intact.
				// Priority-only door writes preserve geometry; tile/flips do not.
				if(exact && !seed && kind>=2) {
					const int edgeX=x+(kind==2?faceW:0), faceX=x+startX+(kind==2?faceW-1:0);
					if(edgeX>=0 && edgeX<64)for(int yy=0;yy<faceH;yy++) {
						const auto &a=output[(y+yy)*64+edgeX],&b=output[(y+yy)*64+faceX];
						if(!(*authoredWallSeeds)[(y+yy)*64+edgeX] || !a.hasHeight)continue;
						int delta=INT_MIN;bool compatible=true;
						for(int k=0;k<8;k++) {
							auto h=[](const RemasterDungeonFloorContext::Placement &t,int xx,int yy){return int(t.height[(t.vflip?7-yy:yy)*8+(t.hflip?7-xx:xx)]);};
							const int ax=kind==2?0:7,bx=kind==2?7:0;
							const int slope=h(b,bx,k)-h(b,7-bx,k);
							if(slope>=0 || h(a,ax,k)-h(a,7-ax,k)<=0)compatible=false;
							const int v=h(a,ax,k)-h(b,bx,k);
							if(delta!=INT_MIN && delta!=v)compatible=false;delta=v;
						}
						seed|=compatible;
					}
				}
				if(exact && seed) for(int yy=startY;yy<startY+faceH;yy++) for(int xx=startX;xx<startX+faceW;xx++)
				{
					auto &t=output[(y+yy)*64+x+xx];t.surface=Surface::Wall;
					t.wallTemplateOffset=offsets[kind];
					t.highPerimeterFace=true;
				}
			}
		}
		// Decorated end columns can overwrite the companion column without
		// Lower east doors split their 3x4 template across BGs: two columns on
		// BG1, one on BG2, followed by priority-only writes on the original high
		// perimeter. Require the exact split footprint and intact high faces at
		// BOTH ends. Never promote the replaced door foot to an authored ramp.
		if(context.placementBackgroundIndex==1 &&
			(context.bg1GraphicsBase==0xffff || context.bg1GraphicsBase==context.graphicsBase))
		for(int offset:{0x2e82,0x2f72})for(int phase=0;phase<2;phase++)for(int y=1;y<59;y++)for(int x=2;x<61;x++) {
			bool exact=true;
			for(int col=0;exact && col<3;col++)for(int row=0;exact && row<4;row++) {
				const int i=(y+row)*64+x-2+col,bg=col==2?0:8192;
				const int word=context.constructedMaps[bg+i*2]|context.constructedMaps[bg+i*2+1]<<8;
				const auto *src=context.dungeonTemplateData+offset+(col*4+row)*2;
				if(word!=(src[0]|src[1]<<8))exact=false;
			}
			for(int row=-1;exact && row<=4;row++)for(int col=1;exact && col<=3;col++) {
				const int i=(y+row)*64+x+col,word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
				const auto *src=context.dungeonTemplateData+0x2d8+(((row+phase+2)%2)*4+col)*2;
				const auto &t=output[i];
				if((word&0xdfff)!=(int(src[0]|src[1]<<8)&0xdfff) || !t.hasHeight ||
					t.diagnosticAuthoredFloor || (t.surface!=Surface::Unknown && t.surface!=Surface::Wall))exact=false;
			}
			if(exact)for(int row=-1;row<=4;row++)for(int col=1;col<=3;col++) {
				auto &t=output[(y+row)*64+x+col];t.surface=Surface::Wall;t.wallTemplateOffset=0x2d8;t.highPerimeterFace=true;
			}
			if(exact)for(int row=0;row<4;row++) {
				auto &t=output[(y+row)*64+x];
				if(t.surface==Surface::Unknown && t.hasHeight && !t.diagnosticAuthoredFloor) {
					t.surface=Surface::Obstacle;t.lowerDoorWallPart=true;t.supportingTrim=true;
				}
			}
		}
		// Decorated end columns can overwrite the companion column without
		// changing this emitted face column. Admit only a bounded three-cell
		// sequence with BOTH classified wall endpoints and exact ROM words.
		for(int kind=0;kind<4;kind++) for(int column=0;column<2;column++)
		{
			const int offsets[]={0x2e8,0x2f8,0x2c8,0x2d8};
			const bool vertical=kind<2;const int start=(kind&1)?1:0;
			for(int y=0;y<64-(vertical?2:0);y++) for(int x=0;x<64-(vertical?0:2);x++)
			{
				bool exact=true;
				for(int k=0;exact && k<3;k++)
				{
					const int i=(y+(vertical?k:0))*64+x+(vertical?0:k);
					const int n=column*4+start+k;const auto *src=context.dungeonTemplateData+offsets[kind]+n*2;
					const int word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
					const auto &t=output[i];
					if(word!=(src[0]|src[1]<<8) || !t.hasHeight ||
						(k==1 ? t.surface!=Surface::Wall && t.surface!=Surface::Unknown : t.surface!=Surface::Wall))exact=false;
				}
				if(exact) for(int k=0;k<3;k++)
				{
					auto &t=output[(y+(vertical?k:0))*64+x+(vertical?0:k)];
					t.surface=Surface::Wall;t.wallTemplateOffset=offsets[kind];
				}
			}
		}
		// Subtype2 high/low outer corners emit ordered 4x4 templates. Match
		// their three-cell face strips even where later dispatches overwrite
		// the adjoining column. Both endpoints must already be walls; the
		// middle may be unclassified, never floor/obstacle/stair. This is the
		// same bounded provenance rule as straight perimeter end columns.
		// The first eight subtype2 corner templates are HIGH corner faces
		// (LoadType1ObjectSubtype2 00..07), not only straight perimeters.
		// Retain the same ordered three-word/end-seed ownership proof for their
		// surviving columns. Multiple distinct high-template claims are ambiguous.
		auto highCornerOwners=std::make_unique<std::array<uint16_t,4096>>();
		for(int offset=0xb66;offset<=0xd46;offset+=0x20)
			for(int column=0;column<4;column++) for(int start=0;start<=1;start++)
				for(int y=0;y<62;y++)for(int x=0;x<64;x++)
				{
					bool exact=true;
					for(int k=0;exact && k<3;k++){
						const auto *src=context.dungeonTemplateData+offset+(column*4+start+k)*2;
						const int i=(y+k)*64+x,word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
						const auto &t=output[i];
						if(word!=(src[0]|src[1]<<8) || !t.hasHeight ||
							(k==1 ? t.surface!=Surface::Wall && t.surface!=Surface::Unknown : t.surface!=Surface::Wall))exact=false;
					}
					if(exact)for(int k=0;k<3;k++){
						const int cell=(y+k)*64+x;
						auto &t=output[cell];t.surface=Surface::Wall;t.wallTemplateOffset=offset;
						if(offset<=0xbc6 || (offset>=0xc66 && offset<=0xcc6)) {
							auto &owner=(*highCornerOwners)[cell];
							if(!owner)owner=offset;else if(owner!=offset)owner=0xffff;
						}
					}
				}
		for(int i=0;i<4096;i++)if((*highCornerOwners)[i] && (*highCornerOwners)[i]!=0xffff &&
			output[i].wallTemplateOffset==(*highCornerOwners)[i])output[i].highCornerFace=true;
		// Ordered freestanding wall columns (3x4) retain their end rows as
		// well as their middle. A complete footprint plus original wall seeds
		// proves the unlabelled relief; a single repeated end word does not.
		for(int offset:{0x89a,0x8b2,0xa54,0xa6c,0xa84,0xa9c})
		for(int y=0;y<=60;y++)for(int x=0;x<=61;x++) {
			bool exact=true,seed=false;
			for(int xx=0;exact && xx<3;xx++)for(int yy=0;exact && yy<4;yy++) {
				const int i=(y+yy)*64+x+xx;const auto *src=context.dungeonTemplateData+offset+(xx*4+yy)*2;
				const auto &t=output[i];
				if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(src[0]|src[1]<<8) ||
					!t.hasHeight || t.diagnosticAuthoredFloor || (t.surface!=Surface::Unknown && t.surface!=Surface::Wall))exact=false;
				// Generated flat placeholders are not authored wall geometry. A
				// complete word footprint cannot supply the missing local ramp.
				if(t.surface==Surface::Unknown && std::all_of(t.height.begin(),t.height.end(),[&](uint8_t h){return h==t.height[0];}))exact=false;
				seed|=(*authoredWallSeeds)[i];
			}
			if(exact && seed)for(int xx=0;xx<3;xx++)for(int yy=0;yy<4;yy++) {
				auto &t=output[(y+yy)*64+x+xx];t.surface=Surface::Wall;t.wallTemplateOffset=offset;
			}
		}
		// Low corner terminals retain two ordered branches after interior
		// overlays. The complete original triangle must survive on this BG or
		// its same-graphics counterpart; BOTH displayed branches stay exact.
		for(int kind=0;kind<4;kind++)for(int y=0;y<=60;y++)for(int x=0;x<=60;x++) {
			const int offsets[]={0xbe6,0xc06,0xc26,0xc46};bool exact=true,original[2]={true,true};bool seed[2]={};
			for(int yy=0;exact && yy<3;yy++)for(int xx=0;exact && xx<4-yy;xx++) {
				const int tx=kind>=2?3-xx:xx,ty=kind%2?3-yy:yy,i=(y+ty)*64+x+tx;
				const auto *src=context.dungeonTemplateData+offsets[kind]+(tx*4+ty)*2;const int word=src[0]|src[1]<<8;
				for(int bg=0;bg<2;bg++)original[bg]&=(context.constructedMaps[bg*8192+i*2]|context.constructedMaps[bg*8192+i*2+1]<<8)==word;
				if(xx && yy)continue;
				const auto &t=output[i];
				if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=word || !t.hasHeight ||
					t.diagnosticAuthoredFloor || (t.surface!=Surface::Wall && t.surface!=Surface::Unknown))exact=false;
				if(t.surface==Surface::Unknown && std::all_of(t.height.begin(),t.height.end(),[&](uint8_t h){return h==t.height[0];}))exact=false;
				if(xx)seed[0]|=(*authoredWallSeeds)[i];if(yy)seed[1]|=(*authoredWallSeeds)[i];
			}
			if(!exact)continue;
			const int ci=(y+(kind%2?3:0))*64+x+(kind>=2?3:0);const auto &corner=output[ci];bool fold=corner.hasHeight;
			for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)fold&=corner.height[yy*8+xx]==std::max(7-xx,7-yy);
			if(!exact || !fold || !seed[0] || !seed[1] || !(original[0] ||
				(original[1] && context.placementBackgroundIndex==1 && (context.bg1GraphicsBase==0xffff || context.bg1GraphicsBase==context.graphicsBase))))continue;
			for(int yy=0;yy<3;yy++)for(int xx=0;xx<4-yy;xx++) {
				if(xx && yy)continue;
				const int tx=kind>=2?3-xx:xx,ty=kind%2?3-yy:yy;auto &t=output[(y+ty)*64+x+tx];
				t.surface=Surface::Wall;t.wallTemplateOffset=offsets[kind];t.terminalCorner=!xx && !yy;
			}
		}
		// A later BG cover can erase one inward branch entirely. The other
		// branch plus its perpendicular, outward high-perimeter face still
		// identifies both corner regions, provided the original triangle and
		// final terminal word survive in the same graphics context.
		if(context.placementBackgroundIndex==1 && (context.bg1GraphicsBase==0xffff || context.bg1GraphicsBase==context.graphicsBase))
		for(int kind=0;kind<4;kind++)for(int y=0;y<=60;y++)for(int x=0;x<=60;x++) {
			const int offsets[]={0xbe6,0xc06,0xc26,0xc46};bool original=true;
			for(int yy=0;original && yy<3;yy++)for(int xx=0;original && xx<4-yy;xx++) {
				const int tx=kind>=2?3-xx:xx,ty=kind%2?3-yy:yy,i=(y+ty)*64+x+tx;
				const auto *src=context.dungeonTemplateData+offsets[kind]+(tx*4+ty)*2;
				original&=(context.constructedMaps[8192+i*2]|context.constructedMaps[8193+i*2]<<8)==(src[0]|src[1]<<8);
			}
			if(!original)continue;
			const int cx=x+(kind>=2?3:0),cy=y+(kind%2?3:0),ci=cy*64+cx;auto &corner=output[ci];
			const auto *src=context.dungeonTemplateData+offsets[kind]+((cx-x)*4+cy-y)*2;
			bool fold=corner.hasHeight;
			for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)fold&=corner.height[yy*8+xx]==std::max(7-xx,7-yy);
			if(!original || !fold || corner.diagnosticAuthoredFloor || (corner.surface!=Surface::Wall && corner.surface!=Surface::Unknown) ||
				(context.constructedMaps[ci*2]|context.constructedMaps[ci*2+1]<<8)!=(src[0]|src[1]<<8))continue;
			for(int axis=0;axis<2;axis++) {
				const int dx=axis?0:(kind>=2?-1:1),dy=axis?(kind%2?-1:1):0;
				bool branch=true,seed=false;
				for(int k=1;k<=2;k++) {
					const int px=cx+dx*k,py=cy+dy*k,i=py*64+px;const auto &t=output[i];
					const auto *s=context.dungeonTemplateData+offsets[kind]+((px-x)*4+py-y)*2;
					if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(s[0]|s[1]<<8) || !t.hasHeight ||
						t.diagnosticAuthoredFloor || (t.surface!=Surface::Wall && t.surface!=Surface::Unknown))branch=false;
					if(t.surface==Surface::Unknown && std::all_of(t.height.begin(),t.height.end(),[&](uint8_t h){return h==t.height[0];}))branch=false;
					seed|=(*authoredWallSeeds)[i];
				}
				const int hx=cx-dx,hy=cy-dy;if(hx<0 || hx>=64 || hy<0 || hy>=64)continue;
				const auto &high=output[hy*64+hx];
				if(!branch || !seed || high.surface!=Surface::Wall || !high.hasHeight ||
					(!(*authoredWallSeeds)[hy*64+hx] && !high.wallTemplateOffset))continue;
				// High face complementary to the corner's missing perpendicular
				// region: exact full seam and compatible downhill slope required.
				auto sample=[](const RemasterDungeonFloorContext::Placement &t,int xx,int yy){return int(t.height[(t.vflip?7-yy:yy)*8+(t.hflip?7-xx:xx)]);};
				bool seam=true;int delta=INT_MIN;
				for(int k=0;k<8;k++) {
					const int ex=dx?(dx>0?0:7):k,ey=dy?(dy>0?0:7):k;
					const int nx=dx?(dx>0?7:0):k,ny=dy?(dy>0?7:0):k;
					const int v=sample(high,nx,ny)-sample(corner,ex,ey);
					if(delta!=INT_MIN && delta!=v)seam=false;delta=v;
					if(sample(high,nx,ny)>=sample(high,dx?7-nx:k,dy?7-ny:k))seam=false;
				}
				if(!seam)continue;
				corner.surface=Surface::Wall;corner.wallTemplateOffset=offsets[kind];corner.terminalCorner=true;
				for(int k=1;k<=2;k++){auto &t=output[(cy+dy*k)*64+cx+dx*k];t.surface=Surface::Wall;t.wallTemplateOffset=offsets[kind];}
			}
		}
		// Lit/unlit floor torch: handler emits the entire 2x2 column-major
		// template. Its cap is relief on the same support as the authored
		// obstacle foot, not a walkable lower-plane pixel behind the overlay.
		for(int offset:{0xec2,0xeca})for(int y=0;y<63;y++)for(int x=0;x<63;x++) {
			bool exact=true,seed=false;
			for(int xx=0;exact && xx<2;xx++)for(int yy=0;exact && yy<2;yy++) {
				const int i=(y+yy)*64+x+xx;const auto *src=context.dungeonTemplateData+offset+(xx*2+yy)*2;
				const auto &t=output[i];
				if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(src[0]|src[1]<<8) ||
					!t.hasHeight || t.diagnosticAuthoredFloor || (t.surface!=Surface::Unknown && t.surface!=Surface::Obstacle))exact=false;
				seed|=t.surface==Surface::Obstacle;
			}
			if(exact && seed)for(int xx=0;xx<2;xx++)for(int yy=0;yy<2;yy++)output[(y+yy)*64+x+xx].surface=Surface::Obstacle;
		}
		// Handler2f is a rail wall, NOT the similarly shaped handler47
		// Rail start/end corner folds: full ordered face AND trim run owns
		// the horizontal region; two exact perpendicular low-wall words own
		// the vertical region. Existing authored trim Obstacles are allowed,
		// never converted to walls or used as shell-height anchors.
		for(int kind=0;kind<2;kind++) {
			const int off=kind?0x694:0x688;auto word=[&](int n){const auto *s=context.dungeonTemplateData+off+n*2;return int(s[0]|s[1]<<8);};
			for(int y=2;y<62;y++)for(int x=0;x<51;x++)for(int width=14;width<=29 && x+width<=64;width++) {
				bool exact=true;
				for(int k=0;exact && k<width;k++) {
					const int i=y*64+x+k,trim=i+(kind?-64:64),n=k<2?k+1:k>=width-2?k-width+6:3;
					const auto &t=output[i],&tr=output[trim];
					if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=word(n) || !t.hasHeight ||
						t.diagnosticAuthoredFloor || (t.surface!=Surface::Unknown && t.surface!=Surface::Wall) ||
						(context.constructedMaps[trim*2]|context.constructedMaps[trim*2+1]<<8)!=word(0) ||
						tr.surface!=Surface::Obstacle || !tr.hasHeight)exact=false;
				}
				if(!exact)continue;
				for(int end=0;end<2;end++) {
					const int cx=x+(end?width-1:0),ci=y*64+cx,dy=kind?1:-1;bool branch=true,seed=false;
					for(int k=1;k<=2;k++) {
						const int i=(y+dy*k)*64+cx;const auto &t=output[i];
						// The emitted low-corner branch words, not art similarity.
						const int co=0xbe6+(end?0x40:0)+(kind?0:0x20),tx=end?3:0,ty=kind?k:3-k;
						const auto *s=context.dungeonTemplateData+co+(tx*4+ty)*2;
						if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(s[0]|s[1]<<8) || !t.hasHeight ||
							t.diagnosticAuthoredFloor || (t.surface!=Surface::Wall && t.surface!=Surface::Unknown))branch=false;
						if(t.surface==Surface::Unknown && std::all_of(t.height.begin(),t.height.end(),[&](uint8_t h){return h==t.height[0];}))branch=false;
						seed|=(*authoredWallSeeds)[i];
					}
					auto &ct=output[ci];bool fold=ct.hasHeight;
					for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)fold&=ct.height[yy*8+xx]==std::max(7-xx,7-yy);
					if(!branch || !seed || !fold)continue;
					ct.surface=Surface::Wall;ct.wallTemplateOffset=off;ct.terminalCorner=true;
					output[ci+(end?-1:1)].wallTemplateOffset=off;
					for(int k=1;k<=2;k++){auto &t=output[(y+dy*k)*64+cx];t.surface=Surface::Wall;t.wallTemplateOffset=off;}
				}
			}
		}
		// Handler2f is a rail wall, NOT the similarly shaped handler47
		// waterfall. Require the entire ordered start/middle/end run AND its
		// support trim row. Only the trim may attach to ordinary support; the
		// rail face retains its authored ramp. Nonblocking copies remain occluders.
		for(int railKind=0;railKind<2;railKind++){
		const int railOffset=railKind?0x694:0x688;
		const auto *rail=context.dungeonTemplateData+railOffset;
		auto railWord=[&](int n){return rail[n*2]|rail[n*2+1]<<8;};
		for(int y=0;y<63;y++)for(int x=0;x<61;x++)for(int middle=10;middle<=25 && x+middle+4<=64;middle++)
		{
			bool exact=true;const int width=middle+4;
			for(int k=0;exact && k<width;k++)
			{
				const int i=y*64+x+k,n=k<2?k+1:k>=width-2?k-(width-2)+4:3;
				const int faceCell=i+(railKind?64:0),trimCell=i+(railKind?0:64);
				const auto &face=output[faceCell],&trim=output[trimCell];
				const int faceWord=context.constructedMaps[faceCell*2]|context.constructedMaps[faceCell*2+1]<<8;
				const int trimWord=context.constructedMaps[trimCell*2]|context.constructedMaps[trimCell*2+1]<<8;
				if(faceWord!=railWord(n) || trimWord!=railWord(0) || !face.hasHeight || face.surface!=Surface::Wall ||
					!trim.hasHeight || trim.surface!=Surface::Unknown)exact=false;
			}
			if(exact)for(int k=0;k<width;k++)
			{
				auto &t=output[(y+(railKind?0:1))*64+x+k];t.surface=Surface::Obstacle;t.supportingTrim=true;t.wallTemplateOffset=railOffset;
			}
		}
		}
		// Diagnostic-only final-word ownership. Unlike the height solver this
		// does not require a local array, a solved seam, or a promoted Wall role.
		// The layouts below are emitted by the pinned USA dungeon handlers;
		// never recognize a single matching art word/hash as an assembly.
		auto diagnosticMember=[](const RemasterDungeonFloorContext::Placement &t) {
			return !t.diagnosticAuthoredFloor && !t.diagnosticRomFloorTrim && t.surface!=Surface::Obstacle && t.surface!=Surface::Stair &&
				!(t.surface==Surface::Floor && t.hasHeight &&
				std::all_of(t.height.begin(),t.height.end(),[](uint8_t h){return h==0;}));
		};
		// Subtype0 34/71 paints carpet FLOOR trim from src6c8 in bounded
		// horizontal/vertical runs (minimum four). Keep its own movement
		// component, never infer support from a zero array or neighboring BG.
		const int trimWord=context.dungeonTemplateData[0x6c8]|context.dungeonTemplateData[0x6c9]<<8;
		if(trimWord)for(int vertical=0;vertical<2;vertical++)for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
			int length=0;
			while(x+(vertical?0:length)<64 && y+(vertical?length:0)<64) {
				const int i=(y+(vertical?length:0))*64+x+(vertical?0:length);
				if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=trimWord)break;
				length++;
			}
			if(length>=4)for(int n=0;n<length;n++) {
				auto &t=output[(y+(vertical?n:0))*64+x+(vertical?0:n)];
				if(t.surface==Surface::Unknown)t.diagnosticRomFloorTrim=true;
			}
		}
		auto diagnosticFootprint=[&](int offset,int w,int h,int axis) {
			bool varied=false;
			for(int n=1;n<w*h;n++)if(context.dungeonTemplateData[offset+n*2]!=context.dungeonTemplateData[offset] ||
				context.dungeonTemplateData[offset+n*2+1]!=context.dungeonTemplateData[offset+1])varied=true;
			if(!varied)return; // uniform/default data cannot prove an ordered assembly
			for(int y=0;y<=64-h;y++)for(int x=0;x<=64-w;x++) {
				bool exact=true;
				for(int yy=0;exact && yy<h;yy++)for(int xx=0;exact && xx<w;xx++) {
					const int i=(y+yy)*64+x+xx;const auto *src=context.dungeonTemplateData+offset+(xx*h+yy)*2;
					const int word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
					if(word!=(src[0]|src[1]<<8))exact=false;
				}
				if(exact)for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++) {
					auto &t=output[(y+yy)*64+x+xx];
					if(!diagnosticMember(t))continue;
					t.diagnosticWallTemplateOffset=offset;t.diagnosticUphillDirections=axis;
				}
			}
		};
		// Straight perimeter, columns and wall decorations (column-major).
		for(int offset:{0x2e8,0x2f8,0x328,0x338,0x400,0x410})
			diagnosticFootprint(offset,2,4,offset==0x2f8 || offset==0x338 || offset==0x410?2:1);
		// West/east perimeter is row-major in the ROM: transpose the recognizer
		// by using its emitted 4x2 layout explicitly.
		for(int offset:{0x2c8,0x2d8,0x308,0x318})for(int y=0;y<63;y++)for(int x=0;x<61;x++) {
			bool exact=true;
			for(int yy=0;exact && yy<2;yy++)for(int xx=0;exact && xx<4;xx++) {
				const int i=(y+yy)*64+x+xx;const auto *src=context.dungeonTemplateData+offset+(yy*4+xx)*2;
				if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(src[0]|src[1]<<8))exact=false;
			}
			if(exact)for(int yy=0;yy<2;yy++)for(int xx=0;xx<4;xx++) {
				auto &t=output[(y+yy)*64+x+xx];if(!diagnosticMember(t))continue;
				t.diagnosticWallTemplateOffset=offset;t.diagnosticUphillDirections=offset==0x2c8 || offset==0x308?4:8;
			}
		}
		for(int offset:{0x89a,0x8b2,0xa54,0xa6c,0xa84,0xa9c})diagnosticFootprint(offset,3,4,0);
		for(int offset:{0x86a,0x882})diagnosticFootprint(offset,4,3,0);
		for(int offset=0xb66;offset<=0xd46;offset+=0x20)diagnosticFootprint(offset,4,4,0);
		// Low outer corners can be overwritten by an inner corner on the
		// opposite side. Require the entire surviving ordered 4+3+2 triangle,
		// not a corner hash or a guessed topology. Only those nine words gain
		// ownership; authored derivatives still supply BOTH corner facings.
		for(int kind=0;kind<4;kind++)for(int y=0;y<61;y++)for(int x=0;x<61;x++) {
			const int offsets[]={0xbe6,0xc06,0xc26,0xc46};bool exact=true;
			for(int yy=0;exact && yy<3;yy++)for(int xx=0;exact && xx<4-yy;xx++) {
				const int tx=kind>=2?3-xx:xx,ty=kind%2?yy:3-yy;
				const int i=(y+ty)*64+x+tx,n=tx*4+ty;
				const auto *src=context.dungeonTemplateData+offsets[kind]+n*2;
				if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(src[0]|src[1]<<8))exact=false;
			}
			if(exact)for(int yy=0;yy<3;yy++)for(int xx=0;xx<4-yy;xx++) {
				const int tx=kind>=2?3-xx:xx,ty=kind%2?yy:3-yy;
				auto &t=output[(y+ty)*64+x+tx];
				if(diagnosticMember(t))t.diagnosticWallTemplateOffset=offsets[kind];
			}
		}
		// Perimeter face survives routine trim-row overwrites. Match its full
		// six-word face, exactly as the solver's provenance recognizer, but
		// without requiring a seed/height array or changing any solver role.
		for(int kind=0;kind<4;kind++) {
			const int offsets[]={0x2e8,0x2f8,0x2c8,0x2d8};
			const int w=kind<2?2:4,h=kind<2?4:2;
			const int fw=kind<2?2:3,fh=kind<2?3:2,sx=kind==3?1:0,sy=kind==1?1:0;
			for(int y=0;y<=64-h;y++)for(int x=0;x<=64-w;x++) {
				bool exact=true;
				for(int yy=sy;exact && yy<sy+fh;yy++)for(int xx=sx;exact && xx<sx+fw;xx++) {
					const int i=(y+yy)*64+x+xx,n=kind<2?xx*4+yy:yy*4+xx;
					const auto *src=context.dungeonTemplateData+offsets[kind]+n*2;
					if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(src[0]|src[1]<<8))exact=false;
				}
				if(exact)for(int yy=sy;yy<sy+fh;yy++)for(int xx=sx;xx<sx+fw;xx++) {
					auto &t=output[(y+yy)*64+x+xx];if(!diagnosticMember(t))continue;
					t.diagnosticWallTemplateOffset=offsets[kind];t.diagnosticUphillDirections=1<<kind;
				}
			}
		}
		// A decorated end overwrites one row of the two-row perimeter. Admit
		// one exact FOUR-word face+trim strip only beside an already owned
		// companion strip. Snapshot witnesses prohibit recursive growth.
		auto perimeterOwned=std::make_unique<std::array<uint16_t,4096>>();
		for(int i=0;i<4096;i++)(*perimeterOwned)[i]=output[i].diagnosticWallTemplateOffset;
		for(int kind=0;kind<4;kind++)for(int column=0;column<2;column++) {
			const int offsets[]={0x2e8,0x2f8,0x2c8,0x2d8};const bool vertical=kind<2;
			for(int y=0;y<64-(vertical?3:0);y++)for(int x=0;x<64-(vertical?0:3);x++) {
				bool exact=true,witness=false;
				for(int k=0;exact && k<4;k++) {
					const int i=(y+(vertical?k:0))*64+x+(vertical?0:k);
					const auto *src=context.dungeonTemplateData+offsets[kind]+(column*4+k)*2;
					if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(src[0]|src[1]<<8))exact=false;
				}
				if(!exact)continue;
				for(int sign:{-1,1}) {
					int owned=0;
					for(int k=0;k<4;k++) {
						const int xx=x+(vertical?sign:k),yy=y+(vertical?k:sign);
						if(xx>=0 && xx<64 && yy>=0 && yy<64 && (*perimeterOwned)[yy*64+xx]==offsets[kind])owned++;
					}
					witness|=owned>=3;
				}
				if(exact && witness)for(int k=(kind&1)?1:0;k<((kind&1)?4:3);k++) {
					auto &t=output[(y+(vertical?k:0))*64+x+(vertical?0:k)];
					if(diagnosticMember(t)){t.diagnosticWallTemplateOffset=offsets[kind];t.diagnosticUphillDirections=1<<kind;}
				}
			}
		}
		// Diagonal handlers09..20 emit five-cell columns stepped (+1,+/-1),
		// minimum six repetitions. Require three intact rows in two columns:
		// local ownership survives end overwrites without accepting isolated art.
		for(int kind=0;kind<24;kind++) {
			const int offset=0x420+kind*10,step=(kind%4==0 || kind%4==3)?-1:1;
			for(int y=1;y<59;y++)for(int x=0;x<63;x++) {
				bool exact=false;
				for(int start=0;!exact && start<3;start++) {
					bool strip=true;
					for(int col=0;strip && col<2;col++)for(int row=start;strip && row<start+3;row++) {
						const int i=(y+col*step+row)*64+x+col;const auto *src=context.dungeonTemplateData+offset+row*2;
						if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(src[0]|src[1]<<8))strip=false;
					}
					exact|=strip;
				}
				if(exact)for(int col=0;col<2;col++)for(int row=0;row<5;row++) {
					const int i=(y+col*step+row)*64+x+col;const auto *src=context.dungeonTemplateData+offset+row*2;
					if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(src[0]|src[1]<<8))continue;
					auto &t=output[i];
					if(!diagnosticMember(t))continue;
					t.diagnosticWallTemplateOffset=offset; // derivatives own axis, not template art
				}
			}
		}
		// Terminal diagonal columns: preserve cap members at the room boundary
		// when their complete five ordered words survive beside a previously
		// proven stepped column. Snapshot ownership prevents recursive growth.
		auto diagonalOwned=std::make_unique<std::array<uint16_t,4096>>();
		for(int i=0;i<4096;i++)(*diagonalOwned)[i]=output[i].diagnosticWallTemplateOffset;
		for(int kind=0;kind<24;kind++) {
			const int offset=0x420+kind*10,step=(kind%4==0 || kind%4==3)?-1:1;
			for(int y=0;y<60;y++)for(int x=0;x<64;x++) {
				bool exact=true,neighbor=false;
				for(int row=0;exact && row<5;row++) {
					const int i=(y+row)*64+x;const auto *src=context.dungeonTemplateData+offset+row*2;
					if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(src[0]|src[1]<<8))exact=false;
				}
				for(int sign:{-1,1}) {
					const int nx=x+sign,ny=y+sign*step;if(nx<0 || nx>=64 || ny<0 || ny>59)continue;
					int owned=0;for(int row=0;row<5;row++)if((*diagonalOwned)[(ny+row)*64+nx]==offset)owned++;
					neighbor|=owned>=3;
				}
				if(exact && neighbor)for(int row=0;row<5;row++) {
					auto &t=output[(y+row)*64+x];if(diagnosticMember(t))t.diagnosticWallTemplateOffset=offset;
				}
				// A cardinal wall commonly overwrites the leading end of a
				// reverse-order diagonal. Two surviving ordered cap words plus
				// the three-word proven companion identify that terminal suffix.
				for(int start:{0,3}) {
					bool suffix=true;
					for(int row=start;suffix && row<start+2;row++) {
						const int i=(y+row)*64+x;const auto *src=context.dungeonTemplateData+offset+row*2;
						if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(src[0]|src[1]<<8))suffix=false;
					}
					if(suffix && neighbor)for(int row=start;row<start+2;row++) {
						auto &t=output[(y+row)*64+x];if(diagnosticMember(t))t.diagnosticWallTemplateOffset=offset;
					}
				}
			}
		}
		// The final cap pixel column may retain just one word. Require two
		// stepped companions with the SAME emitted row word, at least one
		// companion owned by the original six-word diagonal proof. This is a
		// bounded terminal witness, not hash propagation through nearby art.
		for(int kind=0;kind<24;kind++) {
			const int offset=0x420+kind*10,step=(kind%4==0 || kind%4==3)?-1:1;
			for(int row:{0,4})for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
				const int i=y*64+x;const auto *src=context.dungeonTemplateData+offset+row*2;
				const int expected=src[0]|src[1]<<8;
				if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=expected)continue;
				for(int sign:{-1,1}) {
					bool witness=true,original=false;
					for(int n=1;n<=2;n++) {
						const int nx=x+sign*n,ny=y+sign*n*step;if(nx<0 || nx>=64 || ny<0 || ny>=64){witness=false;break;}
						const int j=ny*64+nx;
						if(output[j].diagnosticWallTemplateOffset!=offset ||
							(context.constructedMaps[j*2]|context.constructedMaps[j*2+1]<<8)!=expected)witness=false;
						original|=(*diagonalOwned)[j]==offset;
					}
					if(witness && original && diagnosticMember(output[i]))output[i].diagnosticWallTemplateOffset=offset;
				}
			}
		}
		for(auto &t:output)if(t.diagnosticWallTemplateOffset>=0x420 && t.diagnosticWallTemplateOffset<=0x506) {
			const int kind=(t.diagnosticWallTemplateOffset-0x420)/10;
			// Handler templates mirror the same wall footprint N/W, S/W,
			// N/E, S/E. This axis is placement ownership, not generated height.
			const int axes[]={16,64,32,128};t.diagnosticUphillDirections=axes[kind%4];
		}
		// Effective-geometry preview for high diagonal perimeter handlers. Derive
		// the cross section from BOTH authored cardinal perimeter templates,
		// never from a tile hash, room number, configured rise or nearby floor.
		// Three unit-slope eight-pixel bands must agree in canonical space.
		// The stepped ROM column owns the diagonal coordinate x+y; the fourth
		// band is its clipped cap, not another stair or door opening.
		int foot=-1;bool section=true;
		for(int axis=0;axis<2;axis++)for(int band=0;band<3;band++) {
			const int off=(axis?0x2c8:0x2e8)+band*2;
			const int word=context.dungeonTemplateData[off]|context.dungeonTemplateData[off+1]<<8;
			const auto id=hashes.find(uint16_t(context.graphicsBase+(word&1023)*32));
			if(id==hashes.end()){section=false;continue;}
			const auto asset=profile.assets.find(id->second);
			if(asset==profile.assets.end() || !asset->second.hasHeight){section=false;continue;}
			const auto &h=asset->second.height;
			const int f=int(h[63])-8*(2-band);
			if(foot<0)foot=f;else if(foot!=f)section=false;
			for(int y=0;y<8;y++)for(int x=0;x<8;x++)
				if(int(h[y*8+x])!=f+8*(2-band)+7-(axis?x:y))section=false;
		}
		if(section && foot>=0 && foot+24<255)for(int i=0;i<4096;i++) {
			auto &t=output[i];const int off=t.diagnosticWallTemplateOffset;
			if(off<0x420 || off>0x48e || !t.hasHeight || t.diagnosticAuthoredFloor ||
				(t.surface!=Surface::Unknown && t.surface!=Surface::Wall))continue;
			const int word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
			int row=-1;bool ambiguous=false;
			for(int k=0;k<5;k++)if(word==(context.dungeonTemplateData[off+k*2]|context.dungeonTemplateData[off+k*2+1]<<8)) {
				const int canonical=t.vflip?4-k:k;
				if(row>=0 && row!=canonical)ambiguous=true;row=canonical;
			}
			if(ambiguous || row<0 || row>3)continue; // floor trim is not shell
			for(int y=0;y<8;y++)for(int x=0;x<8;x++)
				t.height[y*8+x]=foot+std::max(0,std::min(24,31-row*8-x-y));
			t.surface=Surface::Wall;t.wallTemplateOffset=off;t.diagonalHeightPreview=true;
			t.diagonalPreviewRow=row;t.diagonalPreviewFoot=foot;
			t.diagonalIntercept=foot+31-row*8;t.diagonalMinimum=foot;t.diagonalMaximum=foot+24;
			t.groundedFloorMask=0; // no authored +Z mask was inferred from preview
		}
		// Complementary HIGH/LOW dispatches overwrite each other's trim/foot.
		// Their FINAL seven-word cross section is one face, not two independent
		// four-band caps. Prove two stepped columns before accepting any member;
		// never extend by hash, proximity, or floor flooding. Both independently
		// authored cardinal sections must establish the signed section datum.
		bool lowSection=true;int lowFoot=-1;
		for(int axis=0;axis<2;axis++)for(int band=0;band<3;band++) {
			const int off=(axis?0x308:0x330)+band*2;
			const int word=context.dungeonTemplateData[off]|context.dungeonTemplateData[off+1]<<8;
			const auto id=hashes.find(uint16_t(context.graphicsBase+(word&1023)*32));
			if(id==hashes.end()){lowSection=false;continue;}
			const auto asset=profile.assets.find(id->second);
			if(asset==profile.assets.end() || !asset->second.hasHeight){lowSection=false;continue;}
			const auto &h=asset->second.height;const int f=int(h[63])-8*(2-band);
			if(lowFoot<0)lowFoot=f;else if(lowFoot!=f)lowSection=false;
			for(int y=0;y<8;y++)for(int x=0;x<8;x++)
				if(int(h[y*8+x])!=f+8*(2-band)+7-(axis?x:y))lowSection=false;
		}
		// Cross-section displacement comes from the signed authored datum,
		// not a four-band ceiling. The adapter can supply only actual emitted
		// source slots; the geometry solver itself has no band-count limit.
		const int displacement=foot-lowFoot;
		if(section && lowSection && lowFoot>=0 && displacement>0 && displacement%8==0 &&
			foot+24<255) {
			const int shift=displacement/8,extent=4+shift;
			struct Claim { int intercept=0,minimum=0,maximum=0,offset=0;bool set=false,conflict=false; };
			auto claims=std::make_unique<std::array<Claim,4096>>();
			for(int kind=0;kind<12;kind++) {
				const int high=0x420+kind*10,low=high+120,step=(kind%4==0 || kind%4==3)?-1:1;
				const bool reverse=(context.dungeonTemplateData[high+1]&0x80)!=0;
				auto sourceFor=[&](int row){return reverse ? (row<4?low+(row+1)*2:high+(row-shift+1)*2) :
					(row<shift?high+row*2:low+(row-shift)*2);};
				bool unique=true;
				// Do not read an adjacent ROM template as a nonexistent face strip.
				// This is dispatch-footprint admissibility, not a geometry cap.
				for(int row=0;row<extent;row++) {
					const int source=sourceFor(row);
					if(!((source>=high && source<high+10) || (source>=low && source<low+10)))unique=false;
				}
				if(!unique)continue;
				for(int a=0;a<extent;a++)for(int b=0;b<a;b++) {
					const int sa=sourceFor(a),sb=sourceFor(b);
					if((context.dungeonTemplateData[sa]|context.dungeonTemplateData[sa+1]<<8)==
						(context.dungeonTemplateData[sb]|context.dungeonTemplateData[sb+1]<<8))unique=false;
				}
				if(!unique)continue; // repeated slots cannot prove band ownership
				for(int y=1;y<64-extent;y++)for(int x=0;x<63;x++) {
					bool exact=true;
					for(int col=0;exact && col<2;col++)for(int row=0;exact && row<extent;row++) {
						const int source=sourceFor(row);
						const int i=(y+col*step+row)*64+x+col;
						const int word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
						const auto &t=output[i];
						if((word&0xdfff)!=((context.dungeonTemplateData[source]|context.dungeonTemplateData[source+1]<<8)&0xdfff) ||
							!t.hasHeight || t.diagnosticAuthoredFloor ||
							(t.surface!=Surface::Unknown && t.surface!=Surface::Wall))exact=false;
					}
					if(!exact)continue;
					for(int col=0;col<2;col++)for(int row=0;row<extent;row++) {
						const int i=(y+col*step+row)*64+x+col,b=reverse?extent-1-row:row;
						auto &claim=(*claims)[i];const int intercept=lowFoot+31+displacement-b*8;
						if(claim.set && (claim.intercept!=intercept || claim.minimum!=lowFoot || claim.maximum!=foot+24))claim.conflict=true;
						claim.set=true;claim.intercept=intercept;claim.minimum=lowFoot;claim.maximum=foot+24;
						claim.offset=(reverse?row<4:row>=shift)?low:high;
					}
				}
			}
			// The geometry kernel accepts any evidence-backed extent. This live
			// adapter supplies bounded template witnesses, NOT synthetic extra
			// writers. Corpus final-writer attestation is independently audited.
			std::vector<RemasterDiagonalAssemblyMember> members;
			for(int i=0;i<4096;i++) {
				const auto &claim=(*claims)[i];if(!claim.set || claim.conflict)continue;
				members.push_back({i,claim.intercept,static_cast<uint16_t>(claim.offset),true,true,true,false});
			}
			std::vector<RemasterDiagonalAssemblyGeometry> geometry;
			if(S9xRemasterSolveDiagonalAssembly({lowFoot,foot+24,true,true,true,false},members,geometry))
			for(const auto &g:geometry) {
				auto &t=output[g.cell];t.surface=Surface::Wall;t.wallTemplateOffset=(*claims)[g.cell].offset;
				t.diagonalHeightPreview=true;t.diagonalIntercept=g.intercept;
				t.diagonalMinimum=g.minimum;t.diagonalMaximum=g.maximum;t.height=g.height;
			}
		}
		// Decorated companion columns can overwrite half a perimeter. The
		// remaining three-word face column is accepted only when both endpoints
		// already carry independent wall role or intact-template ownership.
		for(int kind=0;kind<4;kind++)for(int column=0;column<2;column++) {
			const int offsets[]={0x2e8,0x2f8,0x2c8,0x2d8};const bool vertical=kind<2;
			const int start=(kind&1)?1:0;
			for(int y=0;y<64-(vertical?2:0);y++)for(int x=0;x<64-(vertical?0:2);x++) {
				bool exact=true;
				for(int k=0;exact && k<3;k++) {
					const int i=(y+(vertical?k:0))*64+x+(vertical?0:k);const auto &t=output[i];
					const auto *src=context.dungeonTemplateData+offsets[kind]+(column*4+start+k)*2;
					if((context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8)!=(src[0]|src[1]<<8) ||
						!diagnosticMember(t) || (k!=1 && t.surface!=Surface::Wall && !t.diagnosticWallTemplateOffset))exact=false;
				}
				if(exact)for(int k=0;k<3;k++) {
					auto &t=output[(y+(vertical?k:0))*64+x+(vertical?0:k)];
					t.diagnosticWallTemplateOffset=offsets[kind];t.diagnosticUphillDirections=1<<kind;
				}
			}
		}
	}
	S9xRemasterSemanticDiagonalExtents(output);
}

inline void S9xRemasterObserveTile (const uint8_t *indices, uint8_t bitDepth, uint16_t vramAddress, uint16_t tileWord,
	const uint16_t *cgramPalette = nullptr, bool directColour = false)
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
		instance.hasPaletteArtwork = cgramPalette || directColour;
		if (instance.hasPaletteArtwork)
			for (size_t pixel = 0; pixel < 64; pixel++)
				instance.paletteRgb555[pixel] = directColour ? S9xRemasterDirectRgb555(indices[pixel], palette) :
					S9xRemasterPaletteRgb555(cgramPalette[indices[pixel]]);
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

// UI toggles this with the floor/wall overlay; no debug-mode enum coupling.
// Full-room diagnostics are cached with the solve; pixel projection is opt-in.
inline void S9xRemasterSetRoomDiagnosticsEnabled(bool enabled)
{
	S9xRemasterState().requestedRoomDiagnostics.store(enabled,std::memory_order_relaxed);
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
	result.emitterPatchSize = state.activeProfile.emitterPatchSize;
	result.representativeEmitters = state.activeProfile.representativeEmitters;
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
			if (!state.sceneRgb.empty())
			{
				result.sceneRgb555.push_back(state.sceneRgb[sourceOffset]);
				result.scenePixels.push_back(state.scenePixels[sourceOffset]);
			}
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
	// Keep one complete canonical artwork reference per identity for animation
	// browsing; the selected occurrence can still use its own exact palette.
	std::set<RemasterTileContentId> paletteTiles;
	for (const auto &instance : result.tileInstances)
		if (instance.hasPaletteArtwork && paletteTiles.insert(instance.tileId).second)
		{
			RemasterFrameArtworkColors colors;
			colors.tileId = instance.tileId;
			colors.rgb555 = instance.paletteRgb555;
			auto asset = state.observedTiles.find(instance.tileId.hash);
			if (asset != state.observedTiles.end())
				for (size_t pixel = 0; pixel < 64; pixel++)
					if (asset->second.indices[pixel]) colors.visiblePixels |= UINT64_C(1) << pixel;
			result.paletteArtworkColors.push_back(colors);
		}
	// Partial enable/unsupported spans must never publish a stale scene.
	if (std::any_of(result.scenePixels.begin(), result.scenePixels.end(),
		[] (const RemasterFramePixel &p) { return p.owner == REMASTER_OWNER_UNSUPPORTED; }))
	{
		result.scenePixels.clear(); result.sceneRgb555.clear();
	}
	S9xRemasterApplyProfileToFrame(state.activeProfile, result);
	if (!result.scenePixels.empty() && result.profileRomSha256.empty())
		result.profileRomSha256 = "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
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
	const RemasterDungeonFloorContext *dungeonFloor = nullptr,
	const RemasterHeldLampState *heldLamp = nullptr)
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
		if (frame.scenePixels.empty() && heldLamp && !state.mainPreMathRgb.empty())
			S9xRemasterRecoverLampScene(frame, state.mainPreMathRgb.data(),
				state.mainPreMathRgb.size(), screenPitch, *heldLamp);
		// Apply existing placement/actor metadata to the independently visible scene.
		// Restore the stock ownership plane before capture/publication.
		const bool hasScene = !frame.scenePixels.empty();
		if (hasScene) frame.mainPixels.swap(frame.scenePixels);
		RemasterDungeonHeightApplyDiagnostics dungeonHeightDiagnostics;
		bool runtimeInputsPublishable = false;
		const RemasterDungeonHeightMap *appliedDungeonHeightMap = nullptr;
		RemasterDungeonFloorContext liveContext;
		if(!dungeonFloor) {
			state.transitionSupport={};state.runtimeEntrySupport={};state.runtimeBg1EntrySupport={};state.runtimeEntryRoom=0xffff;
		}
		if (dungeonFloor)
		{
			S9xRemasterObserveDungeonTransition(*dungeonFloor);
			liveContext=*dungeonFloor;
			if(state.runtimeEntryRoom==liveContext.roomIndex) {
				liveContext.entrySupport=state.runtimeEntrySupport;
				if(!liveContext.submodule)liveContext.previousRoomIndex=state.runtimeEntrySupport.predecessorRoom;
			}
			else {state.runtimeEntrySupport={};state.runtimeBg1EntrySupport={};state.runtimeEntryRoom=0xffff;}
			dungeonFloor=&liveContext;
			const bool roomChanged = state.runtimeDungeonObservedRoom != dungeonFloor->roomIndex;
			if (roomChanged)
			{
				if (state.runtimeDungeonHeightValid &&
					state.runtimeDungeonHeightMap.roomIndex == dungeonFloor->previousRoomIndex)
					state.previousRuntimeDungeonHeightMap = state.runtimeDungeonHeightMap;
				if (state.runtimeDungeonBg1HeightMap.roomIndex == dungeonFloor->previousRoomIndex)
					state.previousRuntimeDungeonBg1HeightMap = state.runtimeDungeonBg1HeightMap;
				state.runtimeDungeonObservedRoom = dungeonFloor->roomIndex;
				// The room ID can change while construction is in progress. Do not
				// consume that invalidation before the settled publication boundary,
				// even when two rooms happen to have byte-identical table contents.
				state.dungeonSnapshotValid = false;
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
			if (!runtimeInputsPublishable) state.dungeonSnapshotValid = false;
			bool inputChanged = roomChanged || !state.dungeonSnapshotValid || state.dungeonGeometryProfileDirty ||
				state.dungeonObservedRomIdentity != dungeonFloor->romIdentity ||
				state.dungeonObservedEntrySupport.cell != dungeonFloor->entrySupport.cell ||
				state.dungeonObservedEntrySupport.base != dungeonFloor->entrySupport.base ||
				state.dungeonObservedEntrySupport.predecessorRoom != dungeonFloor->entrySupport.predecessorRoom ||
				state.dungeonObservedEntrySupport.predecessorState != dungeonFloor->entrySupport.predecessorState ||
				state.dungeonObservedEntrySupport.transitionEvidence != dungeonFloor->entrySupport.transitionEvidence ||
				state.dungeonObservedEntrySupport.destinationState != dungeonFloor->entrySupport.destinationState ||
				state.dungeonObservedCollisionMode != dungeonFloor->collisionMode ||
				state.dungeonObservedPlane != (dungeonFloor->linkPlane & 1) ||
				state.dungeonObservedRise != state.activeProfile.upperFloorHeight ||
				state.dungeonObservedGraphicsBase != dungeonFloor->graphicsBase ||
				state.dungeonObservedBg1GraphicsBase != dungeonFloor->bg1GraphicsBase ||
				state.dungeonObservedTemplateData != dungeonFloor->dungeonTemplateData ||
				state.dungeonObservedStairCount != dungeonFloor->stairCount;
			// Only artwork-only changes can enter the equivalence fast path. Keep
			// EVERY final word and support dependency exact, including priority.
			bool artworkOnlyEligible = !inputChanged && state.runtimeDungeonHeightValid &&
				state.runtimeDungeonObservedInputKey==state.runtimeDungeonHeightKey &&
				state.dungeonRetainedBg2 && state.dungeonRetainedBg1 &&
				state.runtimeDungeonHeightMap.roomIndex==dungeonFloor->roomIndex &&
				state.runtimeDungeonBg1HeightMap.roomIndex==dungeonFloor->roomIndex &&
				!state.transitionSupport.pending && dungeonFloor->entrySupport.cell<0 &&
				state.runtimeBg1EntrySupport.cell<0 && !state.runtimeDungeonHeightMap.entrySupportConsumed &&
				!state.runtimeDungeonBg1HeightMap.entrySupportConsumed &&
				!dungeonFloor->supportRelations && !dungeonFloor->wallSupportRelations &&
				dungeonFloor->placementBackgroundIndex==1;
			if(dungeonFloor->dungeonTemplateData)
				inputChanged |= std::memcmp(state.dungeonObservedTemplates.data(),dungeonFloor->dungeonTemplateData,12876)!=0;
			if (dungeonFloor->constructedMaps && dungeonFloor->graphics && dungeonFloor->collisionAttributes)
			{
				// memcmp is vectorized and avoids serial FNV and profile scans on
				// unchanged frames. Hash/decode/solve only after a state change.
				inputChanged |= std::memcmp(state.dungeonObservedMaps.data(), dungeonFloor->constructedMaps, 16384) != 0 ||
					std::memcmp(state.dungeonObservedAttributes.data(), dungeonFloor->collisionAttributes, 8192) != 0;
				artworkOnlyEligible &= !inputChanged;
				// Completed topology/profile/support change invalidates ALL retained
				// artwork states, even when a later input hash happens to recur.
				if(!artworkOnlyEligible)state.dungeonArtworkStates.clear();
				std::array<bool, 1024> used = {};
				for (int i = 0; !inputChanged && i < 8192; i++)
				{
					const int tile = (dungeonFloor->constructedMaps[i * 2] | (dungeonFloor->constructedMaps[i * 2 + 1] << 8)) & 1023;
					if(i==4096)used.fill(false);
					if (used[tile]) continue;
					used[tile] = true;
					for (int b = 0; b < 32; b++)
					{
						const int base=i>=4096 && dungeonFloor->bg1GraphicsBase!=0xffff?dungeonFloor->bg1GraphicsBase:dungeonFloor->graphicsBase;
						const uint16_t address = static_cast<uint16_t>(base + tile * 32 + b);
						if (state.dungeonObservedGraphics[address] != dungeonFloor->graphics[address]) inputChanged = true;
					}
				}
			}
			for (uint8_t i = 0; i < dungeonFloor->stairCount; i++)
			{
				const auto &a = state.dungeonObservedStairs[i], &b = dungeonFloor->stairs[i];
				inputChanged |= a.x != b.x || a.y != b.y || a.table != b.table || a.tableEntry != b.tableEntry ||
					a.highIsNorth != b.highIsNorth || a.changesPlane != b.changesPlane;
				artworkOnlyEligible &= a.x==b.x && a.y==b.y && a.table==b.table && a.tableEntry==b.tableEntry &&
					a.highIsNorth==b.highIsNorth && a.changesPlane==b.changesPlane;
			}
			if(!artworkOnlyEligible)state.dungeonArtworkStates.clear();
			// Authored semantics/relief are geometry inputs as well as room RAM.
			if (state.dungeonGeometryProfileDirty)
			{
			state.dungeonGeometryProfileKey = UINT64_C(1469598103934665603);
			for (const auto &asset : state.activeProfile.assets)
			{
				state.dungeonGeometryProfileKey ^= asset.first.hash;
				// Role/facing activation can change ownership without changing a
				// single authored sample. Dirty alone is insufficient: the solve
				// below is keyed, so equal relief must not reuse the old result.
				auto mixGeometry = [&] (uint64_t v) { state.dungeonGeometryProfileKey =
					(state.dungeonGeometryProfileKey ^ v) * UINT64_C(1099511628211); };
				mixGeometry(asset.first.bitDepth);mixGeometry(static_cast<uint8_t>(asset.second.tileRole));
				mixGeometry(asset.second.hasTileSemantics);mixGeometry(asset.second.tileSemanticContextDependent);
				for(const auto &facing:asset.second.tileSemanticNormals){mixGeometry(facing.size());for(unsigned char c:facing)mixGeometry(c);}
				mixGeometry(asset.second.tileSemanticNormals.size());
				for(unsigned char c:asset.second.tileSemanticCorner)mixGeometry(c);
				mixGeometry(asset.second.tileSemanticCorner.size());
				mixGeometry(asset.second.hasDoorframeMask);mixGeometry(asset.second.doorframeMask);
				// Exact excluded pixels constrain inferred ROM-owned frame regions.
				mixGeometry(asset.second.hasOcclusion);
				if(asset.second.hasOcclusion)for(uint8_t o:asset.second.occlusion)mixGeometry(o);
				state.dungeonGeometryProfileKey = (state.dungeonGeometryProfileKey ^ asset.second.hasHeight) * UINT64_C(1099511628211);
				for (uint8_t h : asset.second.height)
					state.dungeonGeometryProfileKey = (state.dungeonGeometryProfileKey ^ h) * UINT64_C(1099511628211);
				// Exact authored floor-normal pixels now constrain mixed feet.
				state.dungeonGeometryProfileKey = (state.dungeonGeometryProfileKey ^ asset.second.hasNormals) * UINT64_C(1099511628211);
				if (asset.second.hasNormals) for (uint8_t n : asset.second.normalXyz)
					state.dungeonGeometryProfileKey = (state.dungeonGeometryProfileKey ^ n) * UINT64_C(1099511628211);
			}
			for (const auto &group : state.activeProfile.assetGroups)
			{
				for (char c : group.first) state.dungeonGeometryProfileKey = (state.dungeonGeometryProfileKey ^ c) * UINT64_C(1099511628211);
				for (const auto &id : group.second.tileIds) state.dungeonGeometryProfileKey = (state.dungeonGeometryProfileKey ^ id.hash) * UINT64_C(1099511628211);
			}
			state.dungeonGeometryProfileDirty = false;
			}
			if (inputChanged && runtimeInputsPublishable)
			{
				state.runtimeDungeonObservedInputKey = S9xRemasterDungeonHeightInputKey(*dungeonFloor,
					state.activeProfile.upperFloorHeight) ^ state.dungeonGeometryProfileKey;
				if (dungeonFloor->constructedMaps && dungeonFloor->graphics && dungeonFloor->collisionAttributes)
				{
					std::copy_n(dungeonFloor->constructedMaps, 16384, state.dungeonObservedMaps.begin());
					std::copy_n(dungeonFloor->collisionAttributes, 8192, state.dungeonObservedAttributes.begin());
					std::copy_n(dungeonFloor->graphics, 65536, state.dungeonObservedGraphics.begin());
					if(dungeonFloor->dungeonTemplateData)std::copy_n(dungeonFloor->dungeonTemplateData,12876,state.dungeonObservedTemplates.begin());
					state.dungeonSnapshotValid = true;
				}
				state.dungeonObservedStairs = dungeonFloor->stairs;
				state.dungeonObservedEntrySupport = dungeonFloor->entrySupport;
				state.dungeonObservedRomIdentity = dungeonFloor->romIdentity;
				state.dungeonObservedStairCount = dungeonFloor->stairCount;
				state.dungeonObservedGraphicsBase = dungeonFloor->graphicsBase;
				state.dungeonObservedBg1GraphicsBase = dungeonFloor->bg1GraphicsBase;
				state.dungeonObservedTemplateData = dungeonFloor->dungeonTemplateData;
				state.dungeonObservedRise = state.activeProfile.upperFloorHeight;
				state.dungeonObservedPlane = dungeonFloor->linkPlane & 1;
				state.dungeonObservedCollisionMode = dungeonFloor->collisionMode;
			}
			if (!reviewMap && runtimeInputsPublishable)
			{
				const uint64_t key = state.runtimeDungeonObservedInputKey;
				if (key != state.runtimeDungeonHeightKey)
				{
					bool exactStateReuse=false;
#ifndef ALTTP_TEST_DISABLE_GEOMETRY_REUSE
#ifndef ALTTP_TEST_CLASSIFY_ARTWORK_REUSE
#ifndef ALTTP_TEST_PRE_ARTWORK_CACHE
					if(artworkOnlyEligible)for(const auto &cached:state.dungeonArtworkStates) {
						if(cached->inputKey!=key)continue;
						// Key is an index only: compare every REFERENCED graphics byte
						// before applying any retained geometry/producer evidence.
						bool exact=true;std::array<bool,2048> used={};
						for(int i=0;exact && i<8192;i++) {
							const int tile=(dungeonFloor->constructedMaps[i*2]|dungeonFloor->constructedMaps[i*2+1]<<8)&1023;
							const int slot=(i>=4096?1024:0)+tile;if(used[slot])continue;used[slot]=true;
							const int base=i>=4096 && dungeonFloor->bg1GraphicsBase!=0xffff?dungeonFloor->bg1GraphicsBase:dungeonFloor->graphicsBase;
							for(int b=0;b<32;b++){const uint16_t at=uint16_t(base+tile*32+b);exact &= cached->graphics[at]==dungeonFloor->graphics[at];}
						}
						if(!exact)continue;
#ifdef ALTTP_TEST_CACHE_STAGES
						auto &stages=S9xRemasterDungeonCacheStages();RemasterDungeonStageTimer stage(stages.restoreMs,stages.restores);
#endif
						state.runtimeDungeonHeightMap=cached->bg2;state.runtimeDungeonBg1HeightMap=cached->bg1;
						*state.dungeonRetainedBg2=cached->placements2;*state.dungeonRetainedBg1=cached->placements1;
						state.runtimeDungeonHeightKey=key;state.runtimeDungeonSolveGeometryKey=key;
						state.runtimeDungeonHeightEpoch++;state.dungeonArtworkStateHits++;exactStateReuse=true;break;
					}
#endif
#endif
#endif
					bool dependencyReuse=false;
#ifndef ALTTP_TEST_DISABLE_GEOMETRY_REUSE
#ifndef ALTTP_TEST_CLASSIFY_ARTWORK_REUSE
#ifndef ALTTP_TEST_PRE_ARTWORK_CACHE
					dependencyReuse=!exactStateReuse && artworkOnlyEligible && S9xRemasterRefreshDungeonArtworkGeometry(*dungeonFloor,state.activeProfile,state);
#endif
#endif
#endif
					if(exactStateReuse) {
						// Distinct state, exact existing solve, no classifier/solver work.
					} else if(dependencyReuse) {
						state.runtimeDungeonHeightKey=key;state.dungeonGeometryReuseCount++;
					} else {
					// Both buffers remain alive through the solve, but must not share
					// the emulation thread's small stack with solver scratch.
					auto candidate = std::make_unique<RemasterDungeonHeightMap>();
					RemasterDungeonFloorContext constructed = *dungeonFloor;
					auto placements = std::make_unique<std::array<RemasterDungeonFloorContext::Placement, 4096>>();
					if (constructed.constructedMaps && constructed.graphics)
					{
						S9xRemasterDungeonPlacements(constructed, state.activeProfile, *placements);
						constructed.placements = placements->data();
					}
					auto lowerMaps = std::make_unique<std::array<uint8_t,16384>>();
					auto lowerPlacements = std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
					auto lower = constructed;
					if(constructed.constructedMaps && constructed.graphics) {
						std::copy_n(constructed.constructedMaps+8192,8192,lowerMaps->begin());
						std::copy_n(constructed.constructedMaps,8192,lowerMaps->begin()+8192);
						lower.placementBackgroundIndex=0;
						lower.entrySupport=state.runtimeEntryRoom==lower.roomIndex?state.runtimeBg1EntrySupport:RemasterDungeonFloorContext::EntrySupport{};
						lower.constructedMaps=lowerMaps->data();
						lower.graphicsBase=constructed.bg1GraphicsBase==0xffff?constructed.graphicsBase:constructed.bg1GraphicsBase;
						lower.bg1GraphicsBase=constructed.graphicsBase;
						S9xRemasterDungeonPlacements(lower,state.activeProfile,*lowerPlacements);
						lower.placements=lowerPlacements->data();
					}
					// Once the exact dependency proof rejects, a full classified scan
					// cannot authorize reuse: identical activated metadata was already
					// required by that path. Do not repeat the failed metadata comparisons.
					bool equivalent=false;
#ifdef ALTTP_TEST_CLASSIFY_ARTWORK_REUSE
					equivalent=artworkOnlyEligible && constructed.placements && lower.placements;
#endif
					state.dungeonClassifiedChangedCells=0;
					std::map<std::pair<uint64_t,uint64_t>,bool> metadataEquivalence;
					if(equivalent)for(int i=0;i<4096;i++) {
						for(int bg=0;bg<2;bg++) {
							const auto &a=bg?(*placements)[i]:(*lowerPlacements)[i];
							const auto &b=bg?(*state.dungeonRetainedBg2)[i]:(*state.dungeonRetainedBg1)[i];
							if(a.tileHash!=b.tileHash)state.dungeonClassifiedChangedCells++;
							equivalent &= a.sameGeometry(b);
							if(a.tileHash!=b.tileHash) {
								const auto pair=std::make_pair(a.tileHash,b.tileHash);
								auto found=metadataEquivalence.find(pair);
								if(found==metadataEquivalence.end())found=metadataEquivalence.emplace(pair,
									S9xRemasterDungeonArtworkGeometryEquivalent(state.activeProfile,a.tileHash,b.tileHash)).first;
								equivalent &= found->second;
							}
						}
					}
#ifdef ALTTP_TEST_DISABLE_GEOMETRY_REUSE
					equivalent=false;
#endif
					if(equivalent) {
						// Refresh producer identity even for unknown/conflicting cells:
						// application, offscreen emitters and capture provenance require it.
						for(int i=0;i<4096;i++) {
							state.runtimeDungeonHeightMap.producerHashes[i]=(*placements)[i].tileHash;
							state.runtimeDungeonBg1HeightMap.producerHashes[i]=(*lowerPlacements)[i].tileHash;
						}
						state.runtimeDungeonHeightKey=key;
						state.dungeonGeometryReuseCount++;
						state.dungeonRetainedBg2=std::move(placements);
						state.dungeonRetainedBg1=std::move(lowerPlacements);
					}
					else if (S9xRemasterBuildDungeonHeightMap(constructed,
						state.activeProfile.upperFloorHeight, *candidate) &&
						candidate->roomIndex == dungeonFloor->roomIndex)
					{
						state.runtimeDungeonHeightMap = std::move(*candidate);
						state.runtimeDungeonHeightKey = key;
						state.runtimeDungeonSolveGeometryKey = key;
						state.runtimeDungeonHeightEpoch++;
						state.runtimeDungeonHeightValid = true;
						// BG1 has independent placed art and relief, including lower
						// walls behind BG2 bridges. Never borrow BG2's visible bases.
						if (constructed.constructedMaps && constructed.graphics)
						{
							// Classification depends on the completed maps, graphics,
							// attributes, templates and profile, not entry datums or
							// transported support relations. Keep each BG's immutable
							// placements alive for all solves at this boundary instead
							// of overwriting BG2 and rescanning its room assemblies.
							auto lowerMap = std::make_unique<RemasterDungeonHeightMap>();
							if (S9xRemasterBuildDungeonHeightMap(lower,state.activeProfile.upperFloorHeight,*lowerMap))
								state.runtimeDungeonBg1HeightMap = std::move(*lowerMap);
							else state.runtimeDungeonBg1HeightMap = RemasterDungeonHeightMap{};
							// Transport signed support relationships, not BG1 art bases.
							if(!state.runtimeDungeonBg1HeightMap.authoredSupportRelations.empty() ||
								!state.runtimeDungeonBg1HeightMap.authoredWallSupportRelations.empty()) {
								constructed.supportRelations=&state.runtimeDungeonBg1HeightMap.authoredSupportRelations;
								constructed.wallSupportRelations=&state.runtimeDungeonBg1HeightMap.authoredWallSupportRelations;
								if(S9xRemasterBuildDungeonHeightMap(constructed,state.activeProfile.upperFloorHeight,*candidate))
									state.runtimeDungeonHeightMap=std::move(*candidate);
							}
							if(state.runtimeEntryRoom==constructed.roomIndex &&
								state.runtimeEntrySupport.destinationState!=S9xRemasterDungeonEntryStateKey(constructed,state.activeProfile.upperFloorHeight)) {
								state.runtimeEntrySupport={};state.runtimeBg1EntrySupport={};state.runtimeEntryRoom=0xffff;
								liveContext.entrySupport={};state.dungeonSnapshotValid=false;
							}
							// Complete an observed level-door transition only after the
							// destination's full support solve and signed relations exist.
							const auto &t=state.transitionSupport;
							if(t.pending && t.doorObserved && t.destination==constructed.roomIndex &&
								constructed.groundedEvidence && t.profile==state.dungeonGeometryProfileKey &&
								std::abs(int(constructed.linkX)-int(t.x))<=128 && std::abs(int(constructed.linkY)-int(t.y))<=128 &&
								t.rom==constructed.romIdentity && t.session==state.dungeonSession) {
								const int p=constructed.linkPlane&1,x=constructed.linkX&511,y=constructed.linkY&511;
								int cell=-1,id=-1;bool legal=x<=496 && y<=487;
								const auto &dest=state.runtimeDungeonHeightMap;
								legal &= bool(dest.roomDiagnostics);
								for(int yy=(y+8)/8;legal && yy<=(y+23)/8;yy++)for(int xx=x/8;xx<=(x+15)/8;xx++) {
									const int i=yy*64+xx;const auto &d=dest.roomDiagnostics->cells[p*4096+i];
									if(!dest.producerZeroFloor[i] || constructed.collisionAttributes[p*4096+i]!=0 || d.floorId<0 || d.heightConflict ||
										dest.planeOffsets[p][i]==255 || (id>=0 && id!=d.floorId))legal=false;
									id=d.floorId;cell=p*4096+i;
								}
								if(legal && cell>=0) {
									constructed.previousRoomIndex=t.room;
									constructed.entrySupport={cell,t.base,t.room,t.state,t.event,S9xRemasterDungeonEntryStateKey(constructed,state.activeProfile.upperFloorHeight)};
									if(S9xRemasterBuildDungeonHeightMap(constructed,state.activeProfile.upperFloorHeight,*candidate) && candidate->entrySupportConsumed) {
										state.runtimeDungeonHeightMap=std::move(*candidate);
										state.runtimeEntrySupport=constructed.entrySupport;state.runtimeEntryRoom=constructed.roomIndex;
										liveContext.entrySupport=constructed.entrySupport;
										liveContext.previousRoomIndex=constructed.previousRoomIndex;
										state.dungeonSnapshotValid=false; // key/dirty pins update next frame
										lower.entrySupport=constructed.entrySupport;
										lower.previousRoomIndex=constructed.previousRoomIndex;
										lower.entrySupport.destinationState=S9xRemasterDungeonEntryStateKey(lower,state.activeProfile.upperFloorHeight);
										state.runtimeBg1EntrySupport=lower.entrySupport;
										if(S9xRemasterBuildDungeonHeightMap(lower,state.activeProfile.upperFloorHeight,*candidate) && candidate->entrySupportConsumed)
											state.runtimeDungeonBg1HeightMap=std::move(*candidate);
									}
								}
								state.transitionSupport={};
							}
							state.dungeonRetainedBg2=std::move(placements);
							state.dungeonRetainedBg1=std::move(lowerPlacements);
#if !defined(ALTTP_TEST_PRE_ARTWORK_CACHE) && !defined(ALTTP_TEST_CLASSIFY_ARTWORK_REUSE) && !defined(ALTTP_TEST_DISABLE_GEOMETRY_REUSE)
							if(!state.transitionSupport.pending && constructed.entrySupport.cell<0 && lower.entrySupport.cell<0 &&
								!state.runtimeDungeonHeightMap.entrySupportConsumed && !state.runtimeDungeonBg1HeightMap.entrySupportConsumed) {
#ifdef ALTTP_TEST_CACHE_STAGES
								auto &stages=S9xRemasterDungeonCacheStages();RemasterDungeonStageTimer stage(stages.storeMs,stages.stores);
#endif
								auto saved=std::make_unique<RemasterState::DungeonSolvedArtworkState>();saved->inputKey=key;
								std::copy_n(constructed.graphics,65536,saved->graphics.begin());
								saved->bg1=state.runtimeDungeonBg1HeightMap;saved->bg2=state.runtimeDungeonHeightMap;
								saved->placements1=*state.dungeonRetainedBg1;saved->placements2=*state.dungeonRetainedBg2;
								if(state.dungeonArtworkStates.size()>=8)state.dungeonArtworkStates.erase(state.dungeonArtworkStates.begin());
								state.dungeonArtworkStates.push_back(std::move(saved));
							}
#endif
						}
					}
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
			else if (sourceRoomMap)
			{
				// Construction has no destination geometry yet, but the source
				// tiles are still on screen. Apply the retained source, with an
				// explicitly unknown destination, rather than dropping all bases.
				auto pending = std::make_unique<RemasterDungeonHeightMap>();
				pending->roomIndex = dungeonFloor->roomIndex;
				pending->offsets.fill(255); pending->hasRoomGeometry = true;
				S9xRemasterApplyDungeonHeightMap(frame,*dungeonFloor,*pending,sourceRoomMap,&dungeonHeightDiagnostics);
			}
			else
				S9xRemasterApplyDungeonFloorHeight(frame, *dungeonFloor,
					state.activeProfile.upperFloorHeight, nullptr);
			// First settled destination is the completion boundary; failure to
			// reproduce grounded support there must not anchor a later position.
			if(!dungeonFloor->submodule && state.transitionSupport.pending &&
				state.transitionSupport.destination==dungeonFloor->roomIndex)state.transitionSupport={};
			S9xRemasterRecordGroundedDungeonSupport(*dungeonFloor,reviewMap,runtimeInputsPublishable);
			if ((!reviewMap || reviewMap == &state.runtimeDungeonHeightMap) &&
				state.runtimeDungeonBg1HeightMap.roomIndex == dungeonFloor->roomIndex)
				S9xRemasterApplyDungeonHeightMap(frame,*dungeonFloor,state.runtimeDungeonBg1HeightMap,
					&state.previousRuntimeDungeonBg1HeightMap,nullptr,0);
			else if (state.previousRuntimeDungeonBg1HeightMap.roomIndex == dungeonFloor->previousRoomIndex)
			{
				auto pending = std::make_unique<RemasterDungeonHeightMap>();
				pending->roomIndex=dungeonFloor->roomIndex;pending->offsets.fill(255);pending->hasRoomGeometry=true;
				S9xRemasterApplyDungeonHeightMap(frame,*dungeonFloor,*pending,&state.previousRuntimeDungeonBg1HeightMap,nullptr,0);
			}
		}
		if (dungeonFloor)
		{
			const char *tracePath = std::getenv("S9X_REMASTER_DUNGEON_HEIGHT_TRACE");
			// Lifecycle events only: camera and Link movement never log per frame.
			const uint64_t eventKey = state.runtimeDungeonHeightEpoch ^
				(uint64_t(dungeonFloor->roomIndex) << 32) ^ (uint64_t(dungeonFloor->submodule) << 24) ^
				(uint64_t(dungeonFloor->linkPlane) << 20) ^ (uint64_t(runtimeInputsPublishable) << 19) ^
				(uint64_t(dungeonFloor->collisionMode) << 8);
			if (tracePath && *tracePath && eventKey != state.dungeonTraceEventKey)
			{
				state.dungeonTraceEventKey = eventKey;
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
						<< ",\"profile_geometry_key\":" << state.dungeonGeometryProfileKey
						<< ",\"observed_input_key\":" << state.runtimeDungeonObservedInputKey
						<< ",\"published_map_key\":" << state.runtimeDungeonHeightKey
						<< ",\"solve_geometry_key\":" << state.runtimeDungeonSolveGeometryKey
						<< ",\"geometry_reuses\":" << state.dungeonGeometryReuseCount
						<< ",\"artwork_state_hits\":" << state.dungeonArtworkStateHits
						<< ",\"classified_changed_cells\":" << state.dungeonClassifiedChangedCells
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
						<< ",\"required_plane_cells\":[" << map.requiredPlaneCells[0] << ',' << map.requiredPlaneCells[1] << ']'
						<< ",\"unknown_plane_cells\":[" << map.unknownPlaneCells[0] << ',' << map.unknownPlaneCells[1] << ']'
						<< ",\"structural_conflicts\":" << map.structuralConflicts
						<< ",\"solved_wall_cells\":" << map.solvedWallCells
						<< ",\"unknown_wall_cells\":" << map.unknownWallCells
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
		if (hasScene) frame.mainPixels.swap(frame.scenePixels);
		if(dungeonFloor && state.requestedRoomDiagnostics.load(std::memory_order_relaxed))
			S9xRemasterProjectRoomDiagnostics(frame,*dungeonFloor,appliedDungeonHeightMap,
				appliedDungeonHeightMap==&state.runtimeDungeonHeightMap ? &state.runtimeDungeonBg1HeightMap : nullptr,
				&state.previousRuntimeDungeonHeightMap,&state.previousRuntimeDungeonBg1HeightMap);
		if (dungeonFloor)
			S9xRemasterAppendOffscreenRoomLights(frame, *dungeonFloor, state.activeProfile,
				appliedDungeonHeightMap,
				state.runtimeDungeonBg1HeightMap.roomIndex == dungeonFloor->roomIndex ? &state.runtimeDungeonBg1HeightMap : nullptr,
				S9xRemasterHashTile);
		if (heldLamp)
		{
			RemasterHeldLampState lamp = *heldLamp;
			// Consume the existing actor-floor solve; do not infer floor from
			// Link's relief/priority or add generated sprite-part heights.
			if (dungeonFloor && appliedDungeonHeightMap && appliedDungeonHeightMap->hasPlaneOffsets)
			{
				const size_t cell = (((dungeonFloor->linkY + 20) & 511) / 8) * 64 +
					((dungeonFloor->linkX + 8) & 511) / 8;
				const int32_t base = appliedDungeonHeightMap->planeOffsets[dungeonFloor->linkPlane & 1][cell];
				if (base != 255) lamp.floorHeight = base * frame.lightingCoordinateScale / 255.0f;
			}
			RemasterFrameLight light;
			if (S9xRemasterHeldLampLight(lamp, RemasterHeldLampSettings{}, light)) frame.lights.push_back(light);
		}
		if (state.frameCaptureActive)
		{
			if (!state.animationCapture.frameCount)
			{
				state.activeFramePath = S9xRemasterCaptureLocationPath(state.activeFramePath,
					dungeonFloor && dungeonFloor->verifiedAlttpRom, dungeonFloor && dungeonFloor->indoors,
					dungeonFloor ? dungeonFloor->roomIndex : 0, dungeonFloor ? dungeonFloor->linkX : 0,
					dungeonFloor ? dungeonFloor->linkY : 0);
				// The retained animation packet is the FIRST frame, not the last.
				// Snapshot pointer-backed inputs/profile/map at that same boundary.
				state.animationDungeonEvidence=S9xRemasterPinDungeonCaptureEvidence(dungeonFloor,
					state.activeProfile,appliedDungeonHeightMap,state.runtimeDungeonHeightKey,
					state.runtimeDungeonHeightEpoch,runtimeInputsPublishable);
			}
			S9xRemasterAddAnimationCaptureFrame(state.animationCapture, frame);
			if (state.animationCapture.frameCount >= state.activeFrameCount)
			{
				RemasterFrame captured = S9xRemasterFinishAnimationCapture(std::move(state.animationCapture));
				std::vector<uint8_t> bytes;
				if (S9xSerializeRemasterFrame(captured, bytes) &&
					S9xRemasterWriteUniqueCapture(bytes, state.activeFramePath, state.lastWrittenCapturePath))
				{
					result |= RemasterCaptureFrame;
					if (state.animationDungeonEvidence){const auto &e=*state.animationDungeonEvidence;
						S9xRemasterWriteDungeonCaptureEvidence(state.lastWrittenCapturePath,e.context,e.profile,
							e.map.get(),e.key,e.epoch,e.publishable);}
				}
				state.frameCaptureActive = false;
				state.animationDungeonEvidence.reset();
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
		state.animationDungeonEvidence.reset();
	}
	return result;
}

inline void S9xRemasterSetTilePixel (uint8_t tilePixel)
{
	S9xRemasterState().currentTilePixel = tilePixel;
}

inline void S9xRemasterWriteOwner (size_t offset)
{
	RemasterState &state = S9xRemasterState();
	if (state.scenePass)
	{
		if (offset < state.scenePixels.size())
			state.scenePixels[offset] = {S9xRemasterCurrentOwner,
				state.currentTileHashValid ? static_cast<uint32_t>(state.tileInstances.size()) : 0,
				state.currentTileHashValid ? state.currentTilePixel : uint8_t(0xff)};
		return;
	}
	if (S9xRemasterCurrentOwners)
		S9xRemasterCurrentOwners[offset] = S9xRemasterCurrentOwner;

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
