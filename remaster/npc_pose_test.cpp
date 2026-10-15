#include "remaster.h"
#include "npc_pose.h"
#include <cstdlib>
#include <iostream>

static void require(bool value, const char *message)
{
	if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

int main()
{
	const auto bottomRamp = S9xRemasterNpcHeightRamp(15, 8, false);
	const auto topRamp = S9xRemasterNpcHeightRamp(15, 0, false);
	const auto flippedRamp = S9xRemasterNpcHeightRamp(15, 8, true);
	for (unsigned p = 0; p < 64; p++)
	{
		require(bottomRamp[p] == 7 - p / 8, "bottom row zero and one unit per pixel upward");
		require(topRamp[p] == 15 - p / 8, "ramp continues across tile boundary, every column");
		require(flippedRamp[p] == p / 8, "vertical flip maps display ramp to canonical pixels");
	}
	require(S9xRemasterNpcHeightRamp(400, 0, false)[0] == 400, "height ramp preserves relief above 255");
	require(S9xRemasterNpcHeightRamp(40000, 0, false)[0] == 32767, "height ramp saturates at signed sample limit");
	require(S9xRemasterNpcHeightRamp(15, 20, false)[0] == 0, "height ramp clamps below base");
	RemasterNpcPosePart front, back;
	std::fill(front.asset.indices, front.asset.indices + 64, 1);
	front.asset.indices[0] = 0;
	front.asset.tileId = {S9xRemasterHashTile(4, front.asset.indices), 1, 4};
	front.x = -8; front.y = -12; front.address = 0x5000;
	front.rgb555.fill(S9xRemasterPaletteRgb555(31));
	back = front; back.slot = 1;
	std::fill(back.asset.indices, back.asset.indices + 64, 2);
	back.asset.tileId = {S9xRemasterHashTile(4, back.asset.indices), 1, 4};
	back.rgb555.fill(S9xRemasterPaletteRgb555(31 << 10));
	const std::string hash(64, 'a');
	RemasterFrame frame;
	require(S9xRemasterBuildNpcPoseFrame({front, back}, hash, frame), "assemble pose");
	require(frame.width == 16 && frame.height == 16, "padded bounds");
	require(frame.mainPixels[4 * 16 + 4].instanceId == 2, "transparent front picks back");
	require(frame.mainPixels[4 * 16 + 5].instanceId == 1, "front OAM wins");
	require(frame.mainPixels[4 * 16 + 5].tilePixel == 1, "canonical pixel");
	require(frame.tileInstances[0].vramAddress == 0xa000, "VRAM word address converted to bytes");
	require(frame.originalRgb555[4 * 16 + 5] == 31 << 10, "SNES BGR converted to editor RGB");
	require(frame.assets.size() == 2 && frame.tileInstances.size() == 2, "identities and placements");
	RemasterNpcPosePart extended = back; extended.x = -24; extended.y = -28;
	RemasterNpcPoseBounds canvas;
	canvas.include(front.x, front.y); canvas.include(extended.x, extended.y);
	RemasterFrame compactFrame, extendedFrame;
	require(S9xRemasterBuildNpcPoseFrame({front}, hash, compactFrame, &canvas), "compact phase on shared canvas");
	require(S9xRemasterBuildNpcPoseFrame({front, extended}, hash, extendedFrame, &canvas), "extended phase on shared canvas");
	require(compactFrame.width == extendedFrame.width && compactFrame.height == extendedFrame.height,
		"animation canvas size and therefore pixel scale remain fixed");
	const size_t anchor = (front.y - canvas.y0 + 4) * compactFrame.width + front.x - canvas.x0 + 5;
	require(compactFrame.mainPixels[anchor].instanceId == 1 && extendedFrame.mainPixels[anchor].instanceId == 1 &&
		compactFrame.mainPixels[anchor].tilePixel == extendedFrame.mainPixels[anchor].tilePixel,
		"unchanged draw coordinates retain identical canvas position despite changing extrema");
	RemasterNpcPoseBounds tooSmall; tooSmall.include(front.x, front.y);
	require(!S9xRemasterBuildNpcPoseFrame({front, extended}, hash, extendedFrame, &tooSmall), "reject clipping shared canvas");
	front.hFlip = front.vFlip = true;
	require(S9xRemasterBuildNpcPoseFrame({front}, hash, frame), "flipped pose");
	require(frame.mainPixels[11 * 16 + 10].tilePixel == 1, "flip retains canonical picked pixel");
	require(frame.mainPixels[11 * 16 + 11].instanceId == 0, "flipped transparency");
	RemasterProfile profile;
	profile.romSha256 = hash; profile.schemaVersion = 5;
	auto &asset = profile.assets[front.asset.tileId];
	asset.tileId = front.asset.tileId; asset.hasHeight = true; asset.height.fill(17);
	require(!S9xRemasterNpcCanAuthorLayer(profile, front.asset.tileId, false, false), "existing heights protected by default");
	require(S9xRemasterNpcCanAuthorLayer(profile, front.asset.tileId, true, false), "existing heights do not prevent adding normals");
	require(S9xRemasterNpcCanAuthorLayer(profile, front.asset.tileId, false, true), "explicit overwrite allows replacing heights");
	require(S9xRemasterNpcCanAuthorLayer(profile, back.asset.tileId, false, false), "missing asset can receive heights");
	require(S9xRemasterNpcCanAuthorLayer(profile, back.asset.tileId, true, false), "missing asset can receive normals");
	S9xRemasterApplyProfileToFrame(profile, frame);
	require(S9xRemasterFrameMetadataForTile(frame, front.asset.tileId)->height[1] == 17, "profile applies to pose");
	const size_t at = 11 * 16 + 10;
	require(S9xRemasterNpcDebugColor(frame, at, RemasterNpcDebugView::Occlusion) == std::array<uint8_t, 3>{{255, 13, 0}}, "default opaque occlusion is red");
	asset.hasOcclusion = true;
	for (const auto value : {0, 128, 255})
	{
		asset.occlusion[1] = value;
		S9xRemasterApplyProfileToFrame(profile, frame);
		const std::array<uint8_t, 3> expected = value == 0 ? std::array<uint8_t, 3>{{0, 89, 13}} :
			value == 128 ? std::array<uint8_t, 3>{{128, 51, 6}} : std::array<uint8_t, 3>{{255, 13, 0}};
		require(S9xRemasterNpcDebugColor(frame, at, RemasterNpcDebugView::Occlusion) == expected,
			"pose occlusion uses inspector green-to-red scale and canonical flipped pixel");
	}
	require(S9xRemasterNpcDebugColor(frame, at, RemasterNpcDebugView::Height) == std::array<uint8_t, 3>{{17, 17, 17}}, "whole pose canonical height");
	asset.hasNormals = true;
	require(!S9xRemasterNpcCanAuthorLayer(profile, front.asset.tileId, true, false), "existing normals protected by default");
	require(S9xRemasterNpcCanAuthorLayer(profile, front.asset.tileId, true, true), "explicit overwrite allows replacing normals");
	for (const auto direction : {std::array<uint8_t, 3>{{128, 128, 255}}, {{128, 128, 0}},
		{{255, 128, 128}}, {{0, 128, 128}}, {{128, 255, 128}}, {{128, 0, 128}}})
	{
		const auto uniform = S9xRemasterNpcUniformNormals(direction);
		for (unsigned p = 0; p < 64; p++)
			require(std::equal(direction.begin(), direction.end(), uniform.begin() + p * 3),
				"cardinal fill covers every canonical pixel including transparent artwork");
	}
	asset.normalXyz.fill(128); asset.normalXyz[3] = 255;
	asset.hasEmission = true;
	asset.emissionRgba[4] = 200; asset.emissionRgba[5] = 100; asset.emissionRgba[6] = 50; asset.emissionRgba[7] = 255;
	S9xRemasterApplyProfileToFrame(profile, frame);
	const auto normal = S9xRemasterNpcDebugColor(frame, at, RemasterNpcDebugView::Normals);
	require(normal[0] < 2 && normal[1] >= 126 && normal[1] <= 129, "pose normals honor instance flips");
	require(S9xRemasterNpcDebugColor(frame, at, RemasterNpcDebugView::Emission) == std::array<uint8_t, 3>{{200, 100, 50}}, "pose emission channels");
	asset.height[1] = 29;
	S9xRemasterApplyProfileToFrame(profile, frame);
	require(S9xRemasterNpcDebugColor(frame, at, RemasterNpcDebugView::Height)[0] == 29, "debug sees unsaved edits");
	for (const auto sample : {0, 8, 16, 24, 32, 64})
	{
		asset.height[1] = sample;
		S9xRemasterApplyProfileToFrame(profile, frame);
		const auto color = S9xRemasterNpcDebugColor(frame, at, RemasterNpcDebugView::Height, 8, 32);
		const uint8_t expected = sample <= 8 ? 0 : sample >= 32 ? 255 : sample == 16 ? 85 : 170;
		require(color == std::array<uint8_t, 3>{{expected, expected, expected}}, "height display range scales and clamps grayscale");
	}
	asset.height[1] = 16;
	S9xRemasterApplyProfileToFrame(profile, frame);
	require(S9xRemasterNpcDebugColor(frame, at, RemasterNpcDebugView::Height, 0, 32)[0] == 128,
		"default NPC display range reveals midpoint gradient");
	profile.assets.clear();
	S9xRemasterApplyProfileToFrame(profile, frame);
	const auto *reset = S9xRemasterFrameMetadataForTile(frame, front.asset.tileId);
	require(!reset || !reset->hasHeight, "reset missing layer is not stale");
	require(S9xRemasterNpcDebugColor(frame, at, RemasterNpcDebugView::Height) == std::array<uint8_t, 3>{{160, 0, 180}}, "missing height distinct from zero");
	require(S9xRemasterNpcDebugColor(frame, at, RemasterNpcDebugView::Height, 0, 32) == std::array<uint8_t, 3>{{160, 0, 180}}, "range preserves missing-height color");
	require(frame.tileInstances[0].hasPaletteArtwork, "profile refresh retains palette art");
	require(S9xRemasterBuildNpcPoseFrame({front, front}, hash, frame) && frame.assets.size() == 1,
		"reuse deduplicates art without discarding occurrences");
	front.asset.tileId.hash++;
	require(!S9xRemasterBuildNpcPoseFrame({front}, hash, frame), "reject forged identity");
	require(!S9xRemasterBuildNpcPoseFrame({}, hash, frame), "reject empty pose");
	back.x = 300;
	require(!S9xRemasterBuildNpcPoseFrame({back}, hash, frame), "reject invalid bounds");
	std::cout << "NPC pose assembly, picks, flips, colors and profile refresh passed\n";
}
