#include "remaster.h"
#include <cstdio>
#include <cstdlib>

static void require (bool condition, const char *message)
{
	if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

int main (int argc, char **argv)
{
	// A completely black composed frame must retain the original palette art,
	// including pixels that never won the screen depth test.
	auto &state = S9xRemasterState();
	state.liveFramesActive = true;
	state.currentDrawSupported = true;
	state.currentSource = RemasterSourceType::Background;
	uint8_t indices[64] = {};
	indices[1] = 1; indices[63] = 2;
	uint16_t palette[16] = {};
	palette[1] = 31; palette[2] = 31 << 10;
	S9xRemasterObserveTile(indices, 4, 0, 0xc000, palette);
	RemasterFrame frame;
	frame.width = frame.height = 1;
	frame.originalRgb555 = { 0 };
	frame.mainPixels = { { 1, 1, 1 } };
	frame.subPixels.resize(1);
	frame.tileInstances = state.tileInstances;
	RemasterFrameAsset asset;
	asset.tileId = frame.tileInstances[0].tileId;
	std::copy(indices, indices + 64, asset.indices);
	frame.assets.push_back(asset);
	RemasterFrameArtworkColors artwork;
	require(S9xRemasterFramePaletteArtwork(frame, asset.tileId, 1, artwork), "palette available in dark frame");
	require(artwork.rgb555[1] == 0x7c00 && artwork.rgb555[63] == 31, "full brightness RGB order and canonical flips");
	require(artwork.visiblePixels == ((UINT64_C(1) << 1) | (UINT64_C(1) << 63)), "unseen opaque pixels retained, transparent zero omitted");
	frame.paletteArtworkColors.push_back(artwork);
	frame.tileInstances.push_back(frame.tileInstances[0]);
	frame.tileInstances[1].paletteRgb555[1] = 31 << 5;
	require(S9xRemasterFramePaletteArtwork(frame, asset.tileId, 2, artwork) && artwork.rgb555[1] == (31 << 5),
		"selected occurrence uses its palette rather than another occurrence");
	frame.sceneRgb555 = { 0 };
	frame.scenePixels = frame.mainPixels;
	std::vector<uint8_t> bytes;
	RemasterFrame decoded;
	require(S9xSerializeRemasterFrame(frame, bytes) && S9xDeserializeRemasterFrame(bytes, decoded), "v30 with scene appendix round trip");
	require(decoded.originalRgb555[0] == 0 && decoded.sceneRgb555[0] == 0, "scene colors remain black");
	require(S9xRemasterFramePaletteArtwork(decoded, asset.tileId, 2, artwork) && artwork.rgb555[1] == (31 << 5), "per-instance palette persists");
	bytes.pop_back();
	require(!S9xDeserializeRemasterFrame(bytes, decoded), "truncated appendix rejected");
	RemasterFrame later = frame;
	later.assets[0].tileId.hash++;
	later.tileInstances[0].tileId = later.assets[0].tileId;
	later.paletteArtworkColors[0].tileId = later.assets[0].tileId;
	RemasterAnimationCapture animation;
	S9xRemasterAddAnimationCaptureFrame(animation, frame);
	S9xRemasterAddAnimationCaptureFrame(animation, later);
	decoded = S9xRemasterFinishAnimationCapture(animation);
	require(S9xRemasterFramePaletteArtwork(decoded, later.assets[0].tileId, 0, artwork), "later animation phase retains palette artwork");
	frame.sceneRgb555.clear(); frame.scenePixels.clear();
	require(S9xSerializeRemasterFrame(frame, bytes) && S9xDeserializeRemasterFrame(bytes, decoded), "v30 without scene appendix round trip");
	require(S9xRemasterDirectRgb555(255, 7) == ((30 << 10) | (30 << 5) | 28), "direct color ignores brightness");
	for (int i = 1; i < argc; i++)
		require(S9xReadRemasterFrame(argv[i], decoded), "legacy capture reads");
	std::puts("Palette artwork checks passed");
}
