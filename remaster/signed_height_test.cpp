#include "remaster.h"
#include "lighting_field_cache.h"
#include "inspector_height.h"
#include <cassert>
#include <iostream>

int main()
{
	RemasterTileContentId id{UINT64_C(0x0123456789abcdef), 1, 4};
	RemasterProfile p; p.schemaVersion = 25; p.gameTitle = "Signed heights";
	p.romSha256 = std::string(64, '0'); p.lightingCoordinateScale = 255;
	auto &a = p.assets[id]; a.tileId = id; a.hasHeight = true;
	a.height.fill(-8); a.height[0] = INT16_MIN; a.height[1] = INT16_MAX; a.height[2] = 512;
	a.geometryByFacing = a.geometryByFlip = true;
	a.facingGeometry[0].hasHeight = a.flipGeometry[3].hasHeight = true;
	a.facingGeometry[0].height.fill(-300); a.flipGeometry[3].height.fill(1024);
	std::string text; std::vector<RemasterProfileDiagnostic> diagnostics;
	assert(S9xRemasterSerializeProfile(p, text, diagnostics));
	RemasterProfile loaded; assert(S9xRemasterParseProfile(text, loaded, diagnostics));
	assert(loaded.schemaVersion == 27 && loaded.assets.at(id).height == a.height);
	assert(loaded.assets.at(id).facingGeometry[0].height == a.facingGeometry[0].height);
	assert(loaded.assets.at(id).flipGeometry[3].height == a.flipGeometry[3].height);
	auto old = text; old.replace(old.find("schema_version = 27"), 19, "schema_version = 25");
	assert(!S9xRemasterParseProfile(old, loaded, diagnostics));
	for (const std::string invalid : {"-32769", "32768", "1.5"}) {
		auto bad = text; bad.replace(bad.find("-32768"), 6, invalid);
		assert(!S9xRemasterParseProfile(bad, loaded, diagnostics));
	}
	assert(S9xRemasterParseProfile(text, loaded, diagnostics));
	RemasterFrame f; f.width = 8; f.height = 8;
	f.originalRgb555.resize(64); f.mainPixels.resize(64); f.subPixels.resize(64);
	RemasterFrameTileInstance instance; instance.tileId = id; instance.source = RemasterSourceType::Background;
	instance.sourceIndex = 1; instance.heightOffset = -16; f.tileInstances.push_back(instance);
	for (unsigned k = 0; k < 64; ++k) { f.mainPixels[k].instanceId = 1; f.mainPixels[k].tilePixel = k; }
	S9xRemasterApplyProfileToFrame(loaded, f);
	std::vector<uint8_t> bytes; assert(S9xSerializeRemasterFrame(f, bytes));
	RemasterFrame decoded; assert(S9xDeserializeRemasterFrame(bytes, decoded));
	assert(decoded.schemaVersion == 35 && decoded.assetMetadata[0].height == a.height);
	assert(decoded.assetMetadata[0].facingGeometry[0].height == a.facingGeometry[0].height);
	assert(decoded.assetMetadata[0].flipGeometry[3].height == a.flipGeometry[3].height);
	RemasterLightingFieldCache fields; fields.update(decoded, false);
	auto near = [](float a, float b) { return std::abs(a-b) < 0.01f; };
	assert(near(fields.surface[0], -32784) && near(fields.surface[4], 32751) && near(fields.surface[8], 496));
	assert(near(fields.surface[12], -24));
	decoded.assetMetadata[0].height[3] = 248; // Same low byte as -8, distinct geometry.
	fields.update(decoded, false); assert(near(fields.surface[12], 232) && fields.updatedPixels);
	decoded.tileInstances[0].hasPlacementHeight = true;
	decoded.tileInstances[0].placementHeight.fill(-512);
	assert(S9xSerializeRemasterFrame(decoded, bytes));
	assert(S9xDeserializeRemasterFrame(bytes, f));
	assert(f.tileInstances[0].placementHeight[0] == -512);
	fields.update(f, false); assert(near(fields.surface[0], -528));
	const auto effective = S9xRemasterInspectorEffectiveHeight(&f.tileInstances[0], &f.assetMetadata[0].height);
	assert(effective.at(0) == -528);
	// Legacy unsigned samples retain their value; v35 is explicit signed LE.
	std::vector<uint8_t> legacy{0, 128, 255}, modern{0,128,255,127,255,255};
	RemasterFrameSerialization::Reader r(legacy), s(modern); int16_t sample;
	for (int expected : {0,128,255}) { assert(r.ReadHeight(sample,33)); assert(sample == expected); }
	for (int expected : {-32768,32767,-1}) { assert(s.ReadHeight(sample,35)); assert(sample == expected); }
	assert(!s.ReadHeight(sample,35));
	std::cout << "signed profile/capture/lighting/inspector/cache regression passed\n";
}
