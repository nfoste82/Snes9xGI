#include "remaster.h"
#include <cassert>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;

int main()
{
	RemasterProfile profile;
	profile.schemaVersion = 25;
	profile.gameTitle = "Area lighting";
	profile.romSha256 = std::string(64, 'a');
	assert(S9xRemasterAreaLight(profile, {97, false}).enabled);
	assert(S9xRemasterDirectionalBrightnessToUi(S9xRemasterAreaLight(profile, {97, false}).brightness) == 4);
	assert(S9xRemasterDirectionalBrightnessToUi(0.05f) == 1);
	assert(S9xRemasterDirectionalBrightnessFromUi(20) == 1);
	assert(S9xRemasterDirectionalBrightnessFromUi(100) == 5);
	assert(S9xRemasterDirectionalBrightnessFromUi(0) == 0);
	assert(!S9xRemasterAreaLight(profile, {97, true}).enabled);
	assert(!S9xRemasterAreaLight(profile, {}).enabled);
	assert(S9xRemasterAreaLight(profile, {320, false}).enabled);
	const auto indoorDefault = S9xRemasterAreaLight(profile, {97, false}).direction;
	assert(std::abs(indoorDefault[1] - (-0.488 / std::hypot(0.488, 0.87))) < 1e-7);
	assert(std::abs(indoorDefault[2] - (-0.87 / std::hypot(0.488, 0.87))) < 1e-7);
	assert(std::abs(indoorDefault[1]*indoorDefault[1] + indoorDefault[2]*indoorDefault[2] - 1) < 1e-7);
	assert((S9xRemasterAreaLight(profile, {320, false}).direction == std::array<float,3>{{0,0,-1}}));
	assert((S9xRemasterAreaLight(profile, {321, false}).direction == std::array<float,3>{{0,0,-1}}));
	assert(S9xRemasterAreaKey(97) == "room_061");
	RemasterDirectionalLight authored;
	authored.direction = {{1, -2, -3}};
	authored.color = {{0.3f, 0.5f, 0.7f}};
	authored.brightness = 2;
	profile.areaLights["room_061"] = authored;
	profile.areaLights["light_world"].brightness = 3;
	profile.areaLights["dark_world"].enabled = false;
	assert(S9xRemasterAreaLight(profile, {97, true}) == authored);
	assert(S9xRemasterAreaLight(profile, {320, false}).brightness == 3);
	assert(!S9xRemasterAreaLight(profile, {321, false}).enabled);
	// Room IDs change during construction: only visible camera movement starts the fade.
	RemasterAreaLightTransition transition;
	RemasterDirectionalLight outgoing, incoming;
	outgoing.brightness = 1; incoming.brightness = 3;
	auto near = [](float a, float b) { assert(std::abs(a-b) < 0.00001f); };
	near(transition.update({97,false}, outgoing, 100, false, 0, 0).brightness, 1);
	for (uint32_t f = 101; f < 110; ++f)
		near(transition.update({98,false}, incoming, f, true, 0, 0).brightness, 1);
	near(transition.update({98,false}, incoming, 109, true, 1, 0, false).brightness, 1);
	near(transition.update({98,false}, incoming, 110, true, 4, 0).brightness, 1.1f);
	// Re-presenting the same emulated frame cannot advance the fade.
	near(transition.update({98,false}, incoming, 110, true, 4, 0).brightness, 1.1f);
	near(transition.update({98,false}, incoming, 119, true, 40, 0).brightness, 2);
	near(transition.update({98,false}, incoming, 129, true, 80, 0).brightness, 3);
	incoming.brightness = 4;
	near(transition.update({98,false}, incoming, 130, false, 80, 0).brightness, 4);
	// Disabled destinations fade to zero, then stop dispatching the light.
	incoming.enabled = false;
	auto fading = transition.update({99,true}, incoming, 131, true, 80, 4);
	assert(fading.enabled); near(fading.brightness, 3.8f);
	fading = transition.update({99,true}, incoming, 150, false, 80, 80);
	assert(!fading.enabled);
	incoming.enabled = true; incoming.brightness = 2;
	near(transition.update({98,false}, incoming, 151, true, 80, 76).brightness, 0.1f);
	// An interrupted fade starts from its current value, with either scroll axis.
	near(transition.update({97,false}, outgoing, 152, true, 80, 72).brightness, 0.145f);
	// An emulated-frame counter discontinuity resets the transient history.
	near(transition.update({98,false}, incoming, 20, false, 0, 0).brightness, 2);
	std::string text; std::vector<RemasterProfileDiagnostic> errors;
	assert(S9xRemasterSerializeProfile(profile, text, errors));
	RemasterProfile decoded;
	assert(S9xRemasterParseProfile(text, decoded, errors));
	assert(decoded.schemaVersion == 26 && decoded.areaLights == profile.areaLights);
	RemasterProfile partialDefaults;
	assert(S9xRemasterParseProfile(text + "\n[area_lights.room_062]\nbrightness = 0.4\n", partialDefaults, errors));
	assert(partialDefaults.areaLights.at("room_062").direction == indoorDefault);
	// Emission depth must not mask the newer schema required by area lighting.
	RemasterProfile withDepth = profile;
	RemasterTileContentId depthTile;
	depthTile.hashVersion = 1; depthTile.bitDepth = 4; depthTile.hash = 1;
	withDepth.assets[depthTile].tileId = depthTile;
	withDepth.assets[depthTile].emissionDepth = 2;
	assert(S9xRemasterSerializeProfile(withDepth, text, errors));
	assert(S9xRemasterParseProfile(text, decoded, errors));
	assert(decoded.schemaVersion == 26 && decoded.areaLights == profile.areaLights);
	assert(decoded.assets.at(depthTile).emissionDepth == 2);
	assert(S9xRemasterSerializeProfile(profile, text, errors));
	const auto validText = text;
	text.replace(text.find("schema_version = 26"), 19, "schema_version = 25");
	assert(!S9xRemasterParseProfile(text, decoded, errors));
	assert(!S9xRemasterParseProfile(validText + "\n[area_lights.room_061]\n", decoded, errors));
	assert(!S9xRemasterParseProfile(validText + "\n[area_lights.room_fff]\n", decoded, errors));
	for (auto invalid : {std::array<float,3>{{0,0,0}}, std::array<float,3>{{NAN,0,-1}}})
	{
		profile.areaLights["room_061"].direction = invalid;
		assert(!S9xRemasterSerializeProfile(profile, text, errors));
	}
	assert(S9xRemasterParseProfile(validText, profile, errors));
	RemasterFrame frame; frame.width = frame.height = 1;
	frame.originalRgb555 = {0}; frame.mainPixels.resize(1); frame.subPixels.resize(1);
	frame.area = {97, true}; frame.directionalLight = authored;
	std::vector<uint8_t> bytes;
	assert(S9xSerializeRemasterFrame(frame, bytes));
	RemasterFrame replay;
	assert(S9xDeserializeRemasterFrame(bytes, replay));
	assert(replay.area.id == 97 && replay.area.romDark && replay.directionalLight == authored);
	bytes.pop_back(); assert(!S9xDeserializeRemasterFrame(bytes, replay));
	frame.area = {}; assert(S9xSerializeRemasterFrame(frame, bytes));
	assert(S9xDeserializeRemasterFrame(bytes, replay) && !replay.directionalLight.enabled);
	frame.area = {321, false}; S9xRemasterApplyProfileToFrame(profile, frame);
	assert(!frame.directionalLight.enabled);
	// Area edits publish independently of the large profile and do not invalidate geometry.
	S9xRemasterSetProfile(profile); S9xRemasterBeginFrame(1,1,1,1);
	profile.areaLights["dark_world"].enabled = true;
	S9xRemasterSetAreaLights(profile.areaLights);
	assert(!S9xRemasterState().activeProfile.areaLights["dark_world"].enabled);
	S9xRemasterBeginFrame(1,1,1,1);
	assert(S9xRemasterState().activeProfile.areaLights["dark_world"].enabled);
	RemasterAreaContext area{97, true};
	S9xRemasterEndFrame(nullptr,0,0,0,nullptr,nullptr,nullptr,&area);
	assert(S9xRemasterCurrentArea().id == 97 && S9xRemasterCurrentArea().romDark);
	S9xRemasterClearProfile(); assert(!S9xRemasterCurrentArea().valid());
	std::cout << "PASS area defaults, isolation, 20-frame transitions, validation, TOML/capture round trips and frame-boundary publication\n";
}
