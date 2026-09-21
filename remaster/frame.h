/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REMASTER_FRAME_H_
#define _REMASTER_FRAME_H_

#include "profile.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static const uint32_t REMASTER_FRAME_SCHEMA_VERSION = 1;

struct RemasterFramePixel
{
	uint32_t owner = 0xffffffffu;
	uint32_t instanceId = 0;
};

struct RemasterFrameAsset
{
	RemasterTileContentId tileId;
	uint8_t indices[64] = {};
};

struct RemasterFrameMaterial
{
	std::string name;
	RemasterSurfaceClass surfaceClass = RemasterSurfaceClass::Unclassified;
	float roughness = 0.8f;
	float metalness = 0.0f;
	float specularLevel = 0.25f;
	float zMin = 0.0f;
	float zMax = 0.0f;
	bool receivesGi = true;
	bool castsShadow = false;
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
	RemasterProfileMatchStatus matchStatus = RemasterProfileMatchStatus::NoMatch;
	std::string assetGroup;
	std::string material;
};

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
	std::vector<uint16_t> originalRgb555;
	std::vector<RemasterFramePixel> mainPixels;
	std::vector<RemasterFramePixel> subPixels;
	std::vector<RemasterFrameAsset> assets;
	std::vector<RemasterFrameMaterial> materials;
	std::vector<RemasterFrameTileInstance> tileInstances;
	std::vector<RemasterFrameLight> lights;
};

namespace RemasterFrameSerialization
{
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
	uint32_t materialCount = 0;
	uint32_t instanceCount = 0;
	uint32_t lightCount = 0;
	if (!input.ReadU32(result.schemaVersion) || result.schemaVersion != REMASTER_FRAME_SCHEMA_VERSION ||
		!input.ReadU32(result.width) || !input.ReadU32(result.height) || !result.width || !result.height ||
		result.width > UINT32_MAX / result.height || !input.ReadString(result.profileRomSha256) ||
		!input.ReadU32(assetCount) || !input.ReadU32(materialCount) ||
		!input.ReadU32(instanceCount) || !input.ReadU32(lightCount))
		return false;
	const size_t pixelCount = static_cast<size_t>(result.width) * result.height;
	if (pixelCount > bytes.size() / 2 || assetCount > bytes.size() / 74 ||
		materialCount > bytes.size() || instanceCount > bytes.size() / 25 || lightCount > bytes.size() / 32)
		return false;

	result.originalRgb555.resize(pixelCount);
	result.mainPixels.resize(pixelCount);
	result.subPixels.resize(pixelCount);
	for (uint16_t &color : result.originalRgb555)
		if (!input.ReadU16(color))
			return false;
	for (RemasterFramePixel &pixel : result.mainPixels)
		if (!input.ReadU32(pixel.owner) || !input.ReadU32(pixel.instanceId))
			return false;
	for (RemasterFramePixel &pixel : result.subPixels)
		if (!input.ReadU32(pixel.owner) || !input.ReadU32(pixel.instanceId))
			return false;

	result.assets.resize(assetCount);
	for (RemasterFrameAsset &asset : result.assets)
	{
		if (!input.ReadTileId(asset.tileId) || input.offset + 64 > bytes.size())
			return false;
		std::copy(bytes.begin() + input.offset, bytes.begin() + input.offset + 64, asset.indices);
		input.offset += 64;
	}
	result.materials.resize(materialCount);
	for (RemasterFrameMaterial &material : result.materials)
	{
		uint8_t surfaceClass = 0;
		uint8_t receivesGi = 0;
		uint8_t castsShadow = 0;
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
	}
	result.tileInstances.resize(instanceCount);
	for (RemasterFrameTileInstance &instance : result.tileInstances)
	{
		uint8_t source = 0;
		uint8_t matchStatus = 0;
		if (!input.ReadTileId(instance.tileId) || !input.ReadU8(source) ||
			source < static_cast<uint8_t>(RemasterSourceType::Backdrop) ||
			source > static_cast<uint8_t>(RemasterSourceType::Object) ||
			!input.ReadU8(instance.sourceIndex) || !input.ReadU16(instance.tileNumber) ||
			!input.ReadU8(instance.palette) || !input.ReadU16(instance.vramAddress) ||
			!input.ReadU8(matchStatus) ||
			matchStatus > static_cast<uint8_t>(RemasterProfileMatchStatus::Ambiguous) ||
			!input.ReadU32(instance.ruleLine) || !input.ReadString(instance.assetGroup) ||
			!input.ReadString(instance.material))
			return false;
		instance.source = static_cast<RemasterSourceType>(source);
		instance.matchStatus = static_cast<RemasterProfileMatchStatus>(matchStatus);
	}
	result.lights.resize(lightCount);
	for (RemasterFrameLight &light : result.lights)
		if (!input.ReadFloat(light.x) || !input.ReadFloat(light.y) || !input.ReadFloat(light.z) ||
			!input.ReadFloat(light.radius) || !input.ReadFloat(light.red) || !input.ReadFloat(light.green) ||
			!input.ReadFloat(light.blue) || !input.ReadFloat(light.intensity))
			return false;
	if (input.offset != bytes.size())
		return false;
	for (const RemasterFramePixel &pixel : result.mainPixels)
		if (pixel.instanceId > result.tileInstances.size())
			return false;
	for (const RemasterFramePixel &pixel : result.subPixels)
		if (pixel.instanceId > result.tileInstances.size())
			return false;
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

	bytes.clear();
	const uint8_t magic[] = { 'S', '9', 'X', 'R', 'M', 'F', 0, 1 };
	bytes.insert(bytes.end(), magic, magic + sizeof(magic));
	RemasterFrameSerialization::U32(bytes, frame.schemaVersion);
	RemasterFrameSerialization::U32(bytes, frame.width);
	RemasterFrameSerialization::U32(bytes, frame.height);
	if (!RemasterFrameSerialization::String(bytes, frame.profileRomSha256) ||
		!RemasterFrameSerialization::Size(bytes, frame.assets.size()) ||
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
	}
	for (const RemasterFramePixel &pixel : frame.subPixels)
	{
		RemasterFrameSerialization::U32(bytes, pixel.owner);
		RemasterFrameSerialization::U32(bytes, pixel.instanceId);
	}
	for (const RemasterFrameAsset &asset : frame.assets)
	{
		RemasterFrameSerialization::TileId(bytes, asset.tileId);
		bytes.insert(bytes.end(), asset.indices, asset.indices + 64);
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
	}
	for (const RemasterFrameTileInstance &instance : frame.tileInstances)
	{
		RemasterFrameSerialization::TileId(bytes, instance.tileId);
		RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(instance.source));
		RemasterFrameSerialization::U8(bytes, instance.sourceIndex);
		RemasterFrameSerialization::U16(bytes, instance.tileNumber);
		RemasterFrameSerialization::U8(bytes, instance.palette);
		RemasterFrameSerialization::U16(bytes, instance.vramAddress);
		RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(instance.matchStatus));
		RemasterFrameSerialization::U32(bytes, instance.ruleLine);
		if (!RemasterFrameSerialization::String(bytes, instance.assetGroup) ||
			!RemasterFrameSerialization::String(bytes, instance.material))
			return false;
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
