/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REMASTER_FRAME_H_
#define _REMASTER_FRAME_H_

#include "profile.h"
#include "room_diagnostics.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

static const uint32_t REMASTER_FRAME_SCHEMA_VERSION = 28;

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
	RemasterTileRole tileRole = RemasterTileRole::Unknown;
	std::array<std::string, 64> materialSelectors;
	std::array<uint8_t, 64> occlusion = S9xRemasterDefaultOcclusion();
	std::array<uint8_t, 64> height = {};
	std::array<uint8_t, 192> normalXyz = {};
	std::array<uint8_t, 256> emissionRgba = {};
	float emissionDepth = 0.0f;
	bool emissionOmnidirectional = false;
	std::vector<RemasterTileContentId> combineEmitters;
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
	int32_t heightOffset = 0; // signed placement datum; authored local arrays stay bytes
	std::array<uint8_t, 64> placementHeight = {};
	int8_t normalYaw = 0;
	bool hFlip = false;
	bool vFlip = false;
	bool hasPlacementHeight = false;
	RemasterProfileMatchStatus matchStatus = RemasterProfileMatchStatus::NoMatch;
	std::string assetGroup;
	std::string material;
};

// CPU/Metal RG32Float v2 wire value. Local profile/placement relief stays unsigned;
// only the composed height is signed. No rendering-origin bias or clipping.
inline float S9xRemasterSignedHeightField (int32_t base, uint8_t local)
{
	return (base + static_cast<int32_t>(local)) / 255.0f;
}

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
	uint8_t emitterPatchSize = 2;
	uint8_t representativeEmitters = 0;
	bool sampleAccumulation = true;
	std::vector<uint16_t> originalRgb555;
	std::vector<RemasterFramePixel> mainPixels;
	std::vector<RemasterFramePixel> subPixels;
	// Optional v28 SCN1 appendix: independent pre-cone scene, same instance table.
	// Empty on legacy/unsupported frames. Stock RGB/owners remain authoritative.
	std::vector<uint16_t> sceneRgb555;
	std::vector<RemasterFramePixel> scenePixels;
	std::vector<RemasterFrameAsset> assets;
	std::vector<RemasterFrameArtworkColors> artworkColors;
	std::vector<RemasterFrameAssetGroup> assetGroups;
	std::vector<RemasterFrameAssetMetadata> assetMetadata;
	std::vector<RemasterFrameMaterial> materials;
	std::vector<RemasterFrameTileInstance> tileInstances;
	// Optional runtime-only parallel pixel vectors. Empty when not requested;
	// capture read/write ignores these fields (schema remains unchanged).
	std::vector<RemasterRoomPixelDiagnostic> mainRoomDiagnostics;
	std::vector<RemasterRoomPixelDiagnostic> subRoomDiagnostics;
	std::vector<RemasterRoomPixelDiagnostic> sceneRoomDiagnostics;
	std::vector<RemasterFrameLight> lights;
};

struct RemasterDungeonFloorContext
{
	enum class Surface : uint8_t { Unknown, Floor, Wall, Obstacle, Stair };
	struct Placement
	{
		Surface surface = Surface::Unknown;
		std::array<uint8_t, 64> height = {};
		// Canonical exact authored +Z, zero-height foot pixels, not wall caps.
		uint64_t groundedFloorMask = 0;
		bool hasHeight = false;
		bool hflip = false, vflip = false;
		// Independently classified, authored-zero flat BG usages. This is
		// candidate support evidence, never a BG-layer elevation label.
		uint8_t floorPlaneMask = 0;
		bool distinctLowerUsage = false;
		// A collision-solid dungeon pattern usage cannot anchor a plateau on
		// that plane. Explicit wall-top assemblies retain their own semantics.
		uint8_t structuralFloorPlaneMask = 3;
		uint16_t wallTemplateOffset = 0;
		uint64_t tileHash = 0;
		bool supportingTrim = false;
		// Observer-only evidence. Does not promote a deferred assembly member
		// into the absolute-height graph or alter runtime placement offsets.
		uint16_t diagnosticWallTemplateOffset = 0;
		uint8_t diagnosticUphillDirections = 0; // displayed handler axis, flat caps only
		uint8_t diagnosticNormalUphill = 0; // canonical, consistent authored face normals
		bool diagnosticAuthoredFloor = false; // preserves floor role before solver defers nonflat trim
		bool diagnosticRomFloorTrim = false; // exact bounded ROM trim run; diagnostic support only
	};
	struct Stair
	{
		uint8_t x = 0;
		uint8_t y = 0;
		uint8_t table = 0;
		uint8_t tableEntry = 0;
		bool highIsNorth = false;
		bool changesPlane = false;
	};
	bool verifiedAlttpRom = false;
	bool indoors = false;
	uint8_t collisionMode = 0;
	uint8_t linkFacing = 0xff;
	uint8_t linkPlane = 0;
	// Which placed artwork is being solved; never a gameplay-plane label.
	uint8_t placementBackgroundIndex = 1;
	uint16_t linkX = 0;
	uint16_t linkY = 0;
	uint16_t roomIndex = 0xffff;
	uint16_t previousRoomIndex = 0xffff;
	uint8_t submodule = 0;
	uint8_t module = 7; // adapters default to settled dungeon module
	uint8_t constructionPhase = 0;
	uint8_t roomTransitionFlags = 0;
	bool groundedEvidence = false; // verified adapter: ordinary grounded Link
	uint64_t romIdentity = 0; // verified ROM identity, not a room/map hash
	uint16_t backgroundScrollX = 0;
	uint16_t backgroundScrollY = 0;
	const uint8_t *collisionAttributes = nullptr;
	// Complete constructed BG2 room, never a viewport inventory. Heights are
	// canonical authored relief; flips are applied by the geometry solver.
	const Placement *placements = nullptr;
	const uint8_t *constructedMaps = nullptr; // both final WRAM BG maps, 16384 bytes
	const uint8_t *graphics = nullptr; // complete VRAM; included in state identity
	// Verified USA ROM predefined tile table ($80:9B52, 6438 words).
	// Optional: old captures/adapters conservatively lack template evidence.
	const uint8_t *dungeonTemplateData = nullptr;
	uint16_t graphicsBase = 0;
	uint16_t bg1GraphicsBase = 0xffff; // unspecified adapters share BG2 base
	std::array<uint16_t, 2> ppuMapBase = {};
	std::array<uint8_t, 2> ppuMapSize = {};
	std::array<Stair, 32> stairs = {};
	uint8_t stairCount = 0;
	// Signed relations proved by the other BG's authored seam graph, before
	// configured stair fallbacks or relative datums. Endpoints name reproduced
	// support cells, not visible artwork or a gameplay-plane elevation label.
	struct SupportRelation { int a, b, rise; };
	const std::vector<SupportRelation> *supportRelations = nullptr;
	// Adapter-supplied completed transition evidence. Never synthesize from
	// Link's current position/plane or a remembered room ID. The destination
	// support cell must be reproduced by this solve; pins expire on input change.
	struct EntrySupport {
		int cell = -1, base = 0;
		uint16_t predecessorRoom = 0xffff;
		uint64_t predecessorState = 0, transitionEvidence = 0, destinationState = 0;
	};
	EntrySupport entrySupport;
};

// Bind an entry attestation to the completed destination support/geometry input.
// Called only for an explicit anchor, not every frame. Includes authored relief
// and usage so profile edits and overwritten/flipped placements expire evidence.
inline uint64_t S9xRemasterDungeonEntryStateKey(const RemasterDungeonFloorContext &c,uint8_t floorRise)
{
	if(!c.placements || !c.collisionAttributes)return 0;
	uint64_t key=UINT64_C(14695981039346656037);
	auto mix=[&](uint64_t v){for(int b=0;b<8;b++)key=(key^uint8_t(v>>(b*8)))*UINT64_C(1099511628211);};
	mix(c.roomIndex);mix(c.collisionMode);mix(c.placementBackgroundIndex);mix(c.graphicsBase);mix(c.bg1GraphicsBase);mix(floorRise);mix(c.romIdentity);
	if(c.constructedMaps)for(int i=0;i<16384;i++)mix(c.constructedMaps[i]);
	for(int i=0;i<8192;i++)mix(c.collisionAttributes[i]);
	for(int i=0;i<4096;i++) {
		const auto &t=c.placements[i];mix(t.tileHash);mix(int(t.surface));mix(t.hasHeight);mix(t.hflip);mix(t.vflip);
		mix(t.wallTemplateOffset);mix(t.distinctLowerUsage);mix(t.structuralFloorPlaneMask);mix(t.floorPlaneMask);
		mix(t.supportingTrim);mix(t.groundedFloorMask);
		for(auto h:t.height)mix(h);
	}
	for(int i=0;i<c.stairCount;i++){const auto &s=c.stairs[i];mix(s.x);mix(s.y);mix(s.highIsNorth);mix(s.changesPlane);}
	return key;
}

// Completed attributes encode the actual in-room handler kind. Counts in WRAM
// are independent (not cumulative endpoints); validate even table-derived
// records against this signature so empty/stale tables cannot erase a stair.
inline void S9xRemasterDungeonStairsFromAttributes(RemasterDungeonFloorContext &c)
{
	if (!c.collisionAttributes) return;
	for (int y = 0; y < 61; y++) for (int x = 0; x < 61; x++)
	{
		int kind = c.collisionAttributes[(y+1)*64+x+1];
		if (kind != 0x1d && kind != 0x1e && kind != 0x1f &&
			kind != 0x3d && kind != 0x3e && kind != 0x3f) continue;
		bool matches = true;
		for (int p=0;p<2;p++) for (int yy=1;yy<=2;yy++) for (int xx=1;xx<=2;xx++)
			if (c.collisionAttributes[p*4096+(y+yy)*64+x+xx] != kind) matches=false;
		// The handler also installs blocking side columns (01/02) on
		// both planes; don't recognize isolated special-attribute bytes.
		for (int yy=1;yy<=2;yy++)
		{
			// Ordered BG construction can cover a side on one plane (room060
			// does this above its bridge). Require a surviving reciprocal pair,
			// not identical surrounding artwork/collision on both planes.
			bool side=false;
			for (int p=0;p<2;p++)
				if (c.collisionAttributes[p*4096+(y+yy)*64+x] == 1 &&
					c.collisionAttributes[p*4096+(y+yy)*64+x+3] == 1) side=true;
			if (!side) matches=false;
		}
		if (!matches) continue;
		bool exists=false;
		for (int i=0;i<c.stairCount;i++) if (c.stairs[i].x==x && c.stairs[i].y==y) exists=true;
		if (exists || c.stairCount == c.stairs.size()) continue;
		auto &s=c.stairs[c.stairCount++]; s.x=x; s.y=y;
		s.table=kind>=0x30; s.tableEntry=255;
		s.highIsNorth=kind==0x1d || kind==0x1f || kind==0x3f;
		s.changesPlane=(kind&15)!=13;
	}
}

inline uint16_t S9xRemasterDungeonRoomForScreenPosition (const RemasterDungeonFloorContext &context,
	uint32_t x, uint32_t y)
{
	if ((!context.submodule && context.module == 7) || context.previousRoomIndex == 0xffff ||
		context.previousRoomIndex == context.roomIndex)
		return context.roomIndex;
	// During an intra-dungeon scroll Zelda updates $A0 to the destination while
	// the source room is still present on the opposite side of the viewport.
	// $A2 and the room-grid delta record the source and direction. $EF cannot be
	// used here: it is a door layer-toggle bitfield and is cleared as scrolling
	// starts. Submodule zero is the settled-room lifecycle boundary.
	// The source/destination split is the room boundary in unwrapped BG2 space;
	// do not modulo first or destination geometry leaks onto source-room pixels.
	const int worldX = static_cast<int>(context.backgroundScrollX) + static_cast<int>(x);
	const int worldY = static_cast<int>(context.backgroundScrollY) + static_cast<int>(y);
	const int currentColumn = context.roomIndex & 15, previousColumn = context.previousRoomIndex & 15;
	const int currentRow = (context.roomIndex >> 4) & 15, previousRow = (context.previousRoomIndex >> 4) & 15;
	const int boundaryX = std::max(currentColumn,previousColumn)*512;
	const int boundaryY = std::max(currentRow,previousRow)*512;
	if (currentColumn < previousColumn && worldX >= boundaryX) return context.previousRoomIndex;
	if (currentColumn > previousColumn && worldX < boundaryX) return context.previousRoomIndex;
	if (currentRow < previousRow && worldY >= boundaryY) return context.previousRoomIndex;
	if (currentRow > previousRow && worldY < boundaryY) return context.previousRoomIndex;
	return context.roomIndex;
}

struct RemasterDungeonHeightMap
{
	std::shared_ptr<const RemasterRoomDiagnostics> roomDiagnostics;
	std::array<uint64_t,4096> producerHashes = {}; // zero: legacy adapter has no producer identity
	std::array<uint8_t,4096> producerFlips = {};
	std::array<bool,4096> producerZeroFloor = {};
	uint16_t roomIndex = 0xffff;
	// 255 remains the reserved unknown marker for legacy API/map compatibility.
	// Solved bases are signed; 255 itself is not an admissible placement base.
	std::array<int32_t, 64 * 64> offsets = {};
	std::array<std::array<int32_t, 64 * 64>, 2> planeOffsets = {};
	uint16_t componentCount = 0;
	uint16_t constraintCount = 0;
	uint16_t conflictingComponentCount = 0;
	std::array<uint16_t, 2> solvedPlaneCells = {};
	bool hasPlaneOffsets = false;
	bool hasRoomGeometry = false;
	std::array<uint8_t, 4096> fallbackDirections = {};
	std::array<std::array<int32_t, 4096>, 2> structuralPlaneOffsets = {};
	std::array<std::array<int32_t, 4096>, 2> mixedFloorBases = {};
	uint16_t mixedFloorUnknown = 0, mixedFloorConflicts = 0;
	std::array<uint16_t, 2> requiredPlaneCells = {};
	std::array<uint16_t, 2> unknownPlaneCells = {};
	uint16_t structuralConflicts = 0;
	uint16_t solvedWallCells = 0;
	uint16_t unknownWallCells = 0;
	std::array<uint16_t,2> candidateFloorCells = {};
	std::array<uint16_t,2> unobservedFloorCells = {};
	bool disconnectedPlaneCandidates = false;
	bool planeRelationshipUnresolved = false;
	struct Conflict { int node, other, expected, actual; };
	std::vector<Conflict> geometryConflicts;
	std::vector<std::array<int, 4>> geometryIssuePlacements;
	struct RelativeAnchor { int node, minimum, supportNodes; bool isolatedFlat; };
	std::vector<RelativeAnchor> relativeAnchors;
	struct GeometryGroupIssue { int start, nodes, mismatches, negative, overflow; bool isolatedFlat; };
	std::vector<GeometryGroupIssue> geometryGroupIssues;
	std::vector<RemasterDungeonFloorContext::SupportRelation> authoredSupportRelations;
	uint16_t sharedStructuralFaces = 0, configuredStairFallbacks = 0;
	bool entrySupportConsumed = false;
};

struct RemasterDungeonHeightApplyDiagnostics
{
	uint32_t destinationPixels = 0;
	uint32_t sourcePixels = 0;
	uint32_t destinationRaisedPixels = 0;
	uint32_t sourceRaisedPixels = 0;
	uint32_t destinationUnknownPixels = 0;
	uint32_t sourceUnknownPixels = 0;
};

// Projection uses the same room/camera mapping as height application and verifies
// full-room producer identity. BG1/BG2 and both gameplay planes stay independent.
inline void S9xRemasterProjectRoomDiagnostics(RemasterFrame &frame,
	const RemasterDungeonFloorContext &context, const RemasterDungeonHeightMap *bg2,
	const RemasterDungeonHeightMap *bg1, const RemasterDungeonHeightMap *previousBg2=nullptr,
	const RemasterDungeonHeightMap *previousBg1=nullptr)
{
	// Prove BG1's lower usage independently of either BG's height equalities.
	// Every required downhill branch must reach its own reproduced floor through
	// the same final words on BG2 or transparent BG2 holes. The terminal floor
	// must be exposed by a hole and belong to the identical movement component.
	// This is compositing/contact evidence, not a preference for the active plane.
	auto lowerOwnership=std::make_unique<std::array<bool,4096>>();
	if(context.verifiedAlttpRom && context.constructedMaps && context.graphics && context.collisionAttributes &&
		bg1 && bg2 && bg1->roomIndex==context.roomIndex && bg2->roomIndex==context.roomIndex &&
		bg1->roomDiagnostics && bg2->roomDiagnostics) {
		auto transparent=[&](int i) {
			const int word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
			for(int b=0;b<32;b++)if(context.graphics[uint16_t(context.graphicsBase+(word&1023)*32+b)])return false;
			return true;
		};
		std::vector<int> order;
		for(int i=0;i<4096;i++) {
			const auto &d=bg1->roomDiagnostics->cells[4096+i];
			if(!d.planeObserved)continue;
			if(!d.wall && d.floorId>=0 && bg1->roomDiagnostics->cells[i].floorId<0 && transparent(i) &&
				bg2->roomDiagnostics->cells[4096+i].floorId==d.floorId+8192)(*lowerOwnership)[i]=true;
			if(d.wall && d.lowerFloorDistance>0)order.push_back(i);
		}
		std::sort(order.begin(),order.end(),[&](int a,int b) {
			return bg1->roomDiagnostics->cells[4096+a].lowerFloorDistance<bg1->roomDiagnostics->cells[4096+b].lowerFloorDistance;
		});
		static const int dx[]={0,0,-1,1,-1,1,-1,1},dy[]={-1,1,0,0,-1,-1,1,1};
		for(int i:order) {
			const auto &d=bg1->roomDiagnostics->cells[4096+i];
			const auto &upper=bg1->roomDiagnostics->cells[i];
			const int a=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
			const int b=context.constructedMaps[8192+i*2]|context.constructedMaps[8193+i*2]<<8;
			if(!context.collisionAttributes[4096+i] || upper.lowerFloorDistance>0 ||
				(a!=b && !transparent(i)))continue;
			bool complete=d.uphillDirections!=0;
			for(int k=0;k<8;k++)if(d.uphillDirections&(1<<k)) {
				const int x=i%64-dx[k],y=i/64-dy[k];
				if(x<0 || x>=64 || y<0 || y>=64){complete=false;continue;}
				const int j=y*64+x;const auto &s=bg1->roomDiagnostics->cells[4096+j];
				if(!(*lowerOwnership)[j] || s.lowerFloorId!=d.lowerFloorId ||
					s.lowerFloorDistance+1!=d.lowerFloorDistance)complete=false;
			}
			(*lowerOwnership)[i]=complete;
		}
	}
	auto project=[&](const std::vector<RemasterFramePixel> &pixels,
		std::vector<RemasterRoomPixelDiagnostic> &output) {
		output.assign(pixels.size(),{});
		for(size_t at=0;at<pixels.size();at++) {
			const auto &pixel=pixels[at];
			if(!frame.width || !pixel.instanceId || pixel.instanceId>frame.tileInstances.size())continue;
			const auto &instance=frame.tileInstances[pixel.instanceId-1];
			if(instance.source!=RemasterSourceType::Background || instance.sourceIndex>1)continue;
			const auto room=S9xRemasterDungeonRoomForScreenPosition(context,
				static_cast<uint32_t>(at%frame.width),static_cast<uint32_t>(at/frame.width));
			const auto *map=room==context.roomIndex ? (instance.sourceIndex?bg2:bg1) : (instance.sourceIndex?previousBg2:previousBg1);
			if(!map || map->roomIndex!=room || !map->roomDiagnostics)continue;
			const size_t cell=(((context.backgroundScrollY+at/frame.width)&511)/8)*64+
				((context.backgroundScrollX+at%frame.width)&511)/8;
			if(map->producerHashes[cell]!=instance.tileId.hash ||
				map->producerFlips[cell]!=(instance.hFlip|(instance.vFlip<<1)))continue;
			int32_t highest=INT_MIN;
			for(int p=0;p<2;p++) {
				output[at].planes[p]=map->roomDiagnostics->cells[p*4096+cell];
				const auto &d=output[at].planes[p];
				const int32_t base=d.wall?map->structuralPlaneOffsets[p][cell]:d.floorBase;
				if(base==255)continue;
				if(base>highest){highest=base;output[at].visiblePlane=p;}
				else if(base==highest)output[at].visiblePlane=0xff;
			}
			if(highest==INT_MIN && output[at].planes[0].planeObserved!=output[at].planes[1].planeObserved)
				output[at].visiblePlane=output[at].planes[1].planeObserved?1:0;
			// A final ROM-owned BG2 face with an accepted blocking-plane
			// downhill path is not owned by its nonblocking underpass. This
			// selects diagnostic counts only, never a structural height/base.
			if(highest==INT_MIN && instance.sourceIndex==1 && room==context.roomIndex &&
				context.verifiedAlttpRom && context.collisionAttributes)
				for(int p=0;p<2;p++) {
					const auto &d=output[at].planes[p],&other=output[at].planes[1-p];
					if(d.wall && d.wallTemplateOffset && d.planeObserved && d.lowerFloorDistance>0 &&
						context.collisionAttributes[p*4096+cell]!=0 && context.collisionAttributes[(1-p)*4096+cell]==0 &&
						other.lowerFloorDistance==-1)output[at].visiblePlane=p;
				}
			if(highest==INT_MIN && instance.sourceIndex==0 && room==context.roomIndex && (*lowerOwnership)[cell]) {
				auto &lower=output[at].planes[1];
				output[at].visiblePlane=1;
				if(!lower.wall) {
					const auto &support=bg2->roomDiagnostics->cells[4096+cell];
					lower.floorBase=support.floorBase;lower.heightConflict=support.heightConflict;
				}
			}
		}
	};
	project(frame.mainPixels,frame.mainRoomDiagnostics);
	project(frame.subPixels,frame.subRoomDiagnostics);
	project(frame.scenePixels,frame.sceneRoomDiagnostics);
}

inline bool S9xRemasterReadDungeonHeightMap (const std::string &path,
	RemasterDungeonHeightMap &result)
{
	std::ifstream input(path, std::ios::binary);
	std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	const bool v1 = bytes.size() == 8 + 2 + 64 * 64 &&
		std::memcmp(bytes.data(), "ALTPHM1\0", 8) == 0;
	const bool v2 = bytes.size() == 8 + 2 + 3 * 64 * 64 &&
		std::memcmp(bytes.data(), "ALTPHM2\0", 8) == 0;
	const bool v3 = bytes.size() == 10 + 3 * 4096 * 4 &&
		std::memcmp(bytes.data(), "ALTPHM3\0", 8) == 0;
	if ((!input.eof() && input.fail()) || (!v1 && !v2 && !v3))
		return false;
	const uint16_t room = static_cast<uint16_t>(bytes[8] | (bytes[9] << 8));
	if (room >= 320)
		return false;
	result.roomIndex = room;
	if (v3)
	{
		result.hasPlaneOffsets = true;
		for (int array = 0; array < 3; array++) for (int i = 0; i < 4096; i++)
		{
			const size_t at = 10 + (array * 4096 + i) * 4;
			const uint32_t bits = uint32_t(bytes[at]) | uint32_t(bytes[at+1]) << 8 |
				uint32_t(bytes[at+2]) << 16 | uint32_t(bytes[at+3]) << 24;
			int32_t value; std::memcpy(&value, &bits, sizeof(value));
			if (value < -32767 || value > 32767) return false;
			if (!array) result.offsets[i] = value;
			else { result.planeOffsets[array-1][i] = value; if (value != 255) result.solvedPlaneCells[array-1]++; }
		}
		return true;
	}
	std::copy(bytes.begin() + 10, bytes.begin() + 10 + 4096, result.offsets.begin());
	result.hasPlaneOffsets = v2;
	if (v2)
		for (int plane = 0; plane < 2; plane++)
		{
			std::copy(bytes.begin() + 10 + (plane + 1) * 4096,
				bytes.begin() + 10 + (plane + 2) * 4096, result.planeOffsets[plane].begin());
			for (int32_t value : result.planeOffsets[plane]) if (value != 255) result.solvedPlaneCells[plane]++;
		}
	return true;
}

inline bool S9xRemasterBuildDungeonHeightMap (const RemasterDungeonFloorContext &context,
	uint8_t floorRise, RemasterDungeonHeightMap &result)
{
	result = RemasterDungeonHeightMap{};
	result.roomIndex = context.roomIndex;
	result.offsets.fill(255);
	if (context.placements) for (int i=0;i<4096;i++) {
		result.producerHashes[i]=context.placements[i].tileHash;
		result.producerFlips[i]=context.placements[i].hflip | (context.placements[i].vflip<<1);
		const auto &t=context.placements[i];
		result.producerZeroFloor[i]=t.hasHeight && t.surface==RemasterDungeonFloorContext::Surface::Floor &&
			std::all_of(t.height.begin(),t.height.end(),[](uint8_t h){return h==0;});
	}
	for (auto &plane : result.planeOffsets) plane.fill(255);
	for (auto &plane : result.structuralPlaneOffsets) plane.fill(255);
	for (auto &plane : result.mixedFloorBases) plane.fill(255);
	result.componentCount = result.constraintCount = result.conflictingComponentCount = 0;
	result.solvedPlaneCells.fill(0);
	result.hasPlaneOffsets = false;
	if (!context.verifiedAlttpRom || !context.indoors || !floorRise ||
		!context.collisionAttributes || context.collisionMode != 0)
		return false;

	// The emulator runs on a ~512 KiB thread stack. Keep room-sized scratch
	// on the heap, with fixed bounds and invocation-local ownership (no shared
	// static scratch or retained per-room allocations).
	auto componentStorage = std::make_unique<std::array<int16_t, 2 * 64 * 64>>();
	auto &components = *componentStorage;
	components.fill(-1);
	std::vector<std::vector<uint16_t>> cells;
	auto visitedStorage = std::make_unique<std::array<uint8_t, 2 * 497 * 488>>();
	auto &visited = *visitedStorage;
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
	std::array<bool, 2> activePlanes = {true, true};
	if (context.placements)
	{
		activePlanes = {false, false}; activePlanes[context.linkPlane & 1] = true;
		for (uint8_t i = 0; i < context.stairCount; i++) if (context.stairs[i].changesPlane) activePlanes = {true, true};
		// Detect separate floor usages even without an intra-room connection.
		// Reproduce a complete legal body, not one 00 byte or one floor hash.
		// Do not activate/flood an unobserved plane merely from its artwork.
		auto candidates=std::make_unique<std::array<std::array<bool,4096>,2>>();
		for(int p=0;p<2;p++)for(int y=0;y<61;y++)for(int x=0;x<62;x++) {
			bool legal=true,distinct=false;
			for(int yy=y+1;yy<=y+2;yy++)for(int xx=x;xx<=x+1;xx++) {
				const int i=yy*64+xx;const auto &t=context.placements[i];
				if(!(t.floorPlaneMask&(1<<p)) || context.collisionAttributes[p*4096+i]!=0)legal=false;
				distinct |= t.distinctLowerUsage || context.collisionAttributes[i]!=context.collisionAttributes[4096+i];
			}
			if(!legal || (p==1 && !distinct))continue;
			for(int yy=y+1;yy<=y+2;yy++)for(int xx=x;xx<=x+1;xx++)(*candidates)[p][yy*64+xx]=true;
		}
		for(int p=0;p<2;p++)for(bool cell:(*candidates)[p])if(cell) {
			result.candidateFloorCells[p]++;
			if(!activePlanes[p])result.unobservedFloorCells[p]++;
		}
		result.disconnectedPlaneCandidates = context.stairCount==0 &&
			result.candidateFloorCells[0]>0 && result.candidateFloorCells[1]>0;
		result.planeRelationshipUnresolved = result.disconnectedPlaneCandidates;
	}
	seeds.push_back((context.linkPlane * 488 + (context.linkY & 511)) * 497 + (context.linkX & 511));
	for (uint8_t i = 0; i < context.stairCount; i++)
	{
		const auto &stair = context.stairs[i];
		const int x = stair.x * 8 + 8, north = stair.y * 8 - 24, south = stair.y * 8 + 16;
		for (int plane = 0; plane < 2; plane++)
			for (int y : { north, south })
				if (activePlanes[plane] && x >= 0 && x <= 496 && y >= 0 && y <= 487)
					seeds.push_back((plane * 488 + y) * 497 + x);
	}
	// Seed legal bodies from completed collision, not the currently selected
	// artwork's floor label. A bridge can hide every classified seed in a
	// connected lower corridor. Special/pit attributes still reject bodies.
	if (context.placements)
		for (int p = 0; p < 2; p++) for (int y = 0; y < 61; y++) for (int x = 0; x < 62; x++)
			if (activePlanes[p] &&
				walkable(p, x * 8, y * 8, 0) && walkable(p, x * 8, y * 8, 1) &&
				walkable(p, x * 8, y * 8, 2) && walkable(p, x * 8, y * 8, 3))
				seeds.push_back((p * 488 + y * 8) * 497 + x * 8);
	for (uint32_t seed : seeds)
	{
		if (seed >= visited.size() || visited[seed]) continue;
		if (context.placements)
		{
			const int x = seed % 497, row = seed / 497, p = row / 488, y = row % 488;
			bool legal = true;
			for (int yy = (y + 8) / 8; yy <= (y + 23) / 8; yy++)
				for (int xx = x / 8; xx <= (x + 15) / 8; xx++)
					if (context.collisionAttributes[p * 4096 + yy * 64 + xx] != 0) legal = false;
			if (!legal) continue;
		}
		const int16_t id = static_cast<int16_t>(cells.size());
		cells.push_back({});
		std::vector<uint32_t> queue(1, seed);
		visited[seed] = 1;
		for (size_t cursor = 0; cursor < queue.size(); cursor++)
		{
			const uint32_t node = queue[cursor];
			const int x = node % 497, row = node / 497, plane = row / 488, y = row % 488;
			// Project the complete 16x16 collision body onto support cells.  Using
			// only its four corners erodes ordinary floors and leaves most visible
			// placements unknown; filling those gaps later from the composited
			// viewport can incorrectly join an upper bridge to its underpass.
			for (int cellY = (y + 8) / 8; cellY <= (y + 23) / 8; cellY++)
				for (int cellX = x / 8; cellX <= (x + 15) / 8; cellX++)
				{
					const int index = cellY * 64 + cellX;
					if (context.collisionAttributes[plane * 4096 + index] == 0 &&
						components[plane * 4096 + index] < 0)
					{
						components[plane * 4096 + index] = id;
						cells[id].push_back(static_cast<uint16_t>(plane * 4096 + index));
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
	if (constraints.empty() && !context.placements)
		return false;
	result.componentCount = static_cast<uint16_t>(std::min<size_t>(UINT16_MAX, cells.size()));
	result.constraintCount = static_cast<uint16_t>(std::min<size_t>(UINT16_MAX, constraints.size()));

	if (context.placements)
	{
		// Geometry graph retains independent gameplay planes. Cardinal movement
		// components are support nodes; authored flat plateaus and walls add
		// geometric connections without adding illegal movement edges.
		struct Edge { int a, b, rise; };
		std::vector<Edge> edges;
		std::vector<RemasterRoomDiagnosticContact> diagnosticContacts;
		auto nodeStorage = std::make_unique<std::array<int, 8192>>();
		auto &nodes = *nodeStorage;
		nodes.fill(-1);
		int count = static_cast<int>(cells.size());
		for (int p = 0; p < 2; p++) for (int i = 0; i < 4096; i++)
		{
			if (!activePlanes[p]) continue;
			const auto &tile = context.placements[i];
			if (tile.surface == RemasterDungeonFloorContext::Surface::Floor &&
				!(tile.structuralFloorPlaneMask & (1 << p)) && components[p * 4096 + i] < 0) continue;
			if (tile.hasHeight && tile.surface == RemasterDungeonFloorContext::Surface::Wall)
				nodes[p * 4096 + i] = count++;
			else if (components[p * 4096 + i] >= 0) nodes[p * 4096 + i] = components[p * 4096 + i];
			else if (tile.hasHeight && (tile.surface == RemasterDungeonFloorContext::Surface::Wall ||
				tile.surface == RemasterDungeonFloorContext::Surface::Floor ||
				tile.surface == RemasterDungeonFloorContext::Surface::Obstacle)) nodes[p * 4096 + i] = count++;
		}
		auto height = [&] (int i, int x, int y) {
			const auto &tile = context.placements[i];
			return static_cast<int>(tile.height[(tile.vflip ? 7 - y : y) * 8 + (tile.hflip ? 7 - x : x)]);
		};
		auto direction = [&] (int i, int dx, int dy) {
			int low = 0, high = 0;
			for (int k = 0; k < 8; k++)
			{
				low += height(i, dx ? (dx > 0 ? 7 : 0) : k, dy ? (dy > 0 ? 7 : 0) : k);
				high += height(i, dx ? (dx > 0 ? 0 : 7) : k, dy ? (dy > 0 ? 0 : 7) : k);
			}
			return low < high;
		};
		struct CornerRamp { int ax=0,ay=0,sx=0,sy=0; bool maximum=false,exact=false; };
		auto cornerModels=std::make_unique<std::array<CornerRamp,4096>>();
		for(int i=0;i<4096;i++){
			if(context.placements[i].surface!=RemasterDungeonFloorContext::Surface::Wall)continue;
			// Exact axis-ramp corners are min/max compositions. At the final
			// corner pixel the last raster derivative can be zero even though
			// the owning face continues one unit beyond the boundary. Recover
			// only a complete exact two-ramp model, never an averaged slope.
			for(bool maximum : {false,true}) {
				int ax=0,ay=0,sx=0,sy=0;bool foundX=false,foundY=false;
				for(int yy=0;yy<8;yy++)for(int xx=0;xx<7;xx++){
					int slope=height(i,xx+1,yy)-height(i,xx,yy);
					if(slope && !foundX){sx=slope;ax=height(i,xx,yy)-sx*xx;foundX=true;}
				}
				for(int yy=0;yy<7;yy++)for(int xx=0;xx<8;xx++){
					int slope=height(i,xx,yy+1)-height(i,xx,yy);
					if(slope && !foundY){sy=slope;ay=height(i,xx,yy)-sy*yy;foundY=true;}
				}
				if(!foundX || !foundY)continue;
				auto model=[&](int xx,int yy){return maximum?std::max(ax+sx*xx,ay+sy*yy):std::min(ax+sx*xx,ay+sy*yy);};
				bool exact=true;for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)if(height(i,xx,yy)!=model(xx,yy))exact=false;
				if(exact){(*cornerModels)[i]={ax,ay,sx,sy,maximum,true};break;}
			}
		}
		auto boundaryStep = [&] (int i, int dx, int dy, int k) {
			const int x=dx?(dx>0?7:0):k,y=dy?(dy>0?7:0):k;
			const int h=height(i,x,y),inner=height(i,x-dx,y-dy);
			const auto &m=(*cornerModels)[i];
			if(m.exact){const int a=m.ax+m.sx*(x+dx),b=m.ay+m.sy*(y+dy);return std::abs((m.maximum?std::max(a,b):std::min(a,b))-h);}
			return std::abs(h-inner);
		};
		auto diagonal = [&] (int i) {
			// A diagonal may terminate in a flat cap or floor half-plane. Require
			// an exact clipped affine profile, not averaged normals or a corner.
			int lo = 255, hi = 0;
			for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++)
				{ lo = std::min(lo, height(i,x,y)); hi = std::max(hi, height(i,x,y)); }
			for (int y = 0; y < 7; y++) for (int x = 0; x < 7; x++)
			{
				const int h = height(i,x,y), sx = height(i,x+1,y)-h, sy = height(i,x,y+1)-h;
				if (!sx || !sy) continue;
				bool exact = true;
				for (int yy = 0; yy < 8; yy++) for (int xx = 0; xx < 8; xx++)
					if (height(i,xx,yy) != std::max(lo, std::min(hi, h + sx*(xx-x) + sy*(yy-y)))) exact = false;
				if (exact) return true;
			}
			return false;
		};
		auto diagonalStorage = std::make_unique<std::array<bool, 4096>>();
		auto &diagonals = *diagonalStorage;
		for (int i = 0; i < 4096; i++) diagonals[i] =
			context.placements[i].surface == RemasterDungeonFloorContext::Surface::Wall && diagonal(i);
		auto diagnosticStorage=std::make_shared<RemasterRoomDiagnostics>();
		auto requiredDownhill=std::make_unique<std::array<uint8_t,8192>>();
		static const int diagnosticDx[8]={0,0,-1,1,-1,1,-1,1};
		static const int diagnosticDy[8]={-1,1,0,0,-1,-1,1,1};
		std::vector<int> diagnosticSupportId(cells.size(),-1);
		for(int encoded=0;encoded<8192;encoded++)if(components[encoded]>=0 && diagnosticSupportId[components[encoded]]<0)
			diagnosticSupportId[components[encoded]]=encoded+context.placementBackgroundIndex*8192;
		for(int p=0;p<2;p++) for(int i=0;i<4096;i++) {
			auto &d=diagnosticStorage->cells[p*4096+i];
			d.room=context.roomIndex;d.cell=i;d.plane=p;d.background=context.placementBackgroundIndex;
			d.planeObserved=activePlanes[p];
			const auto &tile=context.placements[i];
			d.wall=tile.surface==RemasterDungeonFloorContext::Surface::Wall || tile.diagnosticWallTemplateOffset;
			d.wallTemplateOffset=tile.diagnosticWallTemplateOffset?tile.diagnosticWallTemplateOffset:tile.wallTemplateOffset;
			d.wallSource=!d.wall?RemasterRoomDiagnostic::WallSource::None:d.wallTemplateOffset?
				RemasterRoomDiagnostic::WallSource::RomAssembly:RemasterRoomDiagnostic::WallSource::AuthoredRole;
			d.axisUnknown=d.wall;
			if(!d.wall)continue;
			int sx=0,sy=0;bool valid=true;
			for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
				if(x<7){int v=height(i,x+1,y)-height(i,x,y);if(v){int s=v>0?1:-1;if(sx && sx!=s)valid=false;sx=s;}}
				if(y<7){int v=height(i,x,y+1)-height(i,x,y);if(v){int s=v>0?1:-1;if(sy && sy!=s)valid=false;sy=s;}}
			}
			if(!tile.hasHeight)valid=false;
			// Clipped diagonal relief remains a diagonal even when the solver
			// deliberately deferred this member's role (caps/mixed floor feet).
			int displayedNormal=0;
			for(int k=0;k<8;k++)if(tile.diagnosticNormalUphill&(1<<k))
				for(int j=0;j<8;j++)if(diagnosticDx[j]==diagnosticDx[k]*(tile.hflip?-1:1) &&
					diagnosticDy[j]==diagnosticDy[k]*(tile.vflip?-1:1))displayedNormal|=1<<j;
			// Quantized diagonal bands (floor((x+y)/2), clipped caps) are not
			// affine in integer pixel samples. ROM diagonal ownership PLUS a
			// agreeing authored diagonal normal disambiguates these from corners.
			bool normalDiagonal=false;
			if(d.wallTemplateOffset>=0x420 && d.wallTemplateOffset<=0x506)
				for(int k=4;k<8;k++)if((displayedNormal&(1<<k)) && diagnosticDx[k]==sx && diagnosticDy[k]==sy)normalDiagonal=true;
			const bool diagnosticDiagonal=valid && sx && sy && (diagonal(i) || normalDiagonal);
			for(int k=0;valid && k<(diagnosticDiagonal?8:4);k++) {
				const int dx=diagnosticDx[k],dy=diagnosticDy[k];
				if(diagnosticDiagonal ? (dx==sx && dy==sy) : (dx ? dx==sx : dy==sy))
					d.uphillDirections|=1<<k;
			}
			// Handler ownership supplies an axis on flat caps only. Never use
			// lighting roughness/default normals to repair nonmonotone relief.
			if(!tile.hasHeight || (valid && !sx && !sy))d.uphillDirections=tile.diagnosticUphillDirections;
			if(!d.uphillDirections && tile.diagnosticWallTemplateOffset)d.uphillDirections=displayedNormal;
			d.axisUnknown=!d.uphillDirections;
			for(int k=0;k<8;k++)if(d.uphillDirections&(1<<k)) {
				for(int j=0;j<8;j++)if(diagnosticDx[j]==-diagnosticDx[k] && diagnosticDy[j]==-diagnosticDy[k])
					(*requiredDownhill)[p*4096+i]|=1<<j;
			}
		}
		auto acceptEdge=[&](int p,int i,int j,int dx,int dy,int a,int b,int rise) {
			(void)p;(void)i;(void)j;(void)dx;(void)dy;
			edges.push_back({a,b,rise}); // unchanged height solve
		};
		for (int p = 0; p < 2; p++) for (int i = 0; i < 4096; i++)
		{
			const auto &tile = context.placements[i];
			if (!tile.hasHeight || nodes[p * 4096 + i] < 0) continue;
			// In a reproduced two-plane room, different BG1 artwork is not the
			// visible upper-plane assembly. Same-plane rooms can use different
			// BGs for ordinary structures, so differing words alone do not suffice.
			// Movement still owns its support node, but it cannot use the hidden
			// lower artwork to constrain the upper support's structural seams.
			if (context.placementBackgroundIndex == 0 && p == 0 && activePlanes[1] && tile.distinctLowerUsage) continue;
			// A visible wall over nonblocking support on another gameplay plane
			// is an occluder, not a ramp connecting that underpass's floor.
			if (tile.surface == RemasterDungeonFloorContext::Surface::Wall &&
				context.collisionAttributes[p * 4096 + i] == 0) continue;
			for (const auto &d : {std::array<int, 2>{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}})
			{
				const int dx = d[0], dy = d[1], x = i % 64 + dx, y = i / 64 + dy;
				if (x < 0 || x >= 64 || y < 0 || y >= 64) continue;
				const int j = y * 64 + x, a = nodes[p * 4096 + i], b = nodes[p * 4096 + j];
				const auto &other = context.placements[j];
				if (b < 0 || !other.hasHeight) continue;
				if (context.placementBackgroundIndex == 0 && p == 0 && activePlanes[1] && other.distinctLowerUsage) continue;
				const bool wall = tile.surface == RemasterDungeonFloorContext::Surface::Wall;
				const bool otherWall = other.surface == RemasterDungeonFloorContext::Surface::Wall;
				// Occupied space does not make visible unknown/overlay artwork a
				// structural floor seam. Its authored relief can sit ABOVE the
				// support (notably the lower plane beneath a bridge).
				const bool coveredSupport = wall && !otherWall &&
					components[p * 4096 + j] >= 0 && other.surface == RemasterDungeonFloorContext::Surface::Unknown;
				if (wall && !otherWall && !coveredSupport && other.surface != RemasterDungeonFloorContext::Surface::Floor &&
					other.surface != RemasterDungeonFloorContext::Surface::Obstacle) continue;
				if (dx && dy)
				{
					if (!wall || !diagonals[i]) continue;
					// A corner-only contact cannot identify which region of a
					// clipped cap/floor owns the seam. Only complete affine faces
					// may supply these point contacts.
					auto affine = [&] (int cell) {
						const int h = height(cell,0,0), sx = height(cell,1,0)-h, sy = height(cell,0,1)-h;
						for (int yy=0; yy<8; yy++) for (int xx=0; xx<8; xx++)
							if (height(cell,xx,yy) != h+sx*xx+sy*yy) return false;
						return true;
					};
					if (!affine(i) || (otherWall && !affine(j))) continue;
					// A rasterized assembly's full edge contacts own the geometry;
					// do not short-circuit them through the corner of a neighboring
					// placement (often a different cap/floor region).
					if (context.placements[(i / 64) * 64 + x].hasHeight ||
						context.placements[y * 64 + i % 64].hasHeight) continue;
					const bool downX = direction(i, dx, 0), downY = direction(i, 0, dy);
					if (downX != downY || (otherWall && (!diagonals[j] || direction(j, dx, 0) != downX || direction(j, 0, dy) != downY))) continue;
					if (otherWall && context.collisionAttributes[p * 4096 + j] == 0) continue;
					const int h = height(i, dx > 0 ? 7 : 0, dy > 0 ? 7 : 0);
					const int oh = other.surface == RemasterDungeonFloorContext::Surface::Obstacle ? 0 : height(j, dx > 0 ? 0 : 7, dy > 0 ? 0 : 7);
					const int step = std::abs(h - height(i, dx > 0 ? 6 : 1, dy > 0 ? 6 : 1));
					acceptEdge(p,i,j,dx,dy,a,b,h - oh + (!downX ? step : otherWall ? -step : 0));
					continue;
				}
				// Diagonal raster assemblies share cardinal edges too. Test all
				// eight seam samples below instead of dropping those contacts.
				if (otherWall && context.collisionAttributes[p * 4096 + j] == 0) continue;
				if (wall)
				{
					// Occupancy under unknown artwork proves the wall's foot only
					// on its downhill contact. It cannot supply an uphill plateau
					// seam from an unclassified zero/local array.
					if (coveredSupport && !direction(i,dx,dy)) continue;
					// Uphill/downhill sequences and compatible full wall tangent
					// seams establish a relation. Tangents require every sample to
					// agree below; artwork similarity alone never supplies an edge.
					if (!direction(i, dx, dy) && !direction(i, -dx, -dy) && !otherWall) continue;
					if (otherWall && direction(i, dx, dy) != direction(j, dx, dy)) continue;
				}
				else if (tile.supportingTrim && !otherWall && components[p*4096+j]>=0)
				{
					edges.push_back({a,b,0});continue; // proven assembly trim inherits support, relief stays local
				}
				else if (tile.surface == RemasterDungeonFloorContext::Surface::Obstacle &&
					other.surface == RemasterDungeonFloorContext::Surface::Unknown && components[p*4096+j]>=0)
				{
					// Authored obstacle usage can rest beside reproduced ordinary
					// support even when its visible floor artwork is unclassified.
					// Contact relates SUPPORT bases, not either tile's local relief.
					// Keep planes independent; do not traverse obstacle assemblies.
					edges.push_back({a,b,0});continue;
				}
				else if (otherWall) continue; // reciprocal wall iteration owns this edge
				else if (!((tile.surface == RemasterDungeonFloorContext::Surface::Floor &&
					(other.surface == RemasterDungeonFloorContext::Surface::Floor || other.surface == RemasterDungeonFloorContext::Surface::Obstacle)) ||
					(tile.surface == RemasterDungeonFloorContext::Surface::Obstacle && other.surface == RemasterDungeonFloorContext::Surface::Floor))) continue;
				int rise = INT_MIN;
				bool compatible = true;
				for (int k = 0; k < 8; k++)
				{
					const int h = tile.surface == RemasterDungeonFloorContext::Surface::Obstacle ? 0 :
						height(i, dx ? (dx > 0 ? 7 : 0) : k, dy ? (dy > 0 ? 7 : 0) : k);
					const int oh = coveredSupport || other.surface == RemasterDungeonFloorContext::Surface::Obstacle ? 0 :
						height(j, dx ? (dx > 0 ? 0 : 7) : k, dy ? (dy > 0 ? 0 : 7) : k);
					// Extend the authored edge slope to the tile boundary; do not
					// substitute a fixed eight-unit rise for variable authored ramps.
					const int inner = height(i, dx ? (dx > 0 ? 6 : 1) : k, dy ? (dy > 0 ? 6 : 1) : k);
					const int step = wall && otherWall && direction(i, dx, dy) ?
						boundaryStep(j,-dx,-dy,k) : wall ? boundaryStep(i,dx,dy,k) : std::abs(h-inner);
					const int value = h - oh + (wall && !direction(i, dx, dy) ? step :
						wall && otherWall ? -step : 0);
					if (rise != INT_MIN && value != rise) compatible = false;
					rise = value;
				}
				if (compatible) acceptEdge(p,i,j,dx,dy,a,b,rise);
			}
		}
		std::vector<std::vector<std::pair<int, int>>> adjacency(count);
		auto add = [&] (int a, int b, int rise) {
			adjacency[a].push_back({b, rise}); adjacency[b].push_back({a, -rise});
		};
		for (const auto &e : edges) add(e.a, e.b, e.rise);
		// One final ROM-owned face is one physical surface even when it borders
		// supports on different gameplay planes. Require identical paired final
		// artwork and solid usage on BOTH planes; hidden/differing artwork and a
		// nonblocking underpass never license this equality. Share only structural
		// nodes: support components remain independent at overlapping coordinates.
		auto solidFace=[](uint8_t a){return a==1 || a==2 || a==0x28 || a==0x2a;};
		for(int i=0;context.placementBackgroundIndex==0 && i<4096;i++) {
			const auto &t=context.placements[i];
			if(t.surface!=RemasterDungeonFloorContext::Surface::Wall || !t.wallTemplateOffset ||
				(context.bg1GraphicsBase!=0xffff && context.bg1GraphicsBase!=context.graphicsBase) ||
				t.distinctLowerUsage || !solidFace(context.collisionAttributes[i]) ||
				!solidFace(context.collisionAttributes[4096+i]) || nodes[i]<0 || nodes[4096+i]<0)continue;
			add(nodes[i],nodes[4096+i],0);result.sharedStructuralFaces++;
		}
		if(context.supportRelations)for(const auto &r:*context.supportRelations) {
			if(r.a<0 || r.a>=8192 || r.b<0 || r.b>=8192)continue;
			const int a=components[r.a],b=components[r.b];
			if(a>=0 && b>=0 && r.rise>0 && std::any_of(constraints.begin(),constraints.end(),
				[&](const Constraint &e){return e.low==a && e.high==b;}))add(a,b,r.rise);
		}
		// Export only consistent authored-path differences between signed stair
		// endpoints, never configured rises or normalized placement bases.
		std::vector<int> representative(cells.size(),-1), authoredBase(count,INT_MIN);
		for(size_t n=0;n<cells.size();n++)if(!cells[n].empty())representative[n]=cells[n][0];
		for(size_t start=0;start<cells.size();start++) {
			if(representative[start]<0 || authoredBase[start]!=INT_MIN)continue;
			std::vector<int> queue(1,int(start));authoredBase[start]=0;bool conflict=false;
			for(size_t k=0;k<queue.size();k++)for(auto next:adjacency[queue[k]]) {
				const int v=authoredBase[queue[k]]+next.second;
				if(authoredBase[next.first]==INT_MIN){authoredBase[next.first]=v;queue.push_back(next.first);}
				else if(authoredBase[next.first]!=v)conflict=true;
			}
			// Cross-BG identity is proven for reproduced signed stair endpoints,
			// not arbitrary occupied cells beneath differing overlay artwork.
			if(!conflict)for(const auto &e:constraints)
				if(std::find(queue.begin(),queue.end(),e.low)!=queue.end() &&
					std::find(queue.begin(),queue.end(),e.high)!=queue.end())
					result.authoredSupportRelations.push_back({representative[e.low],representative[e.high],authoredBase[e.high]-authoredBase[e.low]});
		}
		// Stair direction remains signed evidence. Its configured rise is a
		// fallback only when no authored wall path already relates its landings.
		for (const auto &e : constraints)
		{
			std::vector<bool> seen(count, false);
			std::vector<int> queue(1, e.low); seen[e.low] = true;
			for (size_t n = 0; n < queue.size(); n++) for (auto next : adjacency[queue[n]])
				if (!seen[next.first]) { seen[next.first] = true; queue.push_back(next.first); }
			if (!seen[e.high]) { add(e.low, e.high, floorRise); result.configuredStairFallbacks++; }
		}
		std::vector<int> bases(count, INT_MIN);
		std::vector<bool> bad(count, false);
		std::vector<bool> supportNode(count, false), flatNode(count, false);
		for (size_t i = 0; i < cells.size(); i++) supportNode[i] = !cells[i].empty();
		for (int encoded = 0; encoded < 8192; encoded++)
			if (nodes[encoded] >= 0 && context.placements[encoded % 4096].surface == RemasterDungeonFloorContext::Surface::Floor)
				flatNode[nodes[encoded]] = true;
		// Every geometric component with established support has its own relative
		// datum. Entry is not a low-floor anchor: solve signed differences first,
		// then apply the explicit lowest-support-relative-zero convention.
		const bool entryStateMatches=context.entrySupport.destinationState &&
			context.entrySupport.destinationState==S9xRemasterDungeonEntryStateKey(context,floorRise);
		for (int start = 0; start < count; start++)
		{
			if (bases[start] != INT_MIN || (!supportNode[start] && !flatNode[start])) continue;
			bases[start] = 0;
			std::vector<int> queue(1, start);
			bool conflict = false;
			int mismatches = 0;
			for (size_t n = 0; n < queue.size(); n++) for (auto next : adjacency[queue[n]])
			{
				const int value = bases[queue[n]] + next.second;
				if (bases[next.first] == INT_MIN) { bases[next.first] = value; queue.push_back(next.first); }
				else if (bases[next.first] != value)
				{
					conflict = true;
					mismatches++;
					if (result.geometryConflicts.size() < 64)
						result.geometryConflicts.push_back({queue[n], next.first, value, bases[next.first]});
				}
			}
			int minimum = INT_MAX, supports = 0;
			for (int node : queue) if (supportNode[node]) { minimum = std::min(minimum, bases[node]); supports++; }
			const bool isolatedFlat = supports == 0;
			if (isolatedFlat) for (int node : queue) if (flatNode[node]) minimum = std::min(minimum, bases[node]);
			const auto &entry=context.entrySupport;
			const int entryNode=entry.cell>=0 && entry.cell<8192 ? components[entry.cell] : -1;
			const bool anchored=entryStateMatches && entryNode>=0 && entry.predecessorRoom==context.previousRoomIndex &&
				entry.predecessorRoom!=0xffff && entry.predecessorRoom!=context.roomIndex &&
				entry.predecessorState && entry.transitionEvidence &&
				std::find(queue.begin(),queue.end(),entryNode)!=queue.end();
			if(!anchored)result.relativeAnchors.push_back({start, minimum, supports, isolatedFlat});
			const int datum=anchored ? bases[entryNode]-entry.base : minimum;
			for (int node : queue) bases[node] -= datum;
			if(anchored && !conflict)result.entrySupportConsumed=true;
			// Normalization is a declared relative datum, never conflict repair.
			// A support-anchored wall can descend below that datum into a pit;
			// no lower-floor anchor is required. Isolated collidable flat caps
			// do not license negative walls: their legacy relative-only datum is
			// retained for nonnegative review geometry, not new ground evidence.
			// Contradictory equalities and representation overflow still fail.
			int negative = 0, overflow = 0;
			for (int node : queue) if ((isolatedFlat && bases[node] < 0) ||
				bases[node] < -32767 || bases[node] > 32767 || bases[node] == 255)
			{
				if (bases[node] < 0) negative++; else overflow++;
				// Representation failure is placement-local, not a contradictory
				// equality. Preserve other solved members of this relative graph.
				bad[node] = true;
				if (result.geometryConflicts.size() < 64) result.geometryConflicts.push_back({node, node, 0, bases[node]});
			}
			for (const auto &e : constraints) if (bases[e.low] != INT_MIN && bases[e.high] != INT_MIN &&
				bases[e.high] <= bases[e.low]) conflict = true;
			if (conflict) for (int node : queue) bad[node] = true;
			if (conflict || negative || overflow) result.geometryGroupIssues.push_back({start, static_cast<int>(queue.size()), mismatches, negative, overflow, isolatedFlat});
		}
		result.hasRoomGeometry = result.hasPlaneOffsets = true;
		for(int encoded=0;encoded<8192;encoded++) {
			auto &d=diagnosticStorage->cells[encoded];
			const int n=nodes[encoded];
			if(n<0)continue;
			d.heightConflict=bad[n];
			// Backdrop/isolated flat art is not a floor component. Use the
			// independently reproduced support component, never a plateau node.
			if(!d.wall && components[encoded]>=0 && (context.placementBackgroundIndex==1 ||
				context.placements[encoded%4096].surface==RemasterDungeonFloorContext::Surface::Floor ||
				context.placements[encoded%4096].diagnosticAuthoredFloor ||
				context.placements[encoded%4096].diagnosticRomFloorTrim)) {
				d.floorId=diagnosticSupportId[components[encoded]];
				if(!bad[n] && bases[n]!=INT_MIN)d.floorBase=bases[n];
			}
		}
		// Tile-distance topology is independent of seam-height compatibility.
		// Follow only the displayed downhill axis through proven wall members
		// and stop at reproduced support on THIS plane. Nonblocking wall art
		// above an underpass is an occluder, never a path on that plane.
		for(int p=0;p<2;p++)for(int i=0;i<4096;i++) {
			const int encoded=p*4096+i;const auto &d=diagnosticStorage->cells[encoded];
			if(!activePlanes[p] || !d.wall || context.collisionAttributes[encoded]==0)continue;
			for(int k=0;k<8;k++)if((*requiredDownhill)[encoded]&(1<<k)) {
				const int x=i%64+diagnosticDx[k],y=i/64+diagnosticDy[k];
				if(x<0 || x>=64 || y<0 || y>=64)continue;
				const int j=y*64+x;const auto &other=diagnosticStorage->cells[p*4096+j];
				if(other.wall) {
					if(context.collisionAttributes[p*4096+j]==0 ||
						!((*requiredDownhill)[p*4096+j]&(1<<k)))continue;
					if(d.wallTemplateOffset && other.wallTemplateOffset &&
						d.wallTemplateOffset!=other.wallTemplateOffset) {
						// High perimeter and low face templates are emitted as
						// complementary strips on the same axis. Exact ownership
						// and reciprocal downhill axis prove this structural join;
						// authored seam equality remains irrelevant to tile count.
						bool complementary=false;
						for(const auto &pair:{std::array<int,2>{0x2e8,0x328},{0x2f8,0x338},{0x2c8,0x308},{0x2d8,0x318},
							{0x470,0x4e8},{0x47a,0x4f2},{0x484,0x4fc},{0x48e,0x506}})
							if((d.wallTemplateOffset==pair[0] && other.wallTemplateOffset==pair[1]) ||
								(d.wallTemplateOffset==pair[1] && other.wallTemplateOffset==pair[0]))complementary=true;
						if(!complementary)continue;
					}
				} else if(other.floorId<0 || components[p*4096+j]<0 ||
					context.collisionAttributes[p*4096+j]!=0)continue;
				// Diagonal point contacts require assembly ownership and both
				// intervening edge cells to be wall/support, not corner hopping
				// over unknown artwork, a blocker, door, or an unrelated object.
				if(k>=4) {
					if(!d.wallTemplateOffset)continue;
					bool compatible=true;
					for(int side:{i/64*64+x,y*64+i%64}) {
						const auto &s=diagnosticStorage->cells[p*4096+side];
						if(!(s.wall && (s.wallTemplateOffset==d.wallTemplateOffset ||
							(s.wallTemplateOffset>=0x420 && s.wallTemplateOffset<=0x506 &&
							(*requiredDownhill)[p*4096+side]&(1<<k)))) &&
							!(s.floorId>=0 && components[p*4096+side]>=0))compatible=false;
					}
					if(!compatible)continue;
				}
				diagnosticContacts.push_back({encoded,p*4096+j,static_cast<uint8_t>(1<<k)});
			}
		}
		S9xRemasterResolveRoomDiagnosticDistances(*diagnosticStorage,diagnosticContacts,*requiredDownhill);
		result.roomDiagnostics=std::move(diagnosticStorage);
		for (bool value : bad) if (value) result.structuralConflicts++;
		for (const auto &issue : result.geometryConflicts)
			for (int encoded = 0; encoded < 8192; encoded++) if ((nodes[encoded] == issue.node || nodes[encoded] == issue.other) &&
				context.placements[encoded % 4096].surface == RemasterDungeonFloorContext::Surface::Wall)
				if (result.geometryIssuePlacements.size() < 128)
					result.geometryIssuePlacements.push_back({encoded / 4096, encoded % 64, (encoded % 4096) / 64, nodes[encoded]});
		for (int p = 0; p < 2; p++) for (int i = 0; i < 4096; i++)
		{
			const int node = components[p * 4096 + i] >= 0 ? components[p * 4096 + i] : nodes[p * 4096 + i];
			if (components[p * 4096 + i] >= 0) result.requiredPlaneCells[p]++;
			if (node >= 0 && bases[node] != INT_MIN && !bad[node])
			{
				result.planeOffsets[p][i] = bases[node];
				if (result.offsets[i] == 255 || bases[node] > result.offsets[i]) result.offsets[i] = bases[node];
				if (components[p * 4096 + i] >= 0) result.solvedPlaneCells[p]++;
			}
			else if (components[p * 4096 + i] >= 0) result.unknownPlaneCells[p]++;
		}
		// Visible wall art and support beneath it are distinct surfaces. Do not
		// collapse a nonblocking wall picture into the actor's floor component.
		for (int i = 0; i < 4096; i++) if (context.placements[i].surface == RemasterDungeonFloorContext::Surface::Wall)
		{
			result.offsets[i] = 255;
			for (int p = 0; p < 2; p++)
			{
				const int node = nodes[p * 4096 + i];
				if (node >= 0 && bases[node] != INT_MIN && !bad[node])
				{
					result.structuralPlaneOffsets[p][i] = bases[node];
					if (result.offsets[i] == 255 || bases[node] > result.offsets[i]) result.offsets[i] = bases[node];
				}
			}
		}
		// A solid floor-pattern cap is not a support datum. Attach it only to
		// the high edge of an already solved, ROM-template-proven face. This
		// one-cell terminal attachment cannot join independent wall chains or
		// propagate through repeated pattern art. All accepted contacts must agree.
		for (int p=0;p<2;p++) for (int i=0;i<4096;i++)
		{
			const auto &cap=context.placements[i];
			if (!activePlanes[p] || cap.surface!=RemasterDungeonFloorContext::Surface::Floor ||
				!cap.hasHeight || (cap.structuralFloorPlaneMask & (1<<p)) || components[p*4096+i]>=0) continue;
			int candidate=INT_MIN; bool conflict=false;
			for (const auto &d : {std::array<int,2>{1,0},{-1,0},{0,1},{0,-1}})
			{
				const int dx=d[0],dy=d[1],x=i%64-dx,y=i/64-dy;
				if(x<0 || x>=64 || y<0 || y>=64)continue;
				const int j=y*64+x;const auto &wall=context.placements[j];
				if(wall.surface!=RemasterDungeonFloorContext::Surface::Wall || !wall.wallTemplateOffset ||
					result.structuralPlaneOffsets[p][j]==255 || !direction(j,-dx,-dy))continue;
				int value=INT_MIN;bool compatible=true;
				for(int k=0;k<8;k++){
					const int edge=height(j,dx?(dx>0?7:0):k,dy?(dy>0?7:0):k);
					const int inner=height(j,dx?(dx>0?6:1):k,dy?(dy>0?6:1):k);
					const int v=result.structuralPlaneOffsets[p][j]+edge+std::abs(edge-inner)-
						height(i,dx?(dx>0?0:7):k,dy?(dy>0?0:7):k);
					if(value!=INT_MIN && value!=v)compatible=false;value=v;
				}
				if(!compatible)continue;
				if(candidate!=INT_MIN && candidate!=value)conflict=true;candidate=value;
			}
			if(conflict){result.structuralConflicts++;continue;}
			if(candidate==INT_MIN || candidate < -32767 || candidate > 32767 || candidate==255)continue;
			result.planeOffsets[p][i]=candidate;
			if(result.offsets[i]==255 || candidate>result.offsets[i])result.offsets[i]=candidate;
		}
		// Ground the zero/+Z foot region through pixel-connected mixed-wall
		// assemblies to established ordinary floors, independently per plane.
		// This is a lower bound on displayed geometry, NOT a graph equality or
		// a change to the authored slope/cap. Unknown/multiple contacts stay explicit.
		auto foot = [&] (int i, int x, int y) {
			const auto &t = context.placements[i];
			return t.surface == RemasterDungeonFloorContext::Surface::Wall && t.hasHeight &&
				((t.groundedFloorMask >> ((t.vflip ? 7-y : y)*8 + (t.hflip ? 7-x : x))) & 1);
		};
		auto seenStorage = std::make_unique<std::array<uint8_t, 4096 * 64>>();
		auto statusStorage = std::make_unique<std::array<uint8_t,4096>>();
		for (int p = 0; p < 2; p++)
		{
			if (!activePlanes[p]) continue;
			seenStorage->fill(0);
			statusStorage->fill(0);
			for (int start = 0; start < 4096 * 64; start++)
			{
				const int i = start / 64, px = start % 8, py = (start % 64) / 8;
				if ((*seenStorage)[start] || !foot(i,px,py) ||
					context.collisionAttributes[p*4096+i] == 0) continue;
				std::vector<int> queue(1,start), members;
				(*seenStorage)[start] = 1;
				int datum = -1; bool conflict = false, unknown = false;
				for (size_t n = 0; n < queue.size(); n++)
				{
					int cell = queue[n]/64, x = cell%64*8 + queue[n]%8, y = cell/64*8 + (queue[n]%64)/8;
					members.push_back(cell);
					for (const auto &d : {std::array<int,2>{1,0}, {-1,0}, {0,1}, {0,-1}})
					{
						int xx=x+d[0], yy=y+d[1];
						if (xx<0 || xx>=512 || yy<0 || yy>=512) continue;
						int j=yy/8*64+xx/8, pixel=j*64+(yy%8)*8+xx%8;
						const auto &t=context.placements[j];
						if (foot(j,xx%8,yy%8) && context.collisionAttributes[p*4096+j] != 0)
						{
							if (!(*seenStorage)[pixel]) { (*seenStorage)[pixel]=1; queue.push_back(pixel); }
						}
						else if (t.surface == RemasterDungeonFloorContext::Surface::Floor &&
							context.collisionAttributes[p*4096+j] == 0)
						{
							if (components[p*4096+j] < 0 || result.planeOffsets[p][j] == 255 || !t.hasHeight ||
								!std::all_of(t.height.begin(),t.height.end(),[] (uint8_t h) { return h == 0; }))
								unknown = true;
							else { int value=result.planeOffsets[p][j]; if (datum>=0 && datum!=value) conflict=true; datum=value; }
						}
					}
				}
				std::sort(members.begin(),members.end()); members.erase(std::unique(members.begin(),members.end()),members.end());
				for (int cell : members)
				{
					auto &status=(*statusStorage)[cell];
					auto &base=result.mixedFloorBases[p][cell];
					if (conflict || (status == 1 && base != datum && datum >= 0)) status=3;
					else if (status != 3 && (unknown || datum < 0 || result.structuralPlaneOffsets[p][cell] == 255)) status=2;
					else if (status == 0 && datum - result.structuralPlaneOffsets[p][cell] <= 255) { status=1; base=datum; }
					else if (status == 0) status=2; // byte local override cannot represent this region
				}
			}
			for (int cell=0; cell<4096; cell++)
			{
				const auto status=(*statusStorage)[cell];
				if (status == 3) result.mixedFloorConflicts++;
				if (status == 2) result.mixedFloorUnknown++;
				if (status > 1) result.mixedFloorBases[p][cell]=255;
			}
		}
		for (int i = 0; i < 4096; i++)
		{
			const auto &tile = context.placements[i];
			if (tile.surface == RemasterDungeonFloorContext::Surface::Wall)
			{
				if (result.offsets[i] == 255) result.unknownWallCells++; else result.solvedWallCells++;
				continue;
			}
			if (tile.surface == RemasterDungeonFloorContext::Surface::Floor || tile.surface == RemasterDungeonFloorContext::Surface::Stair) continue;
			// Ordinary occupied support is not a collidable-border candidate on
			// another plane. Otherwise lower colliders overwrite an upper floor
			// with a 0..7 fallback (exact008's two neutral floor hashes).
			if (components[i] >= 0 || components[4096+i] >= 0) continue;
			// Verified rail trim already has independent plane-specific assembly
			// support. Do not erase it with the identity-less one-cell fallback.
			if(tile.supportingTrim) continue;
			int base = -1, planeIdentity = -1;
			uint8_t directions = 0; bool conflict = false;
			for (int p = 0; p < 2; p++) for (int d = 0; d < 4; d++)
			{
				static const int dx[] = {0, 0, -1, 1}, dy[] = {-1, 1, 0, 0};
				int x = i % 64 - dx[d], y = i / 64 - dy[d];
				if (x < 0 || x >= 64 || y < 0 || y >= 64) continue;
				int j = y * 64 + x;
				if (components[p * 4096 + j] < 0 || result.planeOffsets[p][j] == 255) continue;
				if (tile.surface != RemasterDungeonFloorContext::Surface::Obstacle &&
					context.collisionAttributes[p * 4096 + i] != 1 && context.collisionAttributes[p * 4096 + i] != 2) continue;
				int value = result.planeOffsets[p][j];
				if (base >= 0 && (base != value || planeIdentity != p)) conflict = true;
				base = value; planeIdentity = p; directions |= static_cast<uint8_t>(1 << d);
			}
			if (conflict) { result.structuralConflicts++; result.offsets[i] = 255; }
			else if (base >= 0)
			{
				result.offsets[i] = base;
				if (tile.surface != RemasterDungeonFloorContext::Surface::Obstacle) result.fallbackDirections[i] = directions;
			}
		}
		// The independently decoded BG1 usage is lower support only where the
		// paired constructed artwork differs AND legal lower-plane body support
		// proves it. Do not lift it onto the occluding BG2 bridge's support;
		// walls, stairs, authored obstacle relief and unsupported/pit usages are
		// excluded. This selects an existing signed solve, never invents a datum.
		if (context.placementBackgroundIndex == 0)
			for (int i=0;i<4096;i++)
			{
				const auto &t=context.placements[i];
				if (t.distinctLowerUsage && t.hasHeight &&
					(t.surface==RemasterDungeonFloorContext::Surface::Floor || t.surface==RemasterDungeonFloorContext::Surface::Unknown) &&
					components[4096+i]>=0 && result.planeOffsets[1][i]!=255)
				{
					result.offsets[i]=result.planeOffsets[1][i];
					result.fallbackDirections[i]=0;
				}
			}
		return true; // unresolved is a cached result too, not a reason to reuse stale geometry
	}
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
	for (bool value : conflict) if (value) result.conflictingComponentCount++;
	for (size_t component = 0; component < cells.size(); component++)
		if (levels[component] != INT16_MIN && !conflict[component])
			for (uint16_t encoded : cells[component])
			{
				const int plane = encoded / 4096;
				const uint16_t index = encoded % 4096;
				const int value = levels[component] * floorRise;
				if (value < 0 || value > 32767 || value == 255) continue;
				result.planeOffsets[plane][index] = value;
				result.solvedPlaneCells[plane]++;
				int32_t &output = result.offsets[index];
				// BG2 is the upper visible support at bridge/underpass overlap.
				if (output == 255 || value > output) output = value;
				solved = true;
			}
	result.hasPlaneOffsets = solved;
	return solved;
}

// BG2 selects a solved visible wall plane; never borrow the underpass datum.
inline int32_t S9xRemasterMixedFloorBase (const RemasterDungeonHeightMap &map, size_t cell)
{
	int floor = -1;
	for (int p=0; p<2; p++) if (map.structuralPlaneOffsets[p][cell] == map.offsets[cell] && map.offsets[cell] != 255)
	{
		const int value=map.mixedFloorBases[p][cell];
		if (value == 255 || (floor >= 0 && floor != value)) return 255;
		floor=value;
	}
	return floor < 0 ? 255 : floor;
}

inline std::array<uint8_t,64> S9xRemasterMixedFloorHeight (const std::array<uint8_t,64> &local,
	int32_t base, int32_t floor)
{
	auto output=local;
	if (floor != 255 && floor > base && floor - base <= 255)
		for (auto &h : output) h=std::max(h,static_cast<uint8_t>(floor-base));
	return output; // canonical sampling; heightOffset supplies base exactly once
}

inline void S9xRemasterApplyDungeonHeightMap (RemasterFrame &frame,
	const RemasterDungeonFloorContext &context, const RemasterDungeonHeightMap &map,
	const RemasterDungeonHeightMap *sourceRoomMap = nullptr,
	RemasterDungeonHeightApplyDiagnostics *diagnostics = nullptr, uint8_t backgroundIndex = 1)
{
	if (map.roomIndex != context.roomIndex)
		return;
	const size_t originalCount = frame.tileInstances.size();
	std::vector<int32_t> firstOffset(originalCount, 255);
	std::vector<uint8_t> firstBorderDirections(originalCount, 255);
	std::vector<int32_t> firstFloor(originalCount, 255);
	std::set<std::string> wallFaceMaterials;
	for (const RemasterFrameMaterial &material : frame.materials)
		if (material.surfaceClass == RemasterSurfaceClass::WallFace)
			wallFaceMaterials.insert(material.name);
	auto offsetStorage = std::make_unique<std::array<int32_t, 64 * 64>>(map.offsets);
	auto &placementOffsets = *offsetStorage;
	struct WallCell
	{
		int8_t dx[2] = {};
		int8_t dy[2] = {};
		uint8_t directions = 0;
		uint8_t localMinimum = 0;
		bool conflict = false;
	};
	auto wallStorage = std::make_unique<std::array<WallCell, 64 * 64>>();
	auto &walls = *wallStorage;
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
				if (S9xRemasterDungeonRoomForScreenPosition(context, x, y) != context.roomIndex) continue;
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
	if (!map.hasRoomGeometry)
	{
		observeWalls(frame.mainPixels);
		observeWalls(frame.subPixels);
	}
	auto visibleWallStorage = std::make_unique<std::array<bool, 64 * 64>>();
	auto &visibleWalls = *visibleWallStorage;
	for (int index = 0; index < 64 * 64; index++)
		visibleWalls[index] = walls[index].directions != 0;
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
	// Placement-local fallback for one ordinary collidable tile immediately
	// outside a solved support. Every plane/contact must agree on both the floor
	// base and displayed outward direction. Wall faces and special attributes
	// retain their dedicated assembly rules.
	struct BorderCell
	{
		int32_t offset = 255;
		uint8_t directions = 0;
		uint8_t plane = 255;
		bool candidate = false;
		bool conflict = false;
	};
	auto borderStorage = std::make_unique<std::array<BorderCell, 64 * 64>>();
	auto &borders = *borderStorage;
	if (map.hasRoomGeometry)
		for (int i = 0; i < 4096; i++) if (map.fallbackDirections[i] && map.offsets[i] != 255)
		{
			borders[i].candidate = true;
			borders[i].offset = map.offsets[i];
			borders[i].directions = map.fallbackDirections[i];
		}
	if (!map.hasRoomGeometry && context.collisionAttributes && map.hasPlaneOffsets)
		for (int index = 0; index < 64 * 64; index++)
		{
			if (visibleWalls[index]) continue;
			const int x = index % 64, y = index / 64;
			static const int dx[4] = { 0, 0, -1, 1 };
			static const int dy[4] = { -1, 1, 0, 0 };
			BorderCell &border = borders[index];
			for (int plane = 0; plane < 2; plane++)
			{
				const uint8_t attribute = context.collisionAttributes[plane * 4096 + index];
				if (attribute != 1 && attribute != 2) continue;
				for (int direction = 0; direction < 4; direction++)
				{
					const int xx = x - dx[direction], yy = y - dy[direction];
					if (xx < 0 || xx >= 64 || yy < 0 || yy >= 64) continue;
					const int contact = yy * 64 + xx;
					if (context.collisionAttributes[plane * 4096 + contact] != 0) continue;
					const int32_t value = map.planeOffsets[plane][contact];
					if (value == 255) continue;
					if (border.candidate && (border.offset != value || border.plane != plane))
						border.conflict = true;
					else
					{
						border.candidate = true;
						border.offset = value;
						border.plane = static_cast<uint8_t>(plane);
						border.directions |= static_cast<uint8_t>(1 << direction);
					}
				}
			}
			if (border.candidate && !border.conflict) placementOffsets[index] = border.offset;
		}
	// Completed collision tables mark axis-aligned door thresholds as 80..8f.
	// Carry a floor base through the doorway only when both gameplay planes and
	// every solved sample on its movement axis agree.
	for (int index = 0; index < 64 * 64; index++)
	{
		if (map.hasRoomGeometry) break;
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
				const int32_t value = map.offsets[yy * 64 + xx];
				if (value == 255) continue;
				if (proposed >= 0 && proposed != value) conflict = true;
				else proposed = value;
				break;
			}
		if (!conflict && proposed >= 0) placementOffsets[index] = static_cast<uint8_t>(proposed);
	}
	auto offsetAt = [&] (const RemasterFramePixel &pixel, uint32_t x, uint32_t y) -> int32_t {
		if (!pixel.instanceId || pixel.instanceId > originalCount)
			return 0;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		if (instance.source != RemasterSourceType::Background || instance.sourceIndex != backgroundIndex ||
			instance.assetGroup == "stair_treads" || instance.assetGroup == "stair_rails")
			return 0;
		const uint16_t pixelRoom = S9xRemasterDungeonRoomForScreenPosition(context, x, y);
		const uint32_t roomX = (context.backgroundScrollX + x) & 511;
		const uint32_t roomY = (context.backgroundScrollY + y) & 511;
		const size_t cell = (roomY / 8) * 64 + roomX / 8;
		const auto *producer = pixelRoom == context.roomIndex ? &map : sourceRoomMap;
		if (producer && producer->producerHashes[cell] && (producer->producerHashes[cell] != instance.tileId.hash ||
			producer->producerFlips[cell] != (instance.hFlip | (instance.vFlip<<1)))) return 0;
		if (pixelRoom != context.roomIndex)
		{
			if (!sourceRoomMap || sourceRoomMap->roomIndex != pixelRoom) return 0;
			const int32_t sourceValue = sourceRoomMap->offsets[cell];
			return sourceValue == 255 ? 0 : sourceValue;
		}
		if (!map.hasRoomGeometry && wallFaceMaterials.count(instance.material) && !walls[cell].directions)
			return 0;
		const int32_t value = placementOffsets[cell];
		return value == 255 ? 0 : value;
	};
	BorderCell sourceBorder;
	auto borderAt = [&] (const RemasterFramePixel &pixel, uint32_t x, uint32_t y) -> const BorderCell * {
		if (!pixel.instanceId || pixel.instanceId > originalCount) return nullptr;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		if (instance.source != RemasterSourceType::Background || instance.sourceIndex != backgroundIndex ||
			instance.assetGroup == "stair_treads" || instance.assetGroup == "stair_rails") return nullptr;
		if (S9xRemasterDungeonRoomForScreenPosition(context, x, y) != context.roomIndex)
		{
			if (!sourceRoomMap) return nullptr;
			const size_t cell=(((context.backgroundScrollY+y)&511)/8)*64+((context.backgroundScrollX+x)&511)/8;
			if (sourceRoomMap->producerHashes[cell] && sourceRoomMap->producerHashes[cell]!=instance.tileId.hash) return nullptr;
			// The source's placement-local fallback is part of its retained
			// geometry, not just its base. Return stable per-cell storage.
			sourceBorder = BorderCell{};
			sourceBorder.candidate=sourceRoomMap->offsets[cell]!=255 && sourceRoomMap->fallbackDirections[cell];
			sourceBorder.directions=sourceRoomMap->fallbackDirections[cell];
			return sourceBorder.candidate ? &sourceBorder : nullptr;
		}
		const uint32_t roomX = (context.backgroundScrollX + x) & 511;
		const uint32_t roomY = (context.backgroundScrollY + y) & 511;
		const BorderCell &border = borders[(roomY / 8) * 64 + roomX / 8];
		return border.candidate && !border.conflict ? &border : nullptr;
	};
	auto assignBorderHeight = [] (RemasterFrameTileInstance &instance, uint8_t directions) {
		instance.hasPlacementHeight = true;
		instance.placementHeight.fill(7);
		static const int dx[4] = { 0, 0, -1, 1 };
		static const int dy[4] = { -1, 1, 0, 0 };
		for (int direction = 0; direction < 4; direction++)
			if (directions & (1 << direction))
			{
				int transformedDx = instance.hFlip ? -dx[direction] : dx[direction];
				int transformedDy = instance.vFlip ? -dy[direction] : dy[direction];
				for (int yy = 0; yy < 8; yy++) for (int xx = 0; xx < 8; xx++)
				{
					const uint8_t ramp = static_cast<uint8_t>(transformedDx > 0 ? xx :
						transformedDx < 0 ? 7 - xx : transformedDy > 0 ? yy : 7 - yy);
					instance.placementHeight[yy * 8 + xx] =
						std::min(instance.placementHeight[yy * 8 + xx], ramp);
				}
			}
	};
	auto floorAt = [&] (const RemasterFramePixel &pixel, uint32_t x, uint32_t y) -> int32_t {
		const auto &instance=frame.tileInstances[pixel.instanceId-1];
		if (instance.source != RemasterSourceType::Background || instance.sourceIndex != backgroundIndex) return 255;
		const auto room=S9xRemasterDungeonRoomForScreenPosition(context,x,y);
		const auto *selected=room==context.roomIndex ? &map : sourceRoomMap;
		if (!selected || selected->roomIndex != room || !selected->hasRoomGeometry) return 255;
		const size_t cell=(((context.backgroundScrollY+y)&511)/8)*64+((context.backgroundScrollX+x)&511)/8;
		if (selected->producerHashes[cell] && selected->producerHashes[cell]!=instance.tileId.hash) return 255;
		return S9xRemasterMixedFloorBase(*selected,cell);
	};
	auto assignFloorHeight = [&] (RemasterFrameTileInstance &instance, size_t original, int32_t base, int32_t floor) {
		if (floor == 255 || !instanceMetadata[original]) return;
		instance.hasPlacementHeight=true;
		instance.placementHeight=S9xRemasterMixedFloorHeight(instanceMetadata[original]->height,base,floor);
	};
	auto visit = [&] (std::vector<RemasterFramePixel> &pixels, bool assign,
		std::map<std::tuple<uint32_t, int32_t, uint8_t, int32_t>, uint32_t> &copies) {
		if (pixels.size() != static_cast<size_t>(frame.width) * frame.height)
			return;
		for (uint32_t y = 0; y < frame.height; y++)
			for (uint32_t x = 0; x < frame.width; x++)
			{
			RemasterFramePixel &pixel = pixels[static_cast<size_t>(y) * frame.width + x];
			if (!pixel.instanceId || pixel.instanceId > originalCount)
				continue;
			const uint32_t originalId = pixel.instanceId;
			const int32_t offset = offsetAt(pixel, x, y);
			const int32_t floor = floorAt(pixel,x,y);
			if (!assign && diagnostics)
			{
				const uint16_t pixelRoom = S9xRemasterDungeonRoomForScreenPosition(context, x, y);
				const bool source = pixelRoom != context.roomIndex;
				const uint32_t roomX = (context.backgroundScrollX + x) & 511;
				const uint32_t roomY = (context.backgroundScrollY + y) & 511;
				const size_t cell = (roomY / 8) * 64 + roomX / 8;
				const int32_t raw = source ?
					(sourceRoomMap && sourceRoomMap->roomIndex == pixelRoom ? sourceRoomMap->offsets[cell] : 255) :
					placementOffsets[cell];
				(source ? diagnostics->sourcePixels : diagnostics->destinationPixels)++;
				if (raw == 255) (source ? diagnostics->sourceUnknownPixels : diagnostics->destinationUnknownPixels)++;
				else if (offset) (source ? diagnostics->sourceRaisedPixels : diagnostics->destinationRaisedPixels)++;
			}
			const BorderCell *border = borderAt(pixel, x, y);
			const uint8_t borderDirections = border ? border->directions : 0;
			int32_t &first = firstOffset[originalId - 1];
			if (!assign)
			{
				if (first == 255)
				{
					first = offset;
					firstBorderDirections[originalId - 1] = borderDirections;
					firstFloor[originalId - 1] = floor;
				}
				continue;
			}
			if (offset == first && borderDirections == firstBorderDirections[originalId - 1] && floor == firstFloor[originalId - 1])
				continue;
			const auto key = std::make_tuple(originalId, offset, borderDirections, floor);
			auto found = copies.find(key);
			if (found == copies.end())
			{
				RemasterFrameTileInstance copy = frame.tileInstances[originalId - 1];
				copy.hasPlacementHeight = false;
				copy.heightOffset += offset;
				if (border) assignBorderHeight(copy, borderDirections);
				assignFloorHeight(copy,originalId-1,offset,floor);
				frame.tileInstances.push_back(copy);
				found = copies.emplace(key, static_cast<uint32_t>(frame.tileInstances.size())).first;
			}
			pixel.instanceId = found->second;
			}
	};
	std::map<std::tuple<uint32_t, int32_t, uint8_t, int32_t>, uint32_t> copies;
	visit(frame.mainPixels, false, copies);
	visit(frame.subPixels, false, copies);
	visit(frame.mainPixels, true, copies);
	visit(frame.subPixels, true, copies);
	for (size_t i = 0; i < originalCount; i++)
		if (firstOffset[i] != 255)
		{
			frame.tileInstances[i].heightOffset += firstOffset[i];
			if (firstBorderDirections[i] != 255 && firstBorderDirections[i])
				assignBorderHeight(frame.tileInstances[i], firstBorderDirections[i]);
			assignFloorHeight(frame.tileInstances[i],i,firstOffset[i],firstFloor[i]);
		}

	// Ground-supported OAM art follows the solved cell beneath the assembled
	if (backgroundIndex != 1) return; // BG1 geometry must not offset actors twice.
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
			const size_t support = (roomY / 8) * 64 + roomX / 8;
			const auto room = S9xRemasterDungeonRoomForScreenPosition(context,
				(bounds.x0+bounds.x1)/2,bounds.y1);
			const auto *selected = room == context.roomIndex ? &map : sourceRoomMap;
			if (!selected || selected->roomIndex != room) continue;
			const int32_t offset = selected->hasPlaneOffsets ?
				selected->planeOffsets[context.linkPlane & 1][support] : selected->offsets[support];
			if (offset != 255)
				frame.tileInstances[i].heightOffset += offset;
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
			frame.tileInstances[i].heightOffset += upperFloorHeight;
		else if (region[i] == 3)
		{
			RemasterFrameTileInstance raised = frame.tileInstances[i];
			raised.heightOffset += upperFloorHeight;
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
		if (a == art.end() || m == metadata.end() || !m->second->hasHeight)
			continue;
		unsigned visible = 0;
		bool exactRamp = true;
		for (size_t p = 0; p < 64; p++)
		{
			const bool ink = a->second->indices[p] != 0;
			visible += ink;
			if (m->second->height[p] != (ink ? 13 - p / 8 : 0) ||
				(m->second->hasOcclusion ? m->second->occlusion[p] : 255) != (ink ? 255 : 0))
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
		instance.heightOffset += rise;
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
	frame.emitterPatchSize = profile.emitterPatchSize;
	frame.representativeEmitters = profile.representativeEmitters;
	frame.sampleAccumulation = profile.sampleAccumulation;

	// Capture-local animation tracks are observations, not semantic profile groups.
	// Keep them when refreshing authored metadata in the replay editor.
	std::vector<RemasterFrameAssetGroup> capturedAnimationGroups;
	std::set<std::string> capturedAnimationNames;
	for (const RemasterFrameAssetGroup &group : frame.assetGroups)
		if (group.name.compare(0, 18, "capture_animation_") == 0)
		{
			capturedAnimationGroups.push_back(group);
			capturedAnimationNames.insert(group.name);
		}
	frame.assetGroups.clear();
	for (const auto &entry : profile.assetGroups)
	{
		RemasterFrameAssetGroup group;
		group.name = entry.second.name;
		group.tileIds = entry.second.tileIds;
		frame.assetGroups.push_back(group);
	}
	frame.assetGroups.insert(frame.assetGroups.end(), capturedAnimationGroups.begin(), capturedAnimationGroups.end());

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
		metadata.emissionDepth = source.emissionDepth;
		metadata.emissionOmnidirectional = source.emissionOmnidirectional;
		metadata.combineEmitters = source.combineEmitters;
		metadata.tileRole = source.tileRole;
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
		if (!capturedAnimationNames.count(instance.assetGroup))
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
		(result.schemaVersion >= 22 && !input.ReadU8(result.emitterPatchSize)) ||
		(result.schemaVersion >= 23 && !input.ReadU8(result.representativeEmitters)) ||
		!input.ReadU32(assetCount) ||
		(result.schemaVersion >= 13 && !input.ReadU32(artworkColorCount)) ||
		(result.schemaVersion >= 2 && !input.ReadU32(groupCount)) ||
		(result.schemaVersion >= 3 && !input.ReadU32(metadataCount)) || !input.ReadU32(materialCount) ||
		!input.ReadU32(instanceCount) || !input.ReadU32(lightCount))
		return false;
	if (result.representativeEmitters > 8 || result.emitterPatchSize < 1 || result.emitterPatchSize > 5 || !std::isfinite(result.lightingCoordinateScale) || result.lightingCoordinateScale <= 0.0f)
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
		if (result.schemaVersion >= 21 && (!input.ReadFloat(metadata.emissionDepth) ||
			!std::isfinite(metadata.emissionDepth) || metadata.emissionDepth < 0 || metadata.emissionDepth > 64)) return false;
		if (result.schemaVersion >= 24) {
			uint32_t count; if (!input.ReadU32(count) || count > (bytes.size()-input.offset)/10) return false;
			for (uint32_t i = 0; i < count; i++) {
				RemasterTileContentId id; if (!input.ReadTileId(id) || id == metadata.tileId ||
					std::find(metadata.combineEmitters.begin(), metadata.combineEmitters.end(), id) != metadata.combineEmitters.end()) return false;
				metadata.combineEmitters.push_back(id);
			}
		}
		if (result.schemaVersion >= 25)
		{
			uint8_t role;
			if (!input.ReadU8(role) || role >= static_cast<uint8_t>(RemasterTileRole::Count)) return false;
			metadata.tileRole = static_cast<RemasterTileRole>(role);
		}
		if (result.schemaVersion >= 27)
		{
			uint8_t omnidirectional;
			if (!input.ReadU8(omnidirectional) || omnidirectional > 1) return false;
			metadata.emissionOmnidirectional = omnidirectional != 0;
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
		auto readOffset = [&] () {
			if (result.schemaVersion >= 26) {
				uint32_t bits; if (!input.ReadU32(bits)) return false;
				std::memcpy(&instance.heightOffset, &bits, sizeof(bits));
				return instance.heightOffset >= -32767 && instance.heightOffset <= 32767;
			}
			uint8_t value; if (!input.ReadU8(value)) return false;
			instance.heightOffset = value; return true;
		};
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
				instance.ppuPriority > 3 || !readOffset())) ||
			(result.schemaVersion >= 19 && (!input.ReadU8(normalYaw) ||
				(normalYaw != 0 && normalYaw != 1 && normalYaw != 255))))
			return false;
		instance.source = static_cast<RemasterSourceType>(source);
		instance.hFlip = hFlip != 0;
		instance.vFlip = vFlip != 0;
		instance.normalYaw = normalYaw == 255 ? -1 : static_cast<int8_t>(normalYaw);
		if (result.schemaVersion >= 20)
		{
			uint8_t hasPlacementHeight = 0;
			if (!input.ReadU8(hasPlacementHeight) || hasPlacementHeight > 1) return false;
			instance.hasPlacementHeight = hasPlacementHeight != 0;
			if (instance.hasPlacementHeight)
			{
				if (input.offset + instance.placementHeight.size() > bytes.size()) return false;
				std::copy(bytes.begin() + input.offset, bytes.begin() + input.offset + instance.placementHeight.size(),
					instance.placementHeight.begin());
				input.offset += instance.placementHeight.size();
			}
		}
		instance.matchStatus = static_cast<RemasterProfileMatchStatus>(matchStatus);
	}
	result.lights.resize(lightCount);
	for (RemasterFrameLight &light : result.lights)
		if (!input.ReadFloat(light.x) || !input.ReadFloat(light.y) || !input.ReadFloat(light.z) ||
			!input.ReadFloat(light.radius) || !input.ReadFloat(light.red) || !input.ReadFloat(light.green) ||
			!input.ReadFloat(light.blue) || !input.ReadFloat(light.intensity))
			return false;
	if (result.schemaVersion >= 28 && input.offset < bytes.size())
	{
		uint32_t tag;
		if (!input.ReadU32(tag) || tag != 0x314e4353u) return false; // SCN1
		const size_t count = result.originalRgb555.size();
		if (bytes.size() - input.offset != count * 11) return false;
		result.sceneRgb555.resize(count);
		result.scenePixels.resize(count);
		for (size_t i = 0; i < count; i++)
		{
			auto &p = result.scenePixels[i];
			if (!input.ReadU16(result.sceneRgb555[i]) || !input.ReadU32(p.owner) ||
				!input.ReadU32(p.instanceId) || !input.ReadU8(p.tilePixel) ||
				p.instanceId > result.tileInstances.size() || (p.instanceId && p.tilePixel >= 64)) return false;
		}
	}
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

// Return the first decoded PPU tile contributing to the composed pixel.  Main
// screen wins when it has a tile; a backdrop-only main pixel may still be
// colored by a real subscreen BG/OBJ through PPU color math.
inline const RemasterFramePixel *S9xRemasterFrameVisibleTilePixelAt (
	const RemasterFrame &frame, uint32_t x, uint32_t y, bool *subscreen = nullptr)
{
	if (x >= frame.width || y >= frame.height)
		return nullptr;
	const size_t offset = static_cast<size_t>(y) * frame.width + x;
	if (offset >= frame.mainPixels.size() || offset >= frame.subPixels.size())
		return nullptr;
	const RemasterFramePixel *pixel = &frame.mainPixels[offset];
	bool sub = false;
	const bool mainBackdrop = static_cast<uint8_t>(pixel->owner >> 24) ==
		static_cast<uint8_t>(RemasterSourceType::Backdrop);
	if (mainBackdrop && (!pixel->instanceId || pixel->instanceId > frame.tileInstances.size()))
	{
		pixel = &frame.subPixels[offset];
		sub = true;
	}
	if (!pixel->instanceId || pixel->instanceId > frame.tileInstances.size())
		return nullptr;
	if (subscreen) *subscreen = sub;
	return pixel;
}

inline const RemasterFrameTileInstance *S9xRemasterFrameVisibleInstanceAt (
	const RemasterFrame &frame, uint32_t x, uint32_t y, bool *subscreen = nullptr)
{
	const RemasterFramePixel *pixel = S9xRemasterFrameVisibleTilePixelAt(frame, x, y, subscreen);
	return pixel ? &frame.tileInstances[pixel->instanceId - 1] : nullptr;
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
	int minimum = INT_MAX, maximum = INT_MIN;
	bool foundHeight = false;
	for (const RemasterFramePixel &pixel : frame.mainPixels)
	{
		if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size() || pixel.tilePixel >= 64)
			continue;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		const RemasterFrameAssetMetadata *metadata = byInstance[pixel.instanceId - 1];
		if ((!metadata || !metadata->hasHeight) && !instance.hasPlacementHeight && !instance.heightOffset)
			continue;
		const int localHeight = instance.hasPlacementHeight ? instance.placementHeight[pixel.tilePixel] :
			(metadata && metadata->hasHeight ? metadata->height[pixel.tilePixel] : 0);
		const int height = instance.heightOffset + localHeight;
		minimum = std::min(minimum, height);
		maximum = std::max(maximum, height);
		foundHeight = true;
	}
	return foundHeight ? std::make_pair(minimum, std::max(maximum, minimum + 1)) :
		std::make_pair(0, 1);
}

inline bool S9xRemasterHeightPreviewRangeValid (int minimum, int maximum)
{
	return minimum >= -32767 && minimum <= 32767 && maximum >= -32766 && maximum <= 33023 &&
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
		const uint8_t localHeight = instance.hasPlacementHeight ? instance.placementHeight[pixel.tilePixel] :
			(metadata->hasHeight ? metadata->height[pixel.tilePixel] : 0);
		accumulator.z += localHeight / 255.0f * intensity;
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

inline std::vector<RemasterTileContentId> S9xRemasterFrameAnimationVariants (
	const RemasterFrame &frame, const RemasterTileContentId &tileId, const std::string &groupName)
{
	// Observed animation tracks take precedence over broad semantic classifications.
	// Resolve by identity even if an instance's profile group has been refreshed.
	for (const RemasterFrameAssetGroup &group : frame.assetGroups)
		if (group.name.compare(0, 18, "capture_animation_") == 0 &&
			std::find(group.tileIds.begin(), group.tileIds.end(), tileId) != group.tileIds.end())
			return group.tileIds;
	std::vector<RemasterTileContentId> variants = S9xRemasterFrameAssetGroupVariants(frame, groupName);
	if (std::find(variants.begin(), variants.end(), tileId) == variants.end())
		return { tileId };
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
	if (frame.emitterPatchSize < 1 || frame.emitterPatchSize > 5) return false;
	RemasterFrameSerialization::U8(bytes, frame.emitterPatchSize);
	if (frame.representativeEmitters > 8) return false;
	RemasterFrameSerialization::U8(bytes, frame.representativeEmitters);
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
		const bool storeOcclusion = S9xRemasterOcclusionNeedsStorage(metadata.hasOcclusion, metadata.occlusion);
		RemasterFrameSerialization::TileId(bytes, metadata.tileId);
		RemasterFrameSerialization::U8(bytes, metadata.hasMaterialSelectors ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, storeOcclusion ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, metadata.hasHeight ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(metadata.heightSampling));
		RemasterFrameSerialization::U8(bytes, metadata.hasEmission ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, metadata.hasNormals ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, metadata.directLightingOppositeFacing ? 1 : 0);
		if (metadata.hasMaterialSelectors)
			for (const std::string &name : metadata.materialSelectors)
				if (!RemasterFrameSerialization::String(bytes, name))
					return false;
		if (storeOcclusion)
			bytes.insert(bytes.end(), metadata.occlusion.begin(), metadata.occlusion.end());
		if (metadata.hasHeight)
			bytes.insert(bytes.end(), metadata.height.begin(), metadata.height.end());
		if (metadata.hasNormals)
			bytes.insert(bytes.end(), metadata.normalXyz.begin(), metadata.normalXyz.end());
		if (metadata.hasEmission)
			bytes.insert(bytes.end(), metadata.emissionRgba.begin(), metadata.emissionRgba.end());
		if (!std::isfinite(metadata.emissionDepth) || metadata.emissionDepth < 0 || metadata.emissionDepth > 64) return false;
		RemasterFrameSerialization::Float(bytes, metadata.emissionDepth);
		if (metadata.combineEmitters.size() > UINT32_MAX) return false;
		RemasterFrameSerialization::U32(bytes, uint32_t(metadata.combineEmitters.size()));
		std::vector<RemasterTileContentId> peers = metadata.combineEmitters; std::sort(peers.begin(), peers.end());
		for (size_t i = 0; i < peers.size(); i++) {
			if (peers[i] == metadata.tileId || (i && peers[i] == peers[i-1])) return false;
			RemasterFrameSerialization::TileId(bytes, peers[i]);
		}
		if (metadata.tileRole >= RemasterTileRole::Count) return false;
		RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(metadata.tileRole));
		RemasterFrameSerialization::U8(bytes, metadata.emissionOmnidirectional ? 1 : 0);
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
		if (instance.heightOffset < -32767 || instance.heightOffset > 32767) return false;
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
		RemasterFrameSerialization::U32(bytes, static_cast<uint32_t>(instance.heightOffset));
		RemasterFrameSerialization::U8(bytes, instance.normalYaw < 0 ? 255 :
			static_cast<uint8_t>(instance.normalYaw));
		RemasterFrameSerialization::U8(bytes, instance.hasPlacementHeight ? 1 : 0);
		if (instance.hasPlacementHeight)
			bytes.insert(bytes.end(), instance.placementHeight.begin(), instance.placementHeight.end());
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
	if (!frame.sceneRgb555.empty() || !frame.scenePixels.empty())
	{
		if (frame.sceneRgb555.size() != pixelCount || frame.scenePixels.size() != pixelCount) return false;
		RemasterFrameSerialization::U32(bytes, 0x314e4353u);
		for (size_t i = 0; i < pixelCount; i++)
		{
			const auto &p = frame.scenePixels[i];
			if (p.instanceId > frame.tileInstances.size() || (p.instanceId && p.tilePixel >= 64)) return false;
			RemasterFrameSerialization::U16(bytes, frame.sceneRgb555[i]);
			RemasterFrameSerialization::U32(bytes, p.owner);
			RemasterFrameSerialization::U32(bytes, p.instanceId);
			RemasterFrameSerialization::U8(bytes, p.tilePixel);
		}
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
