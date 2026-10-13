#include "frame.h"
#include <cassert>
#include <iostream>

int main ()
{
	RemasterProfile profile;
	profile.gameTitle = "Door opening with stair geometry";
	profile.romSha256 = std::string(64, 'a');
	profile.schemaVersion = 20;
	const RemasterTileContentId id {123, 1, 4}, animation {124, 1, 4};
	auto &door = profile.assets[id];
	door.tileId = id;
	door.tileRole = RemasterTileRole::Door;
	door.hasHeight = door.hasNormals = door.hasTileSemantics = true;
	door.tileSemanticNote = "opening may contain stairs";
	for (unsigned i = 0; i < 64; i++) door.height[i] = i / 8;
	door.normalXyz.fill(128);
	const auto heights = door.height;
	const auto normals = door.normalXyz;
	std::string text;
	std::vector<RemasterProfileDiagnostic> errors;
	RemasterProfile decoded;
	assert(S9xRemasterSerializeProfile(profile, text, errors));
	assert(text.find("schema_version = 21") != std::string::npos);
	assert(text.find("doorframe_mask") == std::string::npos); // unknown, not empty
	assert(S9xRemasterParseProfile(text, decoded, errors));
	assert(!decoded.assets.at(id).hasDoorframeMask);
	assert(S9xRemasterPaintDoorframe(door, 0, false));
	assert(door.hasDoorframeMask && door.doorframeMask == 0);
	assert(S9xRemasterSerializeProfile(profile, text, errors));
	assert(text.find("doorframe_mask = \"0000000000000000\"") != std::string::npos);
	assert(S9xRemasterParseProfile(text, decoded, errors));
	assert(decoded.assets.at(id).hasDoorframeMask && decoded.assets.at(id).doorframeMask == 0);
	for (const auto &bad : {"0", "000000000000000g", "000000000000000A", "-000000000000001", "0x00000000000000"})
	{
		auto invalid = text;
		invalid.replace(invalid.find("0000000000000000"), 16, bad);
		assert(!S9xRemasterParseProfile(invalid, decoded, errors));
	}
	auto invalid = text;
	invalid.replace(invalid.find("schema_version = 21"), 19, "schema_version = 20");
	assert(!S9xRemasterParseProfile(invalid, decoded, errors));
	invalid = text;
	invalid.replace(invalid.find("tile_role = \"door\""), 18, "tile_role = \"floor\"");
	assert(!S9xRemasterParseProfile(invalid, decoded, errors));
	assert(S9xRemasterPaintDoorframe(door, 0, true));
	assert(S9xRemasterPaintDoorframe(door, 17, true));
	assert(S9xRemasterPaintDoorframe(door, 63, true));
	assert(door.doorframeMask == UINT64_C(0x8000000000020001));
	for (bool h : {false, true}) for (bool v : {false, true})
	{
		assert(S9xRemasterDoorframePixel(door.doorframeMask, h ? 7 : 0, v ? 7 : 0, h, v));
		assert(S9xRemasterDoorframePixel(door.doorframeMask, h ? 6 : 1, v ? 5 : 2, h, v));
		assert(!S9xRemasterDoorframePixel(door.doorframeMask, 3, 3, h, v));
	}
	const auto undo = door;
	assert(S9xRemasterPaintDoorframe(door, 17, false));
	door = undo; // same full-asset snapshots used by native undo
	auto &phase = profile.assets[animation];
	phase.tileId = animation;
	phase.tileRole = RemasterTileRole::Door;
	phase.hasHeight = true;
	phase.height.fill(47);
	S9xRemasterCopyDoorframe(door, phase);
	assert(phase.height[0] == 47 && phase.doorframeMask == door.doorframeMask);
	assert(door.height == heights && door.normalXyz == normals);
	RemasterFrame frame;
	frame.width = frame.height = 1;
	frame.originalRgb555.resize(1); frame.mainPixels.resize(1); frame.subPixels.resize(1);
	RemasterFrameAsset asset; asset.tileId = id; frame.assets.push_back(asset);
	S9xRemasterApplyProfileToFrame(profile, frame);
	std::vector<uint8_t> bytes;
	assert(S9xSerializeRemasterFrame(frame, bytes));
	RemasterFrame packet;
	assert(S9xDeserializeRemasterFrame(bytes, packet));
	assert(packet.assetMetadata[0].hasDoorframeMask && packet.assetMetadata[0].doorframeMask == door.doorframeMask);
	assert(packet.assetMetadata[0].height == heights);
	for (unsigned cut = 1; cut <= 8; cut++)
	{
		auto truncated = bytes;
		truncated.resize(truncated.size() - cut);
		assert(!S9xDeserializeRemasterFrame(truncated, packet));
	}
	const size_t withMask = bytes.size();
	profile.assets[id].hasDoorframeMask = false;
	S9xRemasterApplyProfileToFrame(profile, frame);
	assert(S9xSerializeRemasterFrame(frame, bytes));
	assert(bytes.size() + 8 == withMask);
	assert(S9xDeserializeRemasterFrame(bytes, packet) && !packet.assetMetadata[0].hasDoorframeMask);
	const auto unknownBytes = bytes;
	profile.assets[id].hasDoorframeMask = true;
	profile.assets[id].doorframeMask = 0;
	S9xRemasterApplyProfileToFrame(profile, frame);
	assert(S9xSerializeRemasterFrame(frame, bytes));
	assert(S9xDeserializeRemasterFrame(bytes, packet));
	assert(packet.assetMetadata[0].hasDoorframeMask && packet.assetMetadata[0].doorframeMask == 0);
	const size_t presenceOffset = std::mismatch(unknownBytes.begin(), unknownBytes.end(), bytes.begin()).first - unknownBytes.begin();
	assert(presenceOffset < unknownBytes.size());
	auto malformedPacket = bytes;
	malformedPacket[presenceOffset] = 2;
	assert(!S9xDeserializeRemasterFrame(malformedPacket, packet));
	malformedPacket.resize(presenceOffset + 8); // seven mask bytes, not eight
	malformedPacket[presenceOffset] = 1;
	assert(!S9xDeserializeRemasterFrame(malformedPacket, packet));
	profile.assets[id].hasDoorframeMask = false;
	S9xRemasterApplyProfileToFrame(profile, frame);
	assert(S9xSerializeRemasterFrame(frame, bytes));
	const size_t doorUnknown = bytes.size();
	profile.assets[id].tileRole = RemasterTileRole::Other;
	S9xRemasterApplyProfileToFrame(profile, frame);
	assert(S9xSerializeRemasterFrame(frame, bytes));
	assert(bytes.size() + 1 == doorUnknown); // non-door pays no extension byte
	bytes.erase(bytes.begin() + presenceOffset); // v31 normal-transform option follows metadata payload
	bytes.erase(bytes.end() - 4, bytes.end()); // v30 empty palette-artwork count
	bytes[8] = 28; // legacy v28 non-door layout remains decodable
	assert(S9xDeserializeRemasterFrame(bytes, packet));
	assert(!packet.assetMetadata[0].hasDoorframeMask);
	// Refresh replaces mask absence as well as presence; stale capture data is not resurrected.
	profile.assets[id].tileRole = RemasterTileRole::Door;
	profile.assets[id].tileSemanticContextDependent = true;
	S9xRemasterApplyProfileToFrame(profile, frame);
	assert(frame.assetMetadata[0].tileRole == RemasterTileRole::Door && !frame.assetMetadata[0].hasDoorframeMask);
	RemasterAnimationCapture capture;
	S9xRemasterAddAnimationCaptureFrame(capture, frame);
	RemasterFrame phaseFrame = frame;
	phaseFrame.assets[0].tileId = animation;
	S9xRemasterApplyProfileToFrame(profile, phaseFrame);
	S9xRemasterAddAnimationCaptureFrame(capture, phaseFrame);
	assert(capture.representativeFrame.assetMetadata.size() == 2);
	assert(capture.representativeFrame.assetMetadata[1].doorframeMask == undo.doorframeMask);
	profile.assets[id].tileRole = RemasterTileRole::Other;
	profile.assets.erase(animation);
	assert(S9xRemasterSerializeProfile(profile, text, errors));
	assert(text.find("doorframe_mask") == std::string::npos);
	std::cout << "Doorframe profile, capture, transform and edit checks passed\n";
}
