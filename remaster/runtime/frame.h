/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REMASTER_FRAME_H_
#define _REMASTER_FRAME_H_

#include "profile.h"
#include "room_diagnostics.h"
#include "lighting_trace.h"
#include "dungeon_layout_evidence.h"

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
#ifdef ALTTP_TEST_CACHE_STAGES
#include <chrono>
struct RemasterDungeonCacheStages {
	double classifyMs=0,solveMs=0,proofMs=0,storeMs=0,restoreMs=0;
	uint64_t classifications=0,solves=0,proofs=0,stores=0,restores=0;
};
inline RemasterDungeonCacheStages &S9xRemasterDungeonCacheStages(){static RemasterDungeonCacheStages s;return s;}
struct RemasterDungeonStageTimer {
	double &total;
	std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
	RemasterDungeonStageTimer(double &ms,uint64_t &calls):total(ms){calls++;}
	~RemasterDungeonStageTimer(){total+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
};
#endif

static const uint32_t REMASTER_FRAME_SCHEMA_VERSION = 40;

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

// CGRAM is SNES BGR555; capture/editor colors are RGB555. No INIDISP
// brightness, color-window clipping or main/subscreen color math is applied.
inline uint16_t S9xRemasterPaletteRgb555 (uint16_t cgram)
{
	return static_cast<uint16_t>(((cgram & 31) << 10) | (cgram & 0x3e0) | ((cgram >> 10) & 31));
}

inline uint16_t S9xRemasterDirectRgb555 (uint8_t index, uint8_t palette)
{
	return static_cast<uint16_t>(((((index & 7) << 2) | ((palette & 1) << 1)) << 10) |
		((((index & 0x38) >> 1) | (palette & 2)) << 5) |
		((index & 0xc0) >> 3) | (palette & 4));
}

struct RemasterFrameAssetGroup
{
	std::string name;
	std::vector<RemasterTileContentId> tileIds;
};

struct RemasterFrameAssetMetadata
{
	RemasterTileContentId tileId;
	RemasterTileRole tileRole = RemasterTileRole::Unknown;
	uint64_t doorframeMask = 0;
	bool hasDoorframeMask = false;
	bool doorwaySupport = false;
	RemasterSupportDirection doorwaySupportDirection = RemasterSupportDirection::Unknown;
	std::array<std::string, 64> materialSelectors;
	std::array<uint8_t, 64> occlusion = S9xRemasterDefaultOcclusion();
	std::array<int16_t, 64> height = {};
	std::array<uint8_t, 192> normalXyz = {};
	std::array<uint8_t, 256> emissionRgba = {};
	uint64_t emissionColorMask = UINT64_MAX;
	float emissionDepth = 0.0f;
	bool emissionOmnidirectional = false;
	std::vector<RemasterTileContentId> combineEmitters;
	bool hasMaterialSelectors = false;
	bool hasOcclusion = false;
	bool hasHeight = false;
	bool hasNormals = false;
	bool hasEmission = false;
	bool directLightingOppositeFacing = false;
	bool normalIgnoreTileTransform = false;
	bool geometryByFacing = false;
	std::array<RemasterGeometryLayers, 4> facingGeometry;
	bool geometryByFlip = false;
	std::array<RemasterGeometryLayers, 4> flipGeometry;
	RemasterHeightSampling heightSampling = RemasterHeightSampling::Nearest;
};

inline RemasterFrameAssetMetadata S9xRemasterResolveFacingGeometry(const RemasterFrameAssetMetadata &source, unsigned facing)
{
	auto result = source;
	if (source.geometryByFacing && facing >= 1 && facing <= 4)
	{
		const auto &g = source.facingGeometry[facing - 1];
		if (g.hasHeight) { result.height = g.height; result.hasHeight = true; result.heightSampling = g.heightSampling; }
		if (g.hasNormals) { result.normalXyz = g.normalXyz; result.hasNormals = true; }
	}
	return result;
}

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

// Inherit preserves pre-v36 captures. Unresolved/Conflict explicitly prohibit
// hash-global geometry fallback; neither is an implicit flat surface.
enum class RemasterOccurrenceGeometryStatus : uint8_t { Inherit, Resolved, Unresolved, Conflict, Count };

// Independent visual graph evidence, never channel presence or a physical datum.
// A conflicted visual representative may coexist with available physical relief.
enum class RemasterVisualClaimStatus : uint8_t { NotEvaluated, Solved, Unanchored, Conflict, Unsupported, Count };
struct RemasterVisualClaim
{
	RemasterVisualClaimStatus status = RemasterVisualClaimStatus::NotEvaluated;
	uint64_t mask = 0; // canonical artwork pixels, not displayed pixels
	int32_t cell = -1, component = -1;
	bool published = false; // numeric base was actually supplied by this graph
	bool operator==(const RemasterVisualClaim &b) const {
		return status==b.status && mask==b.mask && cell==b.cell && component==b.component && published==b.published;
	}
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
	int32_t heightOffset = 0; // signed placement datum, added to signed int16 local relief
	std::array<int16_t, 64> placementHeight = {};
	int8_t normalYaw = 0;
	uint8_t geometryFacing = 0; // Unknown/default, north, south, east, west.
	bool hFlip = false;
	bool vFlip = false;
	bool hasPlacementHeight = false;
	RemasterOccurrenceGeometryStatus geometryStatus = RemasterOccurrenceGeometryStatus::Inherit;
	RemasterVisualClaim visualClaim;
	// Resolved overrides role (even Unknown); absent geometry arrays continue to
	// inherit the selected asset variant. Supply flat/+Z arrays to replace relief.
	RemasterTileRole placementTileRole = RemasterTileRole::Unknown;
	// All arrays use canonical row-major artwork order, like placementHeight.
	// Vectors use the asset byte encoding and normally follow placement flips/yaw.
	std::array<uint8_t, 192> placementNormalXyz = {};
	std::array<uint8_t, 64> placementOcclusion = {};
	bool hasPlacementNormals = false;
	bool hasPlacementOcclusion = false;
	bool placementNormalIgnoreTileTransform = false;
	bool hasPaletteArtwork = false;
	std::array<uint16_t, 64> paletteRgb555 = {}; // canonical, unflipped
	RemasterProfileMatchStatus matchStatus = RemasterProfileMatchStatus::NoMatch;
	std::string assetGroup;
	std::string material;
};

inline bool S9xRemasterInstanceGeometryUnresolved(const RemasterFrameTileInstance &instance)
{
	return instance.geometryStatus == RemasterOccurrenceGeometryStatus::Unresolved ||
		instance.geometryStatus == RemasterOccurrenceGeometryStatus::Conflict;
}

inline RemasterTileRole S9xRemasterResolveInstanceTileRole(RemasterTileRole canonical,
	const RemasterFrameTileInstance &instance)
{
	return instance.geometryStatus == RemasterOccurrenceGeometryStatus::Inherit ? canonical :
		instance.geometryStatus == RemasterOccurrenceGeometryStatus::Resolved ? instance.placementTileRole : RemasterTileRole::Unknown;
}

inline bool S9xRemasterValidOccurrenceGeometry(const RemasterFrameTileInstance &instance)
{
	const auto &v=instance.visualClaim;
	if(v.status>=RemasterVisualClaimStatus::Count ||
		(v.status==RemasterVisualClaimStatus::NotEvaluated ? (v.mask || v.cell!=-1 || v.component!=-1 || v.published) :
		(!v.mask || v.cell<0 || v.cell>=4096 || v.component < -1 ||
		(instance.source!=RemasterSourceType::Background || instance.sourceIndex>1) ||
		(v.published && v.status!=RemasterVisualClaimStatus::Solved)))) return false;
	if (instance.geometryStatus >= RemasterOccurrenceGeometryStatus::Count || instance.placementTileRole >= RemasterTileRole::Count)
		return false;
	if (instance.geometryStatus != RemasterOccurrenceGeometryStatus::Resolved &&
		(instance.placementTileRole != RemasterTileRole::Unknown || instance.hasPlacementNormals || instance.hasPlacementOcclusion)) return false;
	return !instance.placementNormalIgnoreTileTransform || instance.hasPlacementNormals;
}

inline RemasterFrameAssetMetadata S9xRemasterResolveInstanceGeometry(
	const RemasterFrameAssetMetadata &source, const RemasterFrameTileInstance &instance)
{
	auto result = source.geometryByFlip ? source : S9xRemasterResolveFacingGeometry(source, instance.geometryFacing);
	if (source.geometryByFlip)
	{
		const auto &g = source.flipGeometry[S9xRemasterGeometryFlipIndex(instance.hFlip, instance.vFlip)];
		if (g.hasHeight) { result.height = g.height; result.hasHeight = true; result.heightSampling = g.heightSampling; }
		// Flip variants already describe final vector directions, not pixel order.
		if (g.hasNormals) { result.normalXyz = g.normalXyz; result.hasNormals = true; result.normalIgnoreTileTransform = true; }
	}
	result.tileRole = S9xRemasterResolveInstanceTileRole(source.tileRole, instance);
	if (S9xRemasterInstanceGeometryUnresolved(instance))
	{
		result.hasHeight = result.hasOcclusion = false;
		result.height = {}; result.occlusion = {};
		// OBJ local normal directions do not depend on a solved floor datum.
		// Keep authored normals inspectable while absolute height/occlusion stay
		// explicitly absent. BG unresolved ownership retains its stricter contract.
		if (instance.source != RemasterSourceType::Object) {
			result.hasNormals = false; result.normalXyz = {};
			result.directLightingOppositeFacing = result.normalIgnoreTileTransform = false;
		}
	}
	else
	{
		if (instance.hasPlacementHeight) { result.height = instance.placementHeight; result.hasHeight = true; result.heightSampling = RemasterHeightSampling::Nearest; }
		if (instance.hasPlacementNormals) { result.normalXyz = instance.placementNormalXyz; result.hasNormals = true; result.normalIgnoreTileTransform = instance.placementNormalIgnoreTileTransform; }
		if (instance.hasPlacementOcclusion) { result.occlusion = instance.placementOcclusion; result.hasOcclusion = true; }
	}
	return result;
}

// CPU/Metal RG32Float v2 wire value. Signed local relief and placement datum.
// Retain the historical /255 unit conversion without clipping.
inline float S9xRemasterSignedHeightField (int32_t base, int16_t local)
{
	return (base + static_cast<int32_t>(local)) / 255.0f;
}

inline void S9xRemasterTransformNormalForTileInstance (const RemasterFrameTileInstance &instance,
	float &x, float &y, float &z, bool ignoreTileTransform = false)
{
	if (ignoreTileTransform) return;
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
	bool hideSource = false; // Suppress visible sphere, never its emitted light.
};

// Source-only samples behind HUD ink: no receiver, artwork or occluder plane.
// XY/Z/depth are physical source-pixel units; emission retains authored RGBA.
struct RemasterFrameHiddenEmitter
{
	float x = 0, y = 0, z = 0, depth = 0;
	std::array<float,3> normal = {{0,0,1}};
	std::array<uint8_t,4> emission = {};
	bool omnidirectional = false;
};

struct RemasterSpriteObstacleSupport;

struct RemasterFrame
{
	// Ephemeral producer lineage; deliberately not serialized into capture bytes.
	RemasterLightingTrace::Frame lightingTrace;
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
	uint8_t representativeEmitters = 0;
	bool sampleAccumulation = true;
	std::vector<uint16_t> originalRgb555;
	std::vector<RemasterFramePixel> mainPixels;
	std::vector<RemasterFramePixel> subPixels;
	// Independently observed BG0/BG1 ink before the stock composite depth test.
	// [main BG0, main BG1, sub BG0, sub BG1]; empty means unobserved, not transparent.
	std::array<std::vector<RemasterFramePixel>,4> observedBgPixels;
	std::array<std::vector<uint8_t>,4> observedBgDepth;
	// Optional v28 SCN1 appendix: independent pre-cone scene, same instance table.
	// Empty on legacy/unsupported frames. Stock RGB/owners remain authoritative.
	std::vector<uint16_t> sceneRgb555;
	std::vector<RemasterFramePixel> scenePixels;
	std::vector<RemasterFrameAsset> assets;
	std::vector<RemasterFrameArtworkColors> artworkColors;
	// Full-brightness fallback for animation variants absent from this scene.
	std::vector<RemasterFrameArtworkColors> paletteArtworkColors;
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
	// Live/paused editor identity only; captures already retain the baked intensity.
	size_t heldLampLightIndex = SIZE_MAX;
	std::vector<RemasterFrameHiddenEmitter> hiddenEmitters;
	RemasterAreaContext area;
	RemasterDirectionalLight directionalLight{false};
	// Sparse background occurrence identity and selected context; no per-tile payload.
	std::map<uint32_t, RemasterContextLocation> contextLocations;
	std::map<uint32_t, RemasterTileContext> contextGeometry;
	std::set<uint32_t> selectedContextInstances; // editor highlight only, never captured
	bool contextLocationSelection = false; // empty location selection must not highlight the whole type
	// Obstacle authoring includes OBJ and is not a context binding domain.
	std::set<uint32_t> selectedObstacleInstances; // editor only, never captured
	bool obstacleLocationSelection = false;
	// Value-owned support from this frame, never from current emulator RAM. Not
	// serialized: replay restores it only from matching complete input sidecars.
	std::shared_ptr<const RemasterSpriteObstacleSupport> spriteObstacleSupport;
	int offscreenRepresentativeEmitters = -1; // Old captures follow visible; live frames retain effective 0..8.
};

inline RemasterFrameAssetMetadata S9xRemasterResolveFrameGeometry(const RemasterFrameAssetMetadata &source,
	const RemasterFrame &frame, size_t index)
{
	auto result = S9xRemasterResolveInstanceGeometry(source, frame.tileInstances[index]);
	const auto found = frame.contextGeometry.find(static_cast<uint32_t>(index + 1));
	if (found != frame.contextGeometry.end()) {
		const auto &c = found->second;
		result.height = c.height; result.hasHeight = c.hasHeight; result.heightSampling = c.heightSampling;
		result.normalXyz = c.normalXyz; result.hasNormals = c.hasNormals;
		result.normalIgnoreTileTransform = c.normalIgnoreTileTransform;
		result.directLightingOppositeFacing = c.directLightingOppositeFacing;
	}
	// Context selection supplies local layers, never proof of absolute placement.
	if (S9xRemasterInstanceGeometryUnresolved(frame.tileInstances[index])) {
		result.hasHeight = result.hasOcclusion = false;
		if (frame.tileInstances[index].source != RemasterSourceType::Object) result.hasNormals = false;
	}
	return result;
}

// The cone-free raster intentionally excludes BG1 (which also carries the lamp
// mask). An empty scene receiver can nevertheless have a real subscreen
// surface. Restore only observed, explicitly authored physical wall/floor art, including
// its exact canonical palette sample; never synthesize a placement datum or
// import arbitrary BG1 color-math masks. Run before placement application so the
// restored receiver participates in the same occurrence solve as other scene art.
// A donor is a complete observed sample, not an array spliced into another
// occurrence's coordinate system. Keep the selected pixel (canonical index),
// occurrence (base/transform/context), and visible artwork owner independent.
enum class RemasterMetadataChannel : uint8_t { Height, Normals, Occlusion, Emission, Material, Doorframe, Role, Count };
struct RemasterVisibleChannelSelection
{
	RemasterFramePixel visible;
	std::array<RemasterFramePixel, size_t(RemasterMetadataChannel::Count)> donors = {};
};

inline RemasterVisualClaim S9xRemasterPixelVisualClaim(const RemasterFrame &frame,const RemasterFramePixel &p)
{
	if(!p.instanceId || p.instanceId>frame.tileInstances.size() || p.tilePixel>=64)return {};
	const auto &v=frame.tileInstances[p.instanceId-1].visualClaim;
	return v.mask&(UINT64_C(1)<<p.tilePixel)?v:RemasterVisualClaim{};
}
inline const char *S9xRemasterVisualClaimName(RemasterVisualClaimStatus status)
{
	switch(status) {
	case RemasterVisualClaimStatus::Solved:return "Solved";
	case RemasterVisualClaimStatus::Unanchored:return "Unanchored";
	case RemasterVisualClaimStatus::Conflict:return "Conflict";
	case RemasterVisualClaimStatus::Unsupported:return "Unsupported";
	default:return "NotEvaluated";
	}
}

// Artwork precedence only. Consumers that need just the visible occurrence must
// not resolve channel donors (or materialize unrelated per-occurrence metadata).
inline RemasterFramePixel S9xRemasterSelectVisibleOccurrence(const RemasterFrame &frame,
	size_t offset, bool useScene, bool alttp)
{
	RemasterFramePixel visible;
	auto valid = [&](const RemasterFramePixel &p) {
		return p.instanceId && p.instanceId <= frame.tileInstances.size() && p.tilePixel < 64;
	};
	if (offset >= frame.mainPixels.size()) return visible;
	const auto &main = frame.mainPixels[offset];
	const RemasterFramePixel empty;
	const auto &sub = offset < frame.subPixels.size() ? frame.subPixels[offset] : empty;
	const auto &scene = useScene && offset < frame.scenePixels.size() ? frame.scenePixels[offset] : main;
	// Main/sub are actual stock PPU observations, in presentation precedence.
	// BG3 HUD is not a world donor; OBJ is a boundary, never a BG fallback.
	// Only the verified ALTTP scene contract identifies BG3 as presentation-only.
	// A third BG in another game is ordinary artwork, not implicitly HUD.
	const bool hud = alttp &&
		static_cast<RemasterSourceType>(main.owner >> 24) == RemasterSourceType::Background && ((main.owner >> 16) & 255) == 2;
	visible = valid(main) && !hud ? main : valid(sub) && (!hud || useScene) ? sub : scene;
	if (hud && !useScene) visible = main;
	if (hud && useScene && offset<frame.observedBgPixels[0].size() && offset<frame.observedBgPixels[1].size() &&
		offset<frame.observedBgDepth[0].size() && offset<frame.observedBgDepth[1].size()) {
		// Retained main-screen BG depth supplies actual under-HUD ordering. Never
		// compare main/sub depths: their numeric domains encode different flags.
		const auto &a=frame.observedBgPixels[0][offset], &b=frame.observedBgPixels[1][offset];
		if(valid(a) || valid(b)) visible=valid(a) && (!valid(b) || frame.observedBgDepth[0][offset]>=frame.observedBgDepth[1][offset])?a:b;
	}
	// EndFrame solves the independent scene occurrence, whereas the stock main
	// occurrence may retain an unapplied base. If these are the SAME observed
	// artwork placement at this pixel, use its solved occurrence, not an unrelated
	// layer and not the stale stock datum. Preserve the presentation owner above.
	if (useScene && valid(visible) && valid(scene) && visible.owner == scene.owner &&
		visible.tilePixel == scene.tilePixel) {
		const auto &a=frame.tileInstances[visible.instanceId-1], &b=frame.tileInstances[scene.instanceId-1];
		if (a.tileId==b.tileId && a.source==b.source && a.sourceIndex==b.sourceIndex &&
			a.hFlip==b.hFlip && a.vFlip==b.vFlip) visible=scene;
	}
	return visible;
}

template<class Present>
inline RemasterVisibleChannelSelection S9xRemasterSelectVisibleChannelsWithPresence(const RemasterFrame &frame,
	size_t offset, bool useScene, const Present &present, bool heightOnly, bool alttp)
{
	RemasterVisibleChannelSelection result;
	if(offset>=frame.mainPixels.size())return result;
	result.visible=S9xRemasterSelectVisibleOccurrence(frame,offset,useScene,alttp);
	auto valid = [&](const RemasterFramePixel &p) {
		return p.instanceId && p.instanceId <= frame.tileInstances.size() && p.tilePixel < 64;
	};
	auto worldBg = [&](const RemasterFramePixel &p) {
		return valid(p) && frame.tileInstances[p.instanceId-1].source == RemasterSourceType::Background &&
			frame.tileInstances[p.instanceId-1].sourceIndex < 2 &&
			static_cast<RemasterSourceType>(p.owner >> 24) == RemasterSourceType::Background &&
			((p.owner >> 16) & 255) == frame.tileInstances[p.instanceId-1].sourceIndex;
	};
	const auto &main=frame.mainPixels[offset];
	const RemasterFramePixel empty;
	const auto &sub=offset<frame.subPixels.size()?frame.subPixels[offset]:empty;
	const auto &scene=useScene && offset<frame.scenePixels.size()?frame.scenePixels[offset]:main;
	const auto &preferred = result.visible;
	// Explicit OOB is a visible surface claim, not permission to borrow the
	// lower layer's geometry. Presence remains per-channel (zero is present).
	const bool terminal = present(preferred,size_t(RemasterMetadataChannel::Count));
	const bool terminalUnknown = terminal && !S9xRemasterPixelVisualClaim(frame,preferred).published;
	// For the height-only consumer an explicitly present preferred source
	// (including authored zero) makes every alternate observation irrelevant.
	// Do not inspect fallback owners/depth again or change the visible sample.
	if (heightOnly && !terminalUnknown && present(preferred,0)) {
		result.donors[0]=preferred;
		return result;
	}
	RemasterFramePixel other;
	if (worldBg(preferred)) {
		for (const auto *candidate : {&main, &sub, &scene})
			if (worldBg(*candidate) && frame.tileInstances[candidate->instanceId-1].sourceIndex !=
				frame.tileInstances[preferred.instanceId-1].sourceIndex) { other = *candidate; break; }
		if(!other.instanceId)for(const auto &buffer:frame.observedBgPixels)if(offset<buffer.size()) {
			const auto &p=buffer[offset];
			if(worldBg(p) && frame.tileInstances[p.instanceId-1].sourceIndex!=frame.tileInstances[preferred.instanceId-1].sourceIndex){other=p;break;}
		}
	}
	// Conservation applies to EVERY donor, including the opposite-BG fallback.
	// A stock subscreen observation may be an unsolved duplicate of scene ink.
	if(useScene && valid(other) && valid(scene) && other.owner==scene.owner && other.tilePixel==scene.tilePixel) {
		const auto &a=frame.tileInstances[other.instanceId-1], &b=frame.tileInstances[scene.instanceId-1];
		if(a.tileId==b.tileId && a.source==b.source && a.sourceIndex==b.sourceIndex &&
			a.hFlip==b.hFlip && a.vFlip==b.vFlip)other=scene;
	}
	for (size_t c = 0; c < (heightOnly ? 1 : result.donors.size()); c++)
		result.donors[c] = terminalUnknown && c==0 ? RemasterFramePixel{} : present(preferred,c) ? preferred :
			terminal && c<=size_t(RemasterMetadataChannel::Occlusion) ? RemasterFramePixel{} :
			present(other,c) ? other : RemasterFramePixel{};
	return result;
}

inline RemasterVisibleChannelSelection S9xRemasterSelectVisibleChannels(const RemasterFrame &frame,
	size_t offset, bool useScene, const std::vector<const RemasterFrameAssetMetadata *> &metadata,
	bool heightOnly = false)
{
	auto present = [&](const RemasterFramePixel &p, size_t channel) {
		const bool valid = p.instanceId && p.instanceId <= frame.tileInstances.size() && p.tilePixel < 64;
		if (!valid || p.instanceId > metadata.size() || !metadata[p.instanceId-1]) return false;
		const auto &m = *metadata[p.instanceId-1];
		switch (RemasterMetadataChannel(channel)) {
		case RemasterMetadataChannel::Height: return m.hasHeight;
		case RemasterMetadataChannel::Normals: return m.hasNormals;
		case RemasterMetadataChannel::Occlusion: return m.hasOcclusion;
		case RemasterMetadataChannel::Emission: return m.hasEmission;
		case RemasterMetadataChannel::Material: return m.hasMaterialSelectors || !frame.tileInstances[p.instanceId-1].material.empty();
		case RemasterMetadataChannel::Doorframe: return m.hasDoorframeMask;
		case RemasterMetadataChannel::Role: return m.tileRole != RemasterTileRole::Unknown;
		case RemasterMetadataChannel::Count: return m.tileRole == RemasterTileRole::OutOfBounds;
		default: return false;
		}
	};
	return S9xRemasterSelectVisibleChannelsWithPresence(frame,offset,useScene,present,heightOnly,
		frame.profileRomSha256 == "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb");
}

// Invocation-local borrowed height sources. No authored arrays, material strings,
// facing/flip tables or unrelated channels are copied for each occurrence.
// The frame and its instance/context arrays must remain unchanged while selecting.
struct RemasterFrameHeightResolver
{
	struct Source { const std::array<int16_t,64> *height=nullptr; bool hasHeight=false; bool terminal=false; };
	std::vector<Source> effective;
	bool alttp;
	explicit RemasterFrameHeightResolver(const RemasterFrame &frame) :
		alttp(frame.profileRomSha256 == "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb") {
		std::map<RemasterTileContentId,const RemasterFrameAssetMetadata *> assets;
		for(const auto &m:frame.assetMetadata)assets[m.tileId]=&m;
		effective.reserve(frame.tileInstances.size());
		for(size_t n=0;n<frame.tileInstances.size();++n) {
			const auto &i=frame.tileInstances[n];Source s;
			auto a=assets.find(i.tileId);
			if(a!=assets.end()) {
				const auto &m=*a->second;s={&m.height,m.hasHeight};
				const RemasterGeometryLayers *g=nullptr;
				if(m.geometryByFlip)g=&m.flipGeometry[S9xRemasterGeometryFlipIndex(i.hFlip,i.vFlip)];
				else if(m.geometryByFacing && i.geometryFacing>=1 && i.geometryFacing<=4)g=&m.facingGeometry[i.geometryFacing-1];
				if(g && g->hasHeight)s={&g->height,true};
			}
			if(i.hasPlacementHeight)s={&i.placementHeight,true};
			auto c=frame.contextGeometry.find(uint32_t(n+1));
			if(c!=frame.contextGeometry.end())s={&c->second.height,c->second.hasHeight};
			if(S9xRemasterInstanceGeometryUnresolved(i))s.hasHeight=false;
			s.terminal=i.geometryStatus==RemasterOccurrenceGeometryStatus::Resolved ? i.placementTileRole==RemasterTileRole::OutOfBounds :
				a!=assets.end() && a->second->tileRole==RemasterTileRole::OutOfBounds;
			effective.push_back(s);
		}
	}
	RemasterVisibleChannelSelection selectHeight(const RemasterFrame &frame,size_t pixel,bool scene=true) const {
		auto present=[&](const RemasterFramePixel &p,size_t channel) {
			return p.instanceId && p.instanceId<=effective.size() && p.tilePixel<64 &&
				(channel==size_t(RemasterMetadataChannel::Count)?effective[p.instanceId-1].terminal:effective[p.instanceId-1].hasHeight);
		};
		return S9xRemasterSelectVisibleChannelsWithPresence(frame,pixel,scene,present,true,alttp);
	}
};

// Shared immutable effective-source table for inspection, extraction and debug.
// Occurrence/context precedence is identical to the production field cache.
struct RemasterFrameChannelResolver
{
	std::vector<RemasterFrameAssetMetadata> effective;
	std::vector<const RemasterFrameAssetMetadata *> metadata;
	explicit RemasterFrameChannelResolver(const RemasterFrame &frame) {
		std::map<RemasterTileContentId,const RemasterFrameAssetMetadata *> assets;
		for(const auto &m:frame.assetMetadata)assets[m.tileId]=&m;
		effective.reserve(frame.tileInstances.size());
		for(size_t i=0;i<frame.tileInstances.size();i++) {
			auto m=assets.find(frame.tileInstances[i].tileId);
			effective.push_back(S9xRemasterResolveFrameGeometry(m==assets.end()?RemasterFrameAssetMetadata{}:*m->second,frame,i));
		}
		for(const auto &m:effective)metadata.push_back(&m);
	}
	RemasterVisibleChannelSelection select(const RemasterFrame &frame,size_t pixel,bool scene=true) const {
		return S9xRemasterSelectVisibleChannels(frame,pixel,scene,metadata);
	}
	RemasterVisibleChannelSelection selectHeight(const RemasterFrame &frame,size_t pixel,bool scene=true) const {
		return S9xRemasterSelectVisibleChannels(frame,pixel,scene,metadata,true);
	}
};

inline std::string S9xRemasterChannelProvenance(const RemasterFrame &frame,size_t pixel)
{
	if(pixel>=frame.mainPixels.size())return "No selected screen sample";
	RemasterFrameChannelResolver resolver(frame);const auto s=resolver.select(frame,pixel,!frame.scenePixels.empty());
	static const char *names[]={"height","normal","occlusion","emission","material","doorframe","role"};
	std::ostringstream text;text<<"Visible instance "<<s.visible.instanceId;
	const auto visibleClaim=S9xRemasterPixelVisualClaim(frame,s.visible);
	text<<" visual "<<S9xRemasterVisualClaimName(visibleClaim.status)<<" graph-base-published "<<visibleClaim.published;
	for(size_t c=0;c<s.donors.size();c++) {
		text<<"\n"<<names[c]<<": ";const auto &p=s.donors[c];
		if(!p.instanceId){text<<"absent";continue;}
		const auto &i=frame.tileInstances[p.instanceId-1];
		text<<"instance "<<p.instanceId<<" BG"<<unsigned(i.sourceIndex+1)<<" pixel "<<unsigned(p.tilePixel)
			<<" base "<<i.heightOffset<<(i.hFlip?" H":"")<<(i.vFlip?" V":"");
		const auto v=S9xRemasterPixelVisualClaim(frame,p);
		text<<" visual "<<S9xRemasterVisualClaimName(v.status)<<" cell "<<v.cell<<" component "<<v.component
			<<" graph-base-published "<<v.published;
		if(resolver.effective[p.instanceId-1].tileRole==RemasterTileRole::OutOfBounds && v.published) {
			if(c==0 && i.hasPlacementHeight)text<<" source explicit-role semantic plane (not authored)";
			if(c==1 && i.hasPlacementNormals)text<<" source explicit-role geometric up normal (not authored)";
		}
	}
	return text.str();
}

inline size_t S9xRemasterRestoreSceneSubscreenWalls(RemasterFrame &frame,
	const std::set<RemasterTileContentId> *authoredStairs = nullptr)
{
	const size_t count = frame.mainPixels.size();
	if (frame.profileRomSha256 != "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb" ||
		frame.scenePixels.size() != count || frame.sceneRgb555.size() != count || frame.subPixels.size() != count)
		return 0;
	std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> metadata;
	for (const auto &m : frame.assetMetadata) metadata[m.tileId] = &m;
	std::map<uint32_t, bool> accepted;
	auto backdrop = [](const RemasterFramePixel &p) {
		return !p.instanceId && static_cast<RemasterSourceType>(p.owner >> 24) == RemasterSourceType::Backdrop;
	};
	size_t restored = 0;
	for (size_t k = 0; k < count; k++) {
		// BG3 owns presentation (HUD/dialogue), not the physical world beneath it.
		// Never replace an opaque scene or a main BG2/OBJ, even without metadata.
		const auto &main = frame.mainPixels[k];
		const bool presentation = static_cast<RemasterSourceType>(main.owner >> 24) == RemasterSourceType::Background &&
			((main.owner >> 16) & 255) == 2;
		if (!backdrop(frame.scenePixels[k]) || (!backdrop(main) && !presentation)) continue;
		const auto &p = frame.subPixels[k];
		if (!p.instanceId || p.instanceId > frame.tileInstances.size() || p.tilePixel >= 64 ||
			static_cast<RemasterSourceType>(p.owner >> 24) != RemasterSourceType::Background || ((p.owner >> 16) & 255) != 0) continue;
		const auto &i = frame.tileInstances[p.instanceId - 1];
		auto a = accepted.find(p.instanceId);
		if (a == accepted.end()) {
			bool physical = false;
			const auto m = metadata.find(i.tileId);
			if (i.source == RemasterSourceType::Background && i.sourceIndex == 0 && i.hasPaletteArtwork &&
				!S9xRemasterInstanceGeometryUnresolved(i)) {
				const auto effective = S9xRemasterResolveFrameGeometry(m != metadata.end() ? *m->second : RemasterFrameAssetMetadata{}, frame, p.instanceId - 1);
				// Transport the occurrence, not a fabricated complete geometry tuple.
				// Consumers retain independent has* flags, authored zero and transforms.
				const bool available = effective.hasHeight || effective.hasNormals || effective.hasOcclusion ||
					effective.hasEmission || effective.hasMaterialSelectors || effective.hasDoorframeMask;
				physical = available &&
					(effective.tileRole == RemasterTileRole::Wall || effective.tileRole == RemasterTileRole::WallCorner || effective.tileRole == RemasterTileRole::Doorway);
				physical = physical || effective.tileRole == RemasterTileRole::OutOfBounds;
				// Preserve the approved floor/stair receiver contract. In particular,
				// height-only landing art is not authorization to change stair appearance.
				physical = physical || (effective.hasHeight && effective.hasNormals &&
					(false
#ifndef ALTTP_TEST_PRIOR_SCENE_FLOORS
						 || effective.tileRole == RemasterTileRole::Floor
#endif
#ifndef ALTTP_TEST_PRIOR_SCENE_STAIRS
						 || effective.tileRole == RemasterTileRole::StairTop || effective.tileRole == RemasterTileRole::StairBottom
						 #ifndef ALTTP_TEST_PRIOR_SCENE_STAIR_GROUPS
						 || (effective.tileRole == RemasterTileRole::Unknown &&
							(i.assetGroup == "stair_treads" || i.assetGroup == "stair_rails" || (authoredStairs && authoredStairs->count(i.tileId))))
						 #endif
#endif
						 #ifndef ALTTP_TEST_PRIOR_SCENE_DOORWAYS
						 || effective.tileRole == RemasterTileRole::Doorway
						 #endif
					));
			}
			a = accepted.emplace(p.instanceId, physical).first;
		}
		if (!a->second) continue;
		frame.scenePixels[k] = p;
		frame.sceneRgb555[k] = i.paletteRgb555[p.tilePixel];
		restored++;
	}
	return restored;
}

inline std::set<uint32_t> S9xRemasterContextSelection(const RemasterProfile &profile,
	const RemasterFrame &frame, const std::vector<RemasterTileContentId> &tiles, const std::string &name)
{
	std::set<uint32_t> selected;
	for (const auto *pixels : {&frame.mainPixels, &frame.subPixels, &frame.scenePixels})
		for (const auto &pixel : *pixels) {
			const auto id = pixel.instanceId;
			if (!id || id > frame.tileInstances.size()) continue;
			const auto &instance = frame.tileInstances[id-1];
			if (instance.source != RemasterSourceType::Background ||
				std::find(tiles.begin(), tiles.end(), instance.tileId) == tiles.end()) continue;
			std::string effective;
			const auto contexts = profile.tileContexts.find(instance.tileId);
			if (contexts != profile.tileContexts.end() && contexts->second.size() == 1)
				effective = contexts->second.begin()->first;
			else if (contexts != profile.tileContexts.end() && contexts->second.size() > 1) {
				const auto at = frame.contextLocations.find(id);
				if (at == frame.contextLocations.end()) continue; // legacy capture cannot identify assignments
				const auto binding = profile.contextBindings.find(at->second);
				if (binding != profile.contextBindings.end() && binding->second.tile == instance.tileId &&
					contexts->second.count(binding->second.name)) effective = binding->second.name;
			}
			if (effective == name) selected.insert(id);
		}
	return selected;
}

inline void S9xRemasterApplyContextBindings(const RemasterProfile &profile, RemasterFrame &frame)
{
	frame.contextGeometry.clear();
	for (size_t i = 0; i < frame.tileInstances.size(); i++) {
		const auto &instance = frame.tileInstances[i];
		if (instance.source != RemasterSourceType::Background) continue;
		const auto contexts = profile.tileContexts.find(instance.tileId);
		if (contexts == profile.tileContexts.end() || contexts->second.empty()) continue;
		if (contexts->second.size() == 1) { frame.contextGeometry[i+1] = contexts->second.begin()->second; continue; }
		const auto location = frame.contextLocations.find(i+1);
		if (location == frame.contextLocations.end()) continue;
		const auto binding = profile.contextBindings.find(location->second);
		if (binding == profile.contextBindings.end() || !(binding->second.tile == instance.tileId)) continue;
		const auto selected = contexts->second.find(binding->second.name);
		if (selected != contexts->second.end()) frame.contextGeometry[i+1] = selected->second;
	}
}

// WRAM can already contain the next camera position while the PPU still shows
// the previous one. Use WRAM only to unwrap the rendered 10-bit scroll value.
inline int S9xRemasterUnwrapRenderedScroll(int worldHint, int renderedScroll)
{
	const unsigned delta = (unsigned(renderedScroll) - unsigned(worldHint)) & 1023;
	return worldHint + (delta < 512 ? int(delta) : int(delta) - 1024);
}

// Scroll values must be the renderer's unwrapped game-space BG origin. Reject
// inconsistent raster scroll/ownership rather than binding an arbitrary location.
inline void S9xRemasterLocateContextTiles(RemasterFrame &frame, const std::array<int,4> &scrollX,
	const std::array<int,4> &scrollY)
{
	frame.contextLocations.clear();
	if (!frame.area.valid()) return;
	// Instance IDs are dense and invocation-local. Check every observation, but
	// defer ordered publication until all buffers have had a chance to conflict.
	struct Observation { RemasterContextLocation at; uint8_t state = 0; };
	std::vector<Observation> observed(frame.tileInstances.size());
	for (const auto *pixels : {&frame.mainPixels, &frame.subPixels, &frame.scenePixels})
		for (size_t p=0;p<pixels->size();p++) {
			const auto &pixel=(*pixels)[p];
			if (!pixel.instanceId || pixel.instanceId>frame.tileInstances.size() || pixel.tilePixel>=64) continue;
			const auto &in=frame.tileInstances[pixel.instanceId-1];
			if (in.source!=RemasterSourceType::Background || in.sourceIndex>1) continue;
			int lx=pixel.tilePixel%8, ly=pixel.tilePixel/8;
			if(in.hFlip)lx=7-lx;if(in.vFlip)ly=7-ly;
			const int wx=scrollX[in.sourceIndex]+int(p%frame.width)-lx;
			const int wy=scrollY[in.sourceIndex]+int(p/frame.width)-ly;
			RemasterContextLocation at;at.area=frame.area.id;at.background=in.sourceIndex;
			const int mask=frame.area.id<320?511:65535;
			at.x=(wx&mask)/8;at.y=(wy&mask)/8;
			auto &entry=observed[pixel.instanceId-1];
			if (!entry.state) { entry.at=at; entry.state=1; }
			else if (entry.at<at || at<entry.at) entry.state=2;
		}
	for (size_t i=0;i<observed.size();++i) if (observed[i].state==1) {
		#ifdef REMASTER_TEST_CONTEXT_LOCATION_PUBLICATION
		REMASTER_TEST_CONTEXT_LOCATION_PUBLICATION();
		#endif
		frame.contextLocations.emplace_hint(frame.contextLocations.end(),static_cast<uint32_t>(i+1),observed[i].at);
	}
}

struct RemasterDungeonMovementCache
{
	bool valid=false, placed=false;
	std::array<uint8_t,8192> attributes;
	std::vector<uint32_t> seeds;
	std::array<int16_t,8192> components;
	std::array<int16_t,8192> thresholdOwners; // terminal claims from actual traversed bodies; -2 ambiguous
	std::vector<std::vector<uint16_t>> cells;
};

struct RemasterDungeonHeightMap;
#include "dungeon_producer_writes.h"
#include "dungeon_static_assembly.h"
#define REMASTER_STATIC_DECLARATION_ASSESSMENT 1
struct RemasterDungeonFloorContext
{
	// Immutable ROM declarations, not proof that a particular writer survived.
	// Optional: legacy captures explicitly lack this input.
	std::shared_ptr<const RemasterDungeonConstructionDeclarations> constructionDeclarations;
	const RemasterDungeonWriteTrace *producerWrites=nullptr;
	// Invocation-local EndFrame scratch shared by related BG/relation solves.
	// Exact ordered seeds retain actor-plane/position component ordering.
	RemasterDungeonMovementCache *movementCache=nullptr;
	const uint8_t *retainedOutgoingMaps = nullptr; // frozen complete final words, render-only provenance
	uint16_t retainedOutgoingRoom = 0xffff;
	// Read-only USA attribute builder completed independently of game timing.
	bool hasCompletedTransitionAttributes = false;
	enum class Surface : uint8_t { Unknown, Floor, Wall, Obstacle, Stair, Door, ClosedDoor, OutOfBounds };
	struct Placement
	{
		Surface surface = Surface::Unknown;
		std::array<int16_t, 64> height = {};
		// Canonical exact authored +Z, zero-height foot pixels, not wall caps.
		uint64_t groundedFloorMask = 0;
		uint64_t doorframeMask = 0;
		bool hasDoorframeMask = false;
		bool doorwaySupport = false;
		RemasterSupportDirection doorwaySupportDirection = RemasterSupportDirection::Unknown;
		uint16_t doorTemplateOffset = 0;
		uint16_t doorAssembly = 0; // origin+1; exact final template ownership
		uint8_t doorDirection = 0xff; // north,south,west,east
		bool splitDoorPart = false; // exact twelve-word cross-BG dispatch footprint
		bool closedDoorAssembly = false; // complete final closed dispatch, not an open-frame cutout
		// Surviving inner two-band jamb only. NEVER whole-frame ownership.
		uint16_t doorwayColumnContact = 0, doorwayColumnSource = 0;
		uint8_t doorwayColumnBand = 0;
		bool hasHeight = false;
		bool stairSourceAmbiguous = false; // unsupported alternate local source cannot prove an assembly
		uint8_t removableSupportPlanes = 0; // exact pot dispatch + completed indexed blocker + replacement floor
		uint16_t obstacleAssembly = 0, obstacleTemplate = 0; // exact unique final ROM footprint origin+1
		uint8_t obstacleAssemblyHeight = 0, obstacleAssemblyFlip = 0; // width2; transformed canonical foot
		bool authoredObstacle = false; // includes rejected/incomplete footprints; never adjacency-rescued
		uint16_t authoredObstacleAssembly = 0, authoredObstacleGroup = 0;
		int16_t authoredObstacleZ = 0; // signed tile units, NOT authored local pixels
		uint8_t authoredObstacleFlip = 0;
		bool authoredObstacleContact = false;
		uint8_t authoredObstacleDirection = 1; // transformed layout's supporting end
		bool hflip = false, vflip = false;
		// Independently classified, authored-zero flat BG usages. This is
		// candidate support evidence, never a BG-layer elevation label.
		uint8_t floorPlaneMask = 0;
		bool distinctLowerUsage = false;
		// A collision-solid dungeon pattern usage cannot anchor a plateau on
		// that plane. Explicit wall-top assemblies retain their own semantics.
		uint8_t structuralFloorPlaneMask = 3;
		uint16_t wallTemplateOffset = 0;
		uint16_t wallStackAssembly = 0; // unique complete ordered 4x4 vertical-wall writer footprint
		uint64_t tileHash = 0;
		bool supportingTrim = false;
		// Exact surviving low-corner branches own two separate tangent seams.
		// The corner is terminal: its folded shell is not an obstacle support
		// node and must not join the otherwise independent gameplay planes.
		bool terminalCorner = false;
		bool highPerimeterFace = false; // exact surviving high perimeter, including door priority writes
		bool highCornerFace = false; // ordered HIGH curved face; terminal seam ownership only
		bool lowerDoorWallPart = false; // terminal split-door cover at preserved high face seam
		bool diagonalHeightPreview = false; // effective local geometry only; authored asset is immutable
		bool diagonalAuthoredPreserved = false; // exact ROM cross section uses intrinsic samples
		uint8_t diagonalPreviewRow = 0;
		int16_t diagonalPreviewFoot = 0;
		// Exact assembly-local clipped affine model, including internal cap
		// continuation. Signed intercept is NOT a solved floor placement base.
		int32_t diagonalIntercept = 0; // affine intercept can exceed its clipped int16 samples
		int16_t diagonalMinimum = 0, diagonalMaximum = 0;
		bool semanticSoutheastWall = false;
		bool semanticBoundaryKnown = false;
		// Observer-only evidence. Does not promote a deferred assembly member
		// into the absolute-height graph or alter runtime placement offsets.
		uint16_t diagnosticWallTemplateOffset = 0;
		uint8_t diagnosticUphillDirections = 0; // displayed handler axis, flat caps only
		uint8_t diagnosticNormalUphill = 0; // canonical, consistent authored face normals
		bool diagnosticAuthoredFloor = false; // preserves floor role before solver defers nonflat trim
		bool diagnosticRomFloorTrim = false; // exact bounded ROM trim run; diagnostic support only
		// Complete classifier output except source artwork identity. No padding,
		// hash-level defaults, word-bit masking, or neighbor-based equivalence.
		bool sameGeometry(const Placement &b) const
		{
			auto fields = [](const Placement &t) { return std::tie(
				t.surface,t.height,t.groundedFloorMask,t.doorframeMask,t.hasDoorframeMask,t.doorwaySupport,t.doorwaySupportDirection,
				t.doorTemplateOffset,t.doorAssembly,t.doorDirection,t.splitDoorPart,t.closedDoorAssembly,
				t.doorwayColumnContact,t.doorwayColumnSource,t.doorwayColumnBand,
				t.hasHeight,t.stairSourceAmbiguous,t.removableSupportPlanes,t.obstacleAssembly,t.obstacleTemplate,t.obstacleAssemblyHeight,t.obstacleAssemblyFlip,
				t.authoredObstacle,t.authoredObstacleAssembly,t.authoredObstacleGroup,t.authoredObstacleZ,t.authoredObstacleFlip,t.authoredObstacleContact,t.authoredObstacleDirection,t.hflip,t.vflip,t.floorPlaneMask,
				t.distinctLowerUsage,t.structuralFloorPlaneMask,t.wallTemplateOffset,t.wallStackAssembly,
				t.supportingTrim,t.terminalCorner,t.highPerimeterFace,t.highCornerFace,
				t.lowerDoorWallPart,t.diagonalHeightPreview,t.diagonalAuthoredPreserved,t.diagonalPreviewRow,
				t.diagonalPreviewFoot,t.diagonalIntercept,t.diagonalMinimum,t.diagonalMaximum,
				t.semanticSoutheastWall,t.semanticBoundaryKnown,t.diagnosticWallTemplateOffset,
				t.diagnosticUphillDirections,t.diagnosticNormalUphill,
				t.diagnosticAuthoredFloor,t.diagnosticRomFloorTrim); };
			return fields(*this)==fields(b);
		}
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
	// USA $048a destination selector latched by stair detection, not the
	// current actor plane ($ee), which switches only after the camera scroll.
	uint8_t straightStairPlane = 255;
	// Which placed artwork is being solved; never a gameplay-plane label.
	uint8_t placementBackgroundIndex = 1;
	uint16_t linkX = 0;
	uint16_t linkY = 0;
	uint16_t roomIndex = 0xffff;
	uint16_t previousRoomIndex = 0xffff;
	uint8_t submodule = 0;
	uint8_t module = 7; // adapters default to settled dungeon module
	uint8_t savedModule = 0xff; // USA $010c; unknown in legacy capture evidence
	RemasterDungeonLayoutEvidence layoutEvidence; // value-owned source membership; never geometry-keyed/serialized
	uint8_t constructionPhase = 0;
	// USA $0200: selectable collision builder reaches 5 only after basic,
	// object, door and crystal overrides. 255 means no adapter attestation.
	uint8_t attributeConstructionState = 255;
	uint8_t scrollDirection = 255; // USA $0418: south,north,east,west; unknown legacy
	uint16_t destinationOriginX = 0, destinationOriginY = 0; // loader $062c/$062e
	bool hasDestinationOrigin = false;
	uint8_t roomTransitionFlags = 0;
	bool groundedEvidence = false; // verified adapter: ordinary grounded Link
	uint64_t romIdentity = 0; // verified ROM identity, not a room/map hash
	uint16_t backgroundScrollX = 0;
	uint16_t backgroundScrollY = 0;
	const uint8_t *collisionAttributes = nullptr;
	// Complete constructed BG2 room, never a viewport inventory. Heights are
	// canonical authored relief; flips are applied by the geometry solver.
	const Placement *placements = nullptr;
	const Placement *peerProducerPlacements = nullptr; // independent decoded peer, never a height donor
	const uint8_t *constructedMaps = nullptr; // both final WRAM BG maps, 16384 bytes
	const uint8_t *graphics = nullptr; // complete VRAM; included in state identity
	const uint16_t *emitterPalette = nullptr; // current CGRAM, emitter-only (not geometry state)
	// Verified USA ROM predefined tile table ($80:9B52, 6438 words).
	// Optional: old captures/adapters conservatively lack template evidence.
	const uint8_t *dungeonTemplateData = nullptr;
	// Optional live producer evidence; never retained by pointer or keyed as geometry.
	struct TerrainActor { uint16_t x=0,y=0; uint8_t state=0,kind=0,z=0,plane=0; };
	std::array<TerrainActor,16> terrainActors = {};
	struct TerrainOam { uint16_t x=0; uint8_t y=0,name=0,flags=0,size=0; };
	std::array<TerrainOam,128> terrainOam = {};
	bool hasTerrainActors = false;
	// Verified live sprite producer observations; no actor plane inferred from OBJ priority.
	struct ObstacleActor {
		uint16_t x=0,y=0; uint8_t type=0,state=0,z=0,plane=0;
		uint8_t xSubpixel=0,xVelocity=0,yVelocity=0;
		bool hasMotion=false;
	};
	std::array<ObstacleActor,16> obstacleActors = {};
	bool hasObstacleActors = false;
	uint16_t actorScrollX=0,actorScrollY=0;
	int16_t linkZ=-1;
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
	// Other-BG authored structural path to a reproduced support, not a copied
	// visible base. Consumers require identical final wall identity/transform.
	struct WallSupportRelation {
		int wall, support, rise; uint16_t word; std::array<int16_t,64> height;
		// A terminal low-face export is not paired-art plane equivalence.
		bool terminalLowPath=false;
		uint16_t source=0;
		bool hflip=false,vflip=false;
		std::vector<int> path;
	};
	const std::vector<WallSupportRelation> *wallSupportRelations = nullptr;
	// Visual-shell evidence is separate from physical floor/traversal support.
	// The peer is immutable complete placement classification, never viewport art.
	const Placement *peerShellPlacements = nullptr;
	const RemasterDungeonHeightMap *peerShellMap = nullptr;
	// Other-BG directional landing proof for an exact split-door footprint.
	// This transports a datum, never the other BG's visible/frame band base.
	struct DoorwaySupportRelation { uint16_t assembly, source; uint8_t direction, plane; int base; };
	const std::vector<DoorwaySupportRelation> *doorwaySupportRelations = nullptr;
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

inline bool S9xRemasterReadTerrainEvidence(const std::string &path,RemasterDungeonFloorContext &c)
{
	std::ifstream f(path,std::ios::binary);if(!f)return false;
	std::vector<uint8_t> b{std::istreambuf_iterator<char>(f),{}};
	if(b.size()!=910 || std::memcmp(b.data(),"ALTPDA1\0",8))return false;
	size_t pos=8;auto word=[&](){int v=b[pos]|b[pos+1]<<8;pos+=2;return uint16_t(v);};
	c.actorScrollX=word();c.actorScrollY=word();c.linkZ=int16_t(word());
	for(auto &a:c.terrainActors){a.x=word();a.y=word();a.state=b[pos++];a.kind=b[pos++];a.z=b[pos++];a.plane=b[pos++];if(a.plane>1)return false;}
	for(auto &o:c.terrainOam){o.x=word();o.y=b[pos++];o.name=b[pos++];o.flags=b[pos++];o.size=b[pos++];if(o.x>511 || o.size>2)return false;}
	c.hasTerrainActors=true;return true;
}

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
		const auto &t=c.placements[i];mix(t.tileHash);mix(int(t.surface));mix(t.hasHeight);mix(t.hflip);mix(t.vflip);mix(t.stairSourceAmbiguous);
		mix(t.wallTemplateOffset);mix(t.wallStackAssembly);mix(t.distinctLowerUsage);mix(t.structuralFloorPlaneMask);mix(t.floorPlaneMask);
		mix(t.supportingTrim);mix(t.highPerimeterFace);mix(t.lowerDoorWallPart);mix(t.groundedFloorMask);
		mix(t.doorwaySupport);mix(static_cast<uint8_t>(t.doorwaySupportDirection));
		mix(t.doorwayColumnContact);mix(t.doorwayColumnSource);mix(t.doorwayColumnBand);
		mix(t.obstacleAssembly);mix(t.obstacleTemplate);mix(t.obstacleAssemblyHeight);mix(t.obstacleAssemblyFlip);
		mix(t.authoredObstacle);mix(t.authoredObstacleAssembly);mix(t.authoredObstacleGroup);mix(t.authoredObstacleZ);mix(t.authoredObstacleFlip);mix(t.authoredObstacleContact);
		mix(t.authoredObstacleDirection);
		mix(t.terminalCorner);
		mix(t.highCornerFace);
		mix(t.removableSupportPlanes);
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
		int existing=-1;
		for (int i=0;i<c.stairCount;i++) if (c.stairs[i].x==x && c.stairs[i].y==y) existing=i;
		if (existing<0 && c.stairCount == c.stairs.size()) continue;
		auto &s=c.stairs[existing<0 ? c.stairCount++ : existing]; s.x=x; s.y=y;
		if(existing<0){s.table=kind>=0x30; s.tableEntry=255;}
		// Table 1 is the north-facing assembly (including 1e, handler
		// subtype2/31); table 2's 3e/3d assemblies face south. The low
		// nibble describes transition behavior, NOT the high landing direction.
		s.highIsNorth=kind==0x1d || kind==0x1e || kind==0x1f || kind==0x3f;
		s.changesPlane=(kind&15)!=13;
	}
}

// USA Module07 handlers: ordinary door scroll and straight inter-room stairs.
// Other warps/falls/spiral stairs have different publication/visibility lifetimes.
inline bool S9xRemasterDungeonRoomScroll(uint8_t submodule)
{
	return submodule==2 || submodule==18 || submodule==19;
}

inline uint8_t S9xRemasterDungeonRoomLoadedPhase(uint8_t submodule)
{
	return submodule==2?2:3; // LoadNextRoom vs StraightInterroomStairs/02
}

inline uint16_t S9xRemasterDungeonRoomForScreenPosition (const RemasterDungeonFloorContext &context,
	uint32_t x, uint32_t y)
{
	if (context.module != 7 || !S9xRemasterDungeonRoomScroll(context.submodule) || context.previousRoomIndex == 0xffff ||
		context.previousRoomIndex == context.roomIndex)
		return context.roomIndex;
	const bool straight=context.submodule==18 || context.submodule==19;
	// Stair detection changes A0 before the fade/loader; origin still names the
	// source until handler02 returns. Never project destination art onto it.
	if((straight || context.retainedOutgoingRoom==context.previousRoomIndex) &&
		context.constructionPhase<S9xRemasterDungeonRoomLoadedPhase(context.submodule))return context.previousRoomIndex;
	if(context.hasDestinationOrigin && context.scrollDirection<4) {
		// MoveLinkOutDoor resets $0418 at completion of phase13. By14 the
		// camera scroll has ended and the entire viewport belongs to destination.
		if(context.constructionPhase>=(straight?11:14))return context.roomIndex;
		// Header loader records the actual BG origin, including teleport-door
		// remaps. Compare in 16-bit camera space, not a room-ID-grid guess.
		const int16_t dx=static_cast<int16_t>(uint16_t(context.backgroundScrollX+x-context.destinationOriginX));
		const int16_t dy=static_cast<int16_t>(uint16_t(context.backgroundScrollY+y-context.destinationOriginY));
		const bool source=context.scrollDirection==0?dy<0:context.scrollDirection==1?dy>=512:
			context.scrollDirection==2?dx<0:dx>=512;
		return source?context.previousRoomIndex:context.roomIndex;
	}
	// During an intra-dungeon scroll Zelda updates $A0 to the destination while
	// the source room is still present on the opposite side of the viewport.
	// $A2 and the room-grid delta record the source and direction. $EF cannot be
	// used here: it is a door layer-toggle bitfield and is cleared as scrolling
	// starts. Only supertile handler2 mixes two room IDs; subtile1 and
	// in-room stairs8/16 must ignore a stale predecessor ID.
	// The source/destination split is the room boundary in unwrapped BG2 space;
	// do not modulo first or destination geometry leaks onto source-room pixels.
	const int worldX = static_cast<int>(context.backgroundScrollX) + static_cast<int>(x);
	const int worldY = static_cast<int>(context.backgroundScrollY) + static_cast<int>(y);
	const int currentColumn = context.roomIndex & 15, previousColumn = context.previousRoomIndex & 15;
	const int currentRow = (context.roomIndex >> 4) & 15, previousRow = (context.previousRoomIndex >> 4) & 15;
	// Legacy inputs attest only ordinary cardinal grid neighbors. Unknown
	// teleport/noncardinal pairs cannot safely claim predecessor ownership.
	if(!((currentRow==previousRow && std::abs(currentColumn-previousColumn)==1) ||
		(currentColumn==previousColumn && std::abs(currentRow-previousRow)==1)))return context.roomIndex;
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
	std::array<bool,4096> producerObstacle = {}; // proved usage; local relief is immutable
	std::array<bool,4096> producerOwnedWall = {}; // complete ROM wall usage can override asset-level Door role
	std::array<bool,4096> producerOwnedStair = {}; // terminal complete-ROM assembly; never actor support
	std::array<bool,4096> producerAuthoredObstacle = {}; // explicit footprint outranks stale role/groups
	std::array<bool,4096> producerGeneratedObstacle = {}; // runtime-only local ramp, never profile metadata
	uint16_t roomIndex = 0xffff;
	// 255 remains the reserved unknown marker for legacy API/map compatibility.
	// Solved bases are signed; 255 itself is not an admissible placement base.
	std::array<int32_t, 64 * 64> offsets = {};
	std::array<std::array<int32_t, 64 * 64>, 2> planeOffsets = {};
	// Terminal physical support is not evidence of ownership of BG artwork.
	// Only live solves populate this channel; legacy authored maps have none.
	bool hasActorThresholdSupport = false;
	std::array<std::array<int32_t,4096>,2> actorThresholdBases = {};
	std::array<std::array<int16_t,4096>,2> actorThresholdOwners = {}; // -1 absent, -2 competing bodies
	// Reproduced movement-body ownership, before any semantic floor extension.
	// -1 means no occupied body; these are not visual or structural nodes.
	std::array<std::array<int32_t, 4096>, 2> occupiedFloorOwners = {};
	uint16_t componentCount = 0;
	uint16_t constraintCount = 0;
	uint16_t conflictingComponentCount = 0;
	std::array<uint16_t, 2> solvedPlaneCells = {};
	bool hasPlaneOffsets = false;
	bool hasRoomGeometry = false;
	std::array<uint8_t, 4096> fallbackDirections = {};
	std::array<std::array<int32_t, 4096>, 2> structuralPlaneOffsets = {};
	std::vector<RemasterDungeonFloorContext::WallSupportRelation> authoredWallSupportRelations;
	uint32_t visibleShellContacts = 0, visibleShellRejected = 0;
	// Complete placed-map candidate inventory, not visibility or solved coverage.
	// All sample arrays are displayed (H/V transformed); failed ownership must
	// not hide a later malformed cross section from diagnostics.
	struct VisibleShellCandidate {
		int bg=0, plane=0, cell=0, peer=0, dx=0, dy=0;
		uint16_t source=0, peerSource=0;
		int peerBase=255, proposed=INT_MIN, support=-1;
		bool occupied=false, relation=false, gradient=true, uniform=true;
		std::array<int,8> edge={}, peerEdge={}, slope={}, peerSlope={};
		std::string reason;
	};
	std::vector<VisibleShellCandidate> visibleShellCandidates;
	uint64_t shellMapIdentity=0;
	std::array<std::array<int32_t, 4096>, 2> mixedFloorBases = {};
	uint16_t mixedFloorUnknown = 0, mixedFloorConflicts = 0;
	std::array<uint16_t, 2> requiredPlaneCells = {};
	std::array<uint16_t, 2> unknownPlaneCells = {};
	uint16_t structuralConflicts = 0;
	struct AuthoredObstacleEvidence {
		uint16_t assembly = 0, group = 0;
		int plane = -1, component = -1, floorBase = 255;
		uint16_t members = 0, contacts = 0, witnessedContacts = 0;
		bool conflict = false;
	};
	std::vector<AuthoredObstacleEvidence> authoredObstacleEvidence;
	uint16_t authoredObstacleCells = 0, solvedAuthoredObstacleCells = 0, unmatchedAuthoredObstacleCells = 0;
	std::array<uint16_t,2> persistentSupportRequired = {}, persistentSupportMissing = {};
	struct PersistentSupport { int cell,plane,component,base; }; // component -2 ambiguous, -3 unobserved
	std::vector<PersistentSupport> persistentSupport;
	uint16_t solvedWallCells = 0;
	uint16_t unknownWallCells = 0;
	std::array<uint16_t,2> candidateFloorCells = {};
	std::array<uint16_t,2> unobservedFloorCells = {};
	bool disconnectedPlaneCandidates = false;
	bool planeRelationshipUnresolved = false;
	struct Conflict { int node, other, expected, actual; };
	std::vector<Conflict> geometryConflicts;
	struct CornerBranch { int corner, plane, branch, base, support, rise; };
	std::vector<CornerBranch> cornerBranches;
	struct DoorframeAttachment { int cell, plane, base; uint64_t mask; uint16_t source; bool visible = true; int openingBase = 255; };
	std::vector<DoorframeAttachment> doorframeAttachments;
	std::vector<RemasterDungeonFloorContext::DoorwaySupportRelation> doorwaySupportRelations;
	struct StairAttachment { int origin, low, high, lowPlane, highPlane, rise, base; uint8_t flip; bool conflict; };
	std::vector<StairAttachment> stairAttachments;
	struct LocalHeightPreview { int cell; std::array<int16_t,64> height; };
	std::vector<LocalHeightPreview> localHeightPreviews;
	uint16_t unknownDoorframeCells = 0, conflictingDoorframeCells = 0;
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

#include "dungeon_wall_mounts.h"

// Canonical local source proof; placement flips are applied only when selecting
// displayed seam samples. A cap is not permission to ignore arbitrary relief.
inline bool S9xRemasterDungeonDoorTangentCap(const RemasterDungeonFloorContext::Placement &wall,
	uint8_t doorDirection)
{
	if(doorDirection>=4 || wall.surface!=RemasterDungeonFloorContext::Surface::Wall ||
		!wall.hasHeight || !wall.wallTemplateOffset || (!wall.highCornerFace && !wall.highPerimeterFace))return false;
	const int low=*std::min_element(wall.height.begin(),wall.height.end());
	const int cap=*std::max_element(wall.height.begin(),wall.height.end());
	if(cap<=low || cap>=low+7)return false;
	for(int y=0;y<8;y++)for(int x=0;x<8;x++)
		if(wall.height[y*8+x]!=std::min(cap,low+7-(doorDirection<2?y:x)))return false;
	return true;
}

// Exact final handler footprint, not a tile-hash or collision-height inference.
// The rails prove the assembly's authored zero datum and signed upper endpoint;
// treads retain their independent authored relief. Priority-only writes are safe.
struct RemasterDungeonStairProof { int origin, low, high, rise; uint8_t flip; uint64_t producer=0; };
inline bool S9xRemasterDungeonStairProof(const RemasterDungeonFloorContext &c,
	const RemasterDungeonFloorContext::Stair &s, RemasterDungeonStairProof &out)
{
	using S=RemasterDungeonFloorContext::Surface;
	if(!c.dungeonTemplateData || !c.constructedMaps || !c.placements || s.x>60 || !s.y || s.y>59)return false;
	const int origin=s.y*64+s.x, vf=s.highIsNorth?0:0x8000;
	// Quantized tread contract (explicitly approved convention): four complete
	// equal steps above the zero pre-step landing. This is NOT the smooth rail
	// contract below. Require a live Nx4 both-BG producer and an intact peer;
	// final matching words from another writer cannot satisfy this witness.
	if(S9xRemasterDungeonResearchEnabled() && c.producerWrites && c.producerWrites->enabled && c.producerWrites->room==c.roomIndex)for(int bg=0;bg<2;bg++) {
		const auto *tiles=bg==c.placementBackgroundIndex?c.placements:c.peerProducerPlacements;
		if(!tiles)continue;
		const auto &first=c.producerWrites->cells[bg*4096+origin];
		if(!first.owner || first.operation!=2 || first.origin!=origin ||
			first.source!=(s.highIsNorth?0x10c8:0x10e8))continue;
		bool exact=true;int step=0;
		for(int y=0;y<4;y++)for(int x=0;x<4;x++) {
			const int i=origin+y*64+x;const auto &t=tiles[i];const auto &w=c.producerWrites->cells[bg*4096+i];
			const int offset=(bg==c.placementBackgroundIndex?0:8192)+2*i;
			const int finalWord=c.constructedMaps[offset]|c.constructedMaps[offset+1]<<8;
			if(w.owner!=first.owner || w.source!=first.source || w.part!=x*4+y ||
				w.word!=finalWord || t.hflip!=bool(finalWord&0x4000) || t.vflip!=bool(finalWord&0x8000) ||
				!t.hasHeight || t.stairSourceAmbiguous || t.authoredObstacle || t.obstacleAssembly ||
				(t.surface!=S::Unknown && t.surface!=S::Stair))exact=false;
			const int level=s.highIsNorth?4-y:y+1;
			for(int k=0;k<64;k++) {
				const int h=t.height[k];if(!step && h>0 && h%level==0)step=h/level;
				if(step<=0 || h!=level*step)exact=false;
			}
		}
		if(exact){out={origin,(s.highIsNorth?s.y+4:s.y-1)*64+s.x+1,
			(s.highIsNorth?s.y-1:s.y+4)*64+s.x+1,4*step,uint8_t(vf?2:0),first.owner};return true;}
	}
	int matches=0, rise=0;uint8_t flip=0;
	for(int hf:{0,0x4000}) {
		bool exact=true;
		for(int x=0;x<4;x++)for(int y=0;y<4;y++) {
			const int off=0x10c8+2*((hf?3-x:x)*4+(vf?3-y:y));
			const int expected=(c.dungeonTemplateData[off]|c.dungeonTemplateData[off+1]<<8)^vf^hf;
			const int i=origin+y*64+x,word=c.constructedMaps[i*2]|c.constructedMaps[i*2+1]<<8;
			const auto &t=c.placements[i];
			if((word&0xdfff)!=(expected&0xdfff) || !t.hasHeight || t.stairSourceAmbiguous ||
				(t.surface!=S::Stair && t.surface!=S::Unknown) || t.authoredObstacle || t.obstacleAssembly ||
				t.hflip!=bool(word&0x4000) || t.vflip!=bool(word&0x8000))exact=false;
		}
		auto height=[&](int i,int x,int y){const auto &t=c.placements[i];return t.height[(t.vflip?7-y:y)*8+(t.hflip?7-x:x)];};
		int measured=-1;
		for(int side:{0,3}) {
			if(c.placements[origin+side].surface!=S::Stair || c.placements[origin+3*64+side].surface!=S::Stair)exact=false;
			int first=-1,last=-1,previous=INT_MAX;
			for(int step=0;step<32;step++) {
				const int yy=s.highIsNorth?step:31-step,i=origin+(yy/8)*64+side,h=height(i,0,yy%8);
				for(int x=1;x<8;x++)if(height(i,x,yy%8)!=h)exact=false;
				if(h>previous || (previous!=INT_MAX && previous-h>2))exact=false;
				if(!step)first=h;last=h;previous=h;
			}
			const int r=first+1-last;
			if(last!=0 || r<=0 || (measured>=0 && measured!=r))exact=false;measured=r;
		}
		// A bilaterally symmetric handler can have two identical H descriptions;
		// they are the SAME final footprint/endpoints, not competing owners.
		if(exact && !matches){matches++;rise=measured;flip=(hf?1:0)|(vf?2:0);}
	}
	if(matches!=1)return false;
	out={origin,(s.highIsNorth?s.y+4:s.y-1)*64+s.x+1,
		(s.highIsNorth?s.y-1:s.y+4)*64+s.x+1,rise,flip};return true;
}

inline bool S9xRemasterBuildDungeonHeightMap (const RemasterDungeonFloorContext &inputContext,
	uint8_t floorRise, RemasterDungeonHeightMap &result)
{
	RemasterLightingTrace::Scope lightingTimer(RemasterLightingTrace::current(), RemasterLightingTrace::Solve);
#ifdef ALTTP_TEST_CACHE_STAGES
	auto &stages=S9xRemasterDungeonCacheStages();RemasterDungeonStageTimer stage(stages.solveMs,stages.solves);
#endif
	auto context=inputContext;
	// Persistent structural support is not current movement reachability. Only
	// an intact indexed liftable with ROM-proved ordinary replacement floor can
	// contribute its post-removal support body. The doorway graph stays unchanged.
	std::unique_ptr<std::array<uint8_t,8192>> persistentAttributes;
	if(context.placements && context.collisionAttributes)for(int i=0;i<4096;i++)
		if(context.placements[i].removableSupportPlanes) {
			if(!persistentAttributes){persistentAttributes=std::make_unique<std::array<uint8_t,8192>>();std::copy_n(context.collisionAttributes,8192,persistentAttributes->begin());}
			for(int p=0;p<2;p++)if(context.placements[i].removableSupportPlanes&(1<<p))(*persistentAttributes)[p*4096+i]=0;
		}
	// Do NOT feed post-removal attributes into ordinary movement, wall ownership
	// or border fallbacks. Potential bodies are separately attached below.
	result = RemasterDungeonHeightMap{};
	result.roomIndex = context.roomIndex;
	result.offsets.fill(255);
	if (context.placements) for (int i=0;i<4096;i++) {
		result.producerHashes[i]=context.placements[i].tileHash;
		result.producerFlips[i]=context.placements[i].hflip | (context.placements[i].vflip<<1);
		const auto &t=context.placements[i];
		result.producerObstacle[i]=t.surface==RemasterDungeonFloorContext::Surface::Obstacle;
		result.producerOwnedWall[i]=t.surface==RemasterDungeonFloorContext::Surface::Wall && t.wallTemplateOffset!=0;
		result.producerAuthoredObstacle[i]=t.authoredObstacle;
		result.producerZeroFloor[i]=t.hasHeight && t.surface==RemasterDungeonFloorContext::Surface::Floor &&
			std::all_of(t.height.begin(),t.height.end(),[](int16_t h){return h==0;});
	}
	for (auto &plane : result.planeOffsets) plane.fill(255);
	for (auto &plane : result.actorThresholdBases) plane.fill(255);
	for (auto &plane : result.actorThresholdOwners) plane.fill(-1);
	result.hasActorThresholdSupport = true;
	for (auto &plane : result.occupiedFloorOwners) plane.fill(-1);
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
	auto thresholdOwners=std::make_unique<std::array<int16_t,8192>>();
	thresholdOwners->fill(-1);
	std::vector<std::vector<uint16_t>> cells;
	auto visitedStorage = std::make_unique<std::array<uint8_t, 2 * 497 * 488>>();
	auto &visited = *visitedStorage;
#ifndef ALTTP_TEST_REFERENCE_MOVEMENT
	// Every leading-edge offset is a multiple of eight or one less than a
	// multiple of eight. The exact sampled cells therefore depend only on the
	// origin cell and whether each pixel phase is zero/nonzero. Memoize that
	// predicate, NOT connectivity or support ownership, within this invocation.
	// Low nibble: known directions; high nibble: accepted directions.
	auto movementPredicates=std::make_unique<std::array<uint8_t,2*4096*4>>();
	auto projectedBodies=std::make_unique<std::array<int16_t,2*4096*4>>();
	projectedBodies->fill(-1);
#endif
	auto walkable = [&] (int plane, int x, int y, int direction) {
		static const int sx[4][3] = {{ 0, 8, 15 }, { 8, 0, 15 }, { 0, 0, 0 }, { 15, 15, 15 }};
		static const int sy[4][3] = {{ 8, 8, 8 }, { 24, 24, 24 }, { 8, 16, 23 }, { 8, 16, 23 }};
#ifndef ALTTP_TEST_REFERENCE_MOVEMENT
		const int phase=bool(x&7)+2*bool(y&7),cell=plane*4096+(y/8)*64+x/8;
		auto &cached=(*movementPredicates)[cell*4+phase];
		const uint8_t known=1<<direction,accepted=known<<4;
		if(cached&known)return bool(cached&accepted);
		cached|=known;
#endif
		for (int i = 0; i < 3; i++)
		{
			const uint8_t value = context.collisionAttributes[plane * 4096 +
				((y + sy[direction][i]) / 8) * 64 + (x + sx[direction][i]) / 8];
			if (value != 0 && !(value >= 0x80 && value <= 0x8f &&
				(value & 1) == (direction >= 2))) return false;
		}
#ifndef ALTTP_TEST_REFERENCE_MOVEMENT
		cached|=accepted;
#endif
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
	bool movementReused=false;
#ifndef ALTTP_TEST_REFERENCE_MOVEMENT
	if(auto *cache=context.movementCache) {
		movementReused=cache->valid && cache->placed==bool(context.placements) && cache->seeds==seeds &&
			!std::memcmp(cache->attributes.data(),context.collisionAttributes,8192);
		if(movementReused){components=cache->components;cells=cache->cells;*thresholdOwners=cache->thresholdOwners;}
	}
#endif
	if(!movementReused)for (uint32_t seed : seeds)
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
#ifndef ALTTP_TEST_REFERENCE_MOVEMENT
			const int originCell=plane*4096+(y/8)*64+x/8;
			const int bodyPhase=bool(x&7)+2*bool(y&7);
			const bool project=(*projectedBodies)[originCell*4+bodyPhase]!=id;
			(*projectedBodies)[originCell*4+bodyPhase]=id;
			// Ordinary support keeps its original first claim. Threshold claims
			// must still see another component's identical footprint as ambiguous.
			if(project)
#endif
			for (int cellY = (y + 8) / 8; cellY <= (y + 23) / 8; cellY++)
				for (int cellX = x / 8; cellX <= (x + 15) / 8; cellX++)
				{
					const int index = cellY * 64 + cellX;
					const uint8_t attribute=context.collisionAttributes[plane*4096+index];
					if(attribute>=0x80 && attribute<=0x8f) {
						// Only a complete body reached by the unchanged directional movement
						// predicate can claim a threshold. A nearby floor is not evidence.
						bool legal=true;
						for(int yy=(y+8)/8;yy<=(y+23)/8;yy++)for(int xx=x/8;xx<=(x+15)/8;xx++) {
							const auto v=context.collisionAttributes[plane*4096+yy*64+xx];
							if(v!=0 && !(v>=0x80 && v<=0x8f && (v&1)==(attribute&1)))legal=false;
						}
						if(legal){auto &owner=(*thresholdOwners)[plane*4096+index];
							if(owner==-1)owner=id;else if(owner!=id)owner=-2;}
					}
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
				if (nx < 0 || nx > 496 || ny < 0 || ny > 487) continue;
				const uint32_t next = (plane * 488 + ny) * 497 + nx;
#ifndef ALTTP_TEST_REFERENCE_MOVEMENT
				// Already queued destinations cannot change this traversal. Avoid
				// collision sampling on those edges; no accepted new edge is skipped.
				if(visited[next])continue;
#endif
				if (!walkable(plane, nx, ny, direction)) continue;
				const uint8_t center = context.collisionAttributes[plane * 4096 +
					((y + 16) / 8) * 64 + (x + 8) / 8];
				if (center >= 0x80 && center <= 0x8f && (center & 1) != (direction >= 2)) continue;
				if (!visited[next])
				{
					visited[next] = 1;
					queue.push_back(next);
				}
			}
		}
	}

#ifndef ALTTP_TEST_REFERENCE_MOVEMENT
	if(!movementReused && context.movementCache) {
		auto &cache=*context.movementCache;
		cache.valid=true;cache.placed=bool(context.placements);cache.seeds=seeds;
		std::copy_n(context.collisionAttributes,8192,cache.attributes.begin());
		cache.components=components;cache.cells=cells;
		cache.thresholdOwners=*thresholdOwners;
	}
#endif
	// A temporary blocker cannot merge existing movement components. Enumerate
	// only newly legal complete bodies touching a proved removed footprint. A
	// body must touch exactly ONE original support component; ownership is taken
	// from that reproduced body, not neighboring tile artwork or a flood fill.
	auto occupiedComponents=std::make_unique<std::array<int16_t,8192>>(components);
	std::unique_ptr<std::array<int16_t,8192>> persistentClaims;
	if(persistentAttributes && context.placements) {
		auto claims=std::make_unique<std::array<int16_t,8192>>();claims->fill(-1);
		for(int p=0;p<2;p++)for(int i=0;i<4096;i++)if(context.placements[i].removableSupportPlanes&(1<<p))(*claims)[p*4096+i]=-3; // unobserved required footprint
		// A body can touch a removed 8x8 cell only in this exact bounded origin
		// interval. Deduplicate those origins, not all242536 room positions per
		// plane. This enumerates the IDENTICAL body predicate at every pixel phase.
		auto candidates=std::make_unique<std::array<uint8_t,2*497*488>>();
		std::vector<int> origins;
		for(int p=0;p<2;p++)if(activePlanes[p])for(int i=0;i<4096;i++)if(context.placements[i].removableSupportPlanes&(1<<p)) {
			for(int y=std::max(0,i/64*8-23);y<=std::min(487,i/64*8-1);y++)
				for(int x=std::max(0,i%64*8-15);x<=std::min(496,i%64*8+7);x++) {
					const int origin=(p*488+y)*497+x;
					if(!(*candidates)[origin]){(*candidates)[origin]=1;origins.push_back(origin);}
				}
		}
#ifdef ALTTP_TEST_EXHAUSTIVE_PERSISTENT_BODIES
		origins.clear();for(int p=0;p<2;p++)if(activePlanes[p])for(int y=0;y<=487;y++)for(int x=0;x<=496;x++)origins.push_back((p*488+y)*497+x);
#endif
		for(int origin:origins) {
			const int x=origin%497,row=origin/497,p=row/488,y=row%488;
			bool removed=false,legal=true;int owner=-1;
			for(int yy=(y+8)/8;yy<=(y+23)/8;yy++)for(int xx=x/8;xx<=(x+15)/8;xx++) {
				const int i=p*4096+yy*64+xx;
				removed|=context.placements[i%4096].removableSupportPlanes&(1<<p);
				legal &= (*persistentAttributes)[i]==0;
				const int n=(*occupiedComponents)[i];
				if(n>=0){if(owner==-1)owner=n;else if(owner!=n)owner=-2;}
			}
			if(!removed || !legal || owner==-1)continue;
			for(int yy=(y+8)/8;yy<=(y+23)/8;yy++)for(int xx=x/8;xx<=(x+15)/8;xx++) {
				const int i=p*4096+yy*64+xx;if((*occupiedComponents)[i]>=0)continue;
				auto &n=(*claims)[i];if(n==-1 || n==-3)n=owner;else if(n!=owner)n=-2;
			}
		}
		persistentClaims=std::move(claims);
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
		std::vector<RemasterDungeonFloorContext::SupportRelation> verifiedStairRelations;
		std::vector<int> ownedHighFaceSeeds;
		std::set<std::pair<int,int>> signedShellEdges;
		std::set<std::pair<int,int>> physicalShellBoundaries;
		std::map<int,int> shellSupportAnchors;
		std::vector<RemasterRoomDiagnosticContact> diagnosticContacts;
		auto nodeStorage = std::make_unique<std::array<int, 8192>>();
		auto &nodes = *nodeStorage;
		nodes.fill(-1);
		int count = static_cast<int>(cells.size());
		for (int p = 0; p < 2; p++) for (int i = 0; i < 4096; i++)
		{
			const auto &tile = context.placements[i];
			// Unobserved SUPPORT remains absent. An authored classified solid face
			// may nevertheless participate in structural seams on that plane;
			// it acquires a datum only from an independently solved graph contact.
			// Requiring a fixed four-band ROM template here truncated taller authored
			// stacks and overlays on otherwise legal unobserved structural planes.
			if (!activePlanes[p] && !(context.placementBackgroundIndex==1 && tile.surface==RemasterDungeonFloorContext::Surface::Wall &&
				tile.hasHeight && (context.collisionAttributes[p*4096+i]==1 || context.collisionAttributes[p*4096+i]==4 ||
				context.collisionAttributes[p*4096+i]==2 || context.collisionAttributes[p*4096+i]==0x28 ||
				context.collisionAttributes[p*4096+i]==0x2a))) continue;
			if(tile.removableSupportPlanes&(1<<p))continue; // terminal persistent consumer, never structural graph node
			if(tile.obstacleAssembly || tile.authoredObstacle)continue; // terminal consumers, never seam bridges
			if(tile.wallTemplateOffset>=0xa14 && tile.wallTemplateOffset<=0xa44 &&
				(tile.wallTemplateOffset-0xa14)%0x10==0)continue; // terminal backing-wall fixture, never support/ramp graph bridge
			// A proven fold is terminal region geometry, solved from BOTH face
			// contacts below. It cannot supply a floor graph edge/datum through
			// an outward obstacle support node. Ordinary authored corners retain
			// the existing graph and its legitimate conflicting constraints.
			if(tile.terminalCorner)continue;
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
		// A clipped region of an ordered HIGH corner is a face plus
		// a ceiling plateau, not a shortened affine wall. The plateau cannot
		// supply an exterior floor equality. Prove the entire canonical section
		// and an intact downhill continuation before excluding that contact.
		auto clippedCornerCap = [&] (int i, int dx, int dy) {
			const auto &t=context.placements[i];
			const auto highTemplate=[](uint16_t off){return (off>=0xb66 && off<=0xbc6 && (off-0xb66)%0x20==0) ||
				(off>=0xc66 && off<=0xcc6 && (off-0xc66)%0x20==0);};
			if(!highTemplate(t.wallTemplateOffset) || !t.hasHeight)return false;
			const int lo=*std::min_element(t.height.begin(),t.height.end()),hi=*std::max_element(t.height.begin(),t.height.end());
			if(hi<=lo || hi>=lo+7)return false;
			for(int axis=0;axis<2;axis++)for(int sign:{-1,1}) {
				const int ux=axis?0:sign,uy=axis?sign:0;
				if(dx!=ux || dy!=uy)continue;
				bool exact=false;
				for(int tangentSign=-1;tangentSign<=1;tangentSign++) {
					bool match=true;
					for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
						const int k=axis?y:x,q=axis?x:y;
						const int normal=lo+(sign>0?k:7-k),tangent=lo+(tangentSign>0?q:7-q);
						match &= height(i,x,y)==std::min(hi,tangentSign?std::max(normal,tangent):normal);
					}exact |= match;
				}
				const int x=i%64-ux,y=i/64-uy;
				if(!exact || x<0 || x>=64 || y<0 || y>=64)continue;
				const int j=y*64+x;const auto &n=context.placements[j];
				if(!highTemplate(n.wallTemplateOffset) || !n.hasHeight ||
					n.surface!=RemasterDungeonFloorContext::Surface::Wall)continue;
				// Adjacent complementary phases of one corner facing are allowed;
				// another facing/height family is not ownership of this region.
				if((t.wallTemplateOffset-0xb66)/0x40!=(n.wallTemplateOffset-0xb66)/0x40)continue;
				// The surviving face region must meet its ordered downhill member.
				// A folded neighbor can turn the face; compare only non-cap samples
				// and retain every other seam/conflict in the ordinary graph.
				int samples=0,delta=INT_MIN;bool seam=true;
				for(int k=0;k<8;k++) {
					const int ax=ux?(ux>0?0:7):k,ay=uy?(uy>0?0:7):k;
					if(height(i,ax,ay)==hi)continue;
					const int value=height(i,ax,ay)-height(j,ux?7-ax:ax,uy?7-ay:ay);
					if(delta!=INT_MIN && delta!=value)seam=false;delta=value;samples++;
				}
				bool capEdge=true;
				for(int k=0;k<8;k++)capEdge &= height(i,dx?(dx>0?7:0):k,dy?(dy>0?7:0):k)==hi;
				if(capEdge && exact && seam && samples>=2)return true;
			}
			return false;
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
			const auto &t=context.placements[i];
			if(t.diagonalHeightPreview) {
				const int xx=t.hflip?7-(x+dx):x+dx,yy=t.vflip?7-(y+dy):y+dy;
				const int extended=std::max(int(t.diagonalMinimum),std::min(int(t.diagonalMaximum),int(t.diagonalIntercept)-xx-yy));
				return std::abs(extended-h);
			}
			const auto &m=(*cornerModels)[i];
			if(m.exact){const int a=m.ax+m.sx*(x+dx),b=m.ay+m.sy*(y+dy);return std::abs((m.maximum?std::max(a,b):std::min(a,b))-h);}
			return std::abs(h-inner);
		};
		auto diagonal = [&] (int i) {
			// A diagonal may terminate in a flat cap or floor half-plane. Require
			// an exact clipped affine profile, not averaged normals or a corner.
			int lo = INT16_MAX, hi = INT16_MIN;
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
		auto compositeDiagonalAssembly=std::make_unique<std::array<bool,4096>>();
		std::vector<int> compositeQueue;
		for(int i=0;i<4096;i++)if(context.placements[i].diagonalHeightPreview &&
			context.placements[i].diagonalMaximum-context.placements[i].diagonalMinimum>24) {
			(*compositeDiagonalAssembly)[i]=true;compositeQueue.push_back(i);
		}
		for(size_t n=0;n<compositeQueue.size();n++)for(const auto &d:{std::array<int,2>{1,0},{-1,0},{0,1},{0,-1}}) {
			const int i=compositeQueue[n],x=i%64+d[0],y=i/64+d[1];if(x<0 || x>=64 || y<0 || y>=64)continue;
			const int j=y*64+x;const auto &a=context.placements[i],&b=context.placements[j];
			if((*compositeDiagonalAssembly)[j] || !b.diagonalHeightPreview || a.hflip!=b.hflip || a.vflip!=b.vflip)continue;
			(*compositeDiagonalAssembly)[j]=true;compositeQueue.push_back(j);
		}
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
		for(int encoded=0;encoded<8192;encoded++) {
			const int n=(*occupiedComponents)[encoded];
			if(n>=0)result.occupiedFloorOwners[encoded/4096][encoded%4096]=diagnosticSupportId[n];
		}
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
		// Nonoccupied floor artwork can extend a semantic floor assembly only
		// when its independently related support contacts do not disagree.
		// In particular the background pattern outside a doorway cannot bridge
		// two legal movement components at different elevations.
		auto ambiguousFlat=std::make_unique<std::array<bool,8192>>();
		std::vector<std::set<int>> flatOwners;std::vector<std::vector<int>> flatMembers;
		std::vector<Edge> deferredFlatEdges;
		for(int p=0;p<2;p++) {
			std::array<bool,4096> seen={};
			for(int start=0;start<4096;start++) {
				if(seen[start] || components[p*4096+start]>=0 || context.placements[start].surface!=RemasterDungeonFloorContext::Surface::Floor)continue;
				std::vector<int> queue(1,start);seen[start]=true;std::set<int> owners;
				for(size_t k=0;k<queue.size();k++)for(auto d:{std::array<int,2>{1,0},{-1,0},{0,1},{0,-1}}) {
					const int x=queue[k]%64+d[0],y=queue[k]/64+d[1];if(x<0 || x>=64 || y<0 || y>=64)continue;
					const int j=y*64+x;if(context.placements[j].surface!=RemasterDungeonFloorContext::Surface::Floor)continue;
					const int n=components[p*4096+j];if(n>=0)owners.insert(n);
					else if(!seen[j]){seen[j]=true;queue.push_back(j);}
				}
				if(owners.size()>1){for(int i:queue)(*ambiguousFlat)[p*4096+i]=true;
					flatOwners.push_back(owners);for(int &i:queue)i+=p*4096;flatMembers.push_back(std::move(queue));}
			}
		}
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
				// A placed affine diagonal owns its oblique seam with a cardinal
				// face. The cardinal face's tangent derivative is zero and cannot
				// independently extrapolate that same one-pixel diagonal boundary.
				if(other.diagonalHeightPreview && !tile.diagonalHeightPreview) {
					// Both composite and exact ROM four-band shells own their affine
					// boundary. A reciprocal cardinal constant-tangent vote would
					// contradict its one-pixel extrapolation by one unit.
					if((*compositeDiagonalAssembly)[j] || other.wallTemplateOffset)continue;
				}
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
					if(other.surface==RemasterDungeonFloorContext::Surface::Floor && clippedCornerCap(i,dx,dy))continue;
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
				// Defer multi-component floor-art seams until independent wall
				// relations have been checked; they cannot short-circuit a rise.
				else if(tile.surface==RemasterDungeonFloorContext::Surface::Floor &&
					other.surface==RemasterDungeonFloorContext::Surface::Floor &&
					((*ambiguousFlat)[p*4096+i] || (*ambiguousFlat)[p*4096+j])) {
					deferredFlatEdges.push_back({a,b,height(i,dx>0?7:0,dy>0?7:0)-height(j,dx>0?0:7,dy>0?0:7)});continue;
				}
				else if (!((tile.surface == RemasterDungeonFloorContext::Surface::Floor &&
					(other.surface == RemasterDungeonFloorContext::Surface::Floor || other.surface == RemasterDungeonFloorContext::Surface::Obstacle)) ||
					(tile.surface == RemasterDungeonFloorContext::Surface::Obstacle && other.surface == RemasterDungeonFloorContext::Surface::Floor))) continue;
				// Reproduced support beneath an obstacle and its neighbor already
				// belongs to one movement component. Their visible local relief is
				// NOT a signed support seam (e.g. a nonblocking ceiling behind a
				// doorway obstacle). Keep relief on the placed artwork; do not turn
				// a60 ceiling sample into a60 support self-edge and poison the room.
				if (!wall && !otherWall &&
					(tile.surface==RemasterDungeonFloorContext::Surface::Obstacle || other.surface==RemasterDungeonFloorContext::Surface::Obstacle) &&
					components[p*4096+i]>=0 && components[p*4096+i]==components[p*4096+j]) continue;
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
		for(size_t f=0;f<flatOwners.size();f++) {
			const int start=*flatOwners[f].begin();std::vector<int> values(count,INT_MIN),queue(1,start);values[start]=0;bool conflict=false;
			for(size_t k=0;k<queue.size();k++)for(auto e:adjacency[queue[k]]) {
				const int v=values[queue[k]]+e.second;
				if(values[e.first]==INT_MIN){values[e.first]=v;queue.push_back(e.first);}else if(values[e.first]!=v)conflict=true;
			}
			bool unequal=conflict;
			for(int n:flatOwners[f])if(values[n]!=INT_MIN && values[n]!=0)unequal=true;
			if(unequal)continue;
			std::set<int> members;for(int encoded:flatMembers[f])if(nodes[encoded]>=0)members.insert(nodes[encoded]);
			for(const auto &e:deferredFlatEdges)if(members.count(e.a) || members.count(e.b))add(e.a,e.b,e.rise);
		}
		// The complete ROM intra-room stair owns its two outer rail profiles.
		// Unlike tread relief, these uniform cross sections meet both landings.
		// Reproduce the final template (including placement flips), then measure
		// its signed authored rise. No hash identity or configured-rise scaling.
#ifndef ALTTP_TEST_NO_AUTHORED_STAIR_RELATIONS
		if(context.dungeonTemplateData && context.constructedMaps)for(int si=0;si<context.stairCount;si++) {
			const auto &s=context.stairs[si];if(s.x>60 || !s.y || s.y>59)continue;
			RemasterDungeonStairProof proof;if(!S9xRemasterDungeonStairProof(context,s,proof))continue;
			const int rise=proof.rise;
			const int high=(s.highIsNorth?s.y-1:s.y+4)*64+s.x+1;
			const int low=(s.highIsNorth?s.y+4:s.y-1)*64+s.x+1;
			for(int p=0;p<2;p++) {
				const int hp=s.changesPlane?0:p,lp=s.changesPlane?1:p;
				const int a=components[lp*4096+low],b=components[hp*4096+high];
				bool landings=a>=0 && b>=0 && a!=b;
				for(int end:{high,low})for(int dx:{0,1}) {
					const auto &t=context.placements[end+dx];
					const auto *source=&t;
					if(proof.producer && context.peerProducerPlacements && (!t.hasHeight || t.stairSourceAmbiguous ||
						!std::all_of(t.height.begin(),t.height.end(),[](int16_t h){return h==0;})))source=&context.peerProducerPlacements[end+dx];
					if(!source->hasHeight || source->stairSourceAmbiguous || !std::all_of(source->height.begin(),source->height.end(),[](int16_t h){return h==0;}))landings=false;
				}
				if(landings && std::any_of(constraints.begin(),constraints.end(),[&](const Constraint &e){return e.low==a && e.high==b;})) {
					// A fully connected authored wall path already owns this relation.
					// Rail samples replace only the configured stair fallback, not a
					// proved high/low wall seam with potentially different local relief.
					std::vector<bool> seen(count,false);std::vector<int> queue(1,a);seen[a]=true;
					for(size_t k=0;k<queue.size();k++)for(auto e:adjacency[queue[k]])if(!seen[e.first]){seen[e.first]=true;queue.push_back(e.first);}
					if(!seen[b])add(a,b,rise);
					// The complete rail/landing proof is independent of unrelated
					// wall cycles. Preserve it for cross-BG transport even when the
					// larger structural graph cannot export a consistent path.
					verifiedStairRelations.push_back({lp*4096+low,hp*4096+high,rise});
				}
				if(s.changesPlane)break;
			}
		}

#endif
		// One final ROM-owned visible BG2 face is one physical surface when BOTH
		// planes use it as solid structure. BG1 may contain different lower art:
		// that does not duplicate the visible BG2 shell. BG1's evidence transport
		// still requires paired identical artwork. A nonblocking underpass never
		// licenses equality. Share structural nodes ONLY, never support components.
		auto solidFace=[](uint8_t a){return a==1 || a==2 || a==0x28 || a==0x2a;};
		for(int i=0;(context.placementBackgroundIndex==0 || activePlanes[0]!=activePlanes[1]) && i<4096;i++) {
			const auto &t=context.placements[i];
			if(t.surface!=RemasterDungeonFloorContext::Surface::Wall || !t.wallTemplateOffset ||
				(context.bg1GraphicsBase!=0xffff && context.bg1GraphicsBase!=context.graphicsBase) ||
				(context.placementBackgroundIndex==0 && t.distinctLowerUsage) || !solidFace(context.collisionAttributes[i]) ||
				!solidFace(context.collisionAttributes[4096+i]) || nodes[i]<0 || nodes[4096+i]<0)continue;
			add(nodes[i],nodes[4096+i],0);result.sharedStructuralFaces++;
		}
		if(context.supportRelations)for(const auto &r:*context.supportRelations) {
			if(r.a<0 || r.a>=8192 || r.b<0 || r.b>=8192)continue;
			const int a=components[r.a],b=components[r.b];
			if(a>=0 && b>=0 && r.rise>0 && std::any_of(constraints.begin(),constraints.end(),
				[&](const Constraint &e){return e.low==a && e.high==b;}))add(a,b,r.rise);
		}
		if(context.placementBackgroundIndex==1 && context.wallSupportRelations && context.constructedMaps &&
			(context.bg1GraphicsBase==0xffff || context.bg1GraphicsBase==context.graphicsBase))
			for(const auto &r:*context.wallSupportRelations) {
				if(r.wall<0 || r.wall>=8192 || r.support<0 || r.support>=8192)continue;
				const int i=r.wall%4096,p=0;
				const auto &t=context.placements[i];
				// Complementary high perimeter / low face seam only. Other BG wall
				// paths do not establish ownership of arbitrary visible structures.
				bool perimeter=false;
				for(int dx:{-1,1})if(i%64+dx>=0 && i%64+dx<64) {
					const auto &face=context.placements[i+dx];
					if(face.surface!=RemasterDungeonFloorContext::Surface::Wall || !face.highPerimeterFace ||
						!face.hasHeight)continue;
					bool seam=true;int delta=INT_MIN;
					for(int k=0;k<8;k++) {
						const int edge=height(i,dx>0?7:0,k),otherEdge=height(i+dx,dx>0?0:7,k);
						if(edge<=height(i,dx>0?0:7,k) || otherEdge>=height(i+dx,dx>0?7:0,k))seam=false;
						const int v=edge-otherEdge;if(delta!=INT_MIN && delta!=v)seam=false;delta=v;
					}
					perimeter|=seam;
				}
				if(!perimeter || r.wall/4096!=1)continue;
				const int word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
				const int other=context.constructedMaps[8192+i*2]|context.constructedMaps[8193+i*2]<<8;
				if(t.surface!=RemasterDungeonFloorContext::Surface::Wall || !t.hasHeight ||
					t.height!=r.height ||
					(word&0xdfff)!=(r.word&0xdfff) || (other&0xdfff)!=(r.word&0xdfff) ||
					!solidFace(context.collisionAttributes[p*4096+i]) || nodes[i]<0 || components[r.support]<0)continue;
				add(components[r.support],nodes[i],r.rise);
				ownedHighFaceSeeds.push_back(nodes[i]);
			}
		// Export a signed structural path only from a consistent authored graph
		// with established support. No configured stair relation or datum enters it.
		if(context.constructedMaps) {
			uint64_t id=UINT64_C(14695981039346656037);
			for(int k=0;k<16384;k++)id=(id^context.constructedMaps[k])*UINT64_C(1099511628211);
			result.shellMapIdentity=id;
		}
		if(context.constructedMaps) {
			std::vector<int> value(count,INT_MIN);
			for(size_t start=0;start<cells.size();start++) {
				if(cells[start].empty() || value[start]!=INT_MIN)continue;
				std::vector<int> parent(count,-1);
				std::vector<int> queue(1,int(start));value[start]=0;bool conflict=false;
				for(size_t k=0;k<queue.size();k++)for(auto next:adjacency[queue[k]]) {
					const int v=value[queue[k]]+next.second;
					if(value[next.first]==INT_MIN){value[next.first]=v;parent[next.first]=queue[k];queue.push_back(next.first);}
					else if(value[next.first]!=v)conflict=true;
				}
				if(conflict)continue;
				for(int encoded=0;encoded<8192;encoded++) {
					const int i=encoded%4096,n=nodes[encoded];const auto &t=context.placements[i];
					const bool low=t.wallTemplateOffset>=0xbe6 && t.wallTemplateOffset<=0xc46 && (t.wallTemplateOffset-0xbe6)%0x20==0;
					const int support=cells[start][0];const auto &floor=context.placements[support%4096];
					const bool terminal=low && support/4096==encoded/4096 && (*occupiedComponents)[support]>=0 &&
						context.collisionAttributes[support]==0 && floor.surface==RemasterDungeonFloorContext::Surface::Floor && floor.hasHeight &&
						std::all_of(floor.height.begin(),floor.height.end(),[](int16_t h){return h==0;});
					if(n<0 || std::find(queue.begin(),queue.end(),n)==queue.end() ||
						t.surface!=RemasterDungeonFloorContext::Surface::Wall ||
						(!terminal && (t.distinctLowerUsage || !solidFace(context.collisionAttributes[encoded]))))continue;
					const int word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
					result.authoredWallSupportRelations.push_back({encoded,cells[start][0],value[n],uint16_t(word),t.height});
					auto &r=result.authoredWallSupportRelations.back();r.terminalLowPath=terminal;r.source=t.wallTemplateOffset;r.hflip=t.hflip;r.vflip=t.vflip;
					for(int v=n;v>=0;v=parent[v])r.path.push_back(v);
				}
			}
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
		for(const auto &r:verifiedStairRelations) {
			const int a=components[r.a],b=components[r.b];
			if(a<0 || b<0)continue;
			if(std::none_of(result.authoredSupportRelations.begin(),result.authoredSupportRelations.end(),[&](const RemasterDungeonFloorContext::SupportRelation &e){return components[e.a]==a && components[e.b]==b;}))
				result.authoredSupportRelations.push_back(r);
		}
		// Join the visible shells BEFORE choosing any relative datum. These
		// constraints end at actual support components, never at copied visible
		// bases. Wall nodes remain separate even when bodies occupy their cells.
		// Membership is transitive: a non-occupied wall cell can still belong to
		// a physical graph via its authored foot. The legacy post-datum consumer
		// must respect the same boundary as the pre-datum signed-path consumer.
		std::vector<bool> physicalGraph(count,false);
		std::vector<int> physicalQueue;
		for(size_t n=0;n<cells.size();n++)if(!cells[n].empty()){physicalGraph[n]=true;physicalQueue.push_back(int(n));}
		for(size_t k=0;k<physicalQueue.size();k++)for(auto e:adjacency[physicalQueue[k]])
			if(!physicalGraph[e.first]){physicalGraph[e.first]=true;physicalQueue.push_back(e.first);}
		uint64_t peerIdentity=UINT64_C(14695981039346656037);
		if(context.constructedMaps)for(int k=0;k<16384;k++)peerIdentity=(peerIdentity^context.constructedMaps[(k+8192)%16384])*UINT64_C(1099511628211);
		if(context.peerShellPlacements && context.peerShellMap && context.constructedMaps &&
			context.peerShellMap->shellMapIdentity==peerIdentity)
		for(const auto &r:context.peerShellMap->authoredWallSupportRelations) {
			if(!r.terminalLowPath || r.wall<0 || r.wall>=8192 || r.support<0 || r.support>=8192 || r.support/4096!=r.wall/4096 || r.path.empty())continue;
			const int p=r.wall/4096,j=r.wall%4096;const auto &b=context.peerShellPlacements[j];
			const int peerWord=context.constructedMaps[8192+j*2]|context.constructedMaps[8193+j*2]<<8;
			const auto &peerFloor=context.peerShellPlacements[r.support%4096];
			const int supportBase=context.peerShellMap->planeOffsets[p][r.support%4096];
			if(peerWord!=r.word || b.height!=r.height || b.wallTemplateOffset!=r.source || b.hflip!=r.hflip || b.vflip!=r.vflip ||
				supportBase==255 || peerFloor.surface!=RemasterDungeonFloorContext::Surface::Floor || !peerFloor.hasHeight ||
				!std::all_of(peerFloor.height.begin(),peerFloor.height.end(),[](int16_t h){return h==0;}))continue;
			for(auto d:{std::array<int,2>{1,0},{-1,0},{0,1},{0,-1}}) {
				const int dx=d[0],dy=d[1],x=j%64-dx,y=j/64-dy;if(x<0||x>=64||y<0||y>=64)continue;
				const int i=y*64+x,n=nodes[p*4096+i];const auto &a=context.placements[i];
				if(n<0 || !a.hasHeight || a.surface!=RemasterDungeonFloorContext::Surface::Wall ||
					!(a.highCornerFace || a.highPerimeterFace) || a.wallTemplateOffset+0x80!=b.wallTemplateOffset)continue;
				auto sample=[](const auto&t,int x,int y){return t.height[(t.vflip?7-y:y)*8+(t.hflip?7-x:x)];};
				int delta=INT_MIN;bool exact=true;
				for(int k=0;k<8;k++) {
					const int ax=dx?(dx>0?7:0):k,ay=dy?(dy>0?7:0):k,bx=dx?(dx>0?0:7):k,by=dy?(dy>0?0:7):k;
					const int ga=sample(a,ax,ay)-sample(a,ax-dx,ay-dy),gb=sample(b,bx+dx,by+dy)-sample(b,bx,by);
					const int v=r.rise+sample(b,bx,by)-ga-sample(a,ax,ay);
					if(std::abs(ga)!=1 || ga!=gb || (delta!=INT_MIN && delta!=v))exact=false;delta=v;
				}
				if(exact){
					// A visible shell contact must not replace an existing physical
					// floor graph. Until its cross-BG support identity is independently
					// established, keep this boundary explicit, rather than poisoning
					// every reachable floor with an imported artwork equality.
					if(physicalGraph[n]){physicalShellBoundaries.insert({p*4096+i,j});continue;}
					// External occupied-support witness, NOT the same-coordinate BG2
					// component (which can be a covered floor/ceiling on another datum).
					const int anchor=count++;adjacency.resize(count);shellSupportAnchors[anchor]=supportBase;
					add(anchor,n,delta);result.visibleShellContacts++;signedShellEdges.insert({p*4096+i,j});
				}
			}
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
			context.entrySupport.destinationState==S9xRemasterDungeonEntryStateKey(inputContext,floorRise);
		for (int start = 0; start < count; start++)
		{
			if (bases[start] != INT_MIN || (!supportNode[start] && !flatNode[start] && !shellSupportAnchors.count(start))) continue;
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
			for(int node:queue)if(shellSupportAnchors.count(node)){minimum=std::min(minimum,bases[node]);supports++;}
			const bool isolatedFlat = supports == 0;
			if (isolatedFlat) for (int node : queue) if (flatNode[node]) minimum = std::min(minimum, bases[node]);
			const auto &entry=context.entrySupport;
			const int entryNode=entry.cell>=0 && entry.cell<8192 ? components[entry.cell] : -1;
			const bool anchored=entryStateMatches && entryNode>=0 && entry.predecessorRoom==context.previousRoomIndex &&
				entry.predecessorRoom!=0xffff && entry.predecessorRoom!=context.roomIndex &&
				entry.predecessorState && entry.transitionEvidence &&
				std::find(queue.begin(),queue.end(),entryNode)!=queue.end();
			int shellDatum=INT_MIN;
			for(int node:queue)if(shellSupportAnchors.count(node)) {
				const int vote=bases[node]-shellSupportAnchors.at(node);
				if(shellDatum!=INT_MIN && shellDatum!=vote)conflict=true;shellDatum=vote;
			}
			if(!anchored && shellDatum==INT_MIN)result.relativeAnchors.push_back({start, minimum, supports, isolatedFlat});
			if(anchored && shellDatum!=INT_MIN && shellDatum!=bases[entryNode]-entry.base)conflict=true;
			const int datum=shellDatum!=INT_MIN?shellDatum:anchored ? bases[entryNode]-entry.base : minimum;
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
#ifdef ALTTP_TEST_WALL_COMPONENT_AUDIT
		// Standalone evidence explorer only; never enabled in app/runtime builds.
		if(const char *path=std::getenv("ALTTP_WALL_COMPONENT_AUDIT")) {
			std::ofstream audit(path,std::ios::app);audit<<"{\"bg\":"<<int(context.placementBackgroundIndex)
				<<",\"active\":["<<activePlanes[0]<<','<<activePlanes[1]<<"],\"nodes\":[";
			bool first=true;for(int encoded=0;encoded<8192;encoded++)if(nodes[encoded]>=0) {
				if(!first)audit<<',';first=false;const int n=nodes[encoded];
				const auto &t=context.placements[encoded%4096];
				audit<<'['<<encoded<<','<<n<<','<<bases[n]<<','<<bad[n]<<','<<supportNode[n]<<','<<flatNode[n]
					<<','<<t.wallTemplateOffset<<','<<t.hflip<<','<<t.vflip<<','<<int(context.collisionAttributes[encoded])
					<<','<<components[encoded]<<','<<(*occupiedComponents)[encoded]<<','<<t.tileHash
					<<','<<(context.constructedMaps?(context.constructedMaps[(encoded%4096)*2]|context.constructedMaps[(encoded%4096)*2+1]<<8):0)<<']';
			}audit<<"],\"edges\":[";first=true;for(int n=0;n<count;n++)for(auto e:adjacency[n])if(n<e.first) {
				if(!first)audit<<',';first=false;audit<<'['<<n<<','<<e.first<<','<<e.second<<']';
			}audit<<"]}\n";
		}
#endif
		for(int i=0;i<4096;i++)if(context.placements[i].diagonalHeightPreview && !context.placements[i].diagonalAuthoredPreserved && !result.producerObstacle[i])
			result.localHeightPreviews.push_back({i,context.placements[i].height});
		for(int encoded=0;encoded<8192;encoded++) {
			auto &d=diagnosticStorage->cells[encoded];
			const int n=nodes[encoded];
			if(n<0)continue;
			d.heightConflict=bad[n];
			// A solved authored zero-local floor remains a support surface when
			// its collision byte is the inter-room stair marker22. Such cells
			// deliberately have no ordinary traversal component. Other special
			// handlers are unsupported; do not generalize to pits/ledges/blockers.
			// Publish the proven datum, NOT a fabricated movement/component owner.
			const auto &placed=context.placements[encoded%4096];
			if(!d.wall && context.collisionAttributes[encoded]==0x22 &&
				placed.surface==RemasterDungeonFloorContext::Surface::Floor && placed.hasHeight &&
				(placed.structuralFloorPlaneMask&(1<<(encoded/4096))) &&
				!placed.obstacleAssembly && !placed.authoredObstacle && !placed.removableSupportPlanes &&
				std::all_of(placed.height.begin(),placed.height.end(),[](int16_t h){return h==0;})) {
				d.semanticFloorSupport=true;
				if(!bad[n] && bases[n]!=INT_MIN)d.floorBase=bases[n];
			}
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
		// Complementary visible shells may meet across BGs without sharing a
		// floor component. Admit only exact ROM phase pairs and a full authored
		// unit-gradient seam. Keep physical supports and contradictory datums intact.
		if(context.peerShellPlacements && context.peerShellMap && context.peerShellMap->hasPlaneOffsets) {
			auto complementary=[](int a,int b) {
				for(auto pair:{std::array<int,2>{0x2e8,0x328},{0x2f8,0x338},{0x2c8,0x308},{0x2d8,0x318},
					{0xb66,0xbe6},{0xb86,0xc06},{0xba6,0xc26},{0xbc6,0xc46}})
					if((a==pair[0]&&b==pair[1])||(a==pair[1]&&b==pair[0]))return true;
				return false;
			};
			for(int p=0;p<2;p++)for(int i=0;i<4096;i++) {
				const auto &a=context.placements[i];
				if(a.surface!=RemasterDungeonFloorContext::Surface::Wall || !a.hasHeight)continue;
				int proposed=INT_MIN;bool conflict=false;unsigned contacts=0;
				for(int direction=0;direction<4;direction++) {
					const int dx=direction==0?-1:direction==1?1:0,dy=direction==2?-1:direction==3?1:0;
					const int x=i%64+dx,y=i/64+dy;if(x<0||x>=64||y<0||y>=64)continue;
					const int j=y*64+x;const auto &b=context.peerShellPlacements[j];
					if(b.surface!=RemasterDungeonFloorContext::Surface::Wall || !b.hasHeight ||
						!complementary(a.wallTemplateOffset,b.wallTemplateOffset))continue;
					const int base=context.peerShellMap->planeOffsets[p][j];
					RemasterDungeonHeightMap::VisibleShellCandidate record;
					record.bg=context.placementBackgroundIndex;record.plane=p;record.cell=i;record.peer=j;record.dx=dx;record.dy=dy;
					record.source=a.wallTemplateOffset;record.peerSource=b.wallTemplateOffset;record.peerBase=base;
					record.occupied=components[p*4096+i]>=0;
					const int receivingNode=nodes[p*4096+i];
					const bool physical=receivingNode>=0 && receivingNode<int(physicalGraph.size()) && physicalGraph[receivingNode];
					const bool signedPath=signedShellEdges.count({p*4096+i,j})!=0;
					for(const auto &r:context.peerShellMap->authoredWallSupportRelations)
						if(!r.terminalLowPath && r.wall==p*4096+j && r.support>=p*4096 && r.support<(p+1)*4096 && r.height==b.height) {
							record.relation=true;record.support=r.support;
						}
					auto sample=[](const auto &t,int x,int y){return t.height[(t.vflip?7-y:y)*8+(t.hflip?7-x:x)];};
					int delta=INT_MIN;bool seam=true;
					for(int k=0;k<8;k++) {
						const int ax=dx? (dx>0?7:0):k,ay=dy?(dy>0?7:0):k;
						const int bx=dx?(dx>0?0:7):k,by=dy?(dy>0?0:7):k;
						const int ga=sample(a,ax,ay)-sample(a,ax-dx,ay-dy);
						const int gb=sample(b,bx+dx,by+dy)-sample(b,bx,by);
						record.edge[k]=sample(a,ax,ay);record.peerEdge[k]=sample(b,bx,by);record.slope[k]=ga;record.peerSlope[k]=gb;
						if(std::abs(ga)!=1 || ga!=gb){seam=false;record.gradient=false;}
						const int d=base+sample(b,bx,by)-ga-sample(a,ax,ay);
						if(delta!=INT_MIN&&delta!=d){seam=false;record.uniform=false;}delta=d;
					}
					record.proposed=delta;
					record.reason=!record.gradient?"gradient_mismatch":!record.uniform?"nonuniform_seam":physical?"independent_physical_graph":
						signedPath?"signed_graph_constraint":record.occupied?"occupied_artwork":base==255?"peer_unsolved":!record.relation?"missing_support_path":"accepted";
					result.visibleShellCandidates.push_back(record);
					// Signed paths were integrated before normalization. The peer's
					// separately normalized base is NOT a second absolute anchor.
					if(signedPath)continue;
					if(physical || record.occupied || base==255)continue;
					if(!record.relation){result.visibleShellRejected++;continue;}
					if(!seam)continue;
					if(proposed!=INT_MIN&&proposed!=delta)conflict=true;proposed=delta;contacts++;
				}
				if(!contacts)continue;
				result.visibleShellContacts+=contacts;
				// This evidence is a constraint, not permission to silently pick a
				// different floor datum. A disagreement remains explicit wall-only
				// unresolved until the joint support graph can satisfy both contacts.
				const int node=nodes[p*4096+i];
				if(conflict || (node>=0 && bases[node]!=INT_MIN && !bad[node] && bases[node]!=proposed)) {
					result.visibleShellRejected++;
					diagnosticStorage->cells[p*4096+i].heightConflict=true;
					if(node>=0) {
						bad[node]=true;
						if(result.geometryConflicts.size()<64)result.geometryConflicts.push_back({node,node,proposed,bases[node]});
					}
				} else if(node>=0 && !bad[node] && bases[node]==INT_MIN) {
					// Only an otherwise unsupported STRUCTURAL node can acquire this
					// signed datum; no physical floor component is reassigned.
					bases[node]=proposed;
				}
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
		result.roomDiagnostics=diagnosticStorage;
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
		// Floor relief is local geometry, not a reason to require a complete Link
		// body on every placement. Extend a solved floor BASE through cardinally
		// connected, explicitly classified floor art on this BG and gameplay plane.
		// Nonflat authored floors deliberately remain outside the plateau/wall seam
		// graph above: their bumps must not become rises or new relative datums.
		// This is terminal geometry coverage, NOT movement/component expansion.
#ifndef ALTTP_TEST_NO_FLOOR_CONTINUITY
		for(int p=0;p<2;p++)if(activePlanes[p]) {
			auto eligible=[&](int i) {
				const auto &t=context.placements[i];
				return t.hasHeight && (t.surface==RemasterDungeonFloorContext::Surface::Floor ||
					(t.surface==RemasterDungeonFloorContext::Surface::Unknown && t.diagnosticAuthoredFloor)) &&
					(t.structuralFloorPlaneMask&(1<<p)) && context.collisionAttributes[p*4096+i]==0 &&
					!t.obstacleAssembly && !t.authoredObstacle && !t.removableSupportPlanes &&
					!(context.placementBackgroundIndex==0 && p==0 && activePlanes[1] && t.distinctLowerUsage);
			};
			std::array<bool,4096> seen={};
			for(int start=0;start<4096;start++)if(!seen[start] && eligible(start)) {
				std::vector<int> queue(1,start);seen[start]=true;
				int base=255,owner=-1;bool conflict=false,occupied=false;
				for(size_t k=0;k<queue.size();k++) {
					const int i=queue[k],n=nodes[p*4096+i],value=result.planeOffsets[p][i];
					if(n>=0 && bad[n])conflict=true;
					if(value!=255) {if(base!=255 && base!=value)conflict=true;base=value;}
					if(components[p*4096+i]>=0 && value!=255) {
						occupied=true;const int id=diagnosticSupportId[components[p*4096+i]];
						if(owner==-1)owner=id;else if(owner!=id)owner=-2;
					}
					for(auto d:{std::array<int,2>{1,0},{-1,0},{0,1},{0,-1}}) {
						const int x=i%64+d[0],y=i/64+d[1];if(x<0 || x>=64 || y<0 || y>=64)continue;
						const int j=y*64+x;if(!seen[j] && eligible(j)){seen[j]=true;queue.push_back(j);}
					}
				}
				if(conflict || !occupied || base==255)continue;
				for(int i:queue)if(result.planeOffsets[p][i]==255) {
					result.planeOffsets[p][i]=base;
					if(result.offsets[i]==255 || base>result.offsets[i])result.offsets[i]=base;
					auto &d=diagnosticStorage->cells[p*4096+i];d.floorBase=base;
					if(owner>=0)d.floorId=owner;
				}
			}
		}
#endif
		// A terminal movement claim supports actors only. Preserve the independent
		// BG graph, offsets, diagnostics and structural ownership verbatim.
		for(int p=0;p<2;p++)if(activePlanes[p])for(int i=0;i<4096;i++) {
			const int n=(*thresholdOwners)[p*4096+i];
			result.actorThresholdOwners[p][i]=n;
			if(n>=0 && bases[n]!=INT_MIN && !bad[n])result.actorThresholdBases[p][i]=bases[n];
		}
		// Post-removal bodies are terminal support consumers, never new graph
		// nodes/edges/anchors. Account missing/conflicting ownership separately
		// without inflating actual reachable-support or structural-conflict counts.
		if(persistentClaims)for(int i=0;i<8192;i++)if((*persistentClaims)[i]!=-1) {
			const int p=i/4096,cell=i%4096,n=(*persistentClaims)[i];
			result.persistentSupportRequired[p]++;
			const int value=n<0 || bases[n]==INT_MIN || bad[n]?255:bases[n];
			result.persistentSupport.push_back({cell,p,n,value});
			if(value==255){result.persistentSupportMissing[p]++;continue;}
			result.planeOffsets[p][cell]=bases[n];
			if(context.placements[cell].surface!=RemasterDungeonFloorContext::Surface::Wall &&
				(result.offsets[cell]==255 || bases[n]>result.offsets[cell]))result.offsets[cell]=bases[n];
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
		// A proved visible-shell path owns its representative, not max(planes).
		// Keep both structural/support arrays intact; incompatible proven visible
		// representatives are unresolved rather than choosing a higher hidden cap.
		std::vector<bool> visibleShellNodes(count,false);
		for(const auto &anchor:shellSupportAnchors) {
			std::vector<int> queue(1,anchor.first);visibleShellNodes[anchor.first]=true;
			for(size_t k=0;k<queue.size();k++)for(auto e:adjacency[queue[k]])if(!visibleShellNodes[e.first]) {
				visibleShellNodes[e.first]=true;queue.push_back(e.first);
			}
		}
		for(int i=0;i<4096;i++)if(context.placements[i].surface==RemasterDungeonFloorContext::Surface::Wall) {
			int visible=255;bool conflict=false;
			for(int p=0;p<2;p++){const int n=nodes[p*4096+i];if(n>=0 && visibleShellNodes[n] && !bad[n] && bases[n]!=INT_MIN) {
				if(visible!=255 && visible!=bases[n])conflict=true;visible=bases[n];
			}}
			if(visible!=255)result.offsets[i]=conflict?255:visible;
		}
		// Nonblocking authored wall FEET can occupy the same legal body support
		// as their floor (beveled floor-contact art). Collision00 alone is not
		// permission to attach an occluder. Require occupied same-component Floor,
		// an exact zero-local contact edge, and monotone relief away from that
		// edge on every cross section. This is terminal local geometry: do not
		// turn its irregular outer edge into a guessed wall-stack rise or graph
		// relation. In a reproduced overlap, retain the independent underpass.
#ifndef ALTTP_TEST_NO_WALL_FLOOR_FEET
		for(int p=0;p<2;p++)if(activePlanes[p])for(int i=0;i<4096;i++) {
			const auto &t=context.placements[i];const int owner=(*occupiedComponents)[p*4096+i];
			if(t.surface!=RemasterDungeonFloorContext::Surface::Wall || !t.hasHeight ||
				context.collisionAttributes[p*4096+i]!=0 || owner<0 ||
				(nodes[p*4096+i]>=0 && bad[nodes[p*4096+i]]) ||
				t.obstacleAssembly || t.authoredObstacle || t.removableSupportPlanes ||
				(context.placementBackgroundIndex==0 && p==0 && activePlanes[1] && t.distinctLowerUsage) ||
				(activePlanes[1-p] && context.collisionAttributes[(1-p)*4096+i]==0 &&
					(context.placementBackgroundIndex!=1 || result.planeOffsets[p][i]==255 ||
					 result.planeOffsets[1-p][i]==255 || result.planeOffsets[p][i]<=result.planeOffsets[1-p][i])))continue;
			int candidate=INT_MIN;bool conflict=false;
			for(auto d:{std::array<int,2>{1,0},{-1,0},{0,1},{0,-1}}) {
				const int dx=d[0],dy=d[1],x=i%64+dx,y=i/64+dy;if(x<0 || x>=64 || y<0 || y>=64)continue;
				const int j=y*64+x;const auto &floor=context.placements[j];
				if(floor.surface!=RemasterDungeonFloorContext::Surface::Floor || !floor.hasHeight ||
					(*occupiedComponents)[p*4096+j]!=owner || result.planeOffsets[p][j]==255 ||
					context.collisionAttributes[p*4096+j]!=0 ||
					(nodes[p*4096+j]>=0 && bad[nodes[p*4096+j]]) ||
					!std::all_of(floor.height.begin(),floor.height.end(),[](int16_t h){return h==0;}))continue;
				bool exact=true,nonflat=false;
				for(int k=0;k<8;k++) {
					int previous=0;
					for(int n=0;n<8;n++) {
						const int h=height(i,dx?(dx>0?7-n:n):k,dy?(dy>0?7-n:n):k);
						if((n==0 && h!=0) || h<previous)exact=false;
						nonflat|=h!=0;previous=h;
					}
				}
				if(!exact || !nonflat)continue;
				const int base=result.planeOffsets[p][j];
				if(candidate!=INT_MIN && candidate!=base)conflict=true;candidate=base;
			}
			const int existing=result.structuralPlaneOffsets[p][i];
			if(candidate==INT_MIN)continue;
			if(existing!=255 && existing!=candidate)conflict=true;
			if(conflict){result.structuralConflicts++;continue;}
			result.structuralPlaneOffsets[p][i]=candidate;
			if(result.offsets[i]==255 || candidate>result.offsets[i])result.offsets[i]=candidate;
		}
#endif
		// A proved complementary high face is visible on its own structural
		// plane, not the independently anchored cap/underpass geometry on plane1.
		// Retain both arrays; select only ROM high faces connected by accepted seams.
		std::vector<bool> ownedHighNodes(count,false);
		for(int n:ownedHighFaceSeeds)if(!ownedHighNodes[n]) {
			std::vector<int> queue(1,n);ownedHighNodes[n]=true;
			for(size_t k=0;k<queue.size();k++)for(auto e:adjacency[queue[k]])
				if(!ownedHighNodes[e.first]){ownedHighNodes[e.first]=true;queue.push_back(e.first);}
		}
		for(int i=0;i<4096;i++) {
			const auto &t=context.placements[i];
			if((t.highPerimeterFace || std::find(ownedHighFaceSeeds.begin(),ownedHighFaceSeeds.end(),nodes[i])!=ownedHighFaceSeeds.end()) &&
				nodes[i]>=0 && ownedHighNodes[nodes[i]] && result.structuralPlaneOffsets[0][i]!=255)
				result.offsets[i]=result.structuralPlaneOffsets[0][i];
		}
		// Exact split-door cover is terminal ownership, not a passable support
		// or replacement wall ramp. Attach only its full authored edge to the
		// preserved, support-backed high face, never to neighboring floor color.
		for(int i=0;i<4096;i++)if(context.placements[i].lowerDoorWallPart) {
			if(i%64==63)continue;const int j=i+1;
			if(!context.placements[j].highPerimeterFace || result.structuralPlaneOffsets[0][j]==255)continue;
			int base=INT_MIN;bool compatible=true;
			for(int k=0;k<8;k++) {
				const int v=result.structuralPlaneOffsets[0][j]+height(j,0,k)-height(i,7,k);
				if(base!=INT_MIN && base!=v)compatible=false;base=v;
			}
			if(compatible && base!=255 && base>=-32767 && base<=32767) {
				result.offsets[i]=base;result.planeOffsets[0][i]=base;
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
								!std::all_of(t.height.begin(),t.height.end(),[] (int16_t h) { return h == 0; }))
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
					else if (status == 0 && datum - result.structuralPlaneOffsets[p][cell] <= INT16_MAX) { status=1; base=datum; }
					else if (status == 0) status=2; // signed local override cannot represent this region
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
		// A folded terminal has separate X/Y-owned faces. Its inward branches
		// are tangent seams, not permission to join outward obstacle SUPPORT
		// nodes through the shell. Require both complete transformed edges to
		// independently solve to the same offset on each structural plane.
		for(int i=0;i<4096;i++)if(context.placements[i].terminalCorner) {
			const auto &t=context.placements[i];result.offsets[i]=255;
			for(int p=0;p<2;p++) {
				if(!activePlanes[p] || !solidFace(context.collisionAttributes[p*4096+i]))continue;
				int candidate=INT_MIN;bool compatible=true,seamConflict=false;
				for(int axis=0;axis<2;axis++) {
					const int dx=axis?0:(t.hflip?-1:1),dy=axis?(t.vflip?-1:1):0;
					const int x=i%64+dx,y=i/64+dy;if(x<0 || x>=64 || y<0 || y>=64){compatible=false;continue;}
					const int j=y*64+x;const auto &other=context.placements[j];
					if(other.surface!=RemasterDungeonFloorContext::Surface::Wall || other.wallTemplateOffset!=t.wallTemplateOffset ||
						other.terminalCorner || !solidFace(context.collisionAttributes[p*4096+j])) {
						// Missing inward branch is not an unknown floor. An exact
						// complementary high face can constrain that region from its
						// opposite edge; preserve the existing boundary extension.
						const int hdx=axis?(t.hflip?-1:1):0,hdy=axis?0:(t.vflip?-1:1);
						const int hx=i%64-hdx,hy=i/64-hdy;
						if(hx<0 || hx>=64 || hy<0 || hy>=64){compatible=false;continue;}
						const int high=hy*64+hx;const auto &ht=context.placements[high];
						const uint8_t highAttr=context.collisionAttributes[p*4096+high];
						// Completed04 is an interaction usage, not a height label.
						// A separately proved high-perimeter template face can retain
						// that attribute while its structural graph already solved the
						// base. Still require every exact owned-face seam sample below;
						// plain/nonblocking underpasses and unowned04 never qualify.
						const bool structuralHigh=solidFace(highAttr) || (highAttr==4 && ht.wallTemplateOffset && (ht.highPerimeterFace || ht.highCornerFace));
						if(ht.surface!=RemasterDungeonFloorContext::Surface::Wall || !ht.hasHeight ||
							result.structuralPlaneOffsets[p][high]==255 || !structuralHigh){compatible=false;continue;}
						for(int k=0;k<8;k++) {
							const int cx=hdx?(hdx>0?0:7):k,cy=hdy?(hdy>0?0:7):k;
							const int nx=hdx?(hdx>0?7:0):k,ny=hdy?(hdy>0?7:0):k;
							const int value=result.structuralPlaneOffsets[p][high]+height(high,nx,ny)-boundaryStep(i,-hdx,-hdy,k)-height(i,cx,cy);
							if(candidate!=INT_MIN && candidate!=value){compatible=false;seamConflict=true;}candidate=value;
						}
						continue;
					}
					int branchBase=result.structuralPlaneOffsets[p][j],branchSupport=-1,branchRise=0;
					// Do not resurrect an explicitly contradictory structural node.
					if(nodes[p*4096+j]>=0 && bad[nodes[p*4096+j]]){compatible=false;continue;}
					// Missing visible branch anchors can be witnessed by the exact
					// original same-graphics BG assembly's SIGNED support relation.
					// It supplies neither a borrowed visible base nor a new datum.
					if(branchBase==255 && context.placementBackgroundIndex==1 && context.wallSupportRelations &&
						context.constructedMaps && (context.bg1GraphicsBase==0xffff || context.bg1GraphicsBase==context.graphicsBase)) {
						const int word=context.constructedMaps[j*2]|context.constructedMaps[j*2+1]<<8;
						const int counterpart=context.constructedMaps[8192+j*2]|context.constructedMaps[8193+j*2]<<8;
						int proposed=INT_MIN;bool consistent=true;
						for(const auto &r:*context.wallSupportRelations)if(r.wall/4096==1 && r.wall%4096==j && r.support>=0 && r.support<8192 &&
							r.height==other.height && r.word==word && counterpart==word && result.planeOffsets[r.support/4096][r.support%4096]!=255) {
							const int v=result.planeOffsets[r.support/4096][r.support%4096]+r.rise;
							branchSupport=r.support;branchRise=r.rise;
							if(proposed!=INT_MIN && proposed!=v)consistent=false;proposed=v;
						}
						if(consistent && proposed!=INT_MIN)branchBase=proposed;
					}
					if(branchBase==255){compatible=false;continue;}
					result.cornerBranches.push_back({i,p,j,branchBase,branchSupport,branchRise});
					for(int k=0;k<8;k++) {
						const int cx=dx?(dx>0?7:0):k,cy=dy?(dy>0?7:0):k;
						const int nx=dx?(dx>0?0:7):k,ny=dy?(dy>0?0:7):k;
						const int value=branchBase+height(j,nx,ny)-height(i,cx,cy);
						if(candidate!=INT_MIN && candidate!=value){compatible=false;seamConflict=true;}candidate=value;
					}
				}
				if(compatible && candidate!=INT_MIN && candidate!=255 && candidate>=-32767 && candidate<=32767) {
					result.structuralPlaneOffsets[p][i]=candidate;
					if(result.offsets[i]==255 || candidate>result.offsets[i])result.offsets[i]=candidate;
				}
				else if(seamConflict)result.structuralConflicts++;
			}
		}
		// A ROM-owned curved high corner may include a clipped ceiling/cap.
		// That plateau is NOT a floor contact. Only the two surviving downhill
		// tangent face regions constrain its signed placement base. Preserve the
		// authored cap/relief, and require independent agreement on both facings.
		#ifndef ALTTP_TEST_PRIOR_CLIPPED_CORNER_ATTACHMENT
		for(int i=0;i<4096;i++) {
			const auto &t=context.placements[i];
			if(!t.highCornerFace || !t.wallTemplateOffset || !t.hasHeight || t.surface!=RemasterDungeonFloorContext::Surface::Wall)continue;
			const int lo=*std::min_element(t.height.begin(),t.height.end()),hi=*std::max_element(t.height.begin(),t.height.end());
			if(hi<=lo || hi>=lo+7)continue;
			bool clipped=true;
			for(int y=0;y<8;y++)for(int x=0;x<8;x++)if(t.height[y*8+x]!=std::min(hi,std::max(lo+7-x,lo+7-y)))clipped=false;
			if(!clipped)continue;
			result.offsets[i]=255;
			for(int p=0;p<2;p++)result.structuralPlaneOffsets[p][i]=255;
			for(int p=0;p<2;p++) {
				if(!solidFace(context.collisionAttributes[p*4096+i]))continue;
				if(nodes[p*4096+i]>=0 && bad[nodes[p*4096+i]])continue;
				int candidate=INT_MIN;bool complete=true,conflict=false;
				for(int axis=0;axis<2;axis++) {
					const int dx=axis?0:(t.hflip?-1:1),dy=axis?(t.vflip?-1:1):0;
					const int x=i%64+dx,y=i/64+dy;if(x<0 || x>=64 || y<0 || y>=64){complete=false;continue;}
					const int j=y*64+x;const auto &n=context.placements[j];
					if(n.surface!=RemasterDungeonFloorContext::Surface::Wall || !n.hasHeight || n.wallTemplateOffset!=t.wallTemplateOffset ||
						!solidFace(context.collisionAttributes[p*4096+j]) || result.structuralPlaneOffsets[p][j]==255 ||
						(nodes[p*4096+j]>=0 && bad[nodes[p*4096+j]])){complete=false;continue;}
					int samples=0;
					for(int k=0;k<8;k++) {
						const int cx=dx?(dx>0?7:0):k,cy=dy?(dy>0?7:0):k;
						if(height(i,cx,cy)==hi)continue; // ceiling pixels never vote
						const int nx=dx?(dx>0?0:7):k,ny=dy?(dy>0?0:7):k;
						const int value=result.structuralPlaneOffsets[p][j]+height(j,nx,ny)-height(i,cx,cy);
						if(candidate!=INT_MIN && candidate!=value)conflict=true;
						candidate=value;samples++;
					}
					if(samples<2)complete=false;
				}
				if(complete && !conflict && candidate!=INT_MIN && candidate!=255 && candidate>=-32767 && candidate<=32767) {
					if(result.structuralPlaneOffsets[p][i]!=255 && result.structuralPlaneOffsets[p][i]!=candidate){result.structuralConflicts++;continue;}
					result.structuralPlaneOffsets[p][i]=candidate;
					if(result.offsets[i]==255 || candidate>result.offsets[i])result.offsets[i]=candidate;
				} else if(conflict)result.structuralConflicts++;
			}
		}
		#endif
		// Only explicitly marked, transformed floorward contacts seed doorway
		// datums. Carry the datum through the exact complete ROM assembly,
		// retaining authored frame/opening relief (including stairs). Hidden art
		// at the opposite end is not a vote and visibility never chooses support.
		std::map<std::pair<int,int>,int> frameBases;
		std::set<std::pair<int,int>> visibleFrames;
		std::set<std::pair<int,int>> frameConflicts;
		for(int pass=0;pass<12;pass++) {
			bool changed=false;
			for(int i=0;i<4096;i++) {
				const auto &t=context.placements[i];
				if((t.surface!=RemasterDungeonFloorContext::Surface::Door &&
					!(t.surface==RemasterDungeonFloorContext::Surface::ClosedDoor && t.closedDoorAssembly)) || !t.hasHeight ||
					!t.doorTemplateOffset || !t.doorAssembly || t.doorDirection>=4)continue;
				for(int p=0;p<2;p++) {
					const auto key=std::make_pair(i,p);if(frameConflicts.count(key))continue;
					int base=255;bool conflict=false,visible=false;
					if(t.splitDoorPart && context.doorwaySupportRelations)for(const auto &r:*context.doorwaySupportRelations)
						if(r.assembly==t.doorAssembly && r.source==t.doorTemplateOffset && r.direction==t.doorDirection && r.plane==p) {
							if(base!=255 && base!=r.base)conflict=true;else base=r.base;visible=true;
						}
					// A frame is one proved writer footprint, not a connected mask.
					// Disconnected jamb/cap regions retain that ownership. A mask
					// describes pixels, never which members may carry the datum.
					for(const auto &claim:frameBases)if(claim.first.second==p && !frameConflicts.count(claim.first)) {
						const auto &n=context.placements[claim.first.first];
						if(n.doorAssembly!=t.doorAssembly || n.doorDirection!=t.doorDirection || n.doorTemplateOffset!=t.doorTemplateOffset)continue;
						if(base!=255 && base!=claim.second)conflict=true;else base=claim.second;
						visible|=visibleFrames.count(claim.first)!=0;
					}
					for(int d=0;d<4;d++) {
						static const int dx[]={0,0,-1,1},dy[]={-1,1,0,0};
						int x=i%64+dx[d],y=i/64+dy[d];if(x<0 || x>=64 || y<0 || y>=64)continue;
						int j=y*64+x;const auto &n=context.placements[j];int nb=255;
						if((n.surface==RemasterDungeonFloorContext::Surface::Door || (n.surface==RemasterDungeonFloorContext::Surface::ClosedDoor && n.closedDoorAssembly)) && n.doorAssembly==t.doorAssembly &&
							n.doorTemplateOffset==t.doorTemplateOffset && n.doorDirection==t.doorDirection &&
							frameBases.count({j,p}) && !frameConflicts.count({j,p})) nb=frameBases.at({j,p});
						// Require adjacent reproduced support on this plane, authored
						// flat floor samples, and the user-marked canonical direction.
						const auto supportDirection=S9xRemasterTransformSupportDirection(t.doorwaySupportDirection,t.hflip,t.vflip);
						const int inward=supportDirection==RemasterSupportDirection::North?0:supportDirection==RemasterSupportDirection::South?1:
							supportDirection==RemasterSupportDirection::West?2:supportDirection==RemasterSupportDirection::East?3:-1;
						if(nb==255 && t.doorwaySupport && d==inward && (*occupiedComponents)[p*4096+j]>=0 &&
							context.collisionAttributes[p*4096+j]==0 && result.planeOffsets[p][j]!=255 &&
							(n.surface==RemasterDungeonFloorContext::Surface::Floor || n.diagnosticAuthoredFloor ||
							 n.surface==RemasterDungeonFloorContext::Surface::Unknown)) {
							int samples=0;bool zero=n.hasHeight;
							for(int k=0;k<8;k++) {
								const int ax=dx[d]?(dx[d]>0?7:0):k,ay=dy[d]?(dy[d]>0?7:0):k;
								zero &= height(j,dx[d]?7-ax:ax,dy[d]?7-ay:ay)==0;samples++;
							}
							if(zero && samples>=2) {
								const int vote=result.planeOffsets[p][j];
								if(base!=255 && base!=vote)conflict=true;else {
									base=vote;
									// The completed basic-attribute writer indexes BG2 then BG1
									// into independent gameplay arrays. A distinct authored floor
									// in the other map is hidden support, not another owner of this
									// frame's artwork. Preserve its datum but do not merge the two
									// visible offsets. Equal words may genuinely serve both planes.
									// A diagnostic Unknown landing can lack its own Floor bit;
									// the other map's proved flat Floor bit still identifies the
									// hidden counterpart. Do not require both bits to hide it.
									#ifdef ALTTP_TEST_PRIOR_DOOR_PLANE_OWNERSHIP
									visible=true;
									#else
									visible |= !n.distinctLowerUsage || !(n.floorPlaneMask&2) ||
										p==(context.placementBackgroundIndex==1?0:1);
									#endif
								}
							}
						}
						if(nb==255 || !n.hasHeight)continue;
						// Exact recognized assembly ownership transports the floor datum,
						// not a flattened seam height. Every local authored ramp survives.
						if(base!=255 && base!=nb)conflict=true;
						else {base=nb;visible|=visibleFrames.count({j,p})!=0;}
					}
					if(conflict || (base!=255 && frameBases.count(key) && frameBases.at(key)!=base)) {frameConflicts.insert(key);frameBases.erase(key);changed=true;}
					else if(base!=255 && !frameBases.count(key)) {frameBases[key]=base;changed=true;}
					if(!conflict && base!=255 && visible && !visibleFrames.count(key)){visibleFrames.insert(key);changed=true;}
				}
			}
			if(!changed)break;
		}
		// A conflicting member invalidates its entire assembly on that plane.
		for(const auto &entry:frameBases) {
			const int i=entry.first.first,p=entry.first.second;const auto &t=context.placements[i];bool conflict=false;
			for(const auto &bad:frameConflicts)if(bad.second==p && context.placements[bad.first].doorAssembly==t.doorAssembly)conflict=true;
			if(!conflict) {
				// Only the room-facing landing of this exact ROM assembly supplies
				// the opening datum. Require reproduced body support and agreement;
				// neither a nearby wall band nor a hidden underpass is a floor vote.
				// The ROM footprint is three structural bands deep. Asset relief is
				// tile-local, not assembly-local: carry the landing datum unchanged,
				// then place each frame band at its handler-relative 8-pixel rise.
				// Opening pixels keep the landing datum. Never borrow a wall base.
				const int origin=t.doorAssembly-1;
				const int band=t.doorDirection==0?origin/64+2-i/64:t.doorDirection==1?i/64-origin/64:
					t.doorDirection==2?origin%64+2-i%64:i%64-origin%64;
				if(band<0 || band>2)continue;
				if(t.splitDoorPart) {
					const RemasterDungeonFloorContext::DoorwaySupportRelation r={t.doorAssembly,t.doorTemplateOffset,t.doorDirection,uint8_t(p),entry.second};
					if(std::none_of(result.doorwaySupportRelations.begin(),result.doorwaySupportRelations.end(),[&](const auto &n){return n.assembly==r.assembly && n.source==r.source && n.direction==r.direction && n.plane==r.plane;}))result.doorwaySupportRelations.push_back(r);
				}
				// Legacy intrinsic frame arrays may already contain their ROM band
				// phase. Recognize only a single eight-unit block (+ zero cutouts),
				// never normalize arbitrary relief or change authored metadata.
				uint64_t mask=t.closedDoorAssembly?UINT64_MAX:t.hasDoorframeMask?t.doorframeMask:UINT64_MAX;
				int maximum=INT16_MIN;for(int k=0;k<64;k++)if(mask&(UINT64_C(1)<<k))maximum=std::max(maximum,int(t.height[k]));
				const int block=maximum>=0?(maximum/8)*8:0;
				bool phased=block>0 && block==8*band;for(int k=0;k<64;k++)if(mask&(UINT64_C(1)<<k))
					phased &= (!t.closedDoorAssembly && t.height[k]==0) || (t.height[k]>=block && t.height[k]<=block+7);
				if(phased && !t.closedDoorAssembly)for(int k=0;k<64;k++)if(t.height[k]==0)mask&=~(UINT64_C(1)<<k);
				result.doorframeAttachments.push_back({i,p,entry.second+8*band-(phased?block:0),mask,t.doorTemplateOffset,
					visibleFrames.count(entry.first)!=0,entry.second});
			}
		}
		// Exact opening words classified as Floor keep their intrinsic relief,
		// but inherit the same room-facing datum as the owning frame. Do not add
		// movement support or overwrite either gameplay-plane support array.
		for(int i=0;i<4096;i++) {
			const auto &t=context.placements[i];
			if(!(t.surface==RemasterDungeonFloorContext::Surface::Floor ||
				(t.surface==RemasterDungeonFloorContext::Surface::Unknown && t.diagnosticAuthoredFloor)) || !t.doorAssembly || t.doorDirection>=4)continue;
			for(int p=0;p<2;p++) {
				int base=255;bool visible=false,conflict=false;
				for(const auto &bad:frameConflicts)if(bad.second==p && context.placements[bad.first].doorAssembly==t.doorAssembly)conflict=true;
				for(const auto &e:frameBases)if(e.first.second==p) {
					const auto &n=context.placements[e.first.first];
					// Equivalent ROM templates can share the complete structural U
					// while differing in opening words. The classifier separately
					// verified this opening's final word; unique origin/axis owns it.
					if(n.doorAssembly!=t.doorAssembly || n.doorDirection!=t.doorDirection)continue;
					if(base!=255 && base!=e.second)conflict=true;base=e.second;visible|=visibleFrames.count(e.first)!=0;
				}
				if(!conflict && base!=255)result.doorframeAttachments.push_back({i,p,base,UINT64_MAX,t.doorTemplateOffset,visible,base});
			}
		}
		// A surviving jamb has its OWN exact two-word ROM proof, not ownership
		// of overwritten lintels/opposite jambs. Only its explicitly marked foot
		// contacts occupied same-plane support; transport to its one proved band.
		for(int i=0;i<4096;i++) {
			const auto &t=context.placements[i];if(!t.doorwayColumnContact || !t.doorwayColumnSource || t.doorAssembly)continue;
			const int foot=t.doorwayColumnContact-1;const auto &f=context.placements[foot];
			const auto direction=S9xRemasterTransformSupportDirection(f.doorwaySupportDirection,f.hflip,f.vflip);
			const int dx=direction==RemasterSupportDirection::East?1:direction==RemasterSupportDirection::West?-1:0;
			const int dy=direction==RemasterSupportDirection::South?1:direction==RemasterSupportDirection::North?-1:0;
			const int x=foot%64+dx,y=foot/64+dy;if((!dx && !dy)||x<0||x>=64||y<0||y>=64)continue;
			const int j=y*64+x;const auto &n=context.placements[j];
			if(!f.doorwaySupport || !n.hasHeight || n.stairSourceAmbiguous ||
				!(n.surface==RemasterDungeonFloorContext::Surface::Floor || n.diagnosticAuthoredFloor))continue;
			bool zero=true;for(int k=0;k<8;k++)zero &= height(j,dx?(dx>0?0:7):k,dy?(dy>0?0:7):k)==0;
			if(!zero)continue;
			for(int p=0;p<2;p++)if((*occupiedComponents)[p*4096+j]>=0 && context.collisionAttributes[p*4096+j]==0 && result.planeOffsets[p][j]!=255) {
				const int base=result.planeOffsets[p][j];
				result.doorframeAttachments.push_back({i,p,base+8*t.doorwayColumnBand,t.hasDoorframeMask?t.doorframeMask:UINT64_MAX,t.doorwayColumnSource,true,base});
			}
		}
		for(int i=0;i<4096;i++)if(context.placements[i].surface==RemasterDungeonFloorContext::Surface::Door &&
			((context.placements[i].hasDoorframeMask && context.placements[i].doorframeMask) || context.placements[i].doorwaySupport ||
			 (context.placements[i].hasHeight && context.placements[i].doorAssembly && context.placements[i].doorTemplateOffset))) {
			bool solved=false,conflict=false;int visibleBase=255;
			for(const auto &a:result.doorframeAttachments)if(a.cell==i && a.visible) {
				solved=true;if(visibleBase!=255 && visibleBase!=a.base)conflict=true;visibleBase=a.base;
			}
			for(const auto &bad:frameConflicts)conflict|=bad.first==i;
			if(conflict)result.conflictingDoorframeCells++;else if(!solved)result.unknownDoorframeCells++;
		}
		// TileDetect_ExecuteInner sets the same indoor blocking bit for 6c..6f
		// as ordinary solid art (outdoors they have different behavior). This is
		// only corroboration of an exact footprint's owning plane, never movement
		// support or a height label. Keep the distinct lower-floor/contact proofs.
		auto obstacleBodySolid=[](int a) { return a==1 || a==2 || a==0x27 || (a>=0x6c && a<=0x6f); };
		// Exact freestanding obstacle footprints share ONE supporting component.
		// Contact only the transformed canonical bottom row: heads/side artwork
		// cannot borrow a nearby floor. No support nodes/constraints are added.
		std::set<uint16_t> obstacleAssemblies;
		for(int i=0;i<4096;i++)if(context.placements[i].obstacleAssembly && !context.placements[i].authoredObstacle)obstacleAssemblies.insert(context.placements[i].obstacleAssembly);
		for(auto assembly:obstacleAssemblies) {
			const int origin=assembly-1;const auto &t=context.placements[origin];const int h=t.obstacleAssemblyHeight;
			if(h<2 || h>4 || origin%64>62 || origin/64+h>64)continue;
			bool complete=true;for(int yy=0;yy<h;yy++)for(int xx=0;xx<2;xx++) {
				const auto &part=context.placements[origin+yy*64+xx];
				complete &= part.obstacleAssembly==assembly && part.obstacleTemplate==t.obstacleTemplate &&
					part.obstacleAssemblyHeight==h && part.obstacleAssemblyFlip==t.obstacleAssemblyFlip &&
					part.surface==RemasterDungeonFloorContext::Surface::Obstacle && part.hasHeight;
			}if(!complete)continue;
			const int footY=origin/64+((t.obstacleAssemblyFlip&2)?0:h-1),outsideY=footY+((t.obstacleAssemblyFlip&2)?-1:1);
			// Same ownership proof as authored footprints: a complete distinct
			// lower floor plus exclusively solid upper body identifies the visible
			// obstacle plane, without choosing Link's plane or the higher base.
			bool underpass=context.placementBackgroundIndex==1,solid=false,exclusive=true;
			for(int yy=0;yy<h;yy++)for(int xx=0;xx<2;xx++) {
				const int cell=origin+yy*64+xx;const auto &part=context.placements[cell];
				underpass &= part.distinctLowerUsage && (part.floorPlaneMask&2);
				const int a=context.collisionAttributes[cell];solid |= obstacleBodySolid(a);
				exclusive &= (a==0 || obstacleBodySolid(a)) && context.collisionAttributes[4096+cell]==0;
			}
			const int bodyPlane=underpass && solid && exclusive?0:-1;
			int support=-1,base=255,plane=-1;bool conflict=false;uint8_t supportedFeet=0;
			for(int p=0;p<2;p++)if(activePlanes[p])for(int xx=0;xx<2;xx++) {
				if(bodyPlane>=0 && p!=bodyPlane)continue;
				const int foot=footY*64+origin%64+xx;
				// Occupied support under nonblocking foot, or directly contacted
				// support at its outward ground edge. Both use reproduced bodies.
				for(int cell:{foot,outsideY>=0 && outsideY<64?outsideY*64+origin%64+xx:-1}) {
					if(cell<0)continue;const int n=(*occupiedComponents)[p*4096+cell];
					if(n<0 || result.planeOffsets[p][cell]==255)continue;
					supportedFeet|=1<<xx;
					if(support>=0 && (support!=n || plane!=p || base!=result.planeOffsets[p][cell]))conflict=true;
					support=n;plane=p;base=result.planeOffsets[p][cell];
				}
			}
			if(conflict){result.structuralConflicts++;base=255;}
			if(supportedFeet!=3)base=255; // complete width2foot evidence required
			for(int yy=0;yy<h;yy++)for(int xx=0;xx<2;xx++) {
				const int cell=origin+yy*64+xx;result.offsets[cell]=base;result.fallbackDirections[cell]=0;
				if(base!=255 && plane>=0)result.structuralPlaneOffsets[plane][cell]=base;
				// Structural shell base does not replace independently solved actor
				// support under any nonblocking head/foot artwork on either plane.
			}
		}
		// Authored arbitrary footprints consume already solved movement support.
		// Every named contact must be witnessed; no Z-zero or nearest-floor inference.
		std::map<uint16_t,std::vector<int>> authoredAssemblies;
		for(int i=0;i<4096;i++)if(context.placements[i].authoredObstacle) {
			result.authoredObstacleCells++;
			result.offsets[i]=255;result.fallbackDirections[i]=0;
			if(context.placements[i].authoredObstacleAssembly)authoredAssemblies[context.placements[i].authoredObstacleAssembly].push_back(i);
			else result.unmatchedAuthoredObstacleCells++;
		}
		for(const auto &entry:authoredAssemblies) {
			int support=-1,plane=-1,base=255;bool conflict=false,complete=true;int contacts=0,witnessedContacts=0;
			// A complete distinct BG1 zero-local floor under a BG2 footprint is
			// lower support, not another owner of the visible obstacle. Completed
			// solid/hookshottable body attributes corroborate the upper-plane usage.
			// This is ownership evidence only: floor height still comes exclusively
			// from EVERY declared movement contact on that plane. All-00 bodies,
			// missing lower floor metadata and unsupported special attributes stay ambiguous.
			int bodyPlane=-1;
#ifndef ALTTP_TEST_PRIOR_OBSTACLE_CONTACT_PLANES
			bool underpass=context.placementBackgroundIndex==1;
			for(int i:entry.second)underpass &= context.placements[i].distinctLowerUsage && (context.placements[i].floorPlaneMask&2);
			if(underpass) {
				bool solid=false,exclusive=true;
				for(int i:entry.second) {
					const int a=context.collisionAttributes[i];
					solid |= obstacleBodySolid(a);
					exclusive &= (a==0 || obstacleBodySolid(a)) && context.collisionAttributes[4096+i]==0;
				}
				if(solid && exclusive)bodyPlane=0;
			}
#endif
			for(int i:entry.second) {
				const auto &part=context.placements[i];
				if(!part.authoredObstacleContact)continue;
				contacts++;bool witnessed=false;
				const int direction=part.authoredObstacleDirection;
				const int dx=direction==2?-1:direction==3?1:0,dy=direction==0?-1:direction==1?1:0;
				// A selected contact can be an interior member (e.g. a chair seat).
				// Follow only this exact assembly's contiguous footprint to its ground
				// edge. Another member's lower-plane occupancy is not a floor contact.
				int x=i%64+dx,y=i/64+dy;
				while(x>=0 && x<64 && y>=0 && y<64 && context.placements[y*64+x].authoredObstacleAssembly==entry.first){x+=dx;y+=dy;}
				std::vector<int> contactCells{i,x>=0 && x<64 && y>=0 && y<64?y*64+x:-1};
				bool directContact=false;
				for(int p=0;p<2;p++)if(activePlanes[p] && (bodyPlane<0 || p==bodyPlane))for(int cell:contactCells)
					if(cell>=0 && (*occupiedComponents)[p*4096+cell]>=0 && result.planeOffsets[p][cell]!=255)directContact=true;
				// A declared foot may meet its floor along a side (a pillar at a
				// platform's end), not only below the screen-space bottom. Require
				// the full transformed authored edge to be at local ground zero.
				// Never accept side proximity from heads or unmarked body members.
				if(!directContact && part.hasHeight)for(int dx:{-1,1})if(i%64+dx>=0 && i%64+dx<64) {
					bool ground=true;for(int k=0;k<8;k++) {
						const int x=dx<0?0:7,yy=part.vflip?7-k:k,xx=part.hflip?7-x:x;
						ground &= part.height[yy*8+xx]==0;
					}
					const auto &neighbor=context.placements[i+dx];
					if(ground && neighbor.surface==RemasterDungeonFloorContext::Surface::Floor && neighbor.hasHeight &&
						std::all_of(neighbor.height.begin(),neighbor.height.end(),[](int16_t h){return h==0;}))contactCells.push_back(i+dx);
				}
				for(int p=0;p<2;p++)if(activePlanes[p])for(int cell:contactCells) {
					if(bodyPlane>=0 && p!=bodyPlane)continue;
					if(cell<0)continue;int n=(*occupiedComponents)[p*4096+cell];
					// A narrow explicit floor strip can have a solved structural base
					// without admitting Link's entire 16x16 collision body. Trace only
					// legal, authored Floor placements at that SAME solved base to an
					// independently occupied component. Never traverse furniture/walls,
					// fabricate movement coverage or merge distinct support components.
					if(n<0 && cell!=i && result.planeOffsets[p][cell]!=255) {
						std::set<int> seen;std::vector<int> queue{cell};int owner=-1;
						for(size_t at=0;at<queue.size();at++) {
							const int j=queue[at];if(!seen.insert(j).second)continue;
							const auto &floor=context.placements[j];
							if(context.collisionAttributes[p*4096+j]!=0 || floor.surface!=RemasterDungeonFloorContext::Surface::Floor ||
								!floor.hasHeight || !std::all_of(floor.height.begin(),floor.height.end(),[](int16_t h){return h==0;}) ||
								result.planeOffsets[p][j]!=result.planeOffsets[p][cell])continue;
							const int component=(*occupiedComponents)[p*4096+j];
							if(component>=0){if(owner==-1)owner=component;else if(owner!=component)owner=-2;continue;}
							for(int d=0;d<4;d++){const int xx=j%64+(d==2?-1:d==3?1:0),yy=j/64+(d==0?-1:d==1?1:0);
								if(xx>=0 && xx<64 && yy>=0 && yy<64)queue.push_back(yy*64+xx);}
						}n=owner;
					}
					if(n<0 || result.planeOffsets[p][cell]==255)continue;
					// Another obstacle's rendered member offset is never support evidence.
					witnessed=true;const int b=result.planeOffsets[p][cell];
					if(support>=0 && (support!=n || plane!=p || base!=b))conflict=true;
					support=n;plane=p;base=b;
				}
				complete &= witnessed;
				witnessedContacts+=witnessed;
			}
			result.authoredObstacleEvidence.push_back({entry.first,context.placements[entry.second.front()].authoredObstacleGroup,
				plane,support,base,static_cast<uint16_t>(entry.second.size()),static_cast<uint16_t>(contacts),static_cast<uint16_t>(witnessedContacts),conflict});
			if(conflict)result.structuralConflicts++;
			if(!complete || !contacts || conflict || base==255)continue;
			// 255 is the legacy unresolved sentinel, so never publish that offset.
			for(int i:entry.second) {
				const auto &part=context.placements[i];const int value=base+(part.hasHeight?0:part.authoredObstacleZ*8);
				if(value==255 || value < -32767 || value>32767)complete=false;
			}
			if(!complete) { result.structuralConflicts++;continue; }
			for(int i:entry.second){const auto &part=context.placements[i];
				result.offsets[i]=base+(part.hasHeight?0:part.authoredObstacleZ*8);result.solvedAuthoredObstacleCells++;
				if(!part.hasHeight) {
					result.producerGeneratedObstacle[i]=true;
					RemasterDungeonHeightMap::LocalHeightPreview preview;preview.cell=i;
					// Canonical storage transformed to displayed bottom0/top7. Artwork
					// member/global flips are already composed in placement.vflip.
					for(int k=0;k<64;k++)preview.height[k]=part.vflip?k/8:7-k/8;
					result.localHeightPreviews.push_back(preview);
				}
			}
		}
		S9xRemasterAttachWallMounts(context,result);
		// An explicitly authored obstacle assembly can interrupt a wall's tangent
		// without interrupting its physical surface (decorated wall columns). Its
		// solved SUPPORT base is not the seam height: compare all eight actual
		// transformed edge samples. Wall nodes require complete one-axis sections;
		// a decorated endpoint needs only its owned contacting edge, not an affine
		// interior. Within a unique intact ROM section, signed authored normal
		// continuation joins its bands. Never cross a missing source, corner,
		// incompatible edge, underpass or ambiguous assembly owner.
#ifndef ALTTP_TEST_PRIOR_OBSTACLE_WALL_SEAMS
		for(int sectionPass=0;sectionPass<2;sectionPass++)for(int p=0;p<2;p++)if(activePlanes[p]) {
			std::array<bool,4096> seen={};
			auto axis=[&](int i) {
				const auto &t=context.placements[i];if(!t.hasHeight)return -1;
				for(int a=0;a<2;a++) {
					bool exact=true;const int first=height(i,0,0),last=height(i,a?0:7,a?7:0);
					if(first==last)continue;
					for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
						if(height(i,x,y)!=height(i,a?0:x,a?y:0))exact=false;
						const int k=a?y:x;
						const int derivative=k<7?height(i,a?0:k+1,a?k+1:0)-height(i,a?0:k,a?k:0):0;
						if(derivative && (derivative>0)!=(last>first))exact=false;
					}
					if(exact)return a;
				}return -1;
			};
			auto ownedSolid=[&](int i) {
				const int attr=context.collisionAttributes[p*4096+i];
				// 6e/6f are hookshot-special attributes, NOT generic solid floor.
				// Only the independently proved intact ROM wall section admits them
				// as structural art; never create movement or lower-plane support.
				return solidFace(attr) || (sectionPass && context.placements[i].wallStackAssembly && (attr==0x6e || attr==0x6f));
			};
			for(int start=0;start<4096;start++) {
				const auto &t=context.placements[start];const int a=axis(start);
				if(seen[start] || a<0 || t.surface!=RemasterDungeonFloorContext::Surface::Wall ||
					(context.placementBackgroundIndex==0 && p==0 && activePlanes[1] && t.distinctLowerUsage) ||
					!ownedSolid(start))continue;
				std::vector<int> queue{start};std::map<int,int> relative{{start,0}};std::set<int> anchors;bool conflict=false,assemblyContact=false;
				for(size_t at=0;at<queue.size();at++) {
					const int i=queue[at];seen[i]=true;
					if(nodes[p*4096+i]>=0 && bad[nodes[p*4096+i]])conflict=true;
					// Preserve independent tangent proofs before considering larger
					// writer-section constraints. A contradictory extension remains
					// unresolved; it cannot erase previously proved smaller surfaces.
					for(int along=0;along<=sectionPass;along++)for(int sign:{-1,1}) {
						const bool normal=along==1;
						const int dx=(a!=normal)?sign:0,dy=dx?0:sign;
						const int x=i%64+dx,y=i/64+dy;if(x<0 || x>=64 || y<0 || y>=64)continue;
						const int j=y*64+x;const auto &n=context.placements[j];
						if(!n.hasHeight)continue;
						// Tangent contact owns only the actual edge of a decorated
						// obstacle, not its entire relief array. Full one-axis sections
						// are still required for every wall node. Normal continuation
						// additionally requires the SAME unique ROM writer section.
						if(normal && (!context.placements[i].wallStackAssembly || context.placements[i].wallStackAssembly!=n.wallStackAssembly))continue;
						if(n.surface==RemasterDungeonFloorContext::Surface::Wall ? axis(j)!=a : !n.authoredObstacle)continue;
						if(context.placementBackgroundIndex==0 && p==0 && activePlanes[1] && n.distinctLowerUsage)continue;
						int delta=INT_MIN;bool exact=true;
						for(int k=0;k<8;k++) {
							const int h=height(i,dx?(sign>0?7:0):k,dy?(sign>0?7:0):k);
							const int oh=height(j,dx?(sign>0?0:7):k,dy?(sign>0?0:7):k);
							int value=h-oh;
							if(normal) {
								if(direction(i,dx,dy)!=direction(j,dx,dy))exact=false;
								value+=direction(i,dx,dy)?-boundaryStep(j,-dx,-dy,k):boundaryStep(i,dx,dy,k);
							}
							if(delta!=INT_MIN && delta!=value)exact=false;delta=value;
						}if(!exact)continue;
						const int r=relative[i]+delta;
						if(n.surface==RemasterDungeonFloorContext::Surface::Wall && ownedSolid(j)) {
							if(relative.count(j)){if(relative[j]!=r)conflict=true;}
							else {relative[j]=r;queue.push_back(j);}
						} else if(n.authoredObstacle && n.authoredObstacleAssembly && result.offsets[j]!=255) {
							for(const auto &e:result.authoredObstacleEvidence)if(e.assembly==n.authoredObstacleAssembly &&
								e.plane==p && !e.conflict && e.floorBase!=255){anchors.insert(result.offsets[j]-r);assemblyContact=true;}
						}
					}
				}
				for(int i:queue)if(result.structuralPlaneOffsets[p][i]!=255)anchors.insert(result.structuralPlaneOffsets[p][i]-relative[i]);
				for(int i:queue)if(result.offsets[i]!=255)anchors.insert(result.offsets[i]-relative[i]);
				if(assemblyContact && anchors.size()>1 && !conflict)result.structuralConflicts++;
				if(!assemblyContact || conflict || anchors.size()!=1)continue;
				for(int i:queue) {
					const int base=*anchors.begin()+relative[i];if(base==255 || base<-32767 || base>32767)continue;
					if(result.offsets[i]!=255 && result.offsets[i]!=base)continue;
					result.offsets[i]=base;result.structuralPlaneOffsets[p][i]=base;
				}
			}
		}
#endif
		for (int i = 0; i < 4096; i++)
		{
			const auto &tile = context.placements[i];
			if (tile.surface == RemasterDungeonFloorContext::Surface::Wall)
			{
				if (result.offsets[i] == 255) result.unknownWallCells++; else result.solvedWallCells++;
				continue;
			}
			if (tile.surface == RemasterDungeonFloorContext::Surface::Floor || tile.surface == RemasterDungeonFloorContext::Surface::Stair ||
				tile.surface == RemasterDungeonFloorContext::Surface::Door ||
				tile.surface == RemasterDungeonFloorContext::Surface::ClosedDoor ||
				tile.surface == RemasterDungeonFloorContext::Surface::OutOfBounds) continue; // terminal visual roles never receive guessed border ramps
			// Ordinary occupied support is not a collidable-border candidate on
			// another plane. Otherwise lower colliders overwrite an upper floor
			// with a 0..7 fallback (exact008's two neutral floor hashes).
			if (components[i] >= 0 || components[4096+i] >= 0) continue;
			// Verified rail trim already has independent plane-specific assembly
			// support. Do not erase it with the identity-less one-cell fallback.
			if(tile.supportingTrim) continue;
			if(tile.removableSupportPlanes)continue; // unresolved persistent body cannot be rescued by adjacency fallback
			if(tile.obstacleAssembly || tile.authoredObstacle)continue; // whole-foot ambiguity cannot be rescued by per-part proximity
			// An authored obstacle already attached by plane-specific support
			// constraints is not an identity-less border fallback. Rechecking
			// adjacent contacts across planes erased proved bridge rail bases.
			if(tile.surface==RemasterDungeonFloorContext::Surface::Obstacle && result.offsets[i]!=255) continue;
			// A fully transparent final BG placement is no obstacle surface.
			// Completed collision can still describe another BG/plane beneath it
			// (notably cleared stairwells). Retain independent support arrays, but
			// do not invent a border ramp or a conflict for nonexistent artwork.
			if(tile.surface==RemasterDungeonFloorContext::Surface::Unknown && context.constructedMaps && context.graphics) {
				const int word=context.constructedMaps[i*2]|context.constructedMaps[i*2+1]<<8;
				bool transparent=true;for(int b=0;b<32;b++)transparent &= context.graphics[uint16_t(context.graphicsBase+(word&1023)*32+b)]==0;
				if(transparent)continue;
			}
			int base = -1, planeIdentity = -1;
			uint8_t directions = 0; bool conflict = false;
			for (int p = 0; p < 2; p++) for (int d = 0; d < 4; d++)
			{
				static const int dx[] = {0, 0, -1, 1}, dy[] = {-1, 1, 0, 0};
				int x = i % 64 - dx[d], y = i / 64 - dy[d];
				if (x < 0 || x >= 64 || y < 0 || y >= 64) continue;
				int j = y * 64 + x;
				if ((*occupiedComponents)[p * 4096 + j] < 0 || result.planeOffsets[p][j] == 255) continue;
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
		// Terminal stair attachment consumes solved landing data AFTER the graph.
		// It cannot create movement edges, datums, or bridge gameplay planes. Both
		// full landings must agree with the measured authored upper endpoint.
		auto stairOwners=std::make_unique<std::array<int,4096>>();stairOwners->fill(0);
		std::vector<RemasterDungeonStairProof> stairProofs;
		for(int si=0;si<context.stairCount;si++) {
			RemasterDungeonStairProof proof;if(!S9xRemasterDungeonStairProof(context,context.stairs[si],proof))continue;
			stairProofs.push_back(proof);
			for(int y=0;y<4;y++)for(int x=0;x<4;x++)(*stairOwners)[proof.origin+y*64+x]++;
		}
		for(const auto &proof:stairProofs) {
			const auto found=std::find_if(context.stairs.begin(),context.stairs.begin()+context.stairCount,
				[&](const RemasterDungeonFloorContext::Stair &s){return s.y*64+s.x==proof.origin;});
			std::vector<RemasterDungeonHeightMap::StairAttachment> claims;
			for(int p=0;p<2;p++) {
				const int lp=found->changesPlane?1:p,hp=found->changesPlane?0:p;
				const int low=result.planeOffsets[lp][proof.low],high=result.planeOffsets[hp][proof.high];
				if(low==255 && high==255){if(found->changesPlane)break;else continue;}
				bool valid=low!=255 && high!=255 && high-low==proof.rise;
				for(int end:{proof.low,proof.high})for(int dx:{0,1}) {
					const auto &t=context.placements[end+dx];const int plane=end==proof.low?lp:hp,base=end==proof.low?low:high;
					const auto *source=&t;
					if(proof.producer && context.peerProducerPlacements && (!t.hasHeight || t.stairSourceAmbiguous ||
						!std::all_of(t.height.begin(),t.height.end(),[](int16_t h){return h==0;})))source=&context.peerProducerPlacements[end+dx];
					if(components[plane*4096+end+dx]<0 || result.planeOffsets[plane][end+dx]!=base || !source->hasHeight || source->stairSourceAmbiguous ||
						!std::all_of(source->height.begin(),source->height.end(),[](int16_t h){return h==0;}))valid=false;
				}
				for(int y=0;y<4;y++)for(int x=0;x<4;x++) {
					const int i=proof.origin+y*64+x;
					// Movement projection may touch the decorative end row on the
					// upper landing. That is support, not this stair's relief datum.
					if((*stairOwners)[i]!=1)valid=false;
				}
				claims.push_back({proof.origin,proof.low,proof.high,lp,hp,proof.rise,valid?low:255,proof.flip,!valid});
				if(found->changesPlane)break;
			}
			bool unanimous=!claims.empty();int base=claims.empty()?255:claims[0].base;
			for(const auto &a:claims)if(a.conflict || a.base!=base)unanimous=false;
			for(auto a:claims) {
				if(!unanimous){a.base=255;a.conflict=true;}
				result.stairAttachments.push_back(a);
				if(unanimous)for(int y=0;y<4;y++)for(int x=0;x<4;x++) {
					const int i=proof.origin+y*64+x;result.producerOwnedStair[i]=true;
					if(proof.producer && context.producerWrites->cells[context.placementBackgroundIndex*4096+i].owner!=proof.producer) {
						result.producerOwnedStair[i]=false;continue;
					}
					result.offsets[i]=base;
					result.structuralPlaneOffsets[a.lowPlane][i]=result.structuralPlaneOffsets[a.highPlane][i]=base;
					result.fallbackDirections[i]=0;
				}
			}
		}
		// A producer shared by both BGs has one relief datum. The peer's proved
		// occupied landing relation can place surviving members even where this
		// map's art has no movement-support component. Never copy peer floor
		// offsets or claim overwritten words; only transport the terminal proof.
		if(S9xRemasterDungeonResearchEnabled() && context.producerWrites && context.producerWrites->enabled && context.peerShellMap)for(int si=0;si<context.stairCount;si++) {
			RemasterDungeonStairProof proof;
			if(!S9xRemasterDungeonStairProof(context,context.stairs[si],proof) || !proof.producer)continue;
			for(const auto &a:context.peerShellMap->stairAttachments) {
				if(a.origin!=proof.origin || a.low!=proof.low || a.high!=proof.high || a.rise!=proof.rise || a.conflict || a.base==255)continue;
				for(int y=0;y<4;y++)for(int x=0;x<4;x++) {
					const int i=proof.origin+y*64+x;
					if(context.producerWrites->cells[context.placementBackgroundIndex*4096+i].owner!=proof.producer)continue;
					result.producerOwnedStair[i]=true;result.offsets[i]=a.base;result.fallbackDirections[i]=0;
					result.structuralPlaneOffsets[a.lowPlane][i]=result.structuralPlaneOffsets[a.highPlane][i]=a.base;
				}
				result.stairAttachments.push_back(a);
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
	int floor = 255;
	for (int p=0; p<2; p++) if (map.structuralPlaneOffsets[p][cell] == map.offsets[cell] && map.offsets[cell] != 255)
	{
		const int value=map.mixedFloorBases[p][cell];
		if (value == 255 || (floor != 255 && floor != value)) return 255;
		floor=value;
	}
	return floor;
}

inline std::array<int16_t,64> S9xRemasterMixedFloorHeight (const std::array<int16_t,64> &local,
	int32_t base, int32_t floor)
{
	auto output=local;
	if (floor != 255 && floor > base && floor - base <= 32767)
		for (auto &h : output) h=std::max(h,static_cast<int16_t>(floor-base));
	return output; // canonical sampling; heightOffset supplies base exactly once
}

// Actor support is continuous across an intra-room stair even though ALTTP
// switches its collision plane at one end of traversal. Use solved landing
// datums, not tread relief, OBJ priority, or a configured floor rise.
inline int32_t S9xRemasterLinkFloorHeight(const RemasterDungeonFloorContext &context,
	const RemasterDungeonHeightMap &map)
{
	if (map.roomIndex != context.roomIndex) return 255;
	const int x = (context.linkX + 8) & 511, y = (context.linkY + 20) & 511;
	const int cell = (y / 8) * 64 + x / 8;
	const int plane = context.linkPlane & 1;
	const int32_t ordinary = map.hasPlaneOffsets ? map.planeOffsets[plane][cell] : map.offsets[cell];
	const bool dungeon = context.module == 7 ||
		(context.module == 14 && context.submodule == 2 && context.savedModule == 7);
	if (!context.verifiedAlttpRom || !context.indoors || !dungeon || !map.hasPlaneOffsets ||
		(context.submodule != 0 && context.submodule != 8 && context.submodule != 16 &&
		 context.module != 14)) return ordinary;
	if (map.hasActorThresholdSupport && context.collisionAttributes) {
		const auto attribute=context.collisionAttributes[plane*4096+cell];
		if(attribute>=0x80 && attribute<=0x8f)return map.actorThresholdBases[plane][cell];
	}
	int32_t ramp = 255;
	bool matched = false;
	for (size_t i = 0; i < context.stairCount && i < context.stairs.size(); i++)
	{
		const auto &s = context.stairs[i];
		if (s.x > 60 || !s.y || s.y > 59 || x < (s.x + 1) * 8 || x >= (s.x + 3) * 8 ||
			y < s.y * 8 || y > (s.y + 4) * 8) continue;
		const int north = (s.y - 1) * 64 + s.x + 1;
		const int south = (s.y + 4) * 64 + s.x + 1;
		const int np = s.changesPlane ? (s.highIsNorth ? 0 : 1) : plane;
		const int sp = s.changesPlane ? (s.highIsNorth ? 1 : 0) : plane;
		const int32_t a = map.planeOffsets[np][north], b = map.planeOffsets[sp][south];
		// Both columns must attest the same landing; unresolved or conflicting
		// support must not be rescued by interpolation or neighboring floors.
		if (a == 255 || b == 255 || map.planeOffsets[np][north + 1] != a ||
			map.planeOffsets[sp][south + 1] != b) return ordinary;
		const int progress = y - s.y * 8;
		const int32_t value = static_cast<int32_t>(std::lround(
			(static_cast<double>(a) * (32 - progress) + static_cast<double>(b) * progress) / 32));
		if (matched && ramp != value) return ordinary;
		ramp = value; matched = true;
	}
	return matched ? ramp : ordinary;
}

// Owned by the runtime solve boundary, never by pixels, frame instances, or
// profile pointers. One variant per room cell bounds storage even under animated
// or context-selected art. Alternate variants replace, rather than accumulate.
struct RemasterDungeonFrameRegionIndex {
	std::array<int,4096> first;
	std::vector<int> next;
	explicit RemasterDungeonFrameRegionIndex(const RemasterDungeonHeightMap *m) {
		first.fill(-1);if(!m)return;next.assign(m->doorframeAttachments.size(),-1);
		for(int n=int(next.size())-1;n>=0;n--) {
			const int cell=m->doorframeAttachments[n].cell;if(cell<0 || cell>=4096)continue;
			next[n]=first[cell];first[cell]=n;
		}
	}
};
struct RemasterDungeonProjectionCache
{
	struct Decision { int32_t offset=0, floor=255; uint8_t directions=0; bool border=false; };
	struct Cell {
		bool valid=false;
		uint64_t generation=0;
		RemasterTileContentId tileId;
		uint16_t tileNumber=0;
		uint8_t palette=0, priority=0, facing=0;
		bool h=false,v=false,hasMetadata=false;
		RemasterTileRole role=RemasterTileRole::Unknown;
		std::string group;
		std::array<int16_t,64> local={};
		bool localHeight=false;
		std::array<Decision,64> decisions;
	};
	std::array<std::unique_ptr<Cell>,4096> cells;
	std::unique_ptr<RemasterDungeonFrameRegionIndex> regions;
	std::array<int,4096> previews;
	uint64_t builds=0,hits=0,misses=0,invalidations=0,dynamicVisits=0;
	void invalidate() { for(auto &c:cells)c.reset(); regions.reset(); ++invalidations; }
};

inline void S9xRemasterApplyDungeonHeightMap (RemasterFrame &frame,
	const RemasterDungeonFloorContext &context, const RemasterDungeonHeightMap &map,
	const RemasterDungeonHeightMap *sourceRoomMap = nullptr,
	RemasterDungeonHeightApplyDiagnostics *diagnostics = nullptr, uint8_t backgroundIndex = 1,
	RemasterDungeonProjectionCache *retained = nullptr)
{
#ifdef ALTTP_TEST_REFERENCE_STATIC_PROJECTION
	retained=nullptr; // diagnostic differential: the unchanged uncached consumer
#endif
	if (map.roomIndex != context.roomIndex)
		return;
	const size_t originalCount = frame.tileInstances.size();
	auto currentPlacementMatches=[&](const RemasterFrameTileInstance &instance,size_t cell) {
		if(context.retainedOutgoingMaps && context.retainedOutgoingRoom==context.roomIndex) {
			const size_t at=(backgroundIndex?0:8192)+cell*2;
			const int word=context.retainedOutgoingMaps[at]|context.retainedOutgoingMaps[at+1]<<8;
			return instance.tileNumber==(word&1023) && instance.palette==((word>>10)&7) && instance.hFlip==bool(word&0x4000) && instance.vFlip==bool(word&0x8000);
		}
		if(!S9xRemasterDungeonRoomScroll(context.submodule) || !context.constructedMaps)return true;
		const int start=backgroundIndex?0:8192;
		const uint16_t word=context.constructedMaps[start+cell*2]|context.constructedMaps[start+cell*2+1]<<8;
		return instance.tileNumber==(word&1023) && instance.palette==((word>>10)&7) &&
			instance.hFlip==bool(word&0x4000) && instance.vFlip==bool(word&0x8000);
	};
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
		int16_t localMinimum = 0;
		bool conflict = false;
	};
	auto wallStorage = std::make_unique<std::array<WallCell, 64 * 64>>();
	auto &walls = *wallStorage;
	std::vector<const RemasterFrameAssetMetadata *> instanceMetadata(originalCount, nullptr);
	std::set<RemasterTileContentId> authoredObstacles;
	for(const auto &metadata:frame.assetMetadata)if(metadata.tileRole==RemasterTileRole::Obstacle)authoredObstacles.insert(metadata.tileId);
	for (size_t instanceId = 0; instanceId < originalCount; instanceId++)
		for (const RemasterFrameAssetMetadata &metadata : frame.assetMetadata)
			if (metadata.tileId == frame.tileInstances[instanceId].tileId &&
				(metadata.hasHeight || metadata.tileRole==RemasterTileRole::OutOfBounds))
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
				if(!metadata->hasHeight || metadata->tileRole==RemasterTileRole::OutOfBounds || metadata->tileRole==RemasterTileRole::Obstacle || instance.assetGroup=="obstacles")continue;
				int north = 0, south = 0, west = 0, east = 0;
				int16_t minimum = INT16_MAX;
				for (int i = 0; i < 8; i++)
				{
					north += metadata->height[i];
					south += metadata->height[56 + i];
					west += metadata->height[i * 8];
					east += metadata->height[i * 8 + 7];
				}
				for (int16_t value : metadata->height) minimum = std::min(minimum, value);
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
	// Attachments are room placements, not a pixel search domain. Index once per
	// application and retain every plane/mask claim in original order; do not
	// choose one owner or collapse ambiguous visible region bases.
	using FrameRegionIndex=RemasterDungeonFrameRegionIndex;
	if(retained && !retained->regions) {
		retained->regions=std::make_unique<FrameRegionIndex>(&map);
		retained->previews.fill(-1);
		for(size_t n=0;n<map.localHeightPreviews.size();++n) {
			const int cell=map.localHeightPreviews[n].cell;
			if(cell>=0 && cell<4096 && retained->previews[cell]<0)retained->previews[cell]=int(n);
		}
	}
	auto currentFrameStorage=retained?nullptr:std::make_unique<FrameRegionIndex>(&map);
	const auto *currentFrames=retained?retained->regions.get():currentFrameStorage.get();
	auto sourceFrames=std::make_unique<FrameRegionIndex>(sourceRoomMap);
	auto offsetAt = [&] (const RemasterFramePixel &pixel, uint32_t x, uint32_t y) -> int32_t {
		if (!pixel.instanceId || pixel.instanceId > originalCount)
			return 0;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		const auto *metadata=instanceMetadata[pixel.instanceId-1];
		if (instance.source != RemasterSourceType::Background || instance.sourceIndex != backgroundIndex)
			return 0;
		const uint16_t pixelRoom = S9xRemasterDungeonRoomForScreenPosition(context, x, y);
		const uint32_t roomX = (context.backgroundScrollX + x) & 511;
		const uint32_t roomY = (context.backgroundScrollY + y) & 511;
		const size_t cell = (roomY / 8) * 64 + roomX / 8;
		const auto *producer = pixelRoom == context.roomIndex ? &map : sourceRoomMap;
		if(context.retainedOutgoingMaps && pixelRoom==context.retainedOutgoingRoom) {
			const size_t at=(backgroundIndex?0:8192)+cell*2;
			const int word=context.retainedOutgoingMaps[at]|context.retainedOutgoingMaps[at+1]<<8;
			if(instance.tileNumber!=(word&1023) || instance.palette!=((word>>10)&7))return 0;
		}
		// During incremental upload, complete WRAM destination geometry and
		// displayed PPU placements are different inventories. A repeated hash
		// alone cannot attest that this particular destination word was uploaded.
		if(pixelRoom==context.roomIndex && !currentPlacementMatches(instance,cell))return 0;
		if (producer && producer->producerHashes[cell] && (producer->producerHashes[cell] != instance.tileId.hash ||
			producer->producerFlips[cell] != (instance.hFlip | (instance.vFlip<<1)))) return 0;
		if(metadata && metadata->tileRole==RemasterTileRole::OutOfBounds)return 0; // visual crest pass owns this datum, never a physical floor
		if(producer && producer->producerAuthoredObstacle[cell])return producer->offsets[cell]==255?0:producer->offsets[cell];
		if(producer && producer->producerOwnedStair[cell] && producer->producerHashes[cell])
			return producer->offsets[cell]==255?0:producer->offsets[cell];
		if((metadata && (metadata->tileRole==RemasterTileRole::StairTop || metadata->tileRole==RemasterTileRole::StairBottom)) ||
			((!metadata || (metadata->tileRole!=RemasterTileRole::Doorway && metadata->tileRole!=RemasterTileRole::Door && metadata->tileRole!=RemasterTileRole::Obstacle)) &&
			 instance.assetGroup!="obstacles" && (instance.assetGroup=="stair_treads" || instance.assetGroup=="stair_rails")))return 0;
		const auto &regions=pixelRoom==context.roomIndex?*currentFrames:*sourceFrames;
		bool ownedFrame=producer && regions.first[cell]>=0;
#ifdef ALTTP_TEST_LINEAR_FRAME_REGIONS
		ownedFrame=false;if(producer)for(const auto &a:producer->doorframeAttachments)if(a.cell==int(cell))ownedFrame=true;
#endif
		// A complete final ROM outer door band can be proved ordinary wall by
		// the classifier (both wall ends plus exact authored full ramp). Its
		// asset remains Door for other usages; do not discard the solved wall
		// base by reapplying that weaker hash-wide role at the consumer.
		const bool ownedWall=producer && producer->producerOwnedWall[cell] && producer->producerHashes[cell];
		if(metadata && metadata->tileRole==RemasterTileRole::Door && !ownedFrame)return 0;
		if(!ownedWall && (ownedFrame || (metadata && metadata->tileRole==RemasterTileRole::Doorway))) {
			// Canonical tilePixel already includes the PPU H/V transform. Split
			// instances by effective frame/opening offset. Opening relief remains
			// authored, but its datum is the connected room-facing landing.
			if(!producer || pixel.tilePixel>=64)return 0;
			int base=255;for(int n=regions.first[cell];n>=0;n=regions.next[n]) {
				const auto &a=producer->doorframeAttachments[n];if(!a.visible)continue;
				const bool framePixel=a.mask & (UINT64_C(1)<<pixel.tilePixel);
				int value=framePixel?a.base:a.openingBase;
				// Outside the authored frame mask the owning surface is opening
				// floor, not leftover frame relief. Compensate locally; never edit
				// the immutable asset or flatten the frame's marked pixels.
				if(!framePixel && value!=255 && metadata) {
					const auto local=S9xRemasterResolveFrameGeometry(*metadata,frame,pixel.instanceId-1);
					if(local.hasHeight)value-=local.height[pixel.tilePixel];
				}
				if(value==255)continue;
				if(base!=255 && base!=value)return 0;base=value;
			}
			return base==255?0:base;
		}
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
		const auto *metadata=instanceMetadata[pixel.instanceId-1];
		if((metadata && (metadata->tileRole==RemasterTileRole::Obstacle || metadata->tileRole==RemasterTileRole::OutOfBounds)) || instance.assetGroup=="obstacles")return nullptr;
		if (instance.source != RemasterSourceType::Background || instance.sourceIndex != backgroundIndex ||
			(metadata && (metadata->tileRole==RemasterTileRole::StairTop || metadata->tileRole==RemasterTileRole::StairBottom)) ||
			instance.assetGroup == "stair_treads" || instance.assetGroup == "stair_rails") return nullptr;
		if (S9xRemasterDungeonRoomForScreenPosition(context, x, y) != context.roomIndex)
		{
			if (!sourceRoomMap) return nullptr;
			const size_t cell=(((context.backgroundScrollY+y)&511)/8)*64+((context.backgroundScrollX+x)&511)/8;
			if(context.retainedOutgoingMaps) {
				const size_t at=(backgroundIndex?0:8192)+cell*2;
				const int word=context.retainedOutgoingMaps[at]|context.retainedOutgoingMaps[at+1]<<8;
				if(instance.tileNumber!=(word&1023) || instance.palette!=((word>>10)&7))return nullptr;
			}
			if (sourceRoomMap->producerHashes[cell] && (sourceRoomMap->producerHashes[cell]!=instance.tileId.hash ||
				sourceRoomMap->producerFlips[cell]!=(instance.hFlip|(instance.vFlip<<1)))) return nullptr;
			// The source's placement-local fallback is part of its retained
			// geometry, not just its base. Return stable per-cell storage.
			sourceBorder = BorderCell{};
			if(sourceRoomMap->producerObstacle[cell])return nullptr;
			sourceBorder.candidate=sourceRoomMap->offsets[cell]!=255 && sourceRoomMap->fallbackDirections[cell];
			sourceBorder.directions=sourceRoomMap->fallbackDirections[cell];
			return sourceBorder.candidate ? &sourceBorder : nullptr;
		}
		const uint32_t roomX = (context.backgroundScrollX + x) & 511;
		const uint32_t roomY = (context.backgroundScrollY + y) & 511;
		if(!currentPlacementMatches(instance,(roomY/8)*64+roomX/8))return nullptr;
		if(map.producerObstacle[(roomY/8)*64+roomX/8])return nullptr;
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
					const int16_t ramp = static_cast<int16_t>(transformedDx > 0 ? xx :
						transformedDx < 0 ? 7 - xx : transformedDy > 0 ? yy : 7 - yy);
					instance.placementHeight[yy * 8 + xx] =
						std::min(instance.placementHeight[yy * 8 + xx], ramp);
				}
			}
	};
	auto floorAt = [&] (const RemasterFramePixel &pixel, uint32_t x, uint32_t y) -> int32_t {
		const auto &instance=frame.tileInstances[pixel.instanceId-1];
		if (instance.source != RemasterSourceType::Background || instance.sourceIndex != backgroundIndex) return 255;
		const auto *metadata=instanceMetadata[pixel.instanceId-1];
		if((metadata && (metadata->tileRole==RemasterTileRole::Obstacle || metadata->tileRole==RemasterTileRole::OutOfBounds)) || instance.assetGroup=="obstacles")return 255;
		const auto room=S9xRemasterDungeonRoomForScreenPosition(context,x,y);
		const auto *selected=room==context.roomIndex ? &map : sourceRoomMap;
		if (!selected || selected->roomIndex != room || !selected->hasRoomGeometry) return 255;
		const size_t cell=(((context.backgroundScrollY+y)&511)/8)*64+((context.backgroundScrollX+x)&511)/8;
		if(room==context.roomIndex && !currentPlacementMatches(instance,cell))return 255;
		if(room!=context.roomIndex && context.retainedOutgoingMaps) {
			const size_t at=(backgroundIndex?0:8192)+cell*2;
			const int word=context.retainedOutgoingMaps[at]|context.retainedOutgoingMaps[at+1]<<8;
			if(instance.tileNumber!=(word&1023) || instance.palette!=((word>>10)&7))return 255;
		}
		if (selected->producerHashes[cell] && (selected->producerHashes[cell]!=instance.tileId.hash ||
			selected->producerFlips[cell]!=(instance.hFlip|(instance.vFlip<<1)))) return 255;
		if(selected->producerObstacle[cell])return 255;
		const auto &regions=room==context.roomIndex?*currentFrames:*sourceFrames;
#ifdef ALTTP_TEST_LINEAR_FRAME_REGIONS
		for(const auto &a:selected->doorframeAttachments)if(a.cell==int(cell))return 255;
#else
		if(regions.first[cell]>=0)return 255;
#endif
		return S9xRemasterMixedFloorBase(*selected,cell);
	};
	auto assignFloorHeight = [&] (RemasterFrameTileInstance &instance, size_t original, int32_t base, int32_t floor) {
		if (floor == 255 || !instanceMetadata[original]) return;
		instance.hasPlacementHeight=true;
		instance.placementHeight=S9xRemasterMixedFloorHeight(instanceMetadata[original]->height,base,floor);
	};
	// Selection is immutable between the observation and splitting passes.
	// Retain exact per-pixel decisions for this invocation only (including door
	// masks/source-room ownership), not a camera/animation-sensitive scene cache.
	using PixelPlacement = RemasterDungeonProjectionCache::Decision;
	// Invocation-local spatial memo, never retained between frames/events. Ordinary
	// placement selection depends on instance + room cell, not tilePixel. Door
	// frame/opening compensation does depend on canonical tilePixel and must stay
	// fresh (including ambiguous/multiple region claims). Preserve traversal and
	// splitting order; this changes neither ownership proof nor authored geometry.
	struct LastPlacement { size_t cell=SIZE_MAX; uint16_t room=0xffff; PixelPlacement decision;
		const RemasterDungeonProjectionCache::Cell *retained=nullptr; uint64_t generation=0; };
#ifndef ALTTP_TEST_REFERENCE_SPATIAL_SELECTION
	std::vector<LastPlacement> lastPlacements(originalCount);
#endif
	std::map<const std::vector<RemasterFramePixel> *,std::vector<PixelPlacement>> pixelPlacements;
	// A retained descriptor is already an immutable decision source. Consume it
	// directly in raster order rather than materializing a frame-sized copy and
	// walking the pixels again. Original instances remain unchanged until the end.
	const bool directProjection=retained!=nullptr;
	auto visit = [&] (std::vector<RemasterFramePixel> &pixels, bool assign,
		std::map<std::tuple<uint32_t, int32_t, uint8_t, int32_t>, uint32_t> &copies) {
		if (pixels.size() != static_cast<size_t>(frame.width) * frame.height)
			return;
		auto &decisions=pixelPlacements[&pixels];
#ifndef ALTTP_TEST_REFERENCE_PLACEMENT_SELECTION
		if(!assign && !directProjection)decisions.resize(pixels.size());
#endif
		for (uint32_t y = 0; y < frame.height; y++)
			for (uint32_t x = 0; x < frame.width; x++)
			{
			RemasterFramePixel &pixel = pixels[static_cast<size_t>(y) * frame.width + x];
			if (!pixel.instanceId || pixel.instanceId > originalCount)
				continue;
			const uint32_t originalId = pixel.instanceId;
			// Terminal semantics precede retained decisions as well as fresh map
			// sampling. A cached border/floor tuple cannot bypass this role guard.
			const auto *terminalMetadata=instanceMetadata[originalId-1];
			if((terminalMetadata && terminalMetadata->tileRole==RemasterTileRole::OutOfBounds) ||
				frame.tileInstances[originalId-1].placementTileRole==RemasterTileRole::OutOfBounds)continue;
			PixelPlacement decision;
#ifndef ALTTP_TEST_REFERENCE_PLACEMENT_SELECTION
			if(assign)decision=decisions[static_cast<size_t>(y)*frame.width+x];
			else
#endif
			{
#ifndef ALTTP_TEST_REFERENCE_SPATIAL_SELECTION
				const auto room=S9xRemasterDungeonRoomForScreenPosition(context,x,y);
				const size_t cell=(((context.backgroundScrollY+y)&511)/8)*64+((context.backgroundScrollX+x)&511)/8;
				const auto &regions=room==context.roomIndex?*currentFrames:*sourceFrames;
				auto &last=lastPlacements[originalId-1];
				const bool pixelDependent=regions.first[cell]>=0;
				if(retained)++retained->dynamicVisits;
				if(last.cell==cell && last.room==room && last.retained && last.generation==last.retained->generation && pixel.tilePixel<64)
					decision=last.retained->decisions[pixel.tilePixel];
				else if(!pixelDependent && !last.retained && last.cell==cell && last.room==room)decision=last.decision;
				else
#endif
				{
				// Only the caller's validated settled solve may retain decisions.
				// Door compensation uses selected/context/occurrence local geometry;
				// compare those actual values, not an asset hash or pixel digest.
				bool reused=false;
#ifndef ALTTP_TEST_REFERENCE_SPATIAL_SELECTION
#ifndef ALTTP_TEST_REFERENCE_STATIC_PROJECTION
				if(retained && room==context.roomIndex && map.hasRoomGeometry && pixel.tilePixel<64) {
					const auto &i=frame.tileInstances[originalId-1];
					if(i.source==RemasterSourceType::Background && i.sourceIndex==backgroundIndex) {
						const auto *m=instanceMetadata[originalId-1];
						std::array<int16_t,64> local={};bool hasLocal=false;
						if(pixelDependent && m) { const auto g=S9xRemasterResolveFrameGeometry(*m,frame,originalId-1);local=g.height;hasLocal=g.hasHeight; }
						auto &slot=retained->cells[cell];
						if(!slot)slot=std::make_unique<RemasterDungeonProjectionCache::Cell>();
						auto &c=*slot;
						if(c.valid && c.tileId==i.tileId && c.tileNumber==i.tileNumber && c.palette==i.palette &&
							c.priority==i.ppuPriority && c.facing==i.geometryFacing && c.h==i.hFlip && c.v==i.vFlip &&
							c.hasMetadata==bool(m) && c.role==(m?m->tileRole:RemasterTileRole::Unknown) &&
							c.group==i.assetGroup && c.localHeight==hasLocal && c.local==local)++retained->hits;
						else {
							++retained->misses;++retained->builds;++c.generation;c.valid=true;c.tileId=i.tileId;c.tileNumber=i.tileNumber;
							c.palette=i.palette;c.priority=i.ppuPriority;c.facing=i.geometryFacing;c.h=i.hFlip;c.v=i.vFlip;
							c.hasMetadata=bool(m);c.role=m?m->tileRole:RemasterTileRole::Unknown;c.group=i.assetGroup;c.localHeight=hasLocal;c.local=local;
							for(int p=0;p<(pixelDependent?64:1);++p) {
								auto sample=pixel;sample.tilePixel=uint8_t(p);auto &d=c.decisions[p];
								d.offset=offsetAt(sample,x,y);d.floor=floorAt(sample,x,y);
								const auto *b=borderAt(sample,x,y);d.border=b!=nullptr;d.directions=b?b->directions:0;
							}
							if(!pixelDependent)c.decisions.fill(c.decisions[0]);
						}
						last.cell=cell;last.room=room;last.retained=&c;last.generation=c.generation;decision=c.decisions[pixel.tilePixel];last.decision=decision;reused=true;
					}
				}
#endif
#endif
				if(!reused) {
#ifndef ALTTP_TEST_REFERENCE_SPATIAL_SELECTION
				last.retained=nullptr;
#endif
				if(auto *t=RemasterLightingTrace::current())t->projectionDecisions++;
				decision.offset=offsetAt(pixel,x,y);decision.floor=floorAt(pixel,x,y);
				const auto *border=borderAt(pixel,x,y);decision.border=border!=nullptr;decision.directions=border?border->directions:0;
#ifndef ALTTP_TEST_REFERENCE_SPATIAL_SELECTION
				if(!pixelDependent){last.cell=cell;last.room=room;last.decision=decision;}
#endif
				}
				}
#ifndef ALTTP_TEST_REFERENCE_PLACEMENT_SELECTION
				if(!directProjection)decisions[static_cast<size_t>(y)*frame.width+x]=decision;
#endif
			}
			const int32_t offset=decision.offset,floor=decision.floor;
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
			const bool border=decision.border;
			const uint8_t borderDirections=decision.directions;
			int32_t &first = firstOffset[originalId - 1];
			if (!assign)
			{
				if (first == 255)
				{
					first = offset;
					firstBorderDirections[originalId - 1] = borderDirections;
					firstFloor[originalId - 1] = floor;
				}
				if(!directProjection)continue;
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
	for(auto &pixels:frame.observedBgPixels)visit(pixels,false,copies);
	if(!directProjection) {
		visit(frame.mainPixels, true, copies);
		visit(frame.subPixels, true, copies);
		for(auto &pixels:frame.observedBgPixels)visit(pixels,true,copies);
	}
	for (size_t i = 0; i < originalCount; i++)
		if (firstOffset[i] != 255)
		{
			frame.tileInstances[i].heightOffset += firstOffset[i];
			if (firstBorderDirections[i] != 255 && firstBorderDirections[i])
				assignBorderHeight(frame.tileInstances[i], firstBorderDirections[i]);
			assignFloorHeight(frame.tileInstances[i],i,firstOffset[i],firstFloor[i]);
		}
	// Preview local arrays use canonical tilePixel indexing. Split by placed
	// cell as well as identity: the same hash can have unrelated authored uses.
	// Existing floor/border geometry and unresolved bases are never overwritten.
	if ((!map.localHeightPreviews.empty() || (sourceRoomMap && !sourceRoomMap->localHeightPreviews.empty())) &&
		frame.width && frame.height && frame.width <= INT_MAX && frame.height <= INT_MAX)
	{
	using Preview = RemasterDungeonHeightMap::LocalHeightPreview;
	// Heap-backed direct room-cell indices: build once, not one linear scan of
	// every preview for every BG pixel. Keep first-claim semantics if duplicated.
	auto previewIndex = std::make_unique<std::array<std::array<const Preview *,4096>,2>>();
	for (int which=0;which<2;which++)
	{
		const auto *selected=which ? sourceRoomMap : &map;
		if (!selected) continue;
		if(!which && retained) {
			for(int cell=0;cell<4096;++cell)if(retained->previews[cell]>=0)
				(*previewIndex)[0][cell]=&map.localHeightPreviews[retained->previews[cell]];
			continue;
		}
		for (const auto &preview:selected->localHeightPreviews)
			if (preview.cell>=0 && preview.cell<4096 && !(*previewIndex)[which][preview.cell])
				(*previewIndex)[which][preview.cell]=&preview;
	}
	std::map<std::pair<uint32_t,int>,uint32_t> previewCopies;
	std::set<RemasterTileContentId> terminalSheets;
	for(const auto &m:frame.assetMetadata)if(m.tileRole==RemasterTileRole::OutOfBounds)terminalSheets.insert(m.tileId);
	for(auto *pixels:{&frame.mainPixels,&frame.subPixels})for(size_t at=0;at<pixels->size();at++) {
		if (at>=static_cast<size_t>(frame.width)*frame.height) break;
		auto &px=(*pixels)[at];if(!px.instanceId || px.instanceId>frame.tileInstances.size())continue;
		const auto &instance=frame.tileInstances[px.instanceId-1];
		if(terminalSheets.count(instance.tileId))continue;
		if(instance.source!=RemasterSourceType::Background || instance.sourceIndex!=backgroundIndex)continue;
		// Bounds above guarantee both screen coordinates fit the signed camera API.
		const int x=static_cast<int>(at%frame.width),y=static_cast<int>(at/frame.width);
		const auto room=S9xRemasterDungeonRoomForScreenPosition(context,x,y);
		const auto *selected=room==context.roomIndex?&map:sourceRoomMap;
		if(!selected || selected->roomIndex!=room)continue;
		const int cell=(((context.backgroundScrollY+y)&511)/8)*64+((context.backgroundScrollX+x)&511)/8;
		const bool generated=selected->producerGeneratedObstacle[cell];
		if(context.retainedOutgoingMaps && room==context.retainedOutgoingRoom) {
			int at=(backgroundIndex?0:8192)+cell*2,word=context.retainedOutgoingMaps[at]|context.retainedOutgoingMaps[at+1]<<8;
			if(instance.tileNumber!=(word&1023) || instance.palette!=((word>>10)&7))continue;
		}
		if(!generated && (instance.assetGroup=="obstacles" || authoredObstacles.count(instance.tileId)))continue;
		if(room==context.roomIndex && !currentPlacementMatches(instance,cell))continue;
		if((selected->producerObstacle[cell] && !generated) || selected->offsets[cell]==255 || selected->fallbackDirections[cell] ||
			selected->producerHashes[cell]!=instance.tileId.hash ||
			selected->producerFlips[cell]!=(instance.hFlip|(instance.vFlip<<1)))continue;
		const auto *preview=(*previewIndex)[room==context.roomIndex ? 0 : 1][cell];
		if(preview) {
			const auto key=std::make_pair(px.instanceId,cell);auto found=previewCopies.find(key);
			if(found==previewCopies.end()) {
				auto copy=instance;copy.hasPlacementHeight=true;copy.placementHeight=preview->height;
				frame.tileInstances.push_back(copy);found=previewCopies.emplace(key,frame.tileInstances.size()).first;
			}
			px.instanceId=found->second;
		}
	}
	}

	// Ground-supported OAM art follows the solved cell beneath the assembled
	if (backgroundIndex != 1) return; // BG1 geometry must not offset actors twice.
	// visible object. This is spatial rather than priority-based: ALTTP reuses
	// both artwork and OAM priorities on different visual floors.
	// LinkOam_Main reserves 12 entries at byte offset 0xe0 or 0x190 for
	// body, equipment and ground shadow. Attest the active block with Link's
	// dedicated body-DMA window, not shared shield/shadow hashes or proximity.
	// Every member then uses the same gameplay-plane support at Link's feet;
	// clipped heads, swords and shadows must not sample different floor cells.
	int linkBlock = -1;
	if (context.verifiedAlttpRom)
		for (const auto &instance : frame.tileInstances)
			if (instance.source == RemasterSourceType::Object &&
				instance.vramAddress >= 0x8000 && instance.vramAddress < 0x8400)
			{
				const int slot = instance.sourceIndex;
				const int block = slot >= 56 && slot < 68 ? 56 :
					slot >= 100 && slot < 112 ? 100 : -1;
				if (block < 0) continue;
				if (linkBlock >= 0 && linkBlock != block) { linkBlock = -1; break; }
				linkBlock = block;
			}
	const int32_t linkOffset = S9xRemasterLinkFloorHeight(context, map);
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
	// The follower producer emits two consecutive 16x16 body OBJs (20/22)
	// in one of four reserved regions, optionally after two effect sprites.
	// Verify the final OAM signature and dedicated DMA children; do not join
	// nearby NPCs or reuse Link's plane/base as assembly ownership evidence.
	std::array<int,128> followerRoot;followerRoot.fill(-1);
	std::array<int,128> followerPlane;followerPlane.fill(-1);
	if(context.verifiedAlttpRom && context.hasTerrainActors)
		for(int start:{48,68,92,112})for(int effects:{0,2}) {
			const int head=start+effects,body=head+1;
			const auto &h=context.terrainOam[head],&b=context.terrainOam[body];
			if(h.name!=0x20 || b.name!=0x22 || h.size!=2 || b.size!=2 ||
				h.x!=b.x || uint8_t(b.y-h.y)<6 || uint8_t(b.y-h.y)>11 ||
				(h.flags&0x3f)!=(b.flags&0x3f))continue;
			bool seenHead=false,seenBody=false,invalid=false;
			for(const auto &i:frame.tileInstances)if(i.source==RemasterSourceType::Object &&
				(i.sourceIndex==head || i.sourceIndex==body)) {
				const auto &o=context.terrainOam[i.sourceIndex];
				const int child=(i.tileNumber&0x1ff)-o.name;
				if((child!=0 && child!=1 && child!=16 && child!=17) ||
					i.vramAddress!=0x8000+(i.tileNumber&0x1ff)*32 ||
					i.palette!=((o.flags>>1)&7) || i.hFlip!=bool(o.flags&0x40) || i.vFlip!=bool(o.flags&0x80))invalid=true;
				if(i.sourceIndex==head)seenHead=true;else seenBody=true;
			}
			if(invalid || !seenHead || !seenBody)continue;
			// Tagalong_Draw emits (layerbits & 0xc)<<2 from its delayed
			// movement history: kTagalongFlags[0]=20,[1]=10. After exact
			// producer attestation this is its OWN contact plane, not a generic
			// priority-to-height heuristic or the current Link plane.
			const int layer=h.flags&0x30;
			if(layer!=0x20 && layer!=0x10)continue;
			followerRoot[head]=followerRoot[body]=body;
			followerPlane[body]=layer==0x20?0:1;
			// Contact belongs to the producer's lower 16x16 body, not the
			// surviving opaque/visible pixels. Transparency and stock occlusion
			// must not move an actor's feet upward onto art behind its torso.
			// OAM X is signed nine-bit; Y240 is the producer's hidden sentinel.
			if(b.y>=240){followerRoot[head]=followerRoot[body]=-1;continue;}
			Bounds &whole=slots[body];whole.x0=b.x>=256?int(b.x)-512:int(b.x);
			whole.x1=whole.x0+15;whole.y0=b.y;whole.y1=int(b.y)+15;
		}
	// Visual overlap does not imply standable support. With complete movement
	// diagnostics, accept only independently solved floor cells in the bottom
	// eight pixels of the contact footprint. Ignore blocked relief, but reject
	// conflicting floor bases instead of picking the highest/nearest floor.
	std::array<int32_t,128> actorBases;actorBases.fill(255);
	std::array<bool,128> actorBaseSampled={};
	auto actorFloor=[&](const Bounds &bounds,int plane)->int32_t {
		int32_t base=255;bool witnessed=false;
		for(int y=std::max(bounds.y0,bounds.y1-7);y<=bounds.y1;y++)
			for(int x=bounds.x0;x<=bounds.x1;x++) {
				const auto room=S9xRemasterDungeonRoomForScreenPosition(context,x,y);
				const auto *selected=room==context.roomIndex?&map:sourceRoomMap;
				if(!selected || selected->roomIndex!=room || !selected->roomDiagnostics)continue;
				const int cell=(((context.backgroundScrollY+y)&511)/8)*64+((context.backgroundScrollX+x)&511)/8;
				const auto &d=selected->roomDiagnostics->cells[(plane&1)*4096+cell];
				if((d.floorId<0 && !d.semanticFloorSupport) || d.wall || !d.planeObserved)continue;
				witnessed=true;
				const int32_t value=selected->hasPlaneOffsets?selected->planeOffsets[plane&1][cell]:d.floorBase;
				if(d.heightConflict || value==255 || (base!=255 && base!=value))return 255;
				base=value;
			}
		return witnessed?base:255;
	};
	// Match the actual 16x16 producer footprint, never a pot artwork hash. An
	// OAM slot is frame-local; duplicate sprite/slot claims are unresolved. State
	// zero/breakage and unsupported variants cannot retain last-frame ownership.
	std::array<int,128> terrainOwner;terrainOwner.fill(-1);
	if(context.hasTerrainActors && !context.submodule && context.module==7) {
		for(int k=0;k<16;k++) {
			const auto &a=context.terrainActors[k];
			// Pot1111 -> lift attribute50 -> kLink_Lift_tab index2 -> EC/C2.
			if(a.kind!=2 || (a.state!=9 && a.state!=10))continue;
			const uint16_t sx=(a.x-context.actorScrollX)&511;
			const uint8_t sy=uint8_t(a.y-context.actorScrollY-a.z);
			int matches=0;
			for(const auto &o:context.terrainOam)if(o.x==sx && o.y==sy && o.name==0x46 && (o.flags&15)==12 && o.size==2)matches++;
			for(int slot=0;slot<128;slot++) {
				const auto &o=context.terrainOam[slot];
				if(o.x==sx && o.y==sy && o.name==0x46 && (o.flags&15)==12 && o.size==2) {
					if(matches!=1 || (a.state==10 && !context.groundedEvidence))terrainOwner[slot]=-2;
					else if(terrainOwner[slot]==-1)terrainOwner[slot]=k;else terrainOwner[slot]=-2;
				}
			}
		}
	}
	for (size_t i = 0; i < objectBounds.size(); i++)
	{
		auto &instance = frame.tileInstances[i];
		if (instance.source == RemasterSourceType::Object && linkBlock >= 0 &&
			instance.sourceIndex >= linkBlock && instance.sourceIndex < linkBlock + 12)
		{
			if (linkOffset != 255) instance.heightOffset += linkOffset;
			continue; // Unknown Link support must not fall back to a part's floor.
		}
		if(instance.source==RemasterSourceType::Object && instance.sourceIndex<128 &&
			followerRoot[instance.sourceIndex]>=0 && map.roomDiagnostics) {
			const int root=followerRoot[instance.sourceIndex];
			if(!actorBaseSampled[root]) {
				actorBases[root]=actorFloor(slots[root],followerPlane[root]);actorBaseSampled[root]=true;
			}
			if(actorBases[root]!=255)instance.heightOffset+=actorBases[root];
			continue;
		}
		if (objectBounds[i].x1 >= 0 && instance.sourceIndex < 128)
		{
			const int slot=frame.tileInstances[i].sourceIndex,owner=terrainOwner[slot];
			if(owner==-2)continue; // ambiguous producer, no screen-bottom fallback
			if(owner>=0) {
				const auto &a=context.terrainActors[owner];
				const auto &instance=frame.tileInstances[i];
				const int tile=instance.tileNumber&0x1ff;
				// The producer writes one large OBJ, whose four canonical children
				// are46/47/56/57. Reject replaced OAM observations, not nearby sprites.
				const auto &o=context.terrainOam[slot];
				if((tile!=0x46 && tile!=0x47 && tile!=0x56 && tile!=0x57) ||
					instance.hFlip!=bool(o.flags&0x40) || instance.vFlip!=bool(o.flags&0x80) || instance.palette!=6)continue;
				const int x=a.state==10?context.linkX+8:a.x+8;
				// Sprite_CheckTileProperty(k,0x68): table index52, x+8,y+12.
				// Carried objects instead belong to Link's grounded feet.
				const int y=a.state==10?context.linkY+16:a.y+12;
				const int plane=a.state==10?context.linkPlane:a.plane;
				const int cell=((y&511)/8)*64+(x&511)/8;
				const int base=map.hasPlaneOffsets?map.planeOffsets[plane&1][cell]:255;
				if(base!=255) {
					// Carried Y already subtracts Link z+1 and animation lift in
					// SpriteModule_Carried. Recover that displacement exactly once;
					// sprite_z is the independent held/throw displacement.
					const int lift=a.state==10?int16_t(uint16_t(context.linkY+8-a.y)):0;
					frame.tileInstances[i].heightOffset+=base+a.z+lift;
				}
				continue;
			}
			const Bounds &bounds = slots[followerRoot[slot]>=0?followerRoot[slot]:slot];
			if(map.roomDiagnostics) {
				const int supportSlot=followerRoot[slot]>=0?followerRoot[slot]:slot;
				if(!actorBaseSampled[supportSlot]) {
					actorBases[supportSlot]=actorFloor(bounds,context.linkPlane);actorBaseSampled[supportSlot]=true;
				}
				const int32_t base=actorBases[supportSlot];
				if(base!=255)frame.tileInstances[i].heightOffset+=base;
				continue;
			}
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
	std::set<RemasterTileContentId> terminalSheets;
	for(const auto &m:frame.assetMetadata)if(m.tileRole==RemasterTileRole::OutOfBounds)terminalSheets.insert(m.tileId);
	auto upper = [&] (const RemasterFramePixel &pixel, uint32_t x, uint32_t y) {
		if (!pixel.instanceId || pixel.instanceId > originalCount)
			return false;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		if(terminalSheets.count(instance.tileId) || instance.placementTileRole==RemasterTileRole::OutOfBounds)return false;
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
	if (frame.heldLampLightIndex < frame.lights.size())
		frame.lights[frame.heldLampLightIndex].intensity = profile.linkLampBrightness;
	const auto areaSettings = S9xRemasterAreaSceneSettings(profile, frame.area);
	frame.reflectanceBoost = areaSettings.reflectanceBoost;
	frame.originalSceneContribution = areaSettings.originalSceneContribution;
	if (frame.area.valid()) frame.directionalLight = S9xRemasterAreaLight(profile, frame.area);
	frame.heightPreviewMultiplier = profile.heightPreviewMultiplier;
	frame.samplesPerFrame = profile.samplesPerFrame;
	frame.representativeEmitters = static_cast<uint8_t>(areaSettings.representativeEmitters);
	frame.offscreenRepresentativeEmitters = areaSettings.offscreenRepresentativeEmitters;
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
		metadata.emissionColorMask = source.emissionColorMask;
		metadata.emissionDepth = source.emissionDepth;
		metadata.emissionOmnidirectional = source.emissionOmnidirectional;
		metadata.combineEmitters = source.combineEmitters;
		metadata.tileRole = source.tileSemanticContextDependent ? RemasterTileRole::Unknown : source.tileRole;
		// Door authoring is preserved even when contextual use is unresolved. It is
		// not consumed by placement solving or lighting geometry.
		if (source.tileRole == RemasterTileRole::Doorway || source.tileRole == RemasterTileRole::Door ||
			source.tileRole == RemasterTileRole::OutOfBounds) metadata.tileRole = source.tileRole;
		metadata.doorframeMask = source.doorframeMask;
		metadata.hasDoorframeMask = source.hasDoorframeMask;
		metadata.doorwaySupport = source.doorwaySupport;
		metadata.doorwaySupportDirection = source.doorwaySupportDirection;
		metadata.hasMaterialSelectors = source.hasMaterialSelectors;
		metadata.hasOcclusion = source.hasOcclusion;
		metadata.hasHeight = source.hasHeight;
		metadata.hasNormals = source.hasNormals;
		metadata.hasEmission = source.hasEmission;
		metadata.directLightingOppositeFacing = source.directLightingOppositeFacing;
		metadata.normalIgnoreTileTransform = source.normalIgnoreTileTransform;
		metadata.geometryByFacing = source.geometryByFacing;
		metadata.facingGeometry = source.facingGeometry;
		metadata.geometryByFlip = source.geometryByFlip;
		metadata.flipGeometry = source.flipGeometry;
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

	// Matching depends only on this complete context, not placement/flip or the
	// instance's capture-animation name. Keep the authoritative matcher (including
	// misses and ambiguity) and its profile-owned pointers local to this call so
	// profile edits/replacement can never leave stale results in a later frame.
	using MatchKey = std::tuple<RemasterTileContentId, RemasterSourceType, uint8_t, uint8_t>;
	std::map<MatchKey, RemasterProfileMatch> matches;
	for (RemasterFrameTileInstance &instance : frame.tileInstances)
	{
		RemasterProfileMatchContext context;
		context.tileId = instance.tileId;
		context.source = instance.source;
		context.sourceIndex = instance.sourceIndex;
		context.palette = instance.palette;
		const MatchKey key(context.tileId, context.source, context.sourceIndex, context.palette);
		auto found = matches.find(key);
		if (found == matches.end())
			found = matches.emplace(key, S9xRemasterMatchProfile(profile, context)).first;
		const RemasterProfileMatch &match = found->second;
		instance.matchStatus = match.status;
		instance.ruleLine = match.rule ? static_cast<uint32_t>(match.rule->line) : 0;
		if (!capturedAnimationNames.count(instance.assetGroup))
			instance.assetGroup = match.assetGroup ? match.assetGroup->name : std::string();
		instance.material = match.material ? match.material->name : std::string();
	}
	S9xRemasterApplyContextBindings(profile, frame);
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
	std::set<RemasterTileContentId> capturedPaletteColors;
	for (const auto &artwork : capture.representativeFrame.paletteArtworkColors)
		capturedPaletteColors.insert(artwork.tileId);
	for (const auto &artwork : frame.paletteArtworkColors)
		if (capturedPaletteColors.insert(artwork.tileId).second)
			capture.representativeFrame.paletteArtworkColors.push_back(artwork);
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

		bool ReadHeight (int16_t &value, uint32_t schema)
		{
			if (schema < 35) { uint8_t byte; if (!ReadU8(byte)) return false; value = byte; return true; }
			uint16_t bits; if (!ReadU16(bits)) return false;
			value = static_cast<int16_t>(bits <= 32767 ? int(bits) : int(bits) - 65536);
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
	uint8_t retiredEmitterPatchSize = 1; // v22+ wire slot; no longer affects lighting.
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
		(result.schemaVersion >= 22 && !input.ReadU8(retiredEmitterPatchSize)) ||
		(result.schemaVersion >= 23 && !input.ReadU8(result.representativeEmitters)) ||
		!input.ReadU32(assetCount) ||
		(result.schemaVersion >= 13 && !input.ReadU32(artworkColorCount)) ||
		(result.schemaVersion >= 2 && !input.ReadU32(groupCount)) ||
		(result.schemaVersion >= 3 && !input.ReadU32(metadataCount)) || !input.ReadU32(materialCount) ||
		!input.ReadU32(instanceCount) || !input.ReadU32(lightCount))
		return false;
	if (result.representativeEmitters > 8 || retiredEmitterPatchSize < 1 || retiredEmitterPatchSize > 5 || !std::isfinite(result.lightingCoordinateScale) || result.lightingCoordinateScale <= 0.0f)
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
			for (auto &h : metadata.height) if (!input.ReadHeight(h, result.schemaVersion)) return false;
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
			if (result.schemaVersion < 39 && metadata.tileRole == RemasterTileRole::Doorway) return false;
			if (result.schemaVersion < 29 && metadata.tileRole == RemasterTileRole::Door) return false;
			if (result.schemaVersion < 39 && metadata.tileRole == RemasterTileRole::Door) metadata.tileRole = RemasterTileRole::Doorway;
		}
		if (result.schemaVersion >= 27)
		{
			uint8_t omnidirectional;
			if (!input.ReadU8(omnidirectional) || omnidirectional > 1) return false;
			metadata.emissionOmnidirectional = omnidirectional != 0;
		}
		if (result.schemaVersion >= 29 && metadata.tileRole == RemasterTileRole::Doorway)
		{
			uint8_t present;
			if (!input.ReadU8(present) || present > 1) return false;
			metadata.hasDoorframeMask = present != 0;
			if (present && !input.ReadU64(metadata.doorframeMask)) return false;
		}
		if (result.schemaVersion >= 39) {
			uint8_t support, direction;
			if (!input.ReadU8(support) || support > 1 || !input.ReadU8(direction) || direction > 4) return false;
			metadata.doorwaySupport = support != 0;
			metadata.doorwaySupportDirection = static_cast<RemasterSupportDirection>(direction);
			if ((support && (metadata.tileRole != RemasterTileRole::Doorway || !direction)) || (!support && direction)) return false;
		}
		if (result.schemaVersion >= 31)
		{
			uint8_t fixed;
			if (!input.ReadU8(fixed) || fixed > 1) return false;
			metadata.normalIgnoreTileTransform = fixed != 0;
		}
		for (unsigned variant = 0; variant < (result.schemaVersion >= 33 ? 2u : result.schemaVersion >= 32 ? 1u : 0u); variant++)
		{
			uint8_t enabled;
			if (!input.ReadU8(enabled) || enabled > 1) return false;
			(variant ? metadata.geometryByFlip : metadata.geometryByFacing) = enabled != 0;
			for (auto &g : variant ? metadata.flipGeometry : metadata.facingGeometry)
			{
				uint8_t h, n, sampling;
				if (!input.ReadU8(h) || h > 1 || !input.ReadU8(n) || n > 1 || !input.ReadU8(sampling) || sampling > 1 || (!h && sampling)) return false;
				g.hasHeight = h; g.hasNormals = n; g.heightSampling = static_cast<RemasterHeightSampling>(sampling);
				if (h) for (auto &v : g.height) if (!input.ReadHeight(v, result.schemaVersion)) return false;
				if (n) for (auto &v : g.normalXyz) if (!input.ReadU8(v)) return false;
			}
		}
	}
	if (result.schemaVersion >= 38)
		for (auto &metadata : result.assetMetadata) if (!input.ReadU64(metadata.emissionColorMask)) return false;
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
		if (result.schemaVersion >= 32 && (!input.ReadU8(instance.geometryFacing) || instance.geometryFacing > 4)) return false;
		if (result.schemaVersion >= 20)
		{
			uint8_t hasPlacementHeight = 0;
			if (!input.ReadU8(hasPlacementHeight) || hasPlacementHeight > 1) return false;
			instance.hasPlacementHeight = hasPlacementHeight != 0;
			if (instance.hasPlacementHeight)
			{
				for (auto &h : instance.placementHeight) if (!input.ReadHeight(h, result.schemaVersion)) return false;
			}
		}
		instance.matchStatus = static_cast<RemasterProfileMatchStatus>(matchStatus);
		if (result.schemaVersion >= 30)
		{
			uint8_t present;
			if (!input.ReadU8(present) || present > 1) return false;
			instance.hasPaletteArtwork = present != 0;
			if (present)
				for (auto &color : instance.paletteRgb555)
					if (!input.ReadU16(color) || color > 0x7fff) return false;
		}
		if (result.schemaVersion >= 36)
		{
			uint8_t status, role, flags;
			if (!input.ReadU8(status) || !input.ReadU8(role) || !input.ReadU8(flags) || (flags & ~7u)) return false;
			instance.geometryStatus = static_cast<RemasterOccurrenceGeometryStatus>(status);
			instance.placementTileRole = static_cast<RemasterTileRole>(role);
			instance.hasPlacementNormals = (flags & 1) != 0;
			instance.hasPlacementOcclusion = (flags & 2) != 0;
			instance.placementNormalIgnoreTileTransform = (flags & 4) != 0;
			if (!S9xRemasterValidOccurrenceGeometry(instance)) return false;
			if (instance.hasPlacementNormals) for (auto &v : instance.placementNormalXyz) if (!input.ReadU8(v)) return false;
			if (instance.hasPlacementOcclusion) for (auto &v : instance.placementOcclusion) if (!input.ReadU8(v)) return false;
		}
		if(result.schemaVersion>=40) {
			uint8_t status,published;uint32_t cell,component;
			auto &v=instance.visualClaim;
			if(!input.ReadU8(status) || !input.ReadU64(v.mask) || !input.ReadU32(cell) ||
				!input.ReadU32(component) || !input.ReadU8(published) || published>1)return false;
			v.status=static_cast<RemasterVisualClaimStatus>(status);v.cell=static_cast<int32_t>(cell);
			v.component=static_cast<int32_t>(component);v.published=published!=0;
			if(!S9xRemasterValidOccurrenceGeometry(instance))return false;
		}
	}
	result.lights.resize(lightCount);
	for (RemasterFrameLight &light : result.lights)
		if (!input.ReadFloat(light.x) || !input.ReadFloat(light.y) || !input.ReadFloat(light.z) ||
			!input.ReadFloat(light.radius) || !input.ReadFloat(light.red) || !input.ReadFloat(light.green) ||
			!input.ReadFloat(light.blue) || !input.ReadFloat(light.intensity))
			return false;
	if (result.schemaVersion >= 30)
	{
		uint32_t count;
		if (!input.ReadU32(count) || count > (bytes.size() - input.offset) / 146) return false;
		result.paletteArtworkColors.resize(count);
		std::set<RemasterTileContentId> ids;
		for (auto &artwork : result.paletteArtworkColors)
		{
			if (!input.ReadTileId(artwork.tileId) || !RemasterFrameSerialization::ValidTileId(artwork.tileId) ||
				!ids.insert(artwork.tileId).second || !input.ReadU64(artwork.visiblePixels)) return false;
			for (auto &color : artwork.rgb555)
				if (!input.ReadU16(color) || color > 0x7fff) return false;
		}
	}
	bool hasArea = false;
	bool hasContextLocations = false;
	bool hasLightPresentation = false;
	bool hasOffscreenRepresentatives = false;
	while (result.schemaVersion >= 28 && input.offset < bytes.size())
	{
		uint32_t tag;
		if (!input.ReadU32(tag)) return false;
		if (tag == 0x3152454fu) { // OER1: effective offscreen representative count
			uint8_t count;
			if (hasOffscreenRepresentatives || !input.ReadU8(count) || count > 8) return false;
			hasOffscreenRepresentatives = true; result.offscreenRepresentativeEmitters = count;
			continue;
		}
		if (tag == 0x3150564cu) // LVP1: sparse hidden sphere indices
		{
			uint32_t count;
			if (hasLightPresentation || !input.ReadU32(count) || !count || count > result.lights.size()) return false;
			hasLightPresentation = true;
			for (uint32_t i = 0; i < count; i++)
			{
				uint32_t index;
				if (!input.ReadU32(index) || index >= result.lights.size() || result.lights[index].hideSource) return false;
				result.lights[index].hideSource = true;
			}
			continue;
		}
		if (tag == 0x314c5443u) { // CTL1: sparse stable background locations
			uint32_t count;
			if (hasContextLocations || result.schemaVersion<37 || !input.ReadU32(count) || count>result.tileInstances.size()) return false;
			hasContextLocations=true;
			for(uint32_t i=0;i<count;i++) {
				uint32_t id;RemasterContextLocation at;
				if(!input.ReadU32(id)||!id||id>result.tileInstances.size()||!input.ReadU16(at.area)||at.area>321||
					!input.ReadU8(at.background)||at.background>1||!input.ReadU16(at.x)||!input.ReadU16(at.y)||
					!result.contextLocations.emplace(id,at).second)return false;
			}
			continue;
		}
		if (tag == 0x31415241u) // ARA1: pinned area and parallel-ray lighting
		{
			uint8_t dark, enabled;
			if (hasArea || !input.ReadU16(result.area.id) || !result.area.valid() ||
				!input.ReadU8(dark) || dark > 1 || !input.ReadU8(enabled) || enabled > 1) return false;
			hasArea = true; result.area.romDark = dark; result.directionalLight.enabled = enabled;
			for (auto &v : result.directionalLight.direction) if (!input.ReadFloat(v)) return false;
			for (auto &v : result.directionalLight.color) if (!input.ReadFloat(v)) return false;
			if (!input.ReadFloat(result.directionalLight.brightness) || !S9xRemasterValidDirectionalLight(result.directionalLight)) return false;
			continue;
		}
		if (tag == 0x31534548u || tag == 0x32534548u) // HES1 in-viewport / HES2 signed room sources
		{
			uint32_t count;
			if (!result.hiddenEmitters.empty() || !input.ReadU32(count) || !count ||
				count > (tag==0x31534548u ? size_t(result.width)*result.height*2 : size_t(8192)*64) || count > (bytes.size()-input.offset)/33) return false;
			result.hiddenEmitters.resize(count);
			for (auto &e : result.hiddenEmitters)
			{
				uint8_t omni;
				if (!input.ReadFloat(e.x) || !input.ReadFloat(e.y) || !input.ReadFloat(e.z) ||
					!input.ReadFloat(e.depth) || e.depth < 0 ||
					(tag==0x31534548u ? (e.x < 0 || e.y < 0 || e.x >= result.width || e.y >= result.height) :
					(e.x < -511 || e.y < -511 || e.x > 511.5f || e.y > 511.5f))) return false;
				for (auto &n : e.normal) if (!input.ReadFloat(n)) return false;
				for (auto &v : e.emission) if (!input.ReadU8(v)) return false;
				if (!input.ReadU8(omni) || omni > 1 || !e.emission[3] ||
					!(e.emission[0] || e.emission[1] || e.emission[2])) return false;
				e.omnidirectional = omni;
			}
			continue;
		}
		if(tag==0x314f4742u) { // BGO1: independent observed BG ink/depth
			for(unsigned n=0;n<4;n++) {
				uint32_t count;if(!input.ReadU32(count) || !result.observedBgPixels[n].empty() ||
					(count && count!=result.mainPixels.size()) || count>(bytes.size()-input.offset)/10)return false;
				result.observedBgPixels[n].resize(count);result.observedBgDepth[n].resize(count);
				for(size_t k=0;k<count;k++) {
					auto &p=result.observedBgPixels[n][k];
					if(!input.ReadU32(p.owner)||!input.ReadU32(p.instanceId)||!input.ReadU8(p.tilePixel)||!input.ReadU8(result.observedBgDepth[n][k])||
						p.instanceId>result.tileInstances.size()||(p.instanceId&&p.tilePixel>=64))return false;
				}
			}
			continue;
		}
		if (tag != 0x314e4353u || !result.scenePixels.empty()) return false; // SCN1
		const size_t count = result.originalRgb555.size();
		if (bytes.size() - input.offset < count * 11) return false;
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
	S9xRemasterRestoreSceneSubscreenWalls(result);
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

// ALTTP's outdoor color-zero backdrop is visible ground between transparent
// foliage pixels. It has no decoded tile to author or select. Keep this a
// provisional zero-base surface, never authored object/terrain coverage.
inline bool S9xRemasterFrameBackdropGroundAt (
	const RemasterFrame &frame, size_t offset, bool useScene = false)
{
	if ((frame.area.id != 320 && frame.area.id != 321) ||
		frame.profileRomSha256 != "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb")
		return false;
	const auto &pixels = useScene ? frame.scenePixels : frame.mainPixels;
	if (offset >= pixels.size()) return false;
	const auto &pixel = pixels[offset];
	if (pixel.instanceId || static_cast<uint8_t>(pixel.owner >> 24) !=
		static_cast<uint8_t>(RemasterSourceType::Backdrop)) return false;
	// A real subscreen contributor is not backdrop ground, including captures
	// where the independent scene plane cannot identify its composition.
	return offset >= frame.subPixels.size() || !frame.subPixels[offset].instanceId;
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

// Presentation selection must follow the same main/subscreen precedence as
// picking, not the independent lighting scene (which can expose hidden tiles).
inline std::vector<uint32_t> S9xRemasterFrameVisibleOccurrences (
	const RemasterFrame &frame, const RemasterTileContentId &tileId)
{
	std::vector<uint32_t> occurrences;
	for (uint32_t y = 0; y < frame.height; y++)
		for (uint32_t x = 0; x < frame.width; x++)
		{
			const auto *instance = S9xRemasterFrameVisibleInstanceAt(frame, x, y);
			if (instance && instance->tileId == tileId)
				occurrences.push_back(y * frame.width + x);
		}
	return occurrences;
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

inline std::array<uint8_t,4> S9xRemasterEmissionSample(const RemasterFrameAssetMetadata &metadata,
	unsigned pixel, uint16_t artworkRgb555)
{
	std::array<uint8_t,4> result;
	std::copy_n(metadata.emissionRgba.data()+pixel*4,4,result.begin());
	if (!(metadata.emissionColorMask & (UINT64_C(1)<<pixel)))
		for(unsigned c=0;c<3;c++) result[c]=uint8_t(((((artworkRgb555 >> (10-c*5))&31)*527)+23)>>6);
	return result;
}

inline bool S9xRemasterFramePaletteArtwork (const RemasterFrame &frame,
	const RemasterTileContentId &tileId, uint32_t preferredInstance, RemasterFrameArtworkColors &artwork)
{
	const RemasterFrameTileInstance *instance = nullptr;
	if (preferredInstance && preferredInstance <= frame.tileInstances.size())
	{
		const auto &picked = frame.tileInstances[preferredInstance - 1];
		if (picked.tileId == tileId && picked.hasPaletteArtwork) instance = &picked;
	}
	if (!instance)
		for (const auto &candidate : frame.tileInstances)
			if (candidate.tileId == tileId && candidate.hasPaletteArtwork) { instance = &candidate; break; }
	if (instance)
	{
		const auto *asset = S9xRemasterFrameAssetForTile(frame, tileId);
		if (!asset) return false;
		artwork = {};
		artwork.tileId = tileId;
		artwork.rgb555 = instance->paletteRgb555;
		for (size_t pixel = 0; pixel < 64; pixel++)
			if (asset->indices[pixel]) artwork.visiblePixels |= UINT64_C(1) << pixel;
		return true;
	}
	for (const auto &candidate : frame.paletteArtworkColors)
		if (candidate.tileId == tileId) { artwork = candidate; return true; }
	return false;
}

// Placement-only correction of an authored flat cap that became an internal
// face. No asset normal is edited. Require an exact clipped unit diagonal over
// the ENTIRE placed tile; floor clamps, corners and arbitrary overrides reject.
inline uint64_t S9xRemasterContinuedDiagonalNormalMask(const RemasterFrameTileInstance &instance,
	const RemasterFrameAssetMetadata *metadata)
{
	if(instance.geometryStatus != RemasterOccurrenceGeometryStatus::Inherit || !instance.hasPlacementHeight || !metadata || !metadata->hasNormals || !metadata->hasHeight ||
		metadata->tileRole==RemasterTileRole::Obstacle || instance.assetGroup=="obstacles")return 0;
	const auto &h=instance.placementHeight;const int lo=*std::min_element(h.begin(),h.end()),hi=*std::max_element(h.begin(),h.end());
	int intercept=-1;
	for(int y=0;y<7;y++)for(int x=0;x<7;x++)if(h[y*8+x]-h[y*8+x+1]==1 && h[y*8+x]-h[(y+1)*8+x]==1)
		intercept=h[y*8+x]+x+y;
	if(intercept<0)return 0;
	for(int y=0;y<8;y++)for(int x=0;x<8;x++)if(h[y*8+x]!=std::max(lo,std::min(hi,intercept-x-y)))return 0;
	uint64_t mask=0;
	for(int k=0;k<64;k++)if(h[k]>lo && h[k]<hi && h[k]!=metadata->height[k] && metadata->height[k]>0 &&
		metadata->normalXyz[k*3]==128 && metadata->normalXyz[k*3+1]==128 && metadata->normalXyz[k*3+2]==255)
		mask|=UINT64_C(1)<<k;
	return mask;
}

inline std::pair<int, int> S9xRemasterFrameHeightRange (const RemasterFrame &frame)
{
	RemasterFrameChannelResolver channels(frame);
	std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> byTile;
	for (const RemasterFrameAssetMetadata &metadata : frame.assetMetadata)
		byTile.emplace(metadata.tileId, &metadata);
	std::vector<RemasterFrameAssetMetadata> effective(frame.tileInstances.size());
	std::vector<const RemasterFrameAssetMetadata *> byInstance(frame.tileInstances.size(), nullptr);
	for (size_t i = 0; i < frame.tileInstances.size(); i++)
	{
		const auto found = byTile.find(frame.tileInstances[i].tileId);
		effective[i] = S9xRemasterResolveFrameGeometry(found != byTile.end() ? *found->second : RemasterFrameAssetMetadata{}, frame, i);
		byInstance[i] = &effective[i];
	}
	int minimum = INT_MAX, maximum = INT_MIN;
	bool foundHeight = false;
	for (size_t at=0;at<frame.mainPixels.size();at++)
	{
		const auto pixel=channels.select(frame,at,!frame.scenePixels.empty()).donors[size_t(RemasterMetadataChannel::Height)];
		if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size() || pixel.tilePixel >= 64)
			continue;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		const RemasterFrameAssetMetadata *metadata = byInstance[pixel.instanceId - 1];
		if (S9xRemasterInstanceGeometryUnresolved(instance)) continue;
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
	return minimum >= -65535 && minimum <= 65534 && maximum >= -65534 && maximum <= 65535 &&
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
	RemasterFrameChannelResolver channels(frame);
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
	std::vector<Accumulator> accumulators(frame.tileInstances.size());
	for (size_t offset = 0; offset < frame.mainPixels.size(); offset++)
	{
		const auto selected=channels.select(frame,offset,!frame.scenePixels.empty());
		const auto pixel=selected.donors[size_t(RemasterMetadataChannel::Emission)];
		if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size())
			continue;
		Accumulator &accumulator = accumulators[pixel.instanceId - 1];
		if (pixel.tilePixel >= 64)
			continue;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		const RemasterFrameAssetMetadata *metadata = channels.metadata[pixel.instanceId-1];
		if (!metadata || !metadata->hasEmission)
			continue;
		const size_t emissionOffset = pixel.tilePixel * 4;
		const auto emission = S9xRemasterEmissionSample(*metadata, pixel.tilePixel,
			instance.hasPaletteArtwork ? instance.paletteRgb555[pixel.tilePixel] : frame.originalRgb555[offset]);
		const float intensity = metadata->emissionRgba[emissionOffset + 3] / 25.0f;
		if (intensity <= 0.0f)
			continue;
		accumulator.x += (static_cast<float>(offset % frame.width) + 0.5f) * intensity;
		accumulator.y += (static_cast<float>(offset / frame.width) + 0.5f) * intensity;
		// Eight standard-intensity pixels produce a 1x light; every pixel remains additive.
		// Match the shader's emission decode before summing differently colored pixels.
		accumulator.red += std::pow(emission[0] / 255.0f, 2.2f) * intensity / 8.0f;
		accumulator.green += std::pow(emission[1] / 255.0f, 2.2f) * intensity / 8.0f;
		accumulator.blue += std::pow(emission[2] / 255.0f, 2.2f) * intensity / 8.0f;
		const auto hp=selected.donors[size_t(RemasterMetadataChannel::Height)];
		const int height=hp.instanceId ? frame.tileInstances[hp.instanceId-1].heightOffset+
			channels.metadata[hp.instanceId-1]->height[hp.tilePixel] : 0;
		accumulator.z += height / 255.0f * intensity;
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
			instance.normalYaw < -1 || instance.normalYaw > 1 || !S9xRemasterValidOccurrenceGeometry(instance))
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
	RemasterFrameSerialization::U8(bytes, 1); // Retired v22+ patch-size slot, fixed to per-pixel.
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
			for (int16_t h : metadata.height) RemasterFrameSerialization::U16(bytes, static_cast<uint16_t>(h));
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
		if (metadata.hasDoorframeMask && metadata.tileRole != RemasterTileRole::Doorway) return false;
		if (metadata.tileRole == RemasterTileRole::Doorway)
		{
			RemasterFrameSerialization::U8(bytes, metadata.hasDoorframeMask ? 1 : 0);
			if (metadata.hasDoorframeMask) RemasterFrameSerialization::U64(bytes, metadata.doorframeMask);
		}
		const auto supportDirection = static_cast<uint8_t>(metadata.doorwaySupportDirection);
		if (supportDirection > 4 || (metadata.doorwaySupport && (metadata.tileRole != RemasterTileRole::Doorway || !supportDirection)) || (!metadata.doorwaySupport && supportDirection)) return false;
		RemasterFrameSerialization::U8(bytes, metadata.doorwaySupport ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, supportDirection);
		RemasterFrameSerialization::U8(bytes, metadata.normalIgnoreTileTransform ? 1 : 0);
		for (unsigned variant = 0; variant < 2; variant++)
		{
			RemasterFrameSerialization::U8(bytes, (variant ? metadata.geometryByFlip : metadata.geometryByFacing) ? 1 : 0);
			for (const auto &g : variant ? metadata.flipGeometry : metadata.facingGeometry)
			{
				RemasterFrameSerialization::U8(bytes, g.hasHeight ? 1 : 0);
				RemasterFrameSerialization::U8(bytes, g.hasNormals ? 1 : 0);
				if (g.heightSampling > RemasterHeightSampling::Linear || (!g.hasHeight && g.heightSampling != RemasterHeightSampling::Nearest)) return false;
				RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(g.heightSampling));
				if (g.hasHeight) for (int16_t h : g.height) RemasterFrameSerialization::U16(bytes, static_cast<uint16_t>(h));
				if (g.hasNormals) bytes.insert(bytes.end(), g.normalXyz.begin(), g.normalXyz.end());
			}
		}
	}
	for (const auto &metadata : frame.assetMetadata) RemasterFrameSerialization::U64(bytes, metadata.emissionColorMask);
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
		if (instance.geometryFacing > 4) return false;
		RemasterFrameSerialization::U8(bytes, instance.geometryFacing);
		RemasterFrameSerialization::U8(bytes, instance.hasPlacementHeight ? 1 : 0);
		if (instance.hasPlacementHeight)
			for (int16_t h : instance.placementHeight) RemasterFrameSerialization::U16(bytes, static_cast<uint16_t>(h));
		RemasterFrameSerialization::U8(bytes, instance.hasPaletteArtwork ? 1 : 0);
		if (instance.hasPaletteArtwork)
			for (uint16_t color : instance.paletteRgb555)
			{
				if (color > 0x7fff) return false;
				RemasterFrameSerialization::U16(bytes, color);
			}
		RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(instance.geometryStatus));
		RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(instance.placementTileRole));
		RemasterFrameSerialization::U8(bytes, (instance.hasPlacementNormals ? 1 : 0) |
			(instance.hasPlacementOcclusion ? 2 : 0) | (instance.placementNormalIgnoreTileTransform ? 4 : 0));
		if (instance.hasPlacementNormals) bytes.insert(bytes.end(), instance.placementNormalXyz.begin(), instance.placementNormalXyz.end());
		if (instance.hasPlacementOcclusion) bytes.insert(bytes.end(), instance.placementOcclusion.begin(), instance.placementOcclusion.end());
		RemasterFrameSerialization::U8(bytes,static_cast<uint8_t>(instance.visualClaim.status));
		RemasterFrameSerialization::U64(bytes,instance.visualClaim.mask);
		RemasterFrameSerialization::U32(bytes,static_cast<uint32_t>(instance.visualClaim.cell));
		RemasterFrameSerialization::U32(bytes,static_cast<uint32_t>(instance.visualClaim.component));
		RemasterFrameSerialization::U8(bytes,instance.visualClaim.published?1:0);
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
	if (!RemasterFrameSerialization::Size(bytes, frame.paletteArtworkColors.size())) return false;
	std::set<RemasterTileContentId> paletteIds;
	for (const auto &artwork : frame.paletteArtworkColors)
	{
		if (!RemasterFrameSerialization::ValidTileId(artwork.tileId) || !paletteIds.insert(artwork.tileId).second) return false;
		RemasterFrameSerialization::TileId(bytes, artwork.tileId);
		RemasterFrameSerialization::U64(bytes, artwork.visiblePixels);
		for (uint16_t color : artwork.rgb555)
		{
			if (color > 0x7fff) return false;
			RemasterFrameSerialization::U16(bytes, color);
		}
	}
	if (frame.area.valid())
	{
		if (!S9xRemasterValidDirectionalLight(frame.directionalLight)) return false;
		RemasterFrameSerialization::U32(bytes, 0x31415241u);
		RemasterFrameSerialization::U16(bytes, frame.area.id);
		RemasterFrameSerialization::U8(bytes, frame.area.romDark);
		RemasterFrameSerialization::U8(bytes, frame.directionalLight.enabled);
		for (float v : frame.directionalLight.direction) RemasterFrameSerialization::Float(bytes, v);
		for (float v : frame.directionalLight.color) RemasterFrameSerialization::Float(bytes, v);
		RemasterFrameSerialization::Float(bytes, frame.directionalLight.brightness);
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
	if (!frame.hiddenEmitters.empty())
	{
		if (frame.hiddenEmitters.size() > size_t(8192)*64) return false;
		const bool signedSources=std::any_of(frame.hiddenEmitters.begin(),frame.hiddenEmitters.end(),[&](const auto &e){return e.x<0 || e.y<0 || e.x>=frame.width || e.y>=frame.height;});
		if(!signedSources && frame.hiddenEmitters.size()>pixelCount*2)return false;
		RemasterFrameSerialization::U32(bytes, signedSources ? 0x32534548u : 0x31534548u);
		if (!RemasterFrameSerialization::Size(bytes, frame.hiddenEmitters.size())) return false;
		for (const auto &e : frame.hiddenEmitters)
		{
			if (!std::isfinite(e.x) || !std::isfinite(e.y) || !std::isfinite(e.z) ||
				!std::isfinite(e.depth) || e.depth < 0 || e.x < -511 || e.y < -511 || e.x > 511.5f ||
				e.y > 511.5f || !e.emission[3] || !(e.emission[0] || e.emission[1] || e.emission[2])) return false;
			for (float v : {e.x,e.y,e.z,e.depth}) RemasterFrameSerialization::Float(bytes,v);
			for (float v : e.normal) { if (!std::isfinite(v)) return false; RemasterFrameSerialization::Float(bytes,v); }
			for (auto v : e.emission) RemasterFrameSerialization::U8(bytes,v);
			RemasterFrameSerialization::U8(bytes,e.omnidirectional ? 1 : 0);
		}
	}
	if(std::any_of(frame.observedBgPixels.begin(),frame.observedBgPixels.end(),[](const auto &v){return !v.empty();})) {
		RemasterFrameSerialization::U32(bytes,0x314f4742u);
		for(unsigned n=0;n<4;n++) {
			const auto &v=frame.observedBgPixels[n];
			if((!v.empty()&&v.size()!=pixelCount)||v.size()!=frame.observedBgDepth[n].size())return false;
			RemasterFrameSerialization::U32(bytes,uint32_t(v.size()));
			for(size_t k=0;k<v.size();k++) {
				const auto &p=v[k];
				if(p.instanceId>frame.tileInstances.size()||(p.instanceId&&p.tilePixel>=64))return false;
				RemasterFrameSerialization::U32(bytes,p.owner);RemasterFrameSerialization::U32(bytes,p.instanceId);
				RemasterFrameSerialization::U8(bytes,p.tilePixel);RemasterFrameSerialization::U8(bytes,frame.observedBgDepth[n][k]);
			}
		}
	}
	const size_t hiddenLightCount = std::count_if(frame.lights.begin(), frame.lights.end(),
		[](const RemasterFrameLight &light) { return light.hideSource; });
	if (hiddenLightCount)
	{
		RemasterFrameSerialization::U32(bytes, 0x3150564cu); // LVP1
		if (!RemasterFrameSerialization::Size(bytes, hiddenLightCount)) return false;
		for (size_t i = 0; i < frame.lights.size(); i++)
			if (frame.lights[i].hideSource) RemasterFrameSerialization::U32(bytes, static_cast<uint32_t>(i));
	}
	if (!frame.contextLocations.empty()) {
		RemasterFrameSerialization::U32(bytes,0x314c5443u);
		RemasterFrameSerialization::U32(bytes,static_cast<uint32_t>(frame.contextLocations.size()));
		for(const auto &entry:frame.contextLocations) {
			if(!entry.first || entry.first>frame.tileInstances.size() || entry.second.area>321 || entry.second.background>1)return false;
			RemasterFrameSerialization::U32(bytes,entry.first);RemasterFrameSerialization::U16(bytes,entry.second.area);
			RemasterFrameSerialization::U8(bytes,entry.second.background);
			RemasterFrameSerialization::U16(bytes,entry.second.x);RemasterFrameSerialization::U16(bytes,entry.second.y);
		}
	}
	if (frame.offscreenRepresentativeEmitters < -1 || frame.offscreenRepresentativeEmitters > 8 ||
		(frame.offscreenRepresentativeEmitters >= 0 && frame.schemaVersion < 28)) return false;
	if (frame.offscreenRepresentativeEmitters >= 0) {
		RemasterFrameSerialization::U32(bytes, 0x3152454fu); // OER1
		RemasterFrameSerialization::U8(bytes, uint8_t(frame.offscreenRepresentativeEmitters));
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
