#include "../tileimpl-n1x1.cpp"
#include <cstdlib>
#include <iostream>

uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;
SGFX GFX;
SBG BG;
InternalPPU IPPU;
SSettings Settings;

static void require(bool ok, const char *message)
{
	if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
// Simulate the lamp's stock subtract-to-black calculation at the real plotter.
struct DarkMath { static uint16 Calc(uint16, uint16, uint8) { return 0; } };

int main(int argc, char **argv)
{
	auto &s = S9xRemasterState();
	s.requestedLiveFrames = true;
	S9xRemasterBeginFrame(2, 2, 2, 1);
	s.tileInstances.resize(1);
	s.tileInstances[0].tileId = {123, 1, 4};
	s.currentTileHashValid = true;
	s.currentTilePixel = 9;
	S9xRemasterCurrentOwner = S9xRemasterOwner(RemasterSourceType::Background, 1, 3);
	S9xRemasterSetSubscreen(false);
	uint16 stock[2] = {}, sub[2] = {}, colors[2] = {0, 0x4210};
	uint8 depth[2] = {}, subDepth[2] = {};
	GFX.S = stock; GFX.DB = depth; GFX.SubScreen = sub; GFX.SubZBuffer = subDepth;
	GFX.ScreenColors = colors; GFX.RealScreenColors = colors;
	using Plot = TileImpl::Normal1x1Base<DarkMath, TileImpl::BPProgressive>;
	Plot::Draw(0, 1, 0, 0, 1, 46, 46);
	require(stock[0] == 0 && s.mainInstanceIds[0] == 1, "stock dark color/owner");
	s.sceneRgb.assign(2, 0); s.sceneDepth.assign(2, 0); s.scenePixels.resize(2);
	s.scenePass = true; GFX.S = s.sceneRgb.data(); GFX.DB = s.sceneDepth.data();
	Plot::Draw(0, 1, 0, 0, 1, 46, 46);
	Plot::Draw(1, 1, 0, 0, 1, 46, 46);
	require(s.sceneRgb[0] == 0x4210 && s.scenePixels[0].instanceId == 1 &&
		s.scenePixels[0].tilePixel == 9, "pre-math color + exact scene owner");
	Plot::Draw(0, 1, 0, 0, 0, 45, 45);
	require(s.sceneRgb[0] == 0x4210, "scene follows depth rejection");
	Plot::Draw(0, 0, 0, 0, 0, 47, 47);
	require(s.sceneRgb[0] == 0x4210, "transparent tile retains underlying room");
	require(stock[0] == 0 && depth[0] == 46 && s.mainInstanceIds[0] == 1,
		"scene cannot overwrite stock color/depth/ownership");
	s.scenePass = false;
	RemasterFrame f;
	require(S9xRemasterFinalizeFrame(f, stock, 2, 2, 1), "finalize independent planes");
	require(f.sceneRgb555[0] == 0x4210 && f.originalRgb555[0] == 0, "finalized colors");
	std::vector<uint8_t> bytes;
	require(S9xSerializeRemasterFrame(f, bytes), "serialize scene appendix");
	RemasterFrame decoded;
	require(S9xDeserializeRemasterFrame(bytes, decoded) && decoded.sceneRgb555 == f.sceneRgb555 &&
		decoded.scenePixels[0].tilePixel == 9, "scene round trip");
	RemasterAnimationCapture animation;
	S9xRemasterAddAnimationCaptureFrame(animation, f);
	S9xRemasterAddAnimationCaptureFrame(animation, f);
	auto animated = S9xRemasterFinishAnimationCapture(std::move(animation));
	require(animated.sceneRgb555 == f.sceneRgb555 && animated.scenePixels[0].instanceId == 1,
		"animation capture retains matching representative scene and instance table");
	bytes.pop_back();
	require(!S9xDeserializeRemasterFrame(bytes, decoded), "reject truncated scene appendix");
	f.scenePixels.clear(); f.sceneRgb555.clear();
	require(S9xSerializeRemasterFrame(f, bytes) && S9xDeserializeRemasterFrame(bytes, decoded) &&
		decoded.scenePixels.empty(), "optional appendix absent");
	require(S9xDeserializeRemasterFrame(bytes, decoded), "current schema replay without optional scene");
	f.scenePixels.resize(2); f.sceneRgb555.resize(2);
	f.scenePixels[0].instanceId = 2;
	require(!S9xSerializeRemasterFrame(f, bytes), "reject scene instance outside shared table");
	f.scenePixels[0].instanceId = 1; f.scenePixels[0].tilePixel = 64;
	require(!S9xSerializeRemasterFrame(f, bytes), "reject noncanonical scene tile pixel");
	s.scenePixels[1].owner = REMASTER_OWNER_UNSUPPORTED;
	require(S9xRemasterFinalizeFrame(f, stock, 2, 2, 1) && f.scenePixels.empty(),
		"partial/unsupported scene discarded rather than stale or dark fallback pixels");
	S9xRemasterBeginFrame(2, 2, 2, 1);
	require(s.scenePixels.empty() && s.sceneRgb.empty() && !s.scenePass, "no stale scene on next frame");
	s.requestedLiveFrames = false;
	S9xRemasterBeginFrame(2, 2, 2, 1);
	require(!S9xRemasterFramePacketActive() && !S9xRemasterCurrentOwners && s.sceneRgb.empty(),
		"disabled remaster has no scene target or packet work");
	if (argc > 1)
	{
		require(S9xReadRemasterFrame(argv[1], decoded), "read exact legacy lamp fixture");
		require(decoded.schemaVersion == 21 && decoded.scenePixels.empty(), "legacy fixture has no pre-cone scene");
		size_t room = 0, cone = 0, dark = 0;
		for (size_t i = 0; i < decoded.mainPixels.size(); i++)
		{
			if ((decoded.mainPixels[i].owner >> 16) == 0x201) { room++; dark += !decoded.originalRgb555[i]; }
			cone += (decoded.subPixels[i].owner >> 16) == 0x200;
		}
		std::cout << "Legacy fixture: BG2=" << room << " BG1-cone=" << cone << " black-BG2=" << dark << '\n';
		require(room == 55083 && cone == 7224 && dark >= 48441, "fixture BG2 room / BG1 cone evidence");
	}
	std::cout << "Lamp scene plotter/lifecycle/wire/legacy tests passed\n";
	// Full main BG2 coverage survives even when the independent pass is absent.
	s.requestedLiveFrames = true;
	S9xRemasterBeginFrame(2, 2, 2, 1, true);
	s.tileInstances.resize(1); s.currentTileHashValid = true; s.currentTilePixel = 9;
	S9xRemasterCurrentOwner = S9xRemasterOwner(RemasterSourceType::Background, 1, 3);
	S9xRemasterSetSubscreen(false);
	GFX.S = stock; GFX.DB = depth; depth[0] = depth[1] = 0;
	GFX.ScreenColors = colors; GFX.RealScreenColors = colors;
	Plot::Draw(0, 1, 0, 0, 1, 46, 46);
	Plot::Draw(1, 1, 0, 0, 1, 46, 46);
	require(stock[0] == 0 && s.mainPreMathRgb[0] == 0x4210, "retain raw winning scene despite stock darkness");
	require(S9xRemasterFinalizeFrame(f, stock, 2, 2, 1), "finalize missing independent pass");
	RemasterHeldLampState lamp; lamp.verifiedRom = lamp.darkWithLantern = true;
	require(S9xRemasterRecoverLampScene(f, s.mainPreMathRgb.data(), 2, 2, lamp) &&
		f.sceneRgb555[0] == 0x4210 && f.originalRgb555[0] == 0, "recover cone-free scene, preserve Original");
	lamp.submodule = 2;
	f.mainPixels[1].owner = S9xRemasterOwner(RemasterSourceType::Background, 2, 3);
	require(S9xRemasterHeldLampSceneContext(14, 2, 5, 7) &&
		S9xRemasterRecoverLampScene(f, s.mainPreMathRgb.data(), 2, 2, lamp) &&
		f.sceneRgb555[0] == 0x4210 && f.scenePixels[1].owner == f.mainPixels[1].owner &&
		f.originalRgb555[0] == 0, "dialogue retains cone-free room, BG3 ownership and stock Original");
	f.mainPixels[1].owner = S9xRemasterOwner(RemasterSourceType::Background, 0, 3);
	require(!S9xRemasterRecoverLampScene(f, s.mainPreMathRgb.data(), 2, 2, lamp), "reject cone-owned main composition");
	lamp.darkWithLantern = false;
	require(!S9xRemasterRecoverLampScene(f, s.mainPreMathRgb.data(), 2, 2, lamp), "never recover ordinary room as lamp scene");
	if (argc > 2)
	{
		require(S9xReadRemasterFrame(argv[2], decoded), "read reported room041 packet");
		require(decoded.schemaVersion == 28 && decoded.scenePixels.empty() && decoded.lights.size() == 1,
			"room041 reproduces missing scene with active new lamp");
		// Validate the real packet's ownership against the recovery gate. These
		// synthetic raw colors stand in for new winning pre-math samples only;
		// the old packet's missing colors cannot be reconstructed retrospectively.
		std::vector<uint16_t> raw(decoded.mainPixels.size(), 0x4210);
		const auto original = decoded.originalRgb555;
		lamp.darkWithLantern = true;
		require(S9xRemasterRecoverLampScene(decoded, raw.data(), raw.size(), decoded.width, lamp),
			"room041 complete scene ownership qualifies for raw-color recovery");
		require(decoded.originalRgb555 == original && decoded.sceneRgb555 == raw,
			"recovery never replaces original composed colors");
		require(S9xSerializeRemasterFrame(decoded, bytes) && S9xDeserializeRemasterFrame(bytes, f) &&
			f.scenePixels.size() == raw.size(), "recovered room041 scene persists for replay");
	}
}
