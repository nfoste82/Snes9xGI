/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include "remaster.h"
#include "profile.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;

int main ()
{
	std::vector<uint8_t> bytes;
	assert(RemasterProfileParsing::ParseByteArray("[0, 128, 255]", bytes));
	assert((bytes == std::vector<uint8_t> { 0, 128, 255 }));
	assert(!RemasterProfileParsing::ParseByteArray("[0,]", bytes));
	assert(!RemasterProfileParsing::ParseByteArray("[0 1]", bytes));
	assert(!RemasterProfileParsing::ParseByteArray("[256]", bytes));
	uint8_t indices[64] = {};
	for (size_t i = 0; i < 64; i++)
		indices[i] = i & 15;

	const uint64_t hash = S9xRemasterHashTile(4, indices);
	assert(hash == UINT64_C(0xf99d5746200467e1));
	assert(hash != S9xRemasterHashTile(2, indices));

	RemasterFrame frame;
	frame.width = 2;
	frame.height = 1;
	frame.profileRomSha256 = "0123456789abcdef";
	frame.lightingCoordinateScale = 24.0f;
	frame.cameraDirection = {{ 0.25f, -0.5f, -1.0f }};
	frame.indirectBounceCount = 6;
	frame.reflectanceBoost = 8.0f;
	frame.originalSceneContribution = 0.4f;
	frame.heightPreviewMultiplier = 12;
	frame.samplesPerFrame = 128;
	frame.sampleAccumulation = false;
	frame.originalRgb555 = { 0x001f, 0x03e0 };
	frame.mainPixels.resize(2);
	frame.subPixels.resize(2);
	frame.mainPixels[0].owner = 0x02010002;
	frame.mainPixels[0].instanceId = 1;
	frame.mainPixels[0].tilePixel = 9;
	RemasterFrameAsset frameAsset;
	frameAsset.tileId = { hash, 1, 4 };
	std::copy(indices, indices + 64, frameAsset.indices);
	frame.assets.push_back(frameAsset);
	RemasterFrameAsset secondFrameAsset;
	secondFrameAsset.tileId = { UINT64_C(0x1111111111111111), 1, 4 };
	std::fill(secondFrameAsset.indices, secondFrameAsset.indices + 64, 3);
	frame.assets.push_back(secondFrameAsset);
	RemasterFrameArtworkColors frameArtwork;
	frameArtwork.tileId = frameAsset.tileId;
	frameArtwork.rgb555[9] = 0x4210;
	frameArtwork.visiblePixels = UINT64_C(1) << 9;
	frame.artworkColors.push_back(frameArtwork);
	RemasterFrameAssetGroup frameGroup;
	frameGroup.name = "animated_floor";
	frameGroup.tileIds = { frameAsset.tileId, secondFrameAsset.tileId };
	frame.assetGroups.push_back(frameGroup);
	RemasterFrameAssetMetadata frameMetadata;
	assert(!frameMetadata.hasOcclusion);
	assert(frameMetadata.occlusion == S9xRemasterDefaultOcclusion());
	frameMetadata.tileId = frameAsset.tileId;
	frameMetadata.hasMaterialSelectors = true;
	frameMetadata.materialSelectors.fill("");
	frameMetadata.materialSelectors[0] = "stone";
	frameMetadata.hasOcclusion = true;
	frameMetadata.occlusion.fill(0);
	frameMetadata.occlusion[1] = 255;
	frameMetadata.hasHeight = true;
	frameMetadata.height[2] = 128;
	frameMetadata.heightSampling = RemasterHeightSampling::Linear;
	frameMetadata.hasNormals = true;
	frameMetadata.normalXyz.fill(128);
	frameMetadata.normalXyz[2] = 255;
	frameMetadata.normalXyz[3] = 255;
	frameMetadata.hasEmission = true;
	frameMetadata.emissionDepth = 3.5f;
	frameMetadata.directLightingOppositeFacing = true;
	frameMetadata.emissionRgba[12] = 255;
	frameMetadata.emissionRgba[13] = 96;
	frameMetadata.emissionRgba[14] = 24;
	frameMetadata.emissionRgba[15] = 25;
	frameMetadata.emissionRgba[36] = 255;
	frameMetadata.emissionRgba[37] = 96;
	frameMetadata.emissionRgba[38] = 24;
	frameMetadata.emissionRgba[39] = 25;
	frame.assetMetadata.push_back(frameMetadata);
	RemasterFrameMaterial frameMaterial;
	frameMaterial.name = "stone";
	frameMaterial.surfaceClass = RemasterSurfaceClass::Floor;
	frameMaterial.diffuseReflectance = {{ 0.25f, 0.5f, 0.75f }};
	frameMaterial.hasDiffuseReflectance = true;
	frameMaterial.receivesGi = false;
	frameMaterial.castsShadow = true;
	frameMaterial.zMin = 2.0f;
	frameMaterial.zMax = 6.0f;
	frame.materials.push_back(frameMaterial);
	RemasterFrameTileInstance frameInstance;
	frameInstance.tileId = frameAsset.tileId;
	frameInstance.source = RemasterSourceType::Background;
	frameInstance.sourceIndex = 1;
	frameInstance.tileNumber = 2;
	frameInstance.assetGroup = "animated_floor";
	frameInstance.material = "stone";
	frameInstance.hFlip = true;
	frameInstance.ppuPriority = 2;
	frameInstance.heightOffset = 48;
	frame.tileInstances.push_back(frameInstance);
	RemasterFrame floorFrame;
	RemasterFrameTileInstance lowerBackground = frameInstance;
	lowerBackground.sourceIndex = 0;
	lowerBackground.heightOffset = 0;
	RemasterFrameTileInstance raisedBackground = lowerBackground;
	raisedBackground.sourceIndex = 1;
	RemasterFrameTileInstance lowerSprite = lowerBackground;
	lowerSprite.source = RemasterSourceType::Object;
	lowerSprite.ppuPriority = 1;
	RemasterFrameTileInstance raisedSprite = lowerSprite;
	raisedSprite.ppuPriority = 2;
	floorFrame.tileInstances = { lowerBackground, raisedBackground, lowerSprite, raisedSprite };
	RemasterDungeonFloorContext floorContext;
	floorContext.verifiedAlttpRom = true;
	floorContext.indoors = true;
	floorContext.collisionMode = 1;
	S9xRemasterApplyDungeonFloorHeight(floorFrame, floorContext, 48);
	assert(floorFrame.tileInstances[0].heightOffset == 0);
	assert(floorFrame.tileInstances[1].heightOffset == 0);
	assert(floorFrame.tileInstances[2].heightOffset == 0);
	assert(floorFrame.tileInstances[3].heightOffset == 0);
	floorFrame.tileInstances[3].heightOffset = 0;
	floorContext.collisionMode = 0;
	S9xRemasterApplyDungeonFloorHeight(floorFrame, floorContext, 48);
	assert(floorFrame.tileInstances[1].heightOffset == 0);
	assert(floorFrame.tileInstances[3].heightOffset == 0);
	RemasterFrame room61Frame;
	room61Frame.width = 256;
	room61Frame.height = 224;
	room61Frame.mainPixels.resize(256 * 224);
	room61Frame.subPixels.resize(256 * 224);
	RemasterFrameTileInstance roomBackground = lowerBackground;
	roomBackground.sourceIndex = 1;
	RemasterFrameTileInstance roomDoor = roomBackground;
	RemasterFrameTileInstance roomFence = roomBackground;
	RemasterFrameTileInstance roomStair = roomBackground;
	roomStair.assetGroup = "stair_treads";
	RemasterFrameTileInstance roomOtherLayer = roomBackground;
	roomOtherLayer.sourceIndex = 0;
	RemasterFrameTileInstance roomWall = roomBackground;
	roomWall.tileId.hash = UINT64_C(0x83ff6d16868aaa72);
	roomWall.assetGroup = "wall_faces";
	room61Frame.tileInstances = { roomBackground, roomDoor, roomFence, roomStair,
		roomOtherLayer, roomWall };
	auto roomPixel = [&] (int x, int y, uint32_t id) {
		room61Frame.mainPixels[y * 256 + x].instanceId = id;
	};
	roomPixel(80, 100, 1);  // Checkered upper landing.
	roomPixel(175, 150, 1); // The same tile instance on the lower floor.
	roomPixel(23, 112, 2);  // Raised doorway.
	roomPixel(116, 100, 3); // Raised railing.
	roomPixel(88, 138, 4);  // Stair geometry is already absolute.
	roomPixel(80, 101, 5);  // The other background layer stays unchanged.
	roomPixel(116, 110, 6); // This brick borders the upper floor.
	roomPixel(135, 110, 1); // The eastern floor remains lower.
	roomPixel(50, 190, 6);  // The same brick shape also occurs lower down.
	RemasterDungeonFloorContext room61Context = floorContext;
	room61Context.roomIndex = 0x61;
	room61Context.backgroundScrollX = 0x200;
	room61Context.backgroundScrollY = 0xc10;
	S9xRemasterApplyDungeonFloorHeight(room61Frame, room61Context, 48);
	assert(room61Frame.tileInstances.size() == 8);
	assert(room61Frame.mainPixels[100 * 256 + 80].instanceId == 7);
	assert(room61Frame.tileInstances[6].heightOffset == 48);
	assert(room61Frame.mainPixels[110 * 256 + 116].instanceId == 8);
	assert(room61Frame.tileInstances[7].heightOffset == 48);
	assert(room61Frame.mainPixels[110 * 256 + 135].instanceId == 1);
	assert(room61Frame.tileInstances[5].heightOffset == 0);
	assert(room61Frame.tileInstances[0].heightOffset == 0);
	assert(room61Frame.tileInstances[1].heightOffset == 48);
	assert(room61Frame.tileInstances[2].heightOffset == 48);
	assert(room61Frame.tileInstances[3].heightOffset == 0);
	assert(room61Frame.tileInstances[4].heightOffset == 0);
	assert(S9xRemasterUpperFloorAt(0x61, 200, 79));
	assert(!S9xRemasterUpperFloorAt(0x61, 200, 80));
	assert(S9xRemasterUpperFloorAt(0x61, 127, 120));
	assert(!S9xRemasterUpperFloorAt(0x61, 128, 120));
	assert(S9xRemasterUpperFloorAt(0x61, 20, 167));
	assert(S9xRemasterUpperFloorAt(0x61, 71, 160));
	assert(!S9xRemasterUpperFloorAt(0x61, 72, 160));
	assert(S9xRemasterUpperFloorAt(0x61, 112, 160));
	assert(!S9xRemasterUpperFloorAt(0x61, 20, 168));
	RemasterFrame scrolledRoom = room61Frame;
	scrolledRoom.tileInstances = { roomBackground };
	scrolledRoom.mainPixels.assign(256 * 224, RemasterFramePixel());
	scrolledRoom.mainPixels[92 * 256 + 72].instanceId = 1;
	room61Context.backgroundScrollX += 8;
	room61Context.backgroundScrollY += 8;
	S9xRemasterApplyDungeonFloorHeight(scrolledRoom, room61Context, 48);
	assert(scrolledRoom.tileInstances[0].heightOffset == 48);
	scrolledRoom.tileInstances[0].heightOffset = 0;
	room61Context.roomIndex = 0x62;
	S9xRemasterApplyDungeonFloorHeight(scrolledRoom, room61Context, 48);
	assert(scrolledRoom.tileInstances[0].heightOffset == 0);
	RemasterFrame room60Frame;
	room60Frame.width = 256;
	room60Frame.height = 224;
	room60Frame.mainPixels.resize(256 * 224);
	room60Frame.subPixels.resize(256 * 224);
	RemasterFrameTileInstance excludedFloor = roomBackground;
	excludedFloor.tileId.hash = UINT64_C(0xc40518b112d0814e);
	room60Frame.tileInstances = { roomBackground, roomDoor, roomWall,
		roomStair, excludedFloor };
	room60Frame.mainPixels[100 * 256 + 128].instanceId = 1; // Raised interior.
	room60Frame.mainPixels[180 * 256 + 100].instanceId = 1; // Lower south floor.
	room60Frame.mainPixels[55 * 256 + 128].instanceId = 2;  // Upper doorway.
	room60Frame.mainPixels[110 * 256 + 95].instanceId = 3; // Raised wall brick.
	room60Frame.mainPixels[190 * 256 + 50].instanceId = 3; // Lower reuse.
	room60Frame.mainPixels[160 * 256 + 128].instanceId = 4; // Stair tread.
	room60Frame.mainPixels[80 * 256 + 120].instanceId = 5; // Out-of-bounds tile.
	room61Context.roomIndex = 0x60;
	room61Context.backgroundScrollX = 0x100;
	room61Context.backgroundScrollY = 0xc10;
	S9xRemasterApplyDungeonFloorHeight(room60Frame, room61Context, 48);
	assert(room60Frame.tileInstances.size() == 7);
	assert(room60Frame.tileInstances[5].heightOffset == 48);
	assert(room60Frame.tileInstances[6].heightOffset == 48);
	assert(room60Frame.tileInstances[0].heightOffset == 0);
	assert(room60Frame.tileInstances[1].heightOffset == 48);
	assert(room60Frame.tileInstances[2].heightOffset == 0);
	assert(room60Frame.tileInstances[3].heightOffset == 0);
	assert(room60Frame.tileInstances[4].heightOffset == 0);
	RemasterFrame room55Frame;
	room55Frame.width = 256;
	room55Frame.height = 224;
	room55Frame.mainPixels.resize(256 * 224);
	room55Frame.tileInstances = { roomBackground, roomDoor };
	room55Frame.mainPixels[100 * 256 + 120].instanceId = 1; // Southwest landing.
	room55Frame.mainPixels[100 * 256 + 220].instanceId = 1; // Lower east floor.
	room55Frame.mainPixels[50 * 256 + 120].instanceId = 2; // Northern doorway.
	room61Context.roomIndex = 0x55;
	room61Context.backgroundScrollX = 0;
	room61Context.backgroundScrollY = 288;
	S9xRemasterApplyDungeonFloorHeight(room55Frame, room61Context, 50);
	assert(room55Frame.tileInstances.size() == 3);
	assert(room55Frame.tileInstances[0].heightOffset == 0);
	assert(room55Frame.tileInstances[1].heightOffset == 50);
	assert(room55Frame.tileInstances[2].heightOffset == 50);
	assert(room55Frame.mainPixels[100 * 256 + 120].instanceId == 3);
	assert(room55Frame.mainPixels[100 * 256 + 220].instanceId == 1);
	assert(S9xRemasterUpperFloorAt(0x55, 120, 400)); // Western landing.
	assert(S9xRemasterUpperFloorAt(0x55, 220, 352)); // Northern walkway.
	assert(!S9xRemasterUpperFloorAt(0x55, 220, 400)); // Lower east floor.
	assert(!S9xRemasterUpperFloorAt(0x55, 120, 470)); // Below the stairs.
	std::array<uint8_t, 8192> runtimeAttributes;
	runtimeAttributes.fill(1);
	for (int y = 8; y < 20; y++)
		for (int x = 8; x < 24; x++)
			runtimeAttributes[y * 64 + x] = 0;
	for (int y = 24; y < 36; y++)
		for (int x = 8; x < 24; x++)
			runtimeAttributes[y * 64 + x] = 0;
	RemasterDungeonFloorContext runtimeContext = floorContext;
	runtimeContext.roomIndex = 7;
	runtimeContext.linkX = 80;
	runtimeContext.linkY = 80;
	runtimeContext.collisionAttributes = runtimeAttributes.data();
	runtimeContext.stairCount = 1;
	runtimeContext.stairs[0].x = 10;
	runtimeContext.stairs[0].y = 20;
	runtimeContext.stairs[0].highIsNorth = true;
	RemasterDungeonHeightMap runtimeMap;
	assert(S9xRemasterBuildDungeonHeightMap(runtimeContext, 50, runtimeMap));
	assert(runtimeMap.offsets[10 * 64 + 10] == 50);
	assert(runtimeMap.offsets[30 * 64 + 10] == 0);
	// The complete body projection includes interior cells, not just the four
	// collision-body corners sampled by the movement predicate.
	assert(runtimeMap.offsets[10 * 64 + 11] == 50);
	assert(runtimeMap.offsets[30 * 64 + 11] == 0);
	assert(runtimeMap.offsets[4 * 64 + 4] == 255);
	assert(runtimeMap.hasPlaneOffsets);
	std::array<uint8_t, 8192> overlapAttributes;
	overlapAttributes.fill(1);
	for (int plane = 0; plane < 2; plane++)
		for (int y = 8; y < 36; y++)
			for (int x = 8; x < 24; x++)
				overlapAttributes[plane * 4096 + y * 64 + x] = 0;
	RemasterDungeonFloorContext overlapContext = runtimeContext;
	overlapContext.collisionAttributes = overlapAttributes.data();
	overlapContext.stairs[0].changesPlane = true;
	RemasterDungeonHeightMap overlapMap;
	assert(S9xRemasterBuildDungeonHeightMap(overlapContext, 50, overlapMap));
	const int overlapCell = 15 * 64 + 10;
	assert(overlapMap.planeOffsets[0][overlapCell] == 50);
	assert(overlapMap.planeOffsets[1][overlapCell] == 0);
	assert(overlapMap.offsets[overlapCell] == 50);
	// A room-local solution must not depend on the doorway/camera from which the
	// room was entered.  Model room 0x060's essential topology: an upper H on
	// plane 0, a lower north/south underpass on plane 1, and the oriented
	// plane-changing stair that relates them.  Rebuilding from an upper-H seed
	// or a lower-underpass seed must produce the same placement bases.
	std::array<uint8_t, 8192> room60Attributes;
	room60Attributes.fill(1);
	auto openRoom60 = [&] (int plane, int x, int y) {
		room60Attributes[plane * 4096 + y * 64 + x] = 0;
	};
	for (int y = 8; y <= 48; y++)
	{
		for (int x = 36; x <= 39; x++) openRoom60(0, x, y);
		for (int x = 47; x <= 50; x++) openRoom60(0, x, y);
	}
	for (int y = 17; y <= 20; y++)
		for (int x = 36; x <= 50; x++) openRoom60(0, x, y);
	for (int y = 8; y <= 41; y++)
		for (int x = 46; x <= 49; x++) openRoom60(1, x, y);
	RemasterDungeonFloorContext room60UpperEntry = runtimeContext;
	room60UpperEntry.roomIndex = 0x60;
	room60UpperEntry.collisionAttributes = room60Attributes.data();
	room60UpperEntry.linkPlane = 0;
	room60UpperEntry.linkX = 37 * 8;
	room60UpperEntry.linkY = 10 * 8;
	room60UpperEntry.stairs[0].x = 46;
	room60UpperEntry.stairs[0].y = 42;
	room60UpperEntry.stairs[0].highIsNorth = false;
	room60UpperEntry.stairs[0].changesPlane = true;
	RemasterDungeonFloorContext room60LowerEntry = room60UpperEntry;
	room60LowerEntry.linkPlane = 1;
	room60LowerEntry.linkX = 47 * 8;
	room60LowerEntry.linkY = 18 * 8;
	RemasterDungeonHeightMap room60UpperMap, room60LowerMap;
	assert(S9xRemasterBuildDungeonHeightMap(room60UpperEntry, 50, room60UpperMap));
	assert(S9xRemasterBuildDungeonHeightMap(room60LowerEntry, 50, room60LowerMap));
	assert(room60UpperMap.planeOffsets == room60LowerMap.planeOffsets);
	assert(room60UpperMap.offsets == room60LowerMap.offsets);
	for (const auto &cell : { std::array<int, 2>{37, 10}, {37, 18}, {43, 18},
		{49, 18}, {49, 30} })
		assert(room60UpperMap.planeOffsets[0][cell[1] * 64 + cell[0]] == 50);
	assert(room60UpperMap.planeOffsets[1][18 * 64 + 47] == 0);
	assert(room60UpperMap.planeOffsets[0][18 * 64 + 47] == 50);
	assert(room60UpperMap.offsets[18 * 64 + 47] == 50);
	// Geometry identity excludes viewport scroll. A scroll changes only which
	// room-local cells are visible, never the complete room map cache key.
	const uint64_t room60Key = S9xRemasterDungeonHeightInputKey(room60UpperEntry, 50);
	room60UpperEntry.backgroundScrollX = 500;
	room60UpperEntry.backgroundScrollY = 3000;
	assert(S9xRemasterDungeonHeightInputKey(room60UpperEntry, 50) == room60Key);
	// Trace line 33's lifecycle: $A0 already names room 0x060 while WRAM still
	// holds room 0x061's three table-0 stairs. Reject publication until current
	// room tables arrive; keep the prior valid map independently available.
	RemasterDungeonFloorContext staleTransition = room60UpperEntry;
	staleTransition.previousRoomIndex = 0x61;
	staleTransition.submodule = 2;
	staleTransition.stairCount = 3;
	for (int i = 0; i < 3; i++)
	{
		staleTransition.stairs[i].table = 0;
		staleTransition.stairs[i].tableEntry = i;
		staleTransition.stairs[i].highIsNorth = true;
		staleTransition.stairs[i].changesPlane = false;
	}
	assert(!S9xRemasterDungeonHeightInputsPublishable(staleTransition, 0x61));
	staleTransition.stairCount = 0;
	assert(!S9xRemasterDungeonHeightInputsPublishable(staleTransition, 0x61));
	staleTransition.submodule = 0;
	assert(!S9xRemasterDungeonHeightInputsPublishable(staleTransition, 0x61));
	staleTransition.stairCount = 1;
	staleTransition.stairs[0] = room60UpperEntry.stairs[0];
	staleTransition.stairs[0].table = 1;
	staleTransition.stairs[0].tableEntry = 0;
	assert(S9xRemasterDungeonHeightInputsPublishable(staleTransition, 0x61));
	assert(!staleTransition.stairs[0].highIsNorth && staleTransition.stairs[0].changesPlane);
	// Replay the observed publication sequence through the same policy: the
	// line-33 stale room-0x061 tables and subsequent empty tables cannot replace
	// either cached map. The eventual table-1 south-high record publishes 0x060.
	RemasterDungeonHeightMap cached61 = room60UpperMap;
	cached61.roomIndex = 0x61;
	RemasterDungeonHeightMap cached60;
	cached60.roomIndex = 0xffff;
	uint64_t publishedKey = 0;
	auto publishTraceState = [&] (RemasterDungeonFloorContext state) {
		if (!S9xRemasterDungeonHeightInputsPublishable(state,
			cached60.roomIndex == state.roomIndex ? cached60.roomIndex : cached61.roomIndex)) return false;
		RemasterDungeonHeightMap candidate;
		if (!S9xRemasterBuildDungeonHeightMap(state, 50, candidate) || candidate.roomIndex != state.roomIndex)
			return false;
		cached60 = std::move(candidate);
		publishedKey = S9xRemasterDungeonHeightInputKey(state, 50);
		return true;
	};
	RemasterDungeonFloorContext replay = staleTransition;
	replay.submodule = 2;
	replay.stairCount = 3;
	for (int i = 0; i < 3; i++) replay.stairs[i] = staleTransition.stairs[0];
	assert(!publishTraceState(replay));
	assert(cached61.roomIndex == 0x61 && cached60.roomIndex == 0xffff);
	replay.stairCount = 0;
	for (int scroll = 512; scroll >= 256; scroll -= 12)
	{
		replay.backgroundScrollX = static_cast<uint16_t>(scroll);
		assert(!publishTraceState(replay));
	}
	assert(publishedKey == 0 && cached61.roomIndex == 0x61);
	replay.submodule = 0;
	assert(!publishTraceState(replay));
	replay.stairCount = 1;
	replay.stairs[0] = staleTransition.stairs[0];
	assert(publishTraceState(replay));
	assert(cached60.roomIndex == 0x60 && cached60.planeOffsets[0][18 * 64 + 47] == 50 &&
		cached60.planeOffsets[1][18 * 64 + 47] == 0);
	// The validated compiled format carries the visible maximum and both actor
	// planes, so a settled room remains correct even after Zelda clears stairs.
	const std::string compiledMapPath = "/tmp/snes9x-remaster-height-map-v2.bin";
	{
		std::ofstream output(compiledMapPath, std::ios::binary);
		output.write("ALTPHM2\0", 8);
		const char roomBytes[2] = { 0x60, 0 };
		output.write(roomBytes, 2);
		output.write(reinterpret_cast<const char *>(cached60.offsets.data()), cached60.offsets.size());
		for (const auto &plane : cached60.planeOffsets)
			output.write(reinterpret_cast<const char *>(plane.data()), plane.size());
	}
	RemasterDungeonHeightMap compiledMap;
	assert(S9xRemasterReadDungeonHeightMap(compiledMapPath, compiledMap));
	assert(compiledMap.roomIndex == 0x60 && compiledMap.hasPlaneOffsets);
	assert(compiledMap.offsets[18 * 64 + 47] == 50);
	assert(compiledMap.planeOffsets[0][18 * 64 + 47] == 50);
	assert(compiledMap.planeOffsets[1][18 * 64 + 47] == 0);
	std::remove(compiledMapPath.c_str());
	// Live room scrolling updates the destination room before all source-room
	// pixels leave the viewport. Verify the actual $A2/$EF lifecycle fields keep
	// destination geometry off the still-visible source half.
	RemasterDungeonFloorContext westScroll = room60UpperEntry;
	westScroll.previousRoomIndex = 0x61;
	westScroll.submodule = 2;
	westScroll.backgroundScrollX = 400;
	assert(S9xRemasterDungeonRoomForScreenPosition(westScroll, 20, 100) == 0x60);
	assert(S9xRemasterDungeonRoomForScreenPosition(westScroll, 120, 100) == 0x61);
	RemasterFrame mixedRoomFrame;
	mixedRoomFrame.width = 256;
	mixedRoomFrame.height = 1;
	mixedRoomFrame.mainPixels.resize(256);
	mixedRoomFrame.tileInstances = { roomBackground };
	mixedRoomFrame.mainPixels[20].instanceId = mixedRoomFrame.mainPixels[120].instanceId = 1;
	RemasterDungeonHeightMap mixedRoomMap;
	mixedRoomMap.roomIndex = 0x60;
	mixedRoomMap.offsets.fill(50);
	RemasterDungeonHeightMap sourceRoomMap;
	sourceRoomMap.roomIndex = 0x61;
	sourceRoomMap.offsets.fill(0);
	sourceRoomMap.offsets[((westScroll.backgroundScrollX + 120) & 511) / 8 + 12 * 64] = 50;
	S9xRemasterApplyDungeonHeightMap(mixedRoomFrame, westScroll, mixedRoomMap, &sourceRoomMap);
	assert(mixedRoomFrame.tileInstances[mixedRoomFrame.mainPixels[20].instanceId - 1].heightOffset == 50);
	assert(mixedRoomFrame.tileInstances[mixedRoomFrame.mainPixels[120].instanceId - 1].heightOffset == 0);
	RemasterFrame borderFrame;
	borderFrame.width = 8;
	borderFrame.height = 8;
	borderFrame.mainPixels.resize(64);
	borderFrame.tileInstances = { roomBackground };
	for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++)
	{
		borderFrame.mainPixels[y * 8 + x].instanceId = 1;
		borderFrame.mainPixels[y * 8 + x].tilePixel = static_cast<uint8_t>(y * 8 + x);
	}
	std::array<uint8_t, 8192> borderAttributes;
	borderAttributes.fill(1);
	borderAttributes[1] = 0;
	borderAttributes[4096] = borderAttributes[4096 + 1] = 3;
	RemasterDungeonFloorContext borderContext = runtimeContext;
	borderContext.linkX = borderContext.linkY = 0;
	borderContext.backgroundScrollX = borderContext.backgroundScrollY = 0;
	borderContext.collisionAttributes = borderAttributes.data();
	RemasterDungeonHeightMap borderMap;
	borderMap.roomIndex = runtimeContext.roomIndex;
	borderMap.offsets.fill(255);
	for (auto &plane : borderMap.planeOffsets) plane.fill(255);
	borderMap.offsets[1] = 50;
	borderMap.planeOffsets[0][1] = 50;
	borderMap.hasPlaneOffsets = true;
	S9xRemasterApplyDungeonHeightMap(borderFrame, borderContext, borderMap);
	assert(borderFrame.tileInstances[0].heightOffset == 50);
	assert(borderFrame.tileInstances[0].hasPlacementHeight);
	// The instance is H-flipped, so the canonical profile is mirrored while the
	// displayed ramp still rises away from its eastern support contact.
	assert(borderFrame.tileInstances[0].placementHeight[0] == 0);
	assert(borderFrame.tileInstances[0].placementHeight[7] == 7);
	// Same-floor contacts from two directions compose an outside-corner profile
	// using the minimum of both outward ramps.
	borderFrame.tileInstances[0].heightOffset = 0;
	borderFrame.tileInstances[0].hasPlacementHeight = false;
	borderAttributes[64] = 0;
	borderMap.offsets[64] = 50;
	borderMap.planeOffsets[0][64] = borderMap.planeOffsets[1][64] = 50;
	S9xRemasterApplyDungeonHeightMap(borderFrame, borderContext, borderMap);
	assert(borderFrame.tileInstances[0].hasPlacementHeight);
	assert(borderFrame.tileInstances[0].placementHeight[0] == 0);
	assert(borderFrame.tileInstances[0].placementHeight[7] == 7);
	assert(borderFrame.tileInstances[0].placementHeight[56] == 0);
	assert(borderFrame.tileInstances[0].placementHeight[63] == 0);
	RemasterDungeonHeightMap reviewMap;
	reviewMap.roomIndex = 0x55;
	reviewMap.offsets.fill(255);
	reviewMap.offsets[(400 / 8) * 64 + 120 / 8] = 50;
	reviewMap.offsets[(400 / 8) * 64 + 220 / 8] = 0;
	RemasterFrame reviewFrame;
	reviewFrame.width = 256;
	reviewFrame.height = 224;
	reviewFrame.mainPixels.resize(256 * 224);
	RemasterFrameTileInstance roomWallTop = roomBackground;
	roomWallTop.material = "upper_wall_top";
	RemasterFrameMaterial wallTopMaterial;
	wallTopMaterial.name = "upper_wall_top";
	wallTopMaterial.surfaceClass = RemasterSurfaceClass::WallTop;
	reviewFrame.materials.push_back(wallTopMaterial);
	RemasterFrameTileInstance roomWallFace = roomBackground;
	roomWallFace.material = "transition_wall_face";
	RemasterFrameMaterial wallFaceMaterial;
	wallFaceMaterial.name = "transition_wall_face";
	wallFaceMaterial.surfaceClass = RemasterSurfaceClass::WallFace;
	reviewFrame.materials.push_back(wallFaceMaterial);
	reviewFrame.tileInstances = { roomBackground, roomStair, raisedSprite, roomWallTop, roomWallFace };
	reviewFrame.tileInstances[2].heightOffset = 0;
	reviewFrame.mainPixels[112 * 256 + 120].instanceId = 1;
	reviewFrame.mainPixels[112 * 256 + 220].instanceId = 1;
	reviewFrame.mainPixels[112 * 256 + 121].instanceId = 2;
	reviewFrame.mainPixels[113 * 256 + 120].instanceId = 4;
	reviewFrame.mainPixels[114 * 256 + 120].instanceId = 5;
	S9xRemasterApplyDungeonFloorHeight(reviewFrame, room61Context, 50, &reviewMap);
	assert(reviewFrame.tileInstances.size() == 6);
	assert(reviewFrame.tileInstances[reviewFrame.mainPixels[112 * 256 + 120].instanceId - 1].heightOffset == 50);
	assert(reviewFrame.tileInstances[reviewFrame.mainPixels[112 * 256 + 220].instanceId - 1].heightOffset == 0);
	assert(reviewFrame.tileInstances[reviewFrame.mainPixels[112 * 256 + 121].instanceId - 1].heightOffset == 0);
	assert(reviewFrame.tileInstances[reviewFrame.mainPixels[113 * 256 + 120].instanceId - 1].heightOffset == 50);
	assert(reviewFrame.tileInstances[reviewFrame.mainPixels[114 * 256 + 120].instanceId - 1].heightOffset == 0);
	assert(reviewFrame.tileInstances[2].heightOffset == 0); // Invisible OAM has no spatial support point.
	RemasterFrame wallDistanceFrame;
	wallDistanceFrame.width = 32;
	wallDistanceFrame.height = 8;
	wallDistanceFrame.mainPixels.resize(32 * 8);
	wallDistanceFrame.materials.push_back(wallFaceMaterial);
	roomWallFace.tileId = roomBackground.tileId;
	roomWallFace.hFlip = false;
	wallDistanceFrame.tileInstances = { roomWallFace };
	RemasterFrameAssetMetadata wallDistanceMetadata;
	wallDistanceMetadata.tileId = roomWallFace.tileId;
	wallDistanceMetadata.hasHeight = true;
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++)
			wallDistanceMetadata.height[y * 8 + x] = static_cast<uint8_t>(7 - x);
	wallDistanceFrame.assetMetadata.push_back(wallDistanceMetadata);
	for (int cell = 0; cell < 3; cell++)
	{
		wallDistanceFrame.mainPixels[cell * 8].instanceId = 1;
		wallDistanceFrame.mainPixels[cell * 8].tilePixel = 0;
	}
	RemasterDungeonHeightMap wallDistanceMap;
	wallDistanceMap.roomIndex = 0x55;
	wallDistanceMap.offsets.fill(255);
	wallDistanceMap.offsets[3] = 0;
	RemasterDungeonFloorContext wallDistanceContext = floorContext;
	wallDistanceContext.roomIndex = 0x55;
	wallDistanceContext.backgroundScrollX = 0;
	wallDistanceContext.backgroundScrollY = 0;
	S9xRemasterApplyDungeonHeightMap(wallDistanceFrame, wallDistanceContext, wallDistanceMap);
	assert(wallDistanceFrame.tileInstances[wallDistanceFrame.mainPixels[0].instanceId - 1].heightOffset == 16);
	assert(wallDistanceFrame.tileInstances[wallDistanceFrame.mainPixels[8].instanceId - 1].heightOffset == 8);
	assert(wallDistanceFrame.tileInstances[wallDistanceFrame.mainPixels[16].instanceId - 1].heightOffset == 0);
	RemasterFrame doorwayFrame;
	doorwayFrame.width = 8;
	doorwayFrame.height = 8;
	doorwayFrame.mainPixels.resize(64);
	doorwayFrame.tileInstances = { roomBackground };
	doorwayFrame.mainPixels[0].instanceId = 1;
	RemasterDungeonHeightMap doorwayMap;
	doorwayMap.roomIndex = 0x55;
	doorwayMap.offsets.fill(255);
	doorwayMap.offsets[1] = 50;
	std::array<uint8_t, 8192> doorwayAttributes = {};
	doorwayAttributes[0] = 0x81;
	doorwayAttributes[4096] = 0x81;
	RemasterDungeonFloorContext doorwayContext = wallDistanceContext;
	doorwayContext.collisionAttributes = doorwayAttributes.data();
	S9xRemasterApplyDungeonHeightMap(doorwayFrame, doorwayContext, doorwayMap);
	assert(doorwayFrame.tileInstances[0].heightOffset == 50);
	RemasterFrame visualFloorFrame;
	visualFloorFrame.width = 24;
	visualFloorFrame.height = 8;
	visualFloorFrame.mainPixels.resize(24 * 8);
	RemasterFrameMaterial floorMaterial;
	floorMaterial.name = "visual_floor";
	floorMaterial.surfaceClass = RemasterSurfaceClass::Floor;
	visualFloorFrame.materials.push_back(floorMaterial);
	RemasterFrameTileInstance visualFloor = roomBackground;
	visualFloor.material = "visual_floor";
	visualFloor.assetGroup = "dungeon_floor";
	visualFloorFrame.tileInstances = { visualFloor };
	for (int cell = 0; cell < 3; cell++) visualFloorFrame.mainPixels[cell * 8].instanceId = 1;
	RemasterDungeonHeightMap visualFloorMap;
	visualFloorMap.roomIndex = 0x55;
	visualFloorMap.offsets.fill(255);
	visualFloorMap.offsets[3] = 50; // Outside the captured viewport.
	S9xRemasterApplyDungeonHeightMap(visualFloorFrame, wallDistanceContext, visualFloorMap);
	for (int cell = 0; cell < 3; cell++)
		assert(visualFloorFrame.tileInstances[visualFloorFrame.mainPixels[cell * 8].instanceId - 1].heightOffset == 0);
	RemasterFrame heightRangeFrame;
	heightRangeFrame.width = 2;
	heightRangeFrame.height = 1;
	heightRangeFrame.tileInstances = { roomBackground, roomDoor };
	heightRangeFrame.tileInstances[1].heightOffset = 50;
	RemasterFrameAssetMetadata heightRangeMetadata;
	heightRangeMetadata.tileId = roomBackground.tileId;
	heightRangeMetadata.hasHeight = true;
	heightRangeMetadata.height[0] = 5;
	heightRangeMetadata.height[1] = 7;
	heightRangeFrame.assetMetadata.push_back(heightRangeMetadata);
	heightRangeFrame.mainPixels.resize(2);
	heightRangeFrame.mainPixels[0].instanceId = 1;
	heightRangeFrame.mainPixels[0].tilePixel = 0;
	heightRangeFrame.mainPixels[1].instanceId = 2;
	heightRangeFrame.mainPixels[1].tilePixel = 1;
	assert(S9xRemasterFrameHeightRange(heightRangeFrame) == std::make_pair(5, 57));
	assert(S9xRemasterHeightPreviewRangeValid(50, 100));
	assert(!S9xRemasterHeightPreviewRangeValid(50, 50));
	assert(!S9xRemasterHeightPreviewRangeValid(50, 49));
	RemasterFrame assembledSprite;
	assembledSprite.width = 8;
	assembledSprite.height = 16;
	assembledSprite.mainPixels.resize(128);
	RemasterFrameAsset spriteArt;
	std::fill(spriteArt.indices, spriteArt.indices + 64, 1);
	spriteArt.tileId = { S9xRemasterHashTile(4, spriteArt.indices), 1, 4 };
	assembledSprite.assets.push_back(spriteArt);
	RemasterFrameAssetMetadata spriteMetadata;
	spriteMetadata.tileId = spriteArt.tileId;
	spriteMetadata.hasHeight = spriteMetadata.hasOcclusion = true;
	for (size_t p = 0; p < 64; p++)
	{
		spriteMetadata.height[p] = static_cast<uint8_t>(13 - p / 8);
		spriteMetadata.occlusion[p] = 255;
	}
	assembledSprite.assetMetadata.push_back(spriteMetadata);
	RemasterFrameTileInstance spriteTop;
	spriteTop.tileId = spriteArt.tileId;
	spriteTop.source = RemasterSourceType::Object;
	spriteTop.sourceIndex = 40;
	spriteTop.ppuPriority = 2;
	RemasterFrameTileInstance spriteBottom = spriteTop;
	spriteBottom.sourceIndex = 41;
	assembledSprite.tileInstances = { spriteTop, spriteBottom };
	for (size_t y = 0; y < 16; y++)
		for (size_t x = 0; x < 8; x++)
		{
			RemasterFramePixel &pixel = assembledSprite.mainPixels[y * 8 + x];
			pixel.instanceId = y < 8 ? 1 : 2;
			pixel.tilePixel = static_cast<uint8_t>((y % 8) * 8 + x);
		}
	S9xRemasterAlignGeneratedSpriteParts(assembledSprite);
	assert(assembledSprite.tileInstances[0].heightOffset == 8);
	assert(assembledSprite.tileInstances[1].heightOffset == 0);
	RemasterFrame scanlineSprite = assembledSprite;
	scanlineSprite.tileInstances.clear();
	for (size_t y = 0; y < 16; y++)
	{
		RemasterFrameTileInstance scanline = y < 8 ? spriteTop : spriteBottom;
		scanlineSprite.tileInstances.push_back(scanline);
		for (size_t x = 0; x < 8; x++)
			scanlineSprite.mainPixels[y * 8 + x].instanceId = static_cast<uint32_t>(y + 1);
	}
	S9xRemasterAlignGeneratedSpriteParts(scanlineSprite);
	for (size_t y = 0; y < 16; y++)
		assert(scanlineSprite.tileInstances[y].heightOffset == (y < 8 ? 8 : 0));
	RemasterFrame sidewaysLink = assembledSprite;
	for (RemasterFrameTileInstance &instance : sidewaysLink.tileInstances)
		instance.vramAddress = 0x8000;
	RemasterDungeonFloorContext linkContext;
	linkContext.verifiedAlttpRom = true;
	linkContext.linkFacing = 6;
	S9xRemasterAlignGeneratedSpriteParts(sidewaysLink, &linkContext);
	assert(sidewaysLink.tileInstances[0].normalYaw == 1);
	float sideX = 0.0f, sideY = 0.0f, sideZ = 1.0f;
	S9xRemasterTransformNormalForTileInstance(sidewaysLink.tileInstances[0], sideX, sideY, sideZ);
	assert(std::abs(sideX - 0.5f) < 0.001f);
	linkContext.linkFacing = 4;
	S9xRemasterAlignGeneratedSpriteParts(sidewaysLink, &linkContext);
	assert(sidewaysLink.tileInstances[0].normalYaw == -1);
	std::vector<uint8_t> firstFrameBytes;
	std::vector<uint8_t> secondFrameBytes;
	assert(S9xSerializeRemasterFrame(frame, firstFrameBytes));
	assert(S9xSerializeRemasterFrame(frame, secondFrameBytes));
	assert(firstFrameBytes == secondFrameBytes);
	RemasterFrame invalidSamplesFrame = frame;
	invalidSamplesFrame.samplesPerFrame = 129;
	std::vector<uint8_t> invalidSamplesFrameBytes;
	assert(!S9xSerializeRemasterFrame(invalidSamplesFrame, invalidSamplesFrameBytes));
	RemasterFrame invalidBoostFrame = frame;
	invalidBoostFrame.reflectanceBoost = 8.1f;
	std::vector<uint8_t> invalidBoostFrameBytes;
	assert(!S9xSerializeRemasterFrame(invalidBoostFrame, invalidBoostFrameBytes));
	assert(firstFrameBytes.size() > 8);
	assert(std::string(firstFrameBytes.begin(), firstFrameBytes.begin() + 6) == "S9XRMF");
	RemasterFrame decodedFrame;
	assert(S9xDeserializeRemasterFrame(firstFrameBytes, decodedFrame));
	assert(decodedFrame.width == 2 && decodedFrame.height == 1);
	assert(decodedFrame.lightingCoordinateScale == 24.0f);
	assert(decodedFrame.cameraDirection == frame.cameraDirection);
	assert(decodedFrame.indirectBounceCount == 6);
	assert(decodedFrame.reflectanceBoost == 8.0f);
	assert(decodedFrame.originalSceneContribution == 0.4f);
	assert(decodedFrame.heightPreviewMultiplier == 12);
	assert(decodedFrame.samplesPerFrame == frame.samplesPerFrame);
	assert(!decodedFrame.sampleAccumulation);
	assert(decodedFrame.originalRgb555 == frame.originalRgb555);
	assert(decodedFrame.tileInstances.size() == 1);
	assert(decodedFrame.tileInstances[0].ppuPriority == 2);
	assert(decodedFrame.tileInstances[0].heightOffset == 48);
	std::vector<uint8_t> previousVersionBytes = firstFrameBytes;
	const auto emissionStart = std::search(previousVersionBytes.begin(), previousVersionBytes.end(),
		frameMetadata.emissionRgba.begin(), frameMetadata.emissionRgba.end());
	assert(emissionStart != previousVersionBytes.end());
	previousVersionBytes.erase(emissionStart + 256, emissionStart + 260); // v21 emission depth
	previousVersionBytes.erase(previousVersionBytes.end() - 4, previousVersionBytes.end());
	const size_t boostOffset = 8 + 4 + 4 + 4 + 4 + frame.profileRomSha256.size() + 28;
	previousVersionBytes.erase(previousVersionBytes.begin() + boostOffset,
		previousVersionBytes.begin() + boostOffset + 4);
	previousVersionBytes[8] = 16;
	RemasterFrame previousVersionFrame;
	assert(S9xDeserializeRemasterFrame(previousVersionBytes, previousVersionFrame));
	assert(previousVersionFrame.reflectanceBoost == 0.0f);
	float normalX = 0.6f;
	float normalY = -0.8f;
	float normalZ = 0.25f;
	S9xRemasterTransformNormalForTileInstance(decodedFrame.tileInstances[0], normalX, normalY, normalZ);
	assert(normalX == -0.6f && normalY == -0.8f && normalZ == 0.25f);
	decodedFrame.tileInstances[0].vFlip = true;
	normalX = 0.6f;
	normalY = -0.8f;
	S9xRemasterTransformNormalForTileInstance(decodedFrame.tileInstances[0], normalX, normalY, normalZ);
	assert(normalX == -0.6f && normalY == 0.8f && normalZ == 0.25f);
	decodedFrame.tileInstances[0].vFlip = false;
	RemasterFrame legacyFlipFrame;
	legacyFlipFrame.schemaVersion = 7;
	legacyFlipFrame.width = 2;
	legacyFlipFrame.height = 2;
	legacyFlipFrame.mainPixels.resize(4);
	legacyFlipFrame.subPixels.resize(4);
	legacyFlipFrame.tileInstances.resize(1);
	legacyFlipFrame.mainPixels[0] = { 0, 1, 63 };
	legacyFlipFrame.mainPixels[1] = { 0, 1, 62 };
	legacyFlipFrame.mainPixels[2] = { 0, 1, 55 };
	legacyFlipFrame.mainPixels[3] = { 0, 1, 54 };
	S9xRemasterInferLegacyTileInstanceFlips(legacyFlipFrame);
	assert(legacyFlipFrame.tileInstances[0].hFlip && legacyFlipFrame.tileInstances[0].vFlip);
	assert(decodedFrame.mainPixels[0].tilePixel == 9);
	assert(decodedFrame.schemaVersion == 21);
	assert(decodedFrame.artworkColors.size() == 1);
	assert(S9xRemasterFrameArtworkColorsForTile(decodedFrame, frameAsset.tileId)->rgb555[9] == 0x4210);
	assert(!S9xRemasterFrameArtworkColorsForTile(decodedFrame, secondFrameAsset.tileId));
	assert(decodedFrame.assetGroups.size() == 1);
	assert(decodedFrame.assetMetadata.size() == 1);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->materialSelectors[0] == "stone");
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->occlusion[1] == 255);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->height[2] == 128);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->normalXyz[3] == 255);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->emissionRgba[15] == 25);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->emissionDepth == 3.5f);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->heightSampling == RemasterHeightSampling::Linear);
	assert(S9xRemasterFrameMetadataForTile(decodedFrame, frameAsset.tileId)->directLightingOppositeFacing);
	assert(S9xRemasterFrameInstanceAt(decodedFrame, 0, 0));
	assert(S9xRemasterFrameInstanceAt(decodedFrame, 0, 0)->material == "stone");
	assert(S9xRemasterFrameInstanceAt(decodedFrame, 0, 0)->hFlip);
	RemasterFrame colorMathFrame = decodedFrame;
	colorMathFrame.mainPixels[0].owner = S9xRemasterOwner(RemasterSourceType::Backdrop, 0, 0);
	colorMathFrame.mainPixels[0].instanceId = 0;
	colorMathFrame.mainPixels[0].tilePixel = 0xff;
	colorMathFrame.subPixels[0] = decodedFrame.mainPixels[0];
	bool visibleFromSubscreen = false;
	assert(S9xRemasterFrameVisibleTilePixelAt(colorMathFrame, 0, 0, &visibleFromSubscreen) ==
		&colorMathFrame.subPixels[0]);
	assert(visibleFromSubscreen);
	assert(S9xRemasterFrameVisibleInstanceAt(colorMathFrame, 0, 0)->material == "stone");
	colorMathFrame.mainPixels[0].owner = REMASTER_OWNER_UNSUPPORTED;
	assert(!S9xRemasterFrameVisibleTilePixelAt(colorMathFrame, 0, 0));
	const RemasterFrameMaterial *resolvedMaterial = S9xRemasterFrameMaterialForPixel(decodedFrame,
		decodedFrame.mainPixels[0]);
	assert(resolvedMaterial && resolvedMaterial->surfaceClass == RemasterSurfaceClass::Floor);
	assert(!resolvedMaterial->receivesGi && resolvedMaterial->castsShadow);
	assert(resolvedMaterial->zMin == 2.0f && resolvedMaterial->zMax == 6.0f);
	assert(resolvedMaterial->hasDiffuseReflectance &&
		resolvedMaterial->diffuseReflectance == frameMaterial.diffuseReflectance);
	decodedFrame.assetMetadata[0].materialSelectors[9] = "missing";
	assert(!S9xRemasterFrameMaterialForPixel(decodedFrame, decodedFrame.mainPixels[0]));
	decodedFrame.assetMetadata[0].materialSelectors[9].clear();
	assert(S9xRemasterFrameMaterialForPixel(decodedFrame, decodedFrame.mainPixels[0]) == resolvedMaterial);
	assert(S9xRemasterFrameOccurrences(decodedFrame, frameAsset.tileId).size() == 1);
	assert(S9xRemasterFrameAssetForTile(decodedFrame, frameAsset.tileId));
	assert(S9xRemasterFrameAssetGroupVariants(decodedFrame, "animated_floor").size() == 2);
	std::vector<RemasterFrameLight> emissionLights = S9xRemasterFrameEmissionLights(decodedFrame);
	assert(emissionLights.size() == 1);
	assert(emissionLights[0].x == 0.5f && emissionLights[0].y == 0.5f);
	assert(emissionLights[0].z == decodedFrame.lightingCoordinateScale / 255.0f);
	assert(emissionLights[0].red > emissionLights[0].green);
	assert(std::fabs(emissionLights[0].red - 1.0f / 8.0f) < 1e-6f);
	assert(std::fabs(emissionLights[0].green - std::pow(96.0f / 255.0f, 2.2f) / 8.0f) < 1e-6f);
	assert(std::fabs(emissionLights[0].blue - std::pow(24.0f / 255.0f, 2.2f) / 8.0f) < 1e-6f);
	const float fullRed = emissionLights[0].red;
	decodedFrame.assetMetadata[0].emissionRgba[39] = 12;
	emissionLights = S9xRemasterFrameEmissionLights(decodedFrame);
	assert(emissionLights.size() == 1 && emissionLights[0].red < fullRed);
	decodedFrame.assetMetadata[0].emissionRgba[39] = 25;
	decodedFrame.assetMetadata[0].height[3] = 255;
	decodedFrame.mainPixels[1].instanceId = 1;
	decodedFrame.mainPixels[1].tilePixel = 3;
	emissionLights = S9xRemasterFrameEmissionLights(decodedFrame);
	assert(emissionLights.size() == 1 && emissionLights[0].red > fullRed);
	assert(emissionLights[0].x == 1.0f);
	assert(emissionLights[0].z == decodedFrame.lightingCoordinateScale * 0.5f);
	assert(emissionLights[0].radius == 96.0f);
	decodedFrame.assetMetadata[0].emissionRgba[12] = 128;
	decodedFrame.assetMetadata[0].emissionRgba[15] = 50;
	emissionLights = S9xRemasterFrameEmissionLights(decodedFrame);
	assert(std::fabs(emissionLights[0].red - (1.0f + 2.0f * std::pow(128.0f / 255.0f, 2.2f)) / 8.0f) < 1e-6f);
	decodedFrame.assetMetadata[0].emissionRgba[12] = 255;
	decodedFrame.assetMetadata[0].emissionRgba[15] = 25;
	decodedFrame.mainPixels[1] = RemasterFramePixel();
	decodedFrame.assetGroups.clear();
	assert(S9xRemasterFrameAssetGroupVariants(decodedFrame, "animated_floor").size() == 1);
	RemasterFrame legacyFrame = frame;
	legacyFrame.assetMetadata.clear();
	legacyFrame.artworkColors.clear();
	legacyFrame.assetGroups.clear();
	legacyFrame.materials.clear();
	legacyFrame.tileInstances.clear();
	for (RemasterFramePixel &pixel : legacyFrame.mainPixels)
		pixel.instanceId = 0;
	std::vector<uint8_t> legacyBytes;
	assert(S9xSerializeRemasterFrame(legacyFrame, legacyBytes));
	const size_t legacyScaleOffset = 8 + 4 + 4 + 4 + 4 + legacyFrame.profileRomSha256.size();
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 28,
		legacyBytes.begin() + legacyScaleOffset + 32);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 27);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 15, legacyBytes.begin() + legacyScaleOffset + 27);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 9, legacyBytes.begin() + legacyScaleOffset + 15);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 5, legacyBytes.begin() + legacyScaleOffset + 9);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 4);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset, legacyBytes.begin() + legacyScaleOffset + 4);
	legacyBytes.erase(legacyBytes.begin() + legacyScaleOffset + 4,
		legacyBytes.begin() + legacyScaleOffset + 8);
	legacyBytes[8] = 6;
	const size_t legacyGroupCountOffset = 8 + 4 + 4 + 4 + 4 + legacyFrame.profileRomSha256.size() + 4;
	const size_t legacyPixelOffset = 8 + 4 + 4 + 4 + 4 + legacyFrame.profileRomSha256.size() + 6 * 4 +
		legacyFrame.originalRgb555.size() * 2;
	for (size_t i = legacyFrame.mainPixels.size() + legacyFrame.subPixels.size(); i > 0; i--)
		legacyBytes.erase(legacyBytes.begin() + legacyPixelOffset + (i - 1) * 9 + 8);
	legacyBytes[8] = 3;
	assert(S9xDeserializeRemasterFrame(legacyBytes, decodedFrame));
	assert(decodedFrame.schemaVersion == 3 && decodedFrame.assetMetadata.empty());
	legacyBytes[8] = 2;
	legacyBytes.erase(legacyBytes.begin() + legacyGroupCountOffset + 4,
		legacyBytes.begin() + legacyGroupCountOffset + 8);
	assert(S9xDeserializeRemasterFrame(legacyBytes, decodedFrame));
	assert(decodedFrame.schemaVersion == 2 && decodedFrame.assetMetadata.empty());
	legacyBytes[8] = 1;
	legacyBytes.erase(legacyBytes.begin() + legacyGroupCountOffset,
		legacyBytes.begin() + legacyGroupCountOffset + 4);
	assert(S9xDeserializeRemasterFrame(legacyBytes, decodedFrame));
	assert(decodedFrame.schemaVersion == 1 && decodedFrame.assetGroups.empty());
	std::vector<uint8_t> truncatedFrameBytes = firstFrameBytes;
	truncatedFrameBytes.pop_back();
	assert(!S9xDeserializeRemasterFrame(truncatedFrameBytes, decodedFrame));
	RemasterFrame invalidIdFrame = frame;
	invalidIdFrame.assets[0].tileId.bitDepth = 0;
	assert(!S9xSerializeRemasterFrame(invalidIdFrame, secondFrameBytes));
	RemasterFrame duplicateArtworkFrame = frame;
	duplicateArtworkFrame.artworkColors.push_back(frameArtwork);
	assert(!S9xSerializeRemasterFrame(duplicateArtworkFrame, secondFrameBytes));
	frame.mainPixels.pop_back();
	assert(!S9xSerializeRemasterFrame(frame, secondFrameBytes));

	const std::string path = "/tmp/snes9x-remaster-inventory-test.json";
	S9xRemasterRequestTileInventory(path);
	S9xRemasterBeginFrame(1, 1, 1, 1);
	S9xRemasterSetDraw(RemasterSourceType::Background, 1, 0x1402);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x1402);
	S9xRemasterSetTilePixel(0);
	S9xRemasterWriteOwner(0);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x1402);
	assert(S9xRemasterEndFrame());

	std::ifstream input(path);
	std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	std::ostringstream id;
	id << "v1:4bpp:" << std::hex << std::setfill('0') << std::setw(16) << hash;
	assert(json.find(id.str()) != std::string::npos);
	assert(json.find("\"observations\": 2") != std::string::npos);
	assert(json.find("\"visible_pixels\": 1") != std::string::npos);
	assert(json.find("\"visible_bounds\": { \"min_x\": 0, \"min_y\": 0, \"max_x\": 0, \"max_y\": 0 }") != std::string::npos);
	assert(json.find("\"visible_cells\": [{ \"x\": 0, \"y\": 0, \"pixels\": 1 }]") != std::string::npos);
	assert(json.find("\"contexts\": [{ \"source\": \"background\"") != std::string::npos);
	assert(json.find("\"profile_loaded\": false") != std::string::npos);
	std::remove(path.c_str());

	std::ostringstream profileSource;
	profileSource << R"PROFILE(
		schema_version = 13

[game]
title = "Test Game"
rom_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

[lighting_space]
		coordinate_scale = 24
		camera_direction = [0.25, -0.5, -1]
		indirect_bounces = 6
		indirect_roughness = 0.5
		reflectance_boost = 8
		original_scene_contribution = 0.4
		height_preview_multiplier = 12
		upper_floor_height = 48
		samples_per_frame = 128
		sample_accumulation = false

[materials.stone]
surface_class = "floor"
diffuse_reflectance = [0.25, 0.5, 0.75]
roughness = 0.8

[materials.wet_stone]
surface_class = "floor"
roughness = 0.4

[asset_groups.animated_floor]
tile_hashes = ["v1:4bpp:f99d5746200467e1", "v1:4bpp:1111111111111111"]
material = "stone"

[[rules]]
asset_group = "animated_floor"

[[rules]]
asset_group = "animated_floor"
source = "background"
source_index = 1
palette = 5
material = "wet_stone"
)PROFILE";
	profileSource << "\n[[assets]]\ntile_hash = \"v1:4bpp:f99d5746200467e1\"\nmaterials = [";
	for (size_t i = 0; i < 64; i++)
		profileSource << (i ? ", " : "") << (i == 0 ? "\"wet_stone\"" : "\"\"");
	profileSource << "]\nocclusion = [";
	for (size_t i = 0; i < 64; i++)
		profileSource << (i ? ", " : "") << (i == 1 ? 255 : 0);
	profileSource << "]\nheight = [";
	for (size_t i = 0; i < 64; i++)
		profileSource << (i ? ", " : "") << (i == 2 ? 128 : 0);
	profileSource << "]\nheight_sampling = \"linear\"\n";
	profileSource << "normal_xyz = [";
	for (size_t i = 0; i < 192; i++)
		profileSource << (i ? ", " : "") << (i % 3 == 2 ? 255 : 128);
	profileSource << "]\n";
	profileSource << "direct_lighting_opposite_facing = true\n";
	profileSource << "emission_rgba = [";
	for (size_t i = 0; i < 256; i++)
		profileSource << (i ? ", " : "") << (i == 12 ? 255 : (i == 13 ? 96 : (i == 14 ? 24 : (i == 15 ? 25 : 0))));
	profileSource << "]\n";
	const std::string profileText = profileSource.str();
	RemasterProfile profile;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	assert(S9xRemasterParseProfile(profileText, profile, diagnostics));
	assert(profile.materials.size() == 2);
	assert(profile.materials.at("stone").hasDiffuseReflectance);
	assert(profile.materials.at("stone").diffuseReflectance == frameMaterial.diffuseReflectance);
	assert(!profile.materials.at("wet_stone").hasDiffuseReflectance);
	assert(profile.lightingCoordinateScale == 24.0f);
	assert(profile.cameraDirection == frame.cameraDirection);
	assert(profile.indirectBounceCount == 6);
	assert(profile.indirectRoughness == 0.5f);
	assert(profile.reflectanceBoost == 8.0f);
	assert(profile.originalSceneContribution == 0.4f);
	assert(profile.heightPreviewMultiplier == 12);
	assert(profile.upperFloorHeight == 48);
	assert(profile.samplesPerFrame == 128);
	assert(!profile.sampleAccumulation);
	RemasterSceneSettings sceneSettings = S9xRemasterGetSceneSettings(profile);
	sceneSettings.originalSceneContribution = 0.7f;
	RemasterProfile sceneChangedProfile = profile;
	S9xRemasterApplySceneSettings(sceneChangedProfile, sceneSettings);
	assert(sceneChangedProfile.originalSceneContribution == 0.7f);
	assert(sceneChangedProfile.upperFloorHeight == 48);
	assert(sceneChangedProfile.assets.size() == profile.assets.size());
	assert(sceneChangedProfile.assets.begin()->second.height == profile.assets.begin()->second.height);
	assert(profile.assetGroups.at("animated_floor").tileIds.size() == 2);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).materialSelectors[0] == "wet_stone");
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).occlusion[1] == 255);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).height[2] == 128);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).heightSampling == RemasterHeightSampling::Linear);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).normalXyz[2] == 255);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).emissionRgba[15] == 25);
	assert(profile.assets.at(RemasterTileContentId { hash, 1, 4 }).directLightingOppositeFacing);
	assert(profile.rules.size() == 2);
	std::string serializedProfile;
	assert(S9xRemasterSerializeProfile(profile, serializedProfile, diagnostics));
	// Default coverage is implicit; a single lower pixel must survive saving.
	RemasterProfile compactProfile = profile;
	RemasterAssetMetadata &compactAsset = compactProfile.assets.at(RemasterTileContentId { hash, 1, 4 });
	compactAsset.occlusion.fill(255);
	std::string compactText;
	assert(S9xRemasterSerializeProfile(compactProfile, compactText, diagnostics));
	assert(compactText.find("occlusion = [") == std::string::npos);
	RemasterProfile compactRoundTrip;
	assert(S9xRemasterParseProfile(compactText, compactRoundTrip, diagnostics));
	assert(!compactRoundTrip.assets.at(compactAsset.tileId).hasOcclusion);
	assert(compactRoundTrip.assets.at(compactAsset.tileId).occlusion == S9xRemasterDefaultOcclusion());
	assert(compactRoundTrip.assets.at(compactAsset.tileId).height == compactAsset.height);
	compactAsset.occlusion[63] = 254;
	assert(S9xRemasterSerializeProfile(compactProfile, compactText, diagnostics));
	assert(S9xRemasterParseProfile(compactText, compactRoundTrip, diagnostics));
	assert(compactRoundTrip.assets.at(compactAsset.tileId).hasOcclusion);
	assert(compactRoundTrip.assets.at(compactAsset.tileId).occlusion[0] == 255);
	assert(compactRoundTrip.assets.at(compactAsset.tileId).occlusion[63] == 254);
	compactAsset.occlusion.fill(0);
	assert(S9xRemasterSerializeProfile(compactProfile, compactText, diagnostics));
	assert(S9xRemasterParseProfile(compactText, compactRoundTrip, diagnostics));
	assert(compactRoundTrip.assets.at(compactAsset.tileId).hasOcclusion);
	assert(compactRoundTrip.assets.at(compactAsset.tileId).occlusion[0] == 0);
	// Omitting a default-only tile must not leave an invalid empty asset table.
	compactProfile.assets.clear();
	RemasterAssetMetadata defaultAsset;
	defaultAsset.tileId = frameAsset.tileId;
	defaultAsset.hasOcclusion = true;
	compactProfile.assets.emplace(defaultAsset.tileId, defaultAsset);
	assert(S9xRemasterSerializeProfile(compactProfile, compactText, diagnostics));
	assert(S9xRemasterParseProfile(compactText, compactRoundTrip, diagnostics));
	assert(compactRoundTrip.assets.empty());
	// Captures also omit the redundant 64-byte full-coverage layer.
	RemasterFrame compactFrame;
	assert(S9xDeserializeRemasterFrame(firstFrameBytes, compactFrame));
	compactFrame.assetMetadata[0].occlusion.fill(255);
	std::vector<uint8_t> compactBytes, explicitBytes;
	assert(S9xSerializeRemasterFrame(compactFrame, compactBytes));
	RemasterFrame compactDecoded;
	assert(S9xDeserializeRemasterFrame(compactBytes, compactDecoded));
	assert(!compactDecoded.assetMetadata[0].hasOcclusion);
	assert(compactDecoded.assetMetadata[0].occlusion == S9xRemasterDefaultOcclusion());
	compactFrame.assetMetadata[0].occlusion[63] = 0;
	assert(S9xSerializeRemasterFrame(compactFrame, explicitBytes));
	assert(explicitBytes.size() == compactBytes.size() + 64);
	assert(S9xDeserializeRemasterFrame(explicitBytes, compactDecoded));
	assert(compactDecoded.assetMetadata[0].hasOcclusion);
	assert(compactDecoded.assetMetadata[0].occlusion[0] == 255);
	assert(compactDecoded.assetMetadata[0].occlusion[63] == 0);
	RemasterProfile roundTrippedProfile;
	assert(S9xRemasterParseProfile(serializedProfile, roundTrippedProfile, diagnostics));
	assert(roundTrippedProfile.assets.at(RemasterTileContentId { hash, 1, 4 }).height[2] == 128);
	assert(roundTrippedProfile.materials.at("stone").hasDiffuseReflectance);
	assert(roundTrippedProfile.materials.at("stone").diffuseReflectance == frameMaterial.diffuseReflectance);
	assert(roundTrippedProfile.assets.at(RemasterTileContentId { hash, 1, 4 }).normalXyz[2] == 255);
	assert(roundTrippedProfile.assets.at(RemasterTileContentId { hash, 1, 4 }).directLightingOppositeFacing);
	assert(roundTrippedProfile.indirectRoughness == 0.5f);
	assert(roundTrippedProfile.reflectanceBoost == 8.0f);
	assert(roundTrippedProfile.cameraDirection == frame.cameraDirection);
	assert(roundTrippedProfile.originalSceneContribution == 0.4f);
	assert(roundTrippedProfile.heightPreviewMultiplier == 12);
	assert(roundTrippedProfile.samplesPerFrame == 128);
	assert(!roundTrippedProfile.sampleAccumulation);
	std::string invalidSamplesProfile = serializedProfile;
	const size_t samplesBegin = invalidSamplesProfile.find("samples_per_frame = 128");
	assert(samplesBegin != std::string::npos);
	invalidSamplesProfile.replace(samplesBegin, sizeof("samples_per_frame = 128") - 1, "samples_per_frame = 129");
	assert(!S9xRemasterParseProfile(invalidSamplesProfile, roundTrippedProfile, diagnostics));
	std::string invalidBoostProfile = serializedProfile;
	const size_t boostBegin = invalidBoostProfile.find("reflectance_boost = 8");
	assert(boostBegin != std::string::npos);
	invalidBoostProfile.replace(boostBegin, sizeof("reflectance_boost = 8") - 1, "reflectance_boost = 8.1");
	assert(!S9xRemasterParseProfile(invalidBoostProfile, roundTrippedProfile, diagnostics));
	std::string legacyBoostProfile = serializedProfile;
	legacyBoostProfile.replace(legacyBoostProfile.find("schema_version = 13"), 19, "schema_version = 11");
	assert(!S9xRemasterParseProfile(legacyBoostProfile, roundTrippedProfile, diagnostics));
	const size_t legacyBoostBegin = legacyBoostProfile.find("reflectance_boost = ");
	assert(legacyBoostBegin != std::string::npos);
	legacyBoostProfile.erase(legacyBoostBegin, legacyBoostProfile.find('\n', legacyBoostBegin) - legacyBoostBegin + 1);
	const size_t legacyFloorBegin = legacyBoostProfile.find("upper_floor_height = ");
	assert(legacyFloorBegin != std::string::npos);
	legacyBoostProfile.erase(legacyFloorBegin, legacyBoostProfile.find('\n', legacyFloorBegin) - legacyFloorBegin + 1);
	assert(S9xRemasterParseProfile(legacyBoostProfile, roundTrippedProfile, diagnostics));
	assert(roundTrippedProfile.reflectanceBoost == 0.0f);
	const std::string savedProfilePath = "/tmp/snes9x-remaster-profile-test.toml";
	assert(S9xRemasterWriteProfile(profile, savedProfilePath, diagnostics));
	assert(S9xRemasterLoadProfile(savedProfilePath, roundTrippedProfile, diagnostics));
	std::remove(savedProfilePath.c_str());
	std::string invalidCameraProfile = serializedProfile;
	const size_t cameraBegin = invalidCameraProfile.find("camera_direction = [");
	assert(cameraBegin != std::string::npos);
	const size_t cameraEnd = invalidCameraProfile.find('\n', cameraBegin);
	invalidCameraProfile.replace(cameraBegin, cameraEnd - cameraBegin, "camera_direction = [0, 0, 0]");
	assert(!S9xRemasterParseProfile(invalidCameraProfile, roundTrippedProfile, diagnostics));
	invalidCameraProfile = serializedProfile;
	invalidCameraProfile.replace(invalidCameraProfile.find("schema_version = 13"), 19, "schema_version = 8");
	assert(!S9xRemasterParseProfile(invalidCameraProfile, roundTrippedProfile, diagnostics));
	std::string legacyReflectanceProfile = serializedProfile;
	legacyReflectanceProfile.replace(legacyReflectanceProfile.find("schema_version = 13"), 19, "schema_version = 10");
	assert(!S9xRemasterParseProfile(legacyReflectanceProfile, roundTrippedProfile, diagnostics));
	std::string invalidReflectanceProfile = serializedProfile;
	const size_t reflectanceBegin = invalidReflectanceProfile.find("diffuse_reflectance = [");
	assert(reflectanceBegin != std::string::npos);
	const size_t reflectanceEnd = invalidReflectanceProfile.find('\n', reflectanceBegin);
	invalidReflectanceProfile.replace(reflectanceBegin, reflectanceEnd - reflectanceBegin,
		"diffuse_reflectance = [0, 1.1, 0]");
	assert(!S9xRemasterParseProfile(invalidReflectanceProfile, roundTrippedProfile, diagnostics));

	RemasterProfileMatchContext context;
	context.tileId = { hash, 1, 4 };
	context.source = RemasterSourceType::Background;
	context.sourceIndex = 1;
	context.palette = 5;
	RemasterProfileMatch match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Matched);
	assert(match.material && match.material->name == "wet_stone");
	assert(match.assetGroup && match.assetGroup->name == "animated_floor");

	context.palette = 2;
	match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Matched);
	assert(match.material && match.material->name == "stone");

	RemasterFrame synchronizedFrame = frame;
	synchronizedFrame.tileInstances[0].matchStatus = RemasterProfileMatchStatus::NoMatch;
	synchronizedFrame.tileInstances[0].ruleLine = 999;
	synchronizedFrame.tileInstances[0].assetGroup = "stale_group";
	synchronizedFrame.tileInstances[0].material = "stale_material";
	synchronizedFrame.materials[0].name = "stale_material";
	S9xRemasterApplyProfileToFrame(profile, synchronizedFrame);
	assert(synchronizedFrame.profileRomSha256 == profile.romSha256);
	assert(synchronizedFrame.assetGroups.size() == profile.assetGroups.size());
	assert(synchronizedFrame.assetMetadata.size() == profile.assets.size());
	RemasterProfile atlasProfile = profile;
	for (uint64_t i = 1; i <= 1000; i++)
	{
		RemasterAssetMetadata distantTile;
		distantTile.tileId = { UINT64_C(0x8000000000000000) + i, 1, 4 };
		distantTile.hasHeight = true;
		atlasProfile.assets.emplace(distantTile.tileId, distantTile);
	}
	S9xRemasterApplyProfileToFrame(atlasProfile, synchronizedFrame);
	assert(synchronizedFrame.assetMetadata.size() == 1);
	assert(S9xRemasterFrameMetadataForTile(synchronizedFrame, frameAsset.tileId));
	assert(synchronizedFrame.materials.size() == profile.materials.size());
	assert(synchronizedFrame.materials[0].hasDiffuseReflectance);
	assert(synchronizedFrame.materials[0].diffuseReflectance == frameMaterial.diffuseReflectance);
	assert(synchronizedFrame.tileInstances[0].matchStatus == RemasterProfileMatchStatus::Matched);
	assert(synchronizedFrame.tileInstances[0].ruleLine == profile.rules[0].line);
	assert(synchronizedFrame.tileInstances[0].assetGroup == "animated_floor");
	assert(synchronizedFrame.tileInstances[0].material == "stone");

	S9xRemasterSetProfile(profile);
	const std::string profiledPath = "/tmp/snes9x-remaster-profiled-inventory-test.json";
	S9xRemasterRequestTileInventory(profiledPath);
	S9xRemasterBeginFrame(1, 1, 1, 1);
	S9xRemasterSetDraw(RemasterSourceType::Background, 1, 0x1402);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x1402);
	S9xRemasterSetDraw(RemasterSourceType::Background, 1, 0x1402);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x0002);
	assert(S9xRemasterEndFrame());
	std::ifstream profiledInput(profiledPath);
	json.assign(std::istreambuf_iterator<char>(profiledInput), std::istreambuf_iterator<char>());
	assert(json.find("\"material\": \"wet_stone\"") != std::string::npos);
	assert(json.find("\"material\": \"stone\"") != std::string::npos);
	assert(json.find("\"profile_unmatched_observations\": 0") != std::string::npos);
	assert(json.find("\"profile_loaded\": true") != std::string::npos);
	std::remove(profiledPath.c_str());
	RemasterSceneSettings liveSettings = S9xRemasterGetSceneSettings(profile);
	liveSettings.originalSceneContribution = 0.75f;
	S9xRemasterSetSceneSettings(liveSettings);
	S9xRemasterBeginFrame(1, 1, 1, 1);
	assert(S9xRemasterState().activeProfile.originalSceneContribution == 0.75f);
	assert(S9xRemasterState().activeProfile.assets.size() == profile.assets.size());
	S9xRemasterEndFrame();
	S9xRemasterSetProfile(profile);

	const std::string framePath = "/tmp/snes9x-remaster-frame-test.s9xrmf";
	S9xRemasterRequestFrameCapture(framePath);
	S9xRemasterBeginFrame(2, 2, 2, 1);
	S9xRemasterSetSubscreen(false);
	S9xRemasterSetDraw(RemasterSourceType::Background, 1, 0x1402);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x1402);
	S9xRemasterSetTilePixel(0);
	S9xRemasterWriteOwner(0);
	const uint16_t screen[] = { 0x001f, 0x03e0 };
	RemasterFrame finalizedFrame;
	assert(S9xRemasterEndFrame(screen, 2, 2, 1, &finalizedFrame) & RemasterCaptureFrame);
	std::ifstream frameInput(framePath, std::ios::binary);
	std::vector<uint8_t> capturedFrameBytes((std::istreambuf_iterator<char>(frameInput)),
		std::istreambuf_iterator<char>());
	assert(capturedFrameBytes.size() > 8);
	assert(std::string(capturedFrameBytes.begin(), capturedFrameBytes.begin() + 6) == "S9XRMF");
	std::vector<uint8_t> finalizedFrameBytes;
	assert(S9xSerializeRemasterFrame(finalizedFrame, finalizedFrameBytes));
	assert(finalizedFrameBytes == capturedFrameBytes);
	assert(S9xDeserializeRemasterFrame(capturedFrameBytes, decodedFrame));
	assert(finalizedFrame.originalRgb555 == decodedFrame.originalRgb555);
	assert(finalizedFrame.mainPixels[0].instanceId == decodedFrame.mainPixels[0].instanceId);
	assert(finalizedFrame.tileInstances.size() == decodedFrame.tileInstances.size());
	assert(decodedFrame.schemaVersion == 21);
	assert(decodedFrame.tileInstances[0].ppuPriority == 0);
	assert(decodedFrame.tileInstances[0].heightOffset == 0);
	assert(decodedFrame.lightingCoordinateScale == profile.lightingCoordinateScale);
	assert(decodedFrame.cameraDirection == profile.cameraDirection);
	assert(decodedFrame.indirectBounceCount == profile.indirectBounceCount);
	assert(decodedFrame.indirectRoughness == profile.indirectRoughness);
	assert(decodedFrame.reflectanceBoost == profile.reflectanceBoost);
	assert(decodedFrame.originalSceneContribution == profile.originalSceneContribution);
	assert(decodedFrame.heightPreviewMultiplier == profile.heightPreviewMultiplier);
	assert(decodedFrame.samplesPerFrame == profile.samplesPerFrame);
	assert(decodedFrame.sampleAccumulation == profile.sampleAccumulation);
	const RemasterFrameMaterial *capturedStone = nullptr;
	for (const RemasterFrameMaterial &material : decodedFrame.materials)
		if (material.name == "stone")
			capturedStone = &material;
	assert(capturedStone && capturedStone->hasDiffuseReflectance &&
		capturedStone->diffuseReflectance == frameMaterial.diffuseReflectance);
	assert(decodedFrame.mainPixels[0].tilePixel == 0);
	assert(decodedFrame.assetMetadata.size() == 1);
	assert(decodedFrame.assetMetadata[0].materialSelectors[0] == "wet_stone");
	assert(decodedFrame.assetMetadata[0].occlusion[1] == 255);
	assert(decodedFrame.assetMetadata[0].height[2] == 128);
	assert(decodedFrame.assetMetadata[0].normalXyz[2] == 255);
	assert(decodedFrame.assetMetadata[0].emissionRgba[15] == 25);
	assert(decodedFrame.assetMetadata[0].heightSampling == RemasterHeightSampling::Linear);
	assert(decodedFrame.assetMetadata[0].directLightingOppositeFacing);
	const RemasterFrameArtworkColors *capturedArtwork = S9xRemasterFrameArtworkColorsForTile(
		decodedFrame, decodedFrame.tileInstances[0].tileId);
	assert(capturedArtwork && (capturedArtwork->visiblePixels & UINT64_C(1)));
	assert(capturedArtwork->rgb555[0] == screen[0]);
	std::remove(framePath.c_str());

	RemasterAnimationCapture animationCapture;
	RemasterFrame animationFrame = decodedFrame;
	animationFrame.width = 16;
	animationFrame.height = 8;
	animationFrame.originalRgb555.assign(128, 0);
	animationFrame.mainPixels.assign(128, RemasterFramePixel());
	animationFrame.subPixels.assign(128, RemasterFramePixel());
	animationFrame.tileInstances.resize(2);
	animationFrame.tileInstances[0].tileId = { 1, 1, 4 };
	animationFrame.tileInstances[0].source = RemasterSourceType::Object;
	animationFrame.tileInstances[1].tileId = { 10, 1, 4 };
	animationFrame.tileInstances[1].source = RemasterSourceType::Object;
	animationFrame.assets.clear();
	animationFrame.artworkColors = { { { 1, 1, 4 }, {}, UINT64_C(1) },
		{ { 10, 1, 4 }, {}, UINT64_C(1) } };
	animationFrame.artworkColors[0].rgb555[0] = 0x001f;
	animationFrame.artworkColors[1].rgb555[0] = 0x03e0;
	for (size_t y = 0; y < 8; y++)
		for (size_t x = 0; x < 16; x++)
		{
			animationFrame.mainPixels[y * 16 + x].instanceId = x < 8 ? 1 : 2;
			animationFrame.mainPixels[y * 16 + x].tilePixel = static_cast<uint8_t>(y * 8 + x % 8);
		}
	S9xRemasterAddAnimationCaptureFrame(animationCapture, animationFrame);
	animationFrame.tileInstances[0].tileId = { 2, 1, 4 };
	animationFrame.tileInstances[1].tileId = { 20, 1, 4 };
	animationFrame.artworkColors = { { { 2, 1, 4 }, {}, UINT64_C(1) },
		{ { 20, 1, 4 }, {}, UINT64_C(1) } };
	animationFrame.artworkColors[0].rgb555[0] = 0x7c00;
	animationFrame.artworkColors[1].rgb555[0] = 0x4210;
	S9xRemasterAddAnimationCaptureFrame(animationCapture, animationFrame);
	RemasterFrame animationResult = S9xRemasterFinishAnimationCapture(std::move(animationCapture));
	assert(animationResult.assetGroups.size() == decodedFrame.assetGroups.size() + 2);
	assert(animationResult.tileInstances[0].assetGroup != animationResult.tileInstances[1].assetGroup);
	assert(S9xRemasterFrameAssetGroupVariants(animationResult,
		animationResult.tileInstances[0].assetGroup) ==
		(std::vector<RemasterTileContentId> { { 1, 1, 4 }, { 2, 1, 4 } }));
	assert(S9xRemasterFrameAssetGroupVariants(animationResult,
		animationResult.tileInstances[1].assetGroup) ==
		(std::vector<RemasterTileContentId> { { 10, 1, 4 }, { 20, 1, 4 } }));
	assert(animationResult.artworkColors.size() == 4);
	assert(S9xRemasterFrameArtworkColorsForTile(animationResult, { 2, 1, 4 })->rgb555[0] == 0x7c00);
	assert(S9xRemasterFrameArtworkColorsForTile(animationResult, { 20, 1, 4 })->rgb555[0] == 0x4210);
	assert(S9xSerializeRemasterFrame(animationResult, secondFrameBytes));
	assert(S9xDeserializeRemasterFrame(secondFrameBytes, decodedFrame));
	assert(S9xRemasterFrameArtworkColorsForTile(decodedFrame, { 2, 1, 4 })->rgb555[0] == 0x7c00);
	RemasterAnimationCapture classifiedAnimationCapture;
	animationFrame.artworkColors.clear();
	animationFrame.assetGroups = { { "misclassified", { { 50, 1, 4 }, { 60, 1, 4 } } } };
	animationFrame.tileInstances.resize(2);
	animationFrame.tileInstances[0].tileId = { 30, 1, 4 };
	animationFrame.tileInstances[0].assetGroup.clear();
	animationFrame.tileInstances[1].tileId = { 60, 1, 4 };
	animationFrame.tileInstances[1].assetGroup = "misclassified";
	animationFrame.mainPixels.assign(128, RemasterFramePixel());
	for (size_t pixel = 0; pixel < 128; pixel++)
		animationFrame.mainPixels[pixel].instanceId = pixel % 16 < 8 ? 1 : 2;
	S9xRemasterAddAnimationCaptureFrame(classifiedAnimationCapture, animationFrame);
	animationFrame.tileInstances[0].tileId = { 40, 1, 4 };
	S9xRemasterAddAnimationCaptureFrame(classifiedAnimationCapture, animationFrame);
	animationFrame.tileInstances[0].tileId = { 50, 1, 4 };
	animationFrame.tileInstances[0].assetGroup = "misclassified";
	S9xRemasterAddAnimationCaptureFrame(classifiedAnimationCapture, animationFrame);
	RemasterFrame classifiedAnimationResult = S9xRemasterFinishAnimationCapture(
		std::move(classifiedAnimationCapture));
	assert(classifiedAnimationResult.tileInstances[0].assetGroup == "capture_animation_1");
	assert(S9xRemasterFrameAssetGroupVariants(classifiedAnimationResult, "capture_animation_1") ==
		(std::vector<RemasterTileContentId> { { 30, 1, 4 }, { 40, 1, 4 }, { 50, 1, 4 } }));
	assert(S9xRemasterFrameAssetGroupVariants(classifiedAnimationResult, "misclassified") ==
		(std::vector<RemasterTileContentId> { { 60, 1, 4 } }));
	RemasterAnimationCapture replacedTileCapture;
	animationFrame.assetGroups.clear();
	animationFrame.tileInstances.resize(1);
	animationFrame.tileInstances[0].tileId = { 70, 1, 4 };
	animationFrame.tileInstances[0].tileNumber = 7;
	animationFrame.tileInstances[0].assetGroup.clear();
	animationFrame.mainPixels.assign(128, RemasterFramePixel());
	for (size_t pixel = 0; pixel < 64; pixel++)
		animationFrame.mainPixels[pixel].instanceId = 1;
	S9xRemasterAddAnimationCaptureFrame(replacedTileCapture, animationFrame);
	animationFrame.tileInstances[0].tileId = { 80, 1, 4 };
	animationFrame.tileInstances[0].tileNumber = 8;
	S9xRemasterAddAnimationCaptureFrame(replacedTileCapture, animationFrame);
	RemasterFrame replacedTileResult = S9xRemasterFinishAnimationCapture(std::move(replacedTileCapture));
	assert(replacedTileResult.assetGroups.empty());
	assert(replacedTileResult.tileInstances[0].assetGroup.empty());

	S9xRemasterRequestFrameCapture(framePath);
	S9xRemasterBeginFrame(2, 2, 2, 1);
	finalizedFrame.width = 99;
	assert(!(S9xRemasterEndFrame(screen, 1, 2, 1, &finalizedFrame) & RemasterCaptureFrame));
	assert(finalizedFrame.width == 0);
	std::remove(framePath.c_str());

	S9xRemasterSetLiveFramesEnabled(true);
	S9xRemasterBeginFrame(2, 2, 2, 1);
	assert(!S9xRemasterCompletedFrame());
	S9xRemasterSetSubscreen(false);
	S9xRemasterSetDraw(RemasterSourceType::Background, 1, 0x1402);
	S9xRemasterObserveTile(indices, 4, 0x2000, 0x1402);
	S9xRemasterSetTilePixel(0);
	S9xRemasterWriteOwner(0);
	assert(S9xRemasterEndFrame(screen, 2, 2, 1) == RemasterCaptureNone);
	const RemasterFrame *liveFrame = S9xRemasterCompletedFrame();
	assert(liveFrame && liveFrame->width == 2 && liveFrame->height == 1);
	assert(S9xRemasterFrameHasPixelData(*liveFrame, 2, 1));
	assert(!S9xRemasterFrameHasPixelData(*liveFrame, 1, 2));
	assert(liveFrame->originalRgb555[1] == screen[1]);
	assert(liveFrame->mainPixels[0].instanceId == 1);
	assert(liveFrame->tileInstances.size() == 1);
	S9xRemasterBeginFrame(2, 2, 2, 1);
	assert(!S9xRemasterCompletedFrame());
	S9xRemasterEndFrame(screen, 2, 2, 1);
	S9xRemasterSetLiveFramesEnabled(false);
	S9xRemasterBeginFrame(2, 2, 2, 1);
	assert(!S9xRemasterCompletedFrame());
	S9xRemasterEndFrame(screen, 2, 2, 1);

	const char *ambiguousProfile = R"PROFILE(
schema_version = 1
[game]
title = "Test Game"
rom_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
[materials.first]
surface_class = "floor"
[materials.second]
surface_class = "wall_face"
[[rules]]
tile_hash = "v1:4bpp:f99d5746200467e1"
source = "background"
material = "first"
[[rules]]
tile_hash = "v1:4bpp:f99d5746200467e1"
palette = 0
material = "second"
[[rules]]
tile_hash = "v1:4bpp:f99d5746200467e1"
palette = 0
material = "first"
)PROFILE";
	assert(S9xRemasterParseProfile(ambiguousProfile, profile, diagnostics));
	context.palette = 0;
	match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Ambiguous);
	assert(match.conflictingRuleLines.size() == 3);

	const char *invalidStringProfile = R"PROFILE(
schema_version = 1
[game]
title = "Test"junk"
rom_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
)PROFILE";
	assert(!S9xRemasterParseProfile(invalidStringProfile, profile, diagnostics));

	const char *invalidSourceProfile = R"PROFILE(
schema_version = 1
[game]
title = "Test Game"
rom_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
[materials.stone]
surface_class = "floor"
[[rules]]
tile_hash = "v1:4bpp:f99d5746200467e1"
source_index = 1
material = "stone"
)PROFILE";
	assert(!S9xRemasterParseProfile(invalidSourceProfile, profile, diagnostics));

	const char *invalidProfile = R"PROFILE(
schema_version = 1
[game]
title = "Test Game"
rom_sha256 = "not-a-sha"
[materials.stone]
unexpected = true
[asset_groups.broken]
tile_hashes = ["v1:4bpp:INVALID000000000"]
material = "missing"
)PROFILE";
	assert(!S9xRemasterParseProfile(invalidProfile, profile, diagnostics));
	assert(diagnostics.size() >= 3);

	assert(S9xRemasterLoadProfile("alttp-profile.toml", profile, diagnostics));
	assert(profile.materials.size() == 10);
	assert(profile.assetGroups.size() == 11);
	assert(profile.rules.size() == 11);
	context = {};
	context.tileId = { UINT64_C(0xded4f16afe03288a), 1, 4 };
	context.source = RemasterSourceType::Background;
	context.sourceIndex = 1;
	match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Matched);
	assert(match.assetGroup && match.assetGroup->name == "stair_treads");
	assert(match.material && match.material->name == "dungeon_floor");
	context.tileId = { UINT64_C(0x99863ac4c1bb8e68), 1, 4 };
	match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Matched);
	assert(match.assetGroup && match.assetGroup->name == "stair_rails");
	assert(match.material && match.material->name == "stair_rail");
	assert(match.material->roughness < 0.5f);
	context = {};
	context.tileId = { UINT64_C(0x34eb3798eedfa0fc), 1, 4 };
	context.source = RemasterSourceType::Background;
	context.sourceIndex = 1;
	match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Matched);
	assert(match.assetGroup && match.assetGroup->name == "torch_flame");
	assert(match.material && match.material->name == "torch_flame");
	assert(match.material->receivesGi);
	context.tileId = { UINT64_C(0xc40518b112d0814e), 1, 4 };
	match = S9xRemasterMatchProfile(profile, context);
	assert(match.status == RemasterProfileMatchStatus::Matched);
	assert(match.assetGroup && match.assetGroup->name == "dungeon_floor");
	RemasterFrame profiledFrame;
	S9xRemasterApplyProfileToFrame(profile, profiledFrame);
	assert(S9xRemasterFrameAssetGroupVariants(profiledFrame, "torch_flame") ==
		(std::vector<RemasterTileContentId> { { UINT64_C(0x34eb3798eedfa0fc), 1, 4 },
			{ UINT64_C(0x9c36635bd77c6108), 1, 4 }, { UINT64_C(0xa8d1d0722ff1bda3), 1, 4 } }));
	const RemasterTileContentId flame = { UINT64_C(0x34eb3798eedfa0fc), 1, 4 };
	profile.assets[flame].tileId = flame;
	profile.assets[flame].emissionDepth = 3.25f;
	std::string volumeText;
	std::vector<RemasterProfileDiagnostic> volumeDiagnostics;
	assert(S9xRemasterSerializeProfile(profile, volumeText, volumeDiagnostics));
	RemasterProfile volumeProfile;
	assert(S9xRemasterParseProfile(volumeText, volumeProfile, volumeDiagnostics));
	assert(volumeProfile.schemaVersion == 14 && volumeProfile.assets.at(flame).emissionDepth == 3.25f);
	RemasterFrame volumeFrame;
	RemasterFrameAsset volumeAsset;
	volumeAsset.tileId = flame;
	volumeFrame.assets.push_back(volumeAsset);
	S9xRemasterApplyProfileToFrame(volumeProfile, volumeFrame);
	assert(S9xRemasterFrameMetadataForTile(volumeFrame, flame)->emissionDepth == 3.25f);
	profile.assets[flame].emissionDepth = -1;
	assert(!S9xRemasterSerializeProfile(profile, volumeText, volumeDiagnostics));
	return 0;
}
