#include "../runtime/held_lamp.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main(int argc, char **argv)
{
	assert(S9xRemasterHeldLampDungeonContext(7, 0, 0));
	assert(S9xRemasterHeldLampSceneContext(7, 0, 0, 0));
	assert(!S9xRemasterHeldLampSceneContext(7, 2, 0, 7)); // room transition
	assert(!S9xRemasterHeldLampSceneContext(7, 0, 1, 7));
	for (unsigned submodule = 0; submodule < 256; submodule++)
		for (unsigned textStage = 0; textStage < 256; textStage++)
		{
			assert(S9xRemasterHeldLampDungeonContext(14, submodule, 7) == (submodule == 2));
			assert(S9xRemasterHeldLampSceneContext(14, submodule, textStage, 7) == (submodule == 2));
			assert(!S9xRemasterHeldLampDungeonContext(14, submodule, 9)); // overworld interface
			assert(!S9xRemasterHeldLampSceneContext(14, submodule, textStage, 9));
		}
	RemasterHeldLampState state;
	state.verifiedRom = state.darkWithLantern = true;
	state.linkScreenX = 120; state.linkScreenY = 177; state.floorHeight = 24;
	RemasterHeldLampSettings settings;
	RemasterFrameLight light;
	const float xs[] = {128, 128, 118, 138}, ys[] = {179, 199, 189, 189};
	for (unsigned d = 0; d < 4; d++) {
		state.facing = d * 2;
		assert(S9xRemasterHeldLampLight(state, settings, light));
		assert(light.x == xs[d] && light.y == ys[d] && light.z == 36);
		assert(light.radius == 2 && light.intensity == 200);
		assert(light.hideSource);
		assert(light.red == 1.0f && light.green == 0.72f && light.blue == 0.38f);
	}
	state.submodule = 2; // RenderText uses its own subsubmodule stages.
	assert(S9xRemasterHeldLampLight(state, settings, light) && light.hideSource);
	state.submodule = 0;
	state.darkWithLantern = false;
	assert(!S9xRemasterHeldLampLight(state, settings, light));
	state.darkWithLantern = true; state.submodule = 20;
	assert(!S9xRemasterHeldLampLight(state, settings, light));
	state.submodule = 0; state.verifiedRom = false;
	assert(!S9xRemasterHeldLampLight(state, settings, light));
	state.verifiedRom = true; state.facing = 1;
	assert(!S9xRemasterHeldLampLight(state, settings, light));
	state.facing = 6; settings.intensity = std::numeric_limits<float>::quiet_NaN();
	assert(!S9xRemasterHeldLampLight(state, settings, light));
	settings.intensity = 25; state.linkScreenX = -20;
	assert(S9xRemasterHeldLampLight(state, settings, light) && light.x == -2);
	RemasterFrame frame;
	frame.width = 256; frame.height = 224;
	frame.profileRomSha256 = "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
	frame.mainPixels.resize(256 * 224);
	frame.mainPixels[63 * 256 + 255].owner = 0x02020000; // actual BG3 ink
	assert(S9xRemasterHeldLampHudPixel(frame, 63 * 256 + 255));
	frame.mainPixels[16 * 256 + 120].owner = 0x02010000; // room behind transparent HUD
	assert(!S9xRemasterHeldLampHudPixel(frame, 16 * 256 + 120));
	frame.mainPixels[0].owner = 0x03000000; // world sprite near top
	assert(!S9xRemasterHeldLampHudPixel(frame, 0));
	frame.tileInstances.resize(1);
	frame.tileInstances[0].material = "hud";
	RemasterFrameMaterial hudMaterial;
	hudMaterial.name = "hud"; hudMaterial.surfaceClass = RemasterSurfaceClass::UserInterface;
	frame.materials.push_back(hudMaterial);
	frame.mainPixels[0].instanceId = 1; // explicitly classified HUD sprite
	assert(S9xRemasterHeldLampHudPixel(frame, 0));
	assert(!S9xRemasterHeldLampHudPixel(frame, 64 * 256));
	const size_t dialogue = 160 * 256 + 80;
	frame.mainPixels[dialogue].owner = 0x02020000;
	assert(S9xRemasterHeldLampHudPixel(frame, dialogue));
	frame.mainPixels[dialogue + 1].owner = 0x02010000;
	assert(!S9xRemasterHeldLampHudPixel(frame, dialogue + 1)); // room between letters
	frame.mainPixels[dialogue + 2].owner = 0x02000000;
	frame.mainPixels[dialogue + 2].instanceId = 1;
	assert(S9xRemasterHeldLampHudPixel(frame, dialogue + 2)); // authored UI on another layer
	assert(!S9xRemasterHeldLampHudPixel(frame, frame.mainPixels.size()));
	// Presentation survives capture/replay without changing physical light fields.
	frame.originalRgb555.resize(frame.mainPixels.size());
	frame.subPixels.resize(frame.mainPixels.size());
	frame.mainPixels.assign(frame.mainPixels.size(), {});
	frame.tileInstances.clear();
	frame.lights = {light, RemasterFrameLight{}};
	std::vector<uint8_t> bytes;
	RemasterFrame replay;
	assert(S9xSerializeRemasterFrame(frame, bytes) && S9xDeserializeRemasterFrame(bytes, replay));
	assert(replay.lights.size() == 2 && replay.lights[0].hideSource && !replay.lights[1].hideSource);
	assert(replay.lights[0].x == light.x && replay.lights[0].y == light.y && replay.lights[0].z == light.z);
	assert(replay.lights[0].radius == light.radius && replay.lights[0].intensity == light.intensity);
	assert(replay.lights[0].red == light.red && replay.lights[0].green == light.green && replay.lights[0].blue == light.blue);
	const auto validBytes = bytes;
	bytes.back() = 255; // invalid sparse light index
	assert(!S9xDeserializeRemasterFrame(bytes, replay));
	bytes = validBytes;
	bytes.insert(bytes.end(), validBytes.end() - 12, validBytes.end()); // duplicate LVP1
	assert(!S9xDeserializeRemasterFrame(bytes, replay));
	frame.lights[0].hideSource = false;
	assert(S9xSerializeRemasterFrame(frame, bytes) && S9xDeserializeRemasterFrame(bytes, replay));
	assert(!replay.lights[0].hideSource && !replay.lights[1].hideSource); // no appendix, legacy default
	if (argc > 1) {
		assert(S9xReadRemasterFrame(argv[1], frame));
		unsigned hud = 0, cone = 0, room = 0, actor = 0, knownActor = 0;
		for (size_t i = 0; i < frame.mainPixels.size(); i++) {
			hud += S9xRemasterHeldLampHudPixel(frame, i);
			const auto *main = S9xRemasterFrameInstanceAt(frame, i % frame.width, i / frame.width);
			if (main && main->source == RemasterSourceType::Background && main->sourceIndex == 1) cone++;
			if (main && main->source == RemasterSourceType::Object && main->vramAddress >= 0x8000 && main->vramAddress < 0x8400) {
				actor++;
				const auto *metadata = S9xRemasterFrameMetadataForTile(frame, main->tileId);
				knownActor += metadata && metadata->hasHeight;
			}
			const auto &sub = frame.subPixels[i];
			if (sub.instanceId && frame.tileInstances[sub.instanceId-1].source == RemasterSourceType::Background) room++;
		}
		assert(hud > 0 && hud < 16384);
		for (size_t i = 0; i < 64 * frame.width; i++)
			if ((frame.mainPixels[i].owner >> 16) == 0x0201)
				assert(!S9xRemasterHeldLampHudPixel(frame, i));
		std::printf("capture v%u: HUD ink=%u cone main=%u room sub=%u Link ink=%u height-known Link=%u lights=%zu\n",
			frame.schemaVersion, hud, cone, room, actor, knownActor, frame.lights.size());
	}
	std::puts("Held-lamp anchor/state/HUD tests passed");
}
