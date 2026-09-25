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
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

static const uint32_t REMASTER_FRAME_SCHEMA_VERSION = 17;

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

struct RemasterFrameAssetGroup
{
	std::string name;
	std::vector<RemasterTileContentId> tileIds;
};

struct RemasterFrameAssetMetadata
{
	RemasterTileContentId tileId;
	std::array<std::string, 64> materialSelectors;
	std::array<uint8_t, 64> occlusion = {};
	std::array<uint8_t, 64> height = {};
	std::array<uint8_t, 192> normalXyz = {};
	std::array<uint8_t, 256> emissionRgba = {};
	bool hasMaterialSelectors = false;
	bool hasOcclusion = false;
	bool hasHeight = false;
	bool hasNormals = false;
	bool hasEmission = false;
	bool directLightingOppositeFacing = false;
	RemasterHeightSampling heightSampling = RemasterHeightSampling::Nearest;
};

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

struct RemasterFrameTileInstance
{
	RemasterTileContentId tileId;
	RemasterSourceType source = RemasterSourceType::Backdrop;
	uint32_t ruleLine = 0;
	uint16_t tileNumber = 0;
	uint16_t vramAddress = 0;
	uint8_t sourceIndex = 0;
	uint8_t palette = 0;
	bool hFlip = false;
	bool vFlip = false;
	RemasterProfileMatchStatus matchStatus = RemasterProfileMatchStatus::NoMatch;
	std::string assetGroup;
	std::string material;
};

inline void S9xRemasterTransformNormalForTileInstance (const RemasterFrameTileInstance &instance,
	float &x, float &y, float &z)
{
	if (instance.hFlip)
		x = -x;
	if (instance.vFlip)
		y = -y;
	(void) z;
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
};

struct RemasterFrame
{
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
	bool sampleAccumulation = true;
	std::vector<uint16_t> originalRgb555;
	std::vector<RemasterFramePixel> mainPixels;
	std::vector<RemasterFramePixel> subPixels;
	std::vector<RemasterFrameAsset> assets;
	std::vector<RemasterFrameArtworkColors> artworkColors;
	std::vector<RemasterFrameAssetGroup> assetGroups;
	std::vector<RemasterFrameAssetMetadata> assetMetadata;
	std::vector<RemasterFrameMaterial> materials;
	std::vector<RemasterFrameTileInstance> tileInstances;
	std::vector<RemasterFrameLight> lights;
};

inline void S9xRemasterApplyProfileToFrame (const RemasterProfile &profile, RemasterFrame &frame)
{
	frame.profileRomSha256 = profile.romSha256;
	frame.lightingCoordinateScale = profile.lightingCoordinateScale;
	frame.cameraDirection = profile.cameraDirection;
	frame.indirectBounceCount = profile.indirectBounceCount;
	frame.indirectRoughness = profile.indirectRoughness;
	frame.reflectanceBoost = profile.reflectanceBoost;
	frame.originalSceneContribution = profile.originalSceneContribution;
	frame.heightPreviewMultiplier = profile.heightPreviewMultiplier;
	frame.samplesPerFrame = profile.samplesPerFrame;
	frame.sampleAccumulation = profile.sampleAccumulation;

	frame.assetGroups.clear();
	for (const auto &entry : profile.assetGroups)
	{
		RemasterFrameAssetGroup group;
		group.name = entry.second.name;
		group.tileIds = entry.second.tileIds;
		frame.assetGroups.push_back(group);
	}

	frame.assetMetadata.clear();
	for (const auto &entry : profile.assets)
	{
		const RemasterAssetMetadata &source = entry.second;
		RemasterFrameAssetMetadata metadata;
		metadata.tileId = source.tileId;
		metadata.materialSelectors = source.materialSelectors;
		metadata.occlusion = source.occlusion;
		metadata.height = source.height;
		metadata.normalXyz = source.normalXyz;
		metadata.emissionRgba = source.emissionRgba;
		metadata.hasMaterialSelectors = source.hasMaterialSelectors;
		metadata.hasOcclusion = source.hasOcclusion;
		metadata.hasHeight = source.hasHeight;
		metadata.hasNormals = source.hasNormals;
		metadata.hasEmission = source.hasEmission;
		metadata.directLightingOppositeFacing = source.directLightingOppositeFacing;
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

	for (RemasterFrameTileInstance &instance : frame.tileInstances)
	{
		RemasterProfileMatchContext context;
		context.tileId = instance.tileId;
		context.source = instance.source;
		context.sourceIndex = instance.sourceIndex;
		context.palette = instance.palette;
		const RemasterProfileMatch match = S9xRemasterMatchProfile(profile, context);
		instance.matchStatus = match.status;
		instance.ruleLine = match.rule ? static_cast<uint32_t>(match.rule->line) : 0;
		instance.assetGroup = match.assetGroup ? match.assetGroup->name : std::string();
		instance.material = match.material ? match.material->name : std::string();
	}
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
		!input.ReadU32(assetCount) ||
		(result.schemaVersion >= 13 && !input.ReadU32(artworkColorCount)) ||
		(result.schemaVersion >= 2 && !input.ReadU32(groupCount)) ||
		(result.schemaVersion >= 3 && !input.ReadU32(metadataCount)) || !input.ReadU32(materialCount) ||
		!input.ReadU32(instanceCount) || !input.ReadU32(lightCount))
		return false;
	if (!std::isfinite(result.lightingCoordinateScale) || result.lightingCoordinateScale <= 0.0f)
		return false;
	if (result.indirectBounceCount > 16)
		return false;
	if (!std::isfinite(result.indirectRoughness) || result.indirectRoughness < 0.0f || result.indirectRoughness > 1.0f)
		return false;
	if (!std::isfinite(result.reflectanceBoost) || result.reflectanceBoost < 0.0f || result.reflectanceBoost > 4.0f)
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
			if (input.offset + metadata.height.size() > bytes.size())
				return false;
			std::copy(bytes.begin() + input.offset, bytes.begin() + input.offset + metadata.height.size(),
				metadata.height.begin());
			input.offset += metadata.height.size();
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
	}
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
			!input.ReadString(instance.material))
			return false;
		instance.source = static_cast<RemasterSourceType>(source);
		instance.hFlip = hFlip != 0;
		instance.vFlip = vFlip != 0;
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
		if (pixel.instanceId > result.tileInstances.size() ||
			(result.schemaVersion >= 5 && pixel.instanceId && pixel.tilePixel >= 64))
			return false;
	for (const RemasterFramePixel &pixel : result.subPixels)
		if (pixel.instanceId > result.tileInstances.size() ||
			(result.schemaVersion >= 5 && pixel.instanceId && pixel.tilePixel >= 64))
			return false;
	S9xRemasterInferLegacyTileInstanceFlips(result);
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
	std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> metadataByTile;
	for (const RemasterFrameAssetMetadata &metadata : frame.assetMetadata)
		metadataByTile.emplace(metadata.tileId, &metadata);
	std::vector<Accumulator> accumulators(frame.tileInstances.size());
	for (size_t offset = 0; offset < frame.mainPixels.size(); offset++)
	{
		const RemasterFramePixel &pixel = frame.mainPixels[offset];
		if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size())
			continue;
		Accumulator &accumulator = accumulators[pixel.instanceId - 1];
		if (pixel.tilePixel >= 64)
			continue;
		const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
		const auto metadataEntry = metadataByTile.find(instance.tileId);
		const RemasterFrameAssetMetadata *metadata = metadataEntry == metadataByTile.end() ? nullptr : metadataEntry->second;
		if (!metadata || !metadata->hasEmission)
			continue;
		const size_t emissionOffset = pixel.tilePixel * 4;
		const float intensity = metadata->emissionRgba[emissionOffset + 3] / 25.0f;
		if (intensity <= 0.0f)
			continue;
		accumulator.x += (static_cast<float>(offset % frame.width) + 0.5f) * intensity;
		accumulator.y += (static_cast<float>(offset / frame.width) + 0.5f) * intensity;
		// Eight standard-intensity pixels produce a 1x light; every pixel remains additive.
		// Match the shader's emission decode before summing differently colored pixels.
		accumulator.red += std::pow(metadata->emissionRgba[emissionOffset] / 255.0f, 2.2f) * intensity / 8.0f;
		accumulator.green += std::pow(metadata->emissionRgba[emissionOffset + 1] / 255.0f, 2.2f) * intensity / 8.0f;
		accumulator.blue += std::pow(metadata->emissionRgba[emissionOffset + 2] / 255.0f, 2.2f) * intensity / 8.0f;
		accumulator.z += (metadata->hasHeight ? metadata->height[pixel.tilePixel] / 255.0f : 0.0f) * intensity;
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
		if (!RemasterFrameSerialization::ValidTileId(instance.tileId))
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
		if (!std::isfinite(frame.reflectanceBoost) || frame.reflectanceBoost < 0.0f || frame.reflectanceBoost > 4.0f)
			return false;
		RemasterFrameSerialization::Float(bytes, frame.reflectanceBoost);
	}
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
		RemasterFrameSerialization::TileId(bytes, metadata.tileId);
		RemasterFrameSerialization::U8(bytes, metadata.hasMaterialSelectors ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, metadata.hasOcclusion ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, metadata.hasHeight ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, static_cast<uint8_t>(metadata.heightSampling));
		RemasterFrameSerialization::U8(bytes, metadata.hasEmission ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, metadata.hasNormals ? 1 : 0);
		RemasterFrameSerialization::U8(bytes, metadata.directLightingOppositeFacing ? 1 : 0);
		if (metadata.hasMaterialSelectors)
			for (const std::string &name : metadata.materialSelectors)
				if (!RemasterFrameSerialization::String(bytes, name))
					return false;
		if (metadata.hasOcclusion)
			bytes.insert(bytes.end(), metadata.occlusion.begin(), metadata.occlusion.end());
		if (metadata.hasHeight)
			bytes.insert(bytes.end(), metadata.height.begin(), metadata.height.end());
		if (metadata.hasNormals)
			bytes.insert(bytes.end(), metadata.normalXyz.begin(), metadata.normalXyz.end());
		if (metadata.hasEmission)
			bytes.insert(bytes.end(), metadata.emissionRgba.begin(), metadata.emissionRgba.end());
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
		RemasterFrameSerialization::U8(bytes, material.hasDiffuseReflectance ? 1 : 0);
		if (material.hasDiffuseReflectance)
			for (float component : material.diffuseReflectance)
				RemasterFrameSerialization::Float(bytes, component);
	}
	for (const RemasterFrameTileInstance &instance : frame.tileInstances)
	{
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
