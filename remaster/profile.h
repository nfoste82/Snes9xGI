/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REMASTER_PROFILE_H_
#define _REMASTER_PROFILE_H_

#include "types.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

enum class RemasterSurfaceClass : uint8_t
{
	Unclassified,
	Floor,
	WallFace,
	WallTop,
	Character,
	Prop,
	Effect,
	UserInterface
};

enum class RemasterHeightSampling : uint8_t
{
	Nearest,
	Linear
};

struct RemasterTileContentId
{
	uint64_t hash = 0;
	uint8_t hashVersion = 0;
	uint8_t bitDepth = 0;

	bool operator< (const RemasterTileContentId &other) const
	{
		if (hashVersion != other.hashVersion)
			return hashVersion < other.hashVersion;
		if (bitDepth != other.bitDepth)
			return bitDepth < other.bitDepth;
		return hash < other.hash;
	}

	bool operator== (const RemasterTileContentId &other) const
	{
		return hashVersion == other.hashVersion && bitDepth == other.bitDepth && hash == other.hash;
	}
};

struct RemasterMaterial
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

struct RemasterAssetGroup
{
	std::string name;
	std::vector<RemasterTileContentId> tileIds;
	std::string material;
};

inline std::array<uint8_t, 64> S9xRemasterDefaultOcclusion ()
{
	std::array<uint8_t, 64> coverage;
	coverage.fill(255);
	return coverage;
}

inline bool S9xRemasterOcclusionNeedsStorage (bool hasOcclusion, const std::array<uint8_t, 64> &coverage)
{
	return hasOcclusion && std::any_of(coverage.begin(), coverage.end(), [](uint8_t value) { return value < 255; });
}

// Geometry arrays use canonical artwork pixel positions. Flip-specific normal
// vectors are authored in gameplay space, not reflected again at render time.
struct RemasterGeometryLayers
{
	std::array<int16_t, 64> height = {};
	std::array<uint8_t, 192> normalXyz = {};
	bool hasHeight = false, hasNormals = false;
	RemasterHeightSampling heightSampling = RemasterHeightSampling::Nearest;
};

struct RemasterAssetMetadata : RemasterGeometryLayers
{
	RemasterTileContentId tileId;
	RemasterTileRole tileRole = RemasterTileRole::Unknown;
	uint64_t doorframeMask = 0;
	bool hasDoorframeMask = false;
	bool hasTileSemantics = false;
	std::vector<std::string> tileSemanticNormals;
	std::string tileSemanticCorner;
	bool tileSemanticContextDependent = false;
	std::string tileSemanticNote;
	std::array<std::string, 64> materialSelectors;
	std::array<uint8_t, 64> occlusion = S9xRemasterDefaultOcclusion();
	std::array<uint8_t, 256> emissionRgba = {};
	float emissionDepth = 0.0f;
	bool emissionOmnidirectional = false;
	std::vector<RemasterTileContentId> combineEmitters;
	bool hasMaterialSelectors = false;
	bool hasOcclusion = false;
	bool hasEmission = false;
	bool directLightingOppositeFacing = false;
	bool normalIgnoreTileTransform = false;
	bool geometryByFacing = false;
	std::array<RemasterGeometryLayers, 4> facingGeometry;
	bool geometryByFlip = false;
	std::array<RemasterGeometryLayers, 4> flipGeometry;
};

inline unsigned S9xRemasterGeometryFlipIndex(bool hFlip, bool vFlip)
{
	return unsigned(hFlip) | (unsigned(vFlip) << 1);
}

inline unsigned S9xRemasterGeometryCanonicalPixel(unsigned pixel, unsigned flip)
{
	return pixel ^ ((flip & 1) ? 7u : 0u) ^ ((flip & 2) ? 56u : 0u);
}

inline bool S9xRemasterHasGeometryVariants(const RemasterAssetMetadata &asset)
{
	return asset.geometryByFlip || asset.geometryByFacing;
}

// Editor selection: 0 = shared fallback, 1..4 = normal, H, V, H+V.
inline RemasterGeometryLayers &S9xRemasterFlipGeometryForEditing(RemasterAssetMetadata &asset, unsigned selection)
{
	return asset.geometryByFlip && selection >= 1 && selection <= 4 ? asset.flipGeometry[selection - 1] : static_cast<RemasterGeometryLayers &>(asset);
}

inline const RemasterGeometryLayers &S9xRemasterFlipGeometryForEditing(const RemasterAssetMetadata &asset, unsigned selection)
{
	return asset.geometryByFlip && selection >= 1 && selection <= 4 ? asset.flipGeometry[selection - 1] : static_cast<const RemasterGeometryLayers &>(asset);
}

// Doorframe edits are deliberately independent of authored height/normal arrays.
inline RemasterGeometryLayers &S9xRemasterGeometryForEditing(RemasterAssetMetadata &asset, unsigned facing)
{
	return asset.geometryByFacing && facing >= 1 && facing <= 4 ? asset.facingGeometry[facing - 1] : static_cast<RemasterGeometryLayers &>(asset);
}

inline const RemasterGeometryLayers &S9xRemasterGeometryForEditing(const RemasterAssetMetadata &asset, unsigned facing)
{
	return asset.geometryByFacing && facing >= 1 && facing <= 4 ? asset.facingGeometry[facing - 1] : static_cast<const RemasterGeometryLayers &>(asset);
}

// Absent is unknown; painting even a false pixel creates an explicit mask.
inline bool S9xRemasterPaintDoorframe (RemasterAssetMetadata &metadata, unsigned pixel, bool value)
{
	if (metadata.tileRole != RemasterTileRole::Door || pixel >= 64) return false;
	const bool wasPresent = metadata.hasDoorframeMask;
	const uint64_t before = metadata.doorframeMask;
	if (!wasPresent) metadata.doorframeMask = 0;
	metadata.hasDoorframeMask = true;
	const uint64_t bit = uint64_t(1) << pixel;
	if (value) metadata.doorframeMask |= bit;
	else metadata.doorframeMask &= ~bit;
	return !wasPresent || before != metadata.doorframeMask;
}

inline void S9xRemasterCopyDoorframe (const RemasterAssetMetadata &source, RemasterAssetMetadata &destination)
{
	if (destination.tileRole != RemasterTileRole::Door) return;
	destination.hasDoorframeMask = source.hasDoorframeMask;
	destination.doorframeMask = source.hasDoorframeMask ? source.doorframeMask : 0;
}

struct RemasterProfileRule
{
	std::string tileHash;
	std::string assetGroup;
	std::string material;
	RemasterTileContentId tileId;
	RemasterSourceType source = RemasterSourceType::Backdrop;
	uint8_t sourceIndex = 0;
	uint8_t palette = 0;
	uint8_t specificity = 0;
	bool hasTileHash = false;
	bool hasAssetGroup = false;
	bool hasSource = false;
	bool hasSourceIndex = false;
	bool hasPalette = false;
	size_t line = 0;
};

// One authored example footprint; identities may repeat within it, never in a
// second obstacle group. Placement Z remains contextual, not asset metadata.
struct RemasterObstacleMember
{
	RemasterTileContentId tileId;
	int x = 0, y = 0, z = 0;
	bool hflip = false, vflip = false, supportContact = false;
};
struct RemasterObstacleGroup
{
	std::string name;
	std::vector<RemasterObstacleMember> members;
};

// Direction is the direction rays travel, in source-pixel XYZ (+Z toward camera).
struct RemasterDirectionalLight
{
	bool enabled = true;
	std::array<float, 3> direction = {{0, 0, -1}};
	std::array<float, 3> color = {{1.0f, 0.97f, 0.92f}}; // linear RGB
	float brightness = 0.2f; // UI 4/100; persisted linear illumination (UI / 20).
	bool operator==(const RemasterDirectionalLight &b) const
	{ return enabled == b.enabled && direction == b.direction && color == b.color && brightness == b.brightness; }
};

inline float S9xRemasterDirectionalBrightnessToUi(float brightness) { return brightness * 20.0f; }
inline float S9xRemasterDirectionalBrightnessFromUi(float brightness) { return brightness / 20.0f; }

struct RemasterAreaContext
{
	// 0..319 indoor room; 320 Light World; 321 Dark World; 0xffff unknown.
	uint16_t id = 0xffff;
	bool romDark = false;
	bool valid() const { return id <= 321; }
};

inline std::string S9xRemasterAreaKey(uint16_t id)
{
	if (id == 320) return "light_world";
	if (id == 321) return "dark_world";
	if (id >= 320) return {};
	char key[16]; std::snprintf(key, sizeof(key), "room_%03x", id); return key;
}

inline bool S9xRemasterValidAreaKey(const std::string &key)
{
	for (uint16_t id = 0; id <= 321; ++id) if (key == S9xRemasterAreaKey(id)) return true;
	return false;
}

inline RemasterDirectionalLight S9xRemasterDefaultAreaLight(bool indoors)
{
	RemasterDirectionalLight light;
	// Normalized (0, -0.488, -0.87), stored as a unit vector.
	if (indoors) light.direction = {{0.0f, -0.4892137774788156f, -0.8721639065708393f}};
	return light;
}

inline bool S9xRemasterValidDirectionalLight(const RemasterDirectionalLight &light)
{
	double length = 0;
	for (float v : light.direction) { if (!std::isfinite(v) || std::abs(v) > 1e6f) return false; length += double(v)*v; }
	for (float v : light.color) if (!std::isfinite(v) || v < 0 || v > 1) return false;
	return length > 1e-12 && std::isfinite(light.brightness) && light.brightness >= 0 && light.brightness <= 16;
}

struct RemasterProfile
{
	uint32_t schemaVersion = 0;
	std::string gameTitle;
	std::string romSha256;
	float lightingCoordinateScale = 16.0f;
	std::array<float, 3> cameraDirection = {{ 0.0f, 0.0f, -1.0f }};
	uint8_t indirectBounceCount = 0;
	float indirectRoughness = 1.0f;
	float reflectanceBoost = 0.0f;
	float originalSceneContribution = 0.65f;
	uint8_t heightPreviewMultiplier = 8;
	uint8_t upperFloorHeight = 0;
	uint8_t samplesPerFrame = 1;
	uint8_t emitterPatchSize = 2;
	uint8_t representativeEmitters = 0;
	bool sampleAccumulation = true;
	std::map<std::string, RemasterMaterial> materials;
	std::map<std::string, RemasterAssetGroup> assetGroups;
	std::map<RemasterTileContentId, RemasterAssetMetadata> assets;
	std::vector<RemasterProfileRule> rules;
	std::vector<RemasterObstacleGroup> obstacleGroups;
	std::map<std::string, RemasterDirectionalLight> areaLights;
};

inline RemasterDirectionalLight S9xRemasterAreaLight(const RemasterProfile &profile, RemasterAreaContext area)
{
	const auto found = profile.areaLights.find(S9xRemasterAreaKey(area.id));
	if (area.valid() && found != profile.areaLights.end()) return found->second;
	RemasterDirectionalLight light = S9xRemasterDefaultAreaLight(area.id < 320);
	light.enabled = area.valid() && !area.romDark; return light;
}

// Scene Controls change these small values without replacing the tile atlas.
struct RemasterSceneSettings
{
	uint32_t schemaVersion = 0;
	float lightingCoordinateScale = 16.0f;
	std::array<float, 3> cameraDirection = {{ 0.0f, 0.0f, -1.0f }};
	uint8_t indirectBounceCount = 0;
	float indirectRoughness = 1.0f;
	float reflectanceBoost = 0.0f;
	float originalSceneContribution = 0.65f;
	uint8_t heightPreviewMultiplier = 8;
	uint8_t upperFloorHeight = 0;
	uint8_t samplesPerFrame = 1;
	uint8_t emitterPatchSize = 2;
	uint8_t representativeEmitters = 0;
	bool sampleAccumulation = true;
};

inline RemasterSceneSettings S9xRemasterGetSceneSettings (const RemasterProfile &profile)
{
	return { profile.schemaVersion, profile.lightingCoordinateScale, profile.cameraDirection,
		profile.indirectBounceCount, profile.indirectRoughness, profile.reflectanceBoost,
		profile.originalSceneContribution, profile.heightPreviewMultiplier, profile.upperFloorHeight, profile.samplesPerFrame,
		profile.emitterPatchSize, profile.representativeEmitters, profile.sampleAccumulation };
}

inline void S9xRemasterApplySceneSettings (RemasterProfile &profile, const RemasterSceneSettings &settings)
{
	profile.schemaVersion = settings.schemaVersion;
	profile.lightingCoordinateScale = settings.lightingCoordinateScale;
	profile.cameraDirection = settings.cameraDirection;
	profile.indirectBounceCount = settings.indirectBounceCount;
	profile.indirectRoughness = settings.indirectRoughness;
	profile.reflectanceBoost = settings.reflectanceBoost;
	profile.originalSceneContribution = settings.originalSceneContribution;
	profile.heightPreviewMultiplier = settings.heightPreviewMultiplier;
	profile.upperFloorHeight = settings.upperFloorHeight;
	profile.samplesPerFrame = settings.samplesPerFrame;
	profile.emitterPatchSize = settings.emitterPatchSize;
	profile.representativeEmitters = settings.representativeEmitters;
	profile.sampleAccumulation = settings.sampleAccumulation;
}

inline bool S9xRemasterSceneSettingsEqual (const RemasterSceneSettings &a, const RemasterSceneSettings &b)
{
	return a.schemaVersion == b.schemaVersion &&
		a.lightingCoordinateScale == b.lightingCoordinateScale &&
		a.cameraDirection == b.cameraDirection &&
		a.indirectBounceCount == b.indirectBounceCount &&
		a.indirectRoughness == b.indirectRoughness &&
		a.reflectanceBoost == b.reflectanceBoost &&
		a.originalSceneContribution == b.originalSceneContribution &&
		a.heightPreviewMultiplier == b.heightPreviewMultiplier &&
		a.upperFloorHeight == b.upperFloorHeight &&
		a.samplesPerFrame == b.samplesPerFrame &&
		a.emitterPatchSize == b.emitterPatchSize &&
		a.representativeEmitters == b.representativeEmitters &&
		a.sampleAccumulation == b.sampleAccumulation;
}

struct RemasterProfileDiagnostic
{
	size_t line = 0;
	std::string message;
};

struct RemasterProfileMatchContext
{
	RemasterTileContentId tileId;
	RemasterSourceType source = RemasterSourceType::Backdrop;
	uint8_t sourceIndex = 0;
	uint8_t palette = 0;
};

enum class RemasterProfileMatchStatus : uint8_t
{
	NoMatch,
	Matched,
	Ambiguous
};

struct RemasterProfileMatch
{
	RemasterProfileMatchStatus status = RemasterProfileMatchStatus::NoMatch;
	const RemasterMaterial *material = nullptr;
	const RemasterAssetGroup *assetGroup = nullptr;
	const RemasterProfileRule *rule = nullptr;
	std::vector<size_t> conflictingRuleLines;
};

namespace RemasterProfileParsing
{
	inline std::string Trim (const std::string &value)
	{
		size_t begin = value.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
			return std::string();
		size_t end = value.find_last_not_of(" \t\r\n");
		return value.substr(begin, end - begin + 1);
	}

	inline std::string StripComment (const std::string &line)
	{
		bool quoted = false;
		size_t backslashes = 0;
		for (size_t i = 0; i < line.size(); i++)
		{
			if (line[i] == '"' && backslashes % 2 == 0)
				quoted = !quoted;
			else if (line[i] == '#' && !quoted)
				return line.substr(0, i);
			backslashes = line[i] == '\\' ? backslashes + 1 : 0;
		}
		return line;
	}

	inline bool ParseString (const std::string &value, std::string &parsed)
	{
		if (value.size() < 2 || value.front() != '"' || value.back() != '"')
			return false;
		parsed.clear();
		for (size_t i = 1; i + 1 < value.size(); i++)
		{
			if (value[i] == '"')
				return false;
			if (value[i] == '\\')
			{
				if (++i + 1 >= value.size())
					return false;
				if (value[i] == 'n') { parsed.push_back('\n'); continue; }
				if (value[i] == 'r') { parsed.push_back('\r'); continue; }
				if (value[i] == 't') { parsed.push_back('\t'); continue; }
				if (value[i] != '\\' && value[i] != '"') return false;
			}
			parsed.push_back(value[i]);
		}
		return true;
	}

	inline bool ParseUnsigned (const std::string &value, uint32_t &parsed)
	{
		if (value.empty())
			return false;
		char *end = nullptr;
		errno = 0;
		unsigned long result = std::strtoul(value.c_str(), &end, 10);
		if (errno || !end || *end || result > std::numeric_limits<uint32_t>::max())
			return false;
		parsed = static_cast<uint32_t>(result);
		return true;
	}

	inline bool ParseFloat (const std::string &value, float &parsed)
	{
		if (value.empty())
			return false;
		char *end = nullptr;
		errno = 0;
		float result = std::strtof(value.c_str(), &end);
		if (errno || !end || *end || !std::isfinite(result))
			return false;
		parsed = result;
		return true;
	}

	inline bool ParseFloat3 (const std::string &value, std::array<float, 3> &parsed)
	{
		if (value.size() < 2 || value.front() != '[' || value.back() != ']')
			return false;
		std::string body = value.substr(1, value.size() - 2);
		size_t offset = 0;
		for (size_t component = 0; component < parsed.size(); component++)
		{
			size_t comma = body.find(',', offset);
			if ((component + 1 < parsed.size()) != (comma != std::string::npos))
				return false;
			std::string item = Trim(body.substr(offset, comma == std::string::npos ? comma : comma - offset));
			if (!ParseFloat(item, parsed[component]))
				return false;
			offset = comma == std::string::npos ? body.size() : comma + 1;
		}
		return offset == body.size();
	}

	inline bool ParseBool (const std::string &value, bool &parsed)
	{
		if (value == "true")
		{
			parsed = true;
			return true;
		}
		if (value == "false")
		{
			parsed = false;
			return true;
		}
		return false;
	}

	inline bool ParseDoorframeMask (const std::string &value, uint64_t &parsed)
	{
		std::string hex;
		if (!ParseString(value, hex) || hex.size() != 16) return false;
		uint64_t mask = 0;
		for (char c : hex)
		{
			if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
			mask = (mask << 4) | (c <= '9' ? c - '0' : c - 'a' + 10);
		}
		parsed = mask;
		return true;
	}

	inline bool ParseStringArray (const std::string &value, std::vector<std::string> &parsed)
	{
		if (value.size() < 2 || value.front() != '[' || value.back() != ']')
			return false;
		parsed.clear();
		std::string body = Trim(value.substr(1, value.size() - 2));
		if (body.empty())
			return true;
		size_t offset = 0;
		while (offset < body.size())
		{
			size_t begin = body.find_first_not_of(" \t", offset);
			if (begin == std::string::npos || body[begin] != '"')
				return false;
			size_t end = begin + 1;
			size_t backslashes = 0;
			while (end < body.size())
			{
				if (body[end] == '"' && backslashes % 2 == 0)
					break;
				backslashes = body[end] == '\\' ? backslashes + 1 : 0;
				end++;
			}
			if (end == body.size())
				return false;
			std::string item;
			if (!ParseString(body.substr(begin, end - begin + 1), item))
				return false;
			parsed.push_back(item);
			offset = end + 1;
			size_t next = body.find_first_not_of(" \t", offset);
			if (next == std::string::npos)
				break;
			if (body[next] != ',')
				return false;
			offset = next + 1;
			if (body.find_first_not_of(" \t", offset) == std::string::npos)
				return false;
		}
		return true;
	}

	inline bool ParseByteArray (const std::string &value, std::vector<uint8_t> &parsed)
	{
		if (value.size() < 2 || value.front() != '[' || value.back() != ']')
			return false;
		parsed.clear();
		const char *cursor = value.c_str() + 1;
		const char *end = value.c_str() + value.size() - 1;
		auto skipSpace = [&cursor, end] {
			while (cursor < end && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n'))
				cursor++;
		};
		skipSpace();
		while (cursor < end)
		{
			char *numberEnd = nullptr;
			errno = 0;
			const unsigned long number = std::strtoul(cursor, &numberEnd, 10);
			if (errno || numberEnd == cursor || number > 255 || numberEnd > end)
				return false;
			parsed.push_back(static_cast<uint8_t>(number));
			cursor = numberEnd;
			skipSpace();
			if (cursor == end)
				return true;
			if (*cursor++ != ',')
				return false;
			skipSpace();
			if (cursor == end)
				return false;
		}
		return true;
	}

	inline bool ParseHeightArray (const std::string &value, std::vector<int16_t> &parsed, uint32_t schema)
	{
		if (value.size() < 2 || value.front() != '[' || value.back() != ']') return false;
		parsed.clear();
		const char *cursor = value.c_str() + 1, *end = value.c_str() + value.size() - 1;
		auto space = [&] { while (cursor < end && std::isspace(static_cast<unsigned char>(*cursor))) ++cursor; };
		space();
		while (cursor < end)
		{
			char *next = nullptr; errno = 0;
			const long number = std::strtol(cursor, &next, 10);
			if (errno || next == cursor || next > end || number < (schema >= 27 ? -32768 : 0) ||
				number > (schema >= 27 ? 32767 : 255)) return false;
			parsed.push_back(static_cast<int16_t>(number)); cursor = next; space();
			if (cursor == end) return true;
			if (*cursor++ != ',') return false;
			space(); if (cursor == end) return false;
		}
		return true;
	}

	inline bool ParseTileId (const std::string &value, RemasterTileContentId &id)
	{
		if (value.size() != 24 || value.compare(0, 3, "v1:") || value.compare(4, 4, "bpp:"))
			return false;
		if (value[3] != '2' && value[3] != '4' && value[3] != '8')
			return false;
		uint64_t hash = 0;
		for (size_t i = 8; i < value.size(); i++)
		{
			char c = value[i];
			uint8_t digit;
			if (c >= '0' && c <= '9')
				digit = c - '0';
			else if (c >= 'a' && c <= 'f')
				digit = c - 'a' + 10;
			else
				return false;
			hash = (hash << 4) | digit;
		}
		id.hashVersion = 1;
		id.bitDepth = value[3] - '0';
		id.hash = hash;
		return true;
	}

	inline bool IsName (const std::string &value)
	{
		if (value.empty())
			return false;
		for (char c : value)
			if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
				!(c >= '0' && c <= '9') && c != '_' && c != '-')
				return false;
		return true;
	}

	inline bool ParseSurfaceClass (const std::string &value, RemasterSurfaceClass &surfaceClass)
	{
		static const std::map<std::string, RemasterSurfaceClass> classes = {
			{ "unclassified", RemasterSurfaceClass::Unclassified },
			{ "floor", RemasterSurfaceClass::Floor },
			{ "wall_face", RemasterSurfaceClass::WallFace },
			{ "wall_top", RemasterSurfaceClass::WallTop },
			{ "character", RemasterSurfaceClass::Character },
			{ "prop", RemasterSurfaceClass::Prop },
			{ "effect", RemasterSurfaceClass::Effect },
			{ "user_interface", RemasterSurfaceClass::UserInterface }
		};
		auto found = classes.find(value);
		if (found == classes.end())
			return false;
		surfaceClass = found->second;
		return true;
	}

	inline bool ParseSource (const std::string &value, RemasterSourceType &source)
	{
		if (value == "background")
			source = RemasterSourceType::Background;
		else if (value == "object")
			source = RemasterSourceType::Object;
		else if (value == "backdrop")
			source = RemasterSourceType::Backdrop;
		else
			return false;
		return true;
	}

	inline bool ParseHeightSampling (const std::string &value, RemasterHeightSampling &sampling)
	{
		if (value == "nearest")
			sampling = RemasterHeightSampling::Nearest;
		else if (value == "linear")
			sampling = RemasterHeightSampling::Linear;
		else
			return false;
		return true;
	}
}

inline bool S9xRemasterValidateObstacleGroups(const std::vector<RemasterObstacleGroup> &groups,
	std::vector<RemasterProfileDiagnostic> &diagnostics)
{
	const size_t before=diagnostics.size();
	if(groups.size()>4096)diagnostics.push_back({0,"at most 4096 obstacle groups are supported"});
	std::set<std::string> names;
	std::map<RemasterTileContentId,size_t> ownership;
	for(size_t g=0;g<groups.size();g++) {
		const auto &group=groups[g];bool contact=false;
		auto fail=[&](const std::string &s){diagnostics.push_back({0,"obstacle group '"+group.name+"': "+s});};
		if(!RemasterProfileParsing::IsName(group.name) || !names.insert(group.name).second)fail("invalid or duplicate name");
		if(group.members.empty() || group.members.size()>4096)fail("requires 1..4096 members");
		std::set<std::pair<int,int>> positions;
		int minX=63,minY=63,maxX=-63,maxY=-63;
		for(const auto &m:group.members) {
			if(m.tileId.hashVersion!=1 || (m.tileId.bitDepth!=2 && m.tileId.bitDepth!=4 && m.tileId.bitDepth!=8))fail("invalid tile identity");
			if(m.x < -63 || m.x>63 || m.y < -63 || m.y>63 || m.z < -4095 || m.z>4095)fail("coordinate or signed Z out of range");
			if(!positions.insert({m.x,m.y}).second)fail("duplicate member position");
			const auto inserted=ownership.emplace(m.tileId,g);
			if(!inserted.second && inserted.first->second!=g)fail("tile identity belongs to another obstacle group");
			minX=std::min(minX,m.x);maxX=std::max(maxX,m.x);minY=std::min(minY,m.y);maxY=std::max(maxY,m.y);
			contact|=m.supportContact;
		}
		if(maxX-minX>=64 || maxY-minY>=64)fail("footprint exceeds room dimensions");
		if(!contact)fail("requires an explicit support_contact");
	}
	return diagnostics.size()==before;
}

inline bool S9xRemasterParseProfile (const std::string &text, RemasterProfile &profile,
	std::vector<RemasterProfileDiagnostic> &diagnostics)
{
	using namespace RemasterProfileParsing;
	enum class Section { Root, Game, LightingSpace, Material, AssetGroup, Asset, Rule, ObstacleGroup, ObstacleMember, Area };
	Section section = Section::Root;
	RemasterProfile parsed;
	bool hasLightingSpace = false;
	bool hasIndirectRoughness = false;
	bool hasSceneSampling = false;
	bool hasReflectanceBoost = false;
	bool hasCameraDirection = false;
	bool hasHeightPreviewMultiplier = false;
	bool hasUpperFloorHeight = false;
	RemasterMaterial *material = nullptr;
	RemasterAssetGroup *group = nullptr;
	RemasterAssetMetadata *asset = nullptr;
	RemasterProfileRule *rule = nullptr;
	RemasterObstacleMember *obstacleMember = nullptr;
	size_t obstacleIndex = 0;
	std::string areaKey;
	std::map<std::string, std::set<std::string>> seenKeys;
	diagnostics.clear();

	auto fail = [&diagnostics] (size_t line, const std::string &message) {
		diagnostics.push_back({ line, message });
	};
	auto parseNamedSection = [&fail] (const std::string &header, const std::string &prefix,
		size_t line, std::string &name) {
		if (header.compare(0, prefix.size(), prefix) || header.back() != ']')
			return false;
		name = header.substr(prefix.size(), header.size() - prefix.size() - 1);
		if (!IsName(name))
		{
			fail(line, "invalid section name");
			return false;
		}
		return true;
	};

	std::istringstream input(text);
	std::string rawLine;
	size_t lineNumber = 0;
	while (std::getline(input, rawLine))
	{
		lineNumber++;
		std::string line = Trim(StripComment(rawLine));
		if (line.empty())
			continue;
		if (line.front() == '[')
		{
			material = nullptr;
			group = nullptr;
			asset = nullptr;
			rule = nullptr;
			obstacleMember = nullptr;
			if (line == "[game]")
				section = Section::Game;
			else if (line == "[lighting_space]")
			{
				section = Section::LightingSpace;
				hasLightingSpace = true;
			}
			else if (line == "[[obstacle_groups]]")
			{
				section=Section::ObstacleGroup;parsed.obstacleGroups.emplace_back();
				obstacleIndex=parsed.obstacleGroups.size();
			}
			else if (line == "[[obstacle_groups.members]]")
			{
				if(!obstacleIndex) { fail(lineNumber,"obstacle member requires preceding obstacle group");continue; }
				section=Section::ObstacleMember;
				parsed.obstacleGroups[obstacleIndex-1].members.emplace_back();
				obstacleMember=&parsed.obstacleGroups[obstacleIndex-1].members.back();
			}
			else if (line == "[[rules]]")
			{
				section = Section::Rule;
				parsed.rules.emplace_back();
				rule = &parsed.rules.back();
				rule->line = lineNumber;
			}
			else if (line == "[[assets]]")
			{
				section = Section::Asset;
				RemasterTileContentId placeholder;
				placeholder.hash = UINT64_MAX - lineNumber;
				asset = &parsed.assets[placeholder];
				asset->tileId = placeholder;
			}
			else
			{
				std::string name;
				if (parseNamedSection(line, "[area_lights.", lineNumber, name))
				{
					section = Section::Area; areaKey = name;
					if (!S9xRemasterValidAreaKey(name)) fail(lineNumber, "invalid area light key");
					if (!parsed.areaLights.emplace(name, S9xRemasterDefaultAreaLight(name.compare(0, 5, "room_") == 0)).second) fail(lineNumber, "duplicate area light");
				}
				else if (parseNamedSection(line, "[materials.", lineNumber, name))
				{
					if (parsed.materials.count(name))
						fail(lineNumber, "duplicate material '" + name + "'");
					else
					{
						section = Section::Material;
						material = &parsed.materials[name];
						material->name = name;
					}
				}
				else if (parseNamedSection(line, "[asset_groups.", lineNumber, name))
				{
					if (parsed.assetGroups.count(name))
						fail(lineNumber, "duplicate asset group '" + name + "'");
					else
					{
						section = Section::AssetGroup;
						group = &parsed.assetGroups[name];
						group->name = name;
					}
				}
				else if (diagnostics.empty() || diagnostics.back().line != lineNumber)
					fail(lineNumber, "unknown section");
			}
			continue;
		}

		size_t equals = line.find('=');
		if (equals == std::string::npos)
		{
			fail(lineNumber, "expected key = value");
			continue;
		}
		std::string key = Trim(line.substr(0, equals));
		std::string value = Trim(line.substr(equals + 1));
		std::string keyScope = std::to_string(static_cast<int>(section));
		if (section == Section::Area) keyScope += ":" + areaKey;
		if (material)
			keyScope += ":" + material->name;
		if (group)
			keyScope += ":" + group->name;
		if (rule)
			keyScope += ":" + std::to_string(parsed.rules.size());
		if (asset)
			keyScope += ":" + std::to_string(parsed.assets.size());
		if(section==Section::ObstacleGroup || section==Section::ObstacleMember) {
			keyScope+=":"+std::to_string(obstacleIndex);
			if(obstacleMember)keyScope+=":"+std::to_string(parsed.obstacleGroups[obstacleIndex-1].members.size());
		}
		if (!seenKeys[keyScope].insert(key).second)
		{
			fail(lineNumber, "duplicate key '" + key + "'");
			continue;
		}

		std::string stringValue;
		uint32_t unsignedValue = 0;
		bool valid = true;
		switch (section)
		{
		case Section::Area:
		{
			auto &light = parsed.areaLights[areaKey];
			if (key == "enabled") valid = ParseBool(value, light.enabled);
			else if (key == "direction") valid = ParseFloat3(value, light.direction);
			else if (key == "color") valid = ParseFloat3(value, light.color);
			else if (key == "brightness") valid = ParseFloat(value, light.brightness);
			else { fail(lineNumber, "unknown area light key '" + key + "'"); break; }
			if (!valid) fail(lineNumber, "invalid area light value for '" + key + "'");
			break;
		}
		case Section::ObstacleGroup:
			if(key!="name" || !ParseString(value,parsed.obstacleGroups[obstacleIndex-1].name))fail(lineNumber,"invalid obstacle group key/value");
			break;
		case Section::ObstacleMember:
			if(!obstacleMember) { fail(lineNumber,"missing obstacle member");break; }
			if(key=="tile_hash")valid=ParseString(value,stringValue) && ParseTileId(stringValue,obstacleMember->tileId);
			else if(key=="hflip")valid=ParseBool(value,obstacleMember->hflip);
			else if(key=="vflip")valid=ParseBool(value,obstacleMember->vflip);
			else if(key=="support_contact")valid=ParseBool(value,obstacleMember->supportContact);
			else if(key=="x" || key=="y" || key=="z") {
				const bool negative=!value.empty() && value[0]=='-';
				valid=ParseUnsigned(negative?value.substr(1):value,unsignedValue) && unsignedValue<=4095;
				if(valid) { const int n=negative?-int(unsignedValue):int(unsignedValue);
					if(key=="x")obstacleMember->x=n;else if(key=="y")obstacleMember->y=n;else obstacleMember->z=n; }
			} else valid=false;
			if(!valid)fail(lineNumber,"invalid obstacle member key/value '"+key+"'");
			break;
		case Section::Root:
			if (key != "schema_version")
				fail(lineNumber, "unknown root key '" + key + "'");
			else if (!ParseUnsigned(value, parsed.schemaVersion))
				fail(lineNumber, "schema_version must be an integer");
			break;
		case Section::Game:
			valid = ParseString(value, stringValue);
			if (key == "title" && valid)
				parsed.gameTitle = stringValue;
			else if (key == "rom_sha256" && valid)
				parsed.romSha256 = stringValue;
			else if (key != "title" && key != "rom_sha256")
				fail(lineNumber, "unknown game key '" + key + "'");
			else
				fail(lineNumber, key + " must be a quoted string");
			break;
		case Section::LightingSpace:
			if (key == "coordinate_scale")
			{
				if (!ParseFloat(value, parsed.lightingCoordinateScale))
					fail(lineNumber, "coordinate_scale must be a number");
			}
			else if (key == "camera_direction")
			{
				hasCameraDirection = true;
				if (!ParseFloat3(value, parsed.cameraDirection))
					fail(lineNumber, "camera_direction must contain exactly three numbers");
			}
			else if (key == "indirect_bounces")
			{
				if (!ParseUnsigned(value, unsignedValue) || unsignedValue > 16)
					fail(lineNumber, "indirect_bounces must be an integer in [0, 16]");
				else
					parsed.indirectBounceCount = static_cast<uint8_t>(unsignedValue);
			}
			else if (key == "indirect_roughness")
			{
				hasIndirectRoughness = true;
				if (!ParseFloat(value, parsed.indirectRoughness))
					fail(lineNumber, "indirect_roughness must be a number");
			}
			else if (key == "reflectance_boost")
			{
				hasReflectanceBoost = true;
				if (!ParseFloat(value, parsed.reflectanceBoost))
					fail(lineNumber, "reflectance_boost must be a number");
			}
			else if (key == "original_scene_contribution")
			{
				hasSceneSampling = true;
				if (!ParseFloat(value, parsed.originalSceneContribution))
					fail(lineNumber, "original_scene_contribution must be a number");
			}
			else if (key == "height_preview_multiplier")
			{
				hasHeightPreviewMultiplier = true;
				if (!ParseUnsigned(value, unsignedValue) || unsignedValue < 1 || unsignedValue > 20)
					fail(lineNumber, "height_preview_multiplier must be an integer in [1, 20]");
				else
					parsed.heightPreviewMultiplier = static_cast<uint8_t>(unsignedValue);
			}
			else if (key == "upper_floor_height")
			{
				hasUpperFloorHeight = true;
				if (!ParseUnsigned(value, unsignedValue) || unsignedValue > 255)
					fail(lineNumber, "upper_floor_height must be an integer in [0, 255]");
				else
					parsed.upperFloorHeight = static_cast<uint8_t>(unsignedValue);
			}
			else if (key == "samples_per_frame")
			{
				hasSceneSampling = true;
				if (!ParseUnsigned(value, unsignedValue) || unsignedValue < 1 || unsignedValue > 128)
					fail(lineNumber, "samples_per_frame must be an integer in [1, 128]");
				else
					parsed.samplesPerFrame = static_cast<uint8_t>(unsignedValue);
			}
			else if (key == "representative_emitters")
			{
				if (!ParseUnsigned(value, unsignedValue) || unsignedValue > 8)
					fail(lineNumber, "representative_emitters must be an integer in [0, 8]");
				else parsed.representativeEmitters = static_cast<uint8_t>(unsignedValue);
			}
			else if (key == "emitter_patch_size")
			{
				if (!ParseUnsigned(value, unsignedValue) || unsignedValue < 1 || unsignedValue > 5)
					fail(lineNumber, "emitter_patch_size must be an integer in [1, 5]");
				else parsed.emitterPatchSize = static_cast<uint8_t>(unsignedValue);
			}
			else if (key == "sample_accumulation")
			{
				hasSceneSampling = true;
				if (!ParseBool(value, parsed.sampleAccumulation))
					fail(lineNumber, "sample_accumulation must be true or false");
			}
			else
				fail(lineNumber, "unknown lighting_space key '" + key + "'");
			break;
		case Section::Material:
			if (!material)
				break;
			if (key == "surface_class")
			{
				if (!ParseString(value, stringValue) || !ParseSurfaceClass(stringValue, material->surfaceClass))
					fail(lineNumber, "invalid surface_class");
			}
			else if (key == "roughness") valid = ParseFloat(value, material->roughness);
			else if (key == "diffuse_reflectance")
			{
				valid = ParseFloat3(value, material->diffuseReflectance);
				material->hasDiffuseReflectance = valid;
			}
			else if (key == "metalness") valid = ParseFloat(value, material->metalness);
			else if (key == "specular_level") valid = ParseFloat(value, material->specularLevel);
			else if (key == "z_min") valid = ParseFloat(value, material->zMin);
			else if (key == "z_max") valid = ParseFloat(value, material->zMax);
			else if (key == "receives_gi") valid = ParseBool(value, material->receivesGi);
			else if (key == "casts_shadow") valid = ParseBool(value, material->castsShadow);
			else
			{
				fail(lineNumber, "unknown material key '" + key + "'");
				break;
			}
			if (!valid)
				fail(lineNumber, "invalid value for '" + key + "'");
			break;
		case Section::AssetGroup:
			if (!group)
				break;
			if (key == "material")
			{
				if (ParseString(value, group->material))
					break;
			}
			else if (key == "tile_hashes")
			{
				std::vector<std::string> hashes;
				if (ParseStringArray(value, hashes))
				{
					for (const std::string &hash : hashes)
					{
						RemasterTileContentId id;
						if (!ParseTileId(hash, id))
						{
							valid = false;
							break;
						}
						group->tileIds.push_back(id);
					}
					if (valid)
						break;
				}
			}
			else
			{
				fail(lineNumber, "unknown asset group key '" + key + "'");
				break;
			}
			fail(lineNumber, "invalid value for '" + key + "'");
			break;
		case Section::Asset:
			if (!asset)
				break;
			if (key == "tile_hash")
			{
				RemasterTileContentId tileId;
				if (!ParseString(value, stringValue) || !ParseTileId(stringValue, tileId))
				{
					fail(lineNumber, "invalid value for 'tile_hash'");
					break;
				}
				RemasterAssetMetadata metadata = *asset;
				parsed.assets.erase(asset->tileId);
				metadata.tileId = tileId;
				auto inserted = parsed.assets.emplace(tileId, std::move(metadata));
				if (!inserted.second)
				{
					fail(lineNumber, "duplicate asset tile_hash");
					asset = nullptr;
				}
				else
					asset = &inserted.first->second;
			}
			else if (key == "tile_role")
			{
				bool valid = false;
				if (ParseString(value, stringValue))
					for (unsigned i = 0; i < static_cast<unsigned>(RemasterTileRole::Count); i++)
						if (stringValue == S9xRemasterTileRoleName(static_cast<RemasterTileRole>(i)))
						{ asset->tileRole = static_cast<RemasterTileRole>(i); valid = true; }
				if (!valid) fail(lineNumber, "tile_role must be a supported authored tile role");
				if (parsed.schemaVersion < 18) fail(lineNumber, "tile_role requires schema_version 18");
			}
			else if (key == "doorframe_mask")
			{
				asset->hasDoorframeMask = ParseDoorframeMask(value, asset->doorframeMask);
				if (!asset->hasDoorframeMask) fail(lineNumber, "doorframe_mask must be exactly 16 lowercase hexadecimal digits in a quoted string");
			}
			else if (key == "tile_semantics_authored" || key == "tile_semantic_normals" ||
				key == "tile_semantic_corner" || key == "tile_semantic_context_dependent" || key == "tile_semantic_note")
			{
				if (parsed.schemaVersion < 20) fail(lineNumber, "tile semantic annotation requires schema_version 20");
				if (key == "tile_semantics_authored")
				{
					if (!ParseBool(value, asset->hasTileSemantics)) fail(lineNumber, "invalid tile_semantics_authored");
				}
				else if (key == "tile_semantic_context_dependent")
				{
					if (!ParseBool(value, asset->tileSemanticContextDependent)) fail(lineNumber, "invalid tile_semantic_context_dependent");
				}
				else if (key == "tile_semantic_normals")
				{
					if (!ParseStringArray(value, asset->tileSemanticNormals)) fail(lineNumber, "invalid tile_semantic_normals");
				}
				else if (key == "tile_semantic_corner")
				{
					if (!ParseString(value, asset->tileSemanticCorner)) fail(lineNumber, "invalid tile_semantic_corner");
				}
				else if (!ParseString(value, asset->tileSemanticNote)) fail(lineNumber, "invalid tile_semantic_note");
			}
			else if (key == "materials")
			{
				std::vector<std::string> selectors;
				if (!ParseStringArray(value, selectors) || selectors.size() != 64)
					fail(lineNumber, "materials must contain exactly 64 quoted names");
				else
				{
					std::copy(selectors.begin(), selectors.end(), asset->materialSelectors.begin());
					asset->hasMaterialSelectors = true;
				}
			}
			else if (key == "occlusion")
			{
				std::vector<uint8_t> coverage;
				if (!ParseByteArray(value, coverage) || coverage.size() != 64)
					fail(lineNumber, "occlusion must contain exactly 64 values in [0, 255]");
				else
				{
					std::copy(coverage.begin(), coverage.end(), asset->occlusion.begin());
					asset->hasOcclusion = true;
				}
			}
			else if (key == "height")
			{
				std::vector<int16_t> heights;
				if (!ParseHeightArray(value, heights, parsed.schemaVersion) || heights.size() != 64)
					fail(lineNumber, parsed.schemaVersion >= 27 ? "height must contain exactly 64 integers in [-32768, 32767]" : "height must contain exactly 64 values in [0, 255]");
				else
				{
					std::copy(heights.begin(), heights.end(), asset->height.begin());
					asset->hasHeight = true;
				}
			}
			else if (key == "height_sampling")
			{
				if (!ParseString(value, stringValue) || !ParseHeightSampling(stringValue, asset->heightSampling))
					fail(lineNumber, "height_sampling must be \"nearest\" or \"linear\"");
			}
			else if (key == "normal_xyz")
			{
				std::vector<uint8_t> normals;
				if (!ParseByteArray(value, normals) || normals.size() != 192)
					fail(lineNumber, "normal_xyz must contain exactly 192 values in [0, 255]");
				else
				{
					std::copy(normals.begin(), normals.end(), asset->normalXyz.begin());
					asset->hasNormals = true;
				}
			}
			else if (key == "direct_lighting_opposite_facing")
			{
				if (!ParseBool(value, asset->directLightingOppositeFacing))
					fail(lineNumber, "direct_lighting_opposite_facing must be true or false");
			}
			else if (key == "normal_ignore_tile_transform")
			{
				if (parsed.schemaVersion < 23) fail(lineNumber, "normal_ignore_tile_transform requires schema_version 23");
				if (!ParseBool(value, asset->normalIgnoreTileTransform))
					fail(lineNumber, "normal_ignore_tile_transform must be true or false");
			}
			else if (key == "geometry_by_facing")
			{
				if (parsed.schemaVersion < 24) fail(lineNumber, "geometry_by_facing requires schema_version 24");
				if (!ParseBool(value, asset->geometryByFacing)) fail(lineNumber, "geometry_by_facing must be true or false");
			}
			else if (key == "geometry_by_flip")
			{
				if (parsed.schemaVersion < 25) fail(lineNumber, "geometry_by_flip requires schema_version 25");
				if (!ParseBool(value, asset->geometryByFlip)) fail(lineNumber, "geometry_by_flip must be true or false");
			}
			else if (key.compare(0, 7, "facing_") == 0 || key.compare(0, 5, "flip_") == 0)
			{
				const bool flip = key.compare(0, 5, "flip_") == 0;
				if (parsed.schemaVersion < (flip ? 25u : 24u)) fail(lineNumber, "geometry keys require a newer schema_version");
				const char *facings[] = {"north", "south", "east", "west"};
				const char *flips[] = {"normal", "horz", "vert", "both"};
				bool recognized = false;
				for (unsigned i = 0; i < 4; i++)
				{
					const std::string prefix = std::string(flip ? "flip_" : "facing_") + (flip ? flips[i] : facings[i]) + "_";
					if (key.compare(0, prefix.size(), prefix) != 0) continue;
					auto &layers = flip ? asset->flipGeometry[i] : asset->facingGeometry[i];
					const auto suffix = key.substr(prefix.size());
					if (suffix == "height" || suffix == "normal_xyz")
					{
						recognized = true;
						std::vector<uint8_t> values;
						std::vector<int16_t> heights;
						if (suffix == "height" ? (!ParseHeightArray(value, heights, parsed.schemaVersion) || heights.size() != 64) : (!ParseByteArray(value, values) || values.size() != 192))
							fail(lineNumber, "facing geometry array has invalid size or values");
						else if (suffix == "height") { std::copy(heights.begin(), heights.end(), layers.height.begin()); layers.hasHeight = true; }
						else { std::copy(values.begin(), values.end(), layers.normalXyz.begin()); layers.hasNormals = true; }
					}
					else if (suffix == "height_sampling")
					{
						recognized = true; std::string sampling;
						if (!ParseString(value, sampling) || (sampling != "nearest" && sampling != "linear")) fail(lineNumber, "invalid facing height_sampling");
						else layers.heightSampling = sampling == "linear" ? RemasterHeightSampling::Linear : RemasterHeightSampling::Nearest;
					}
				}
				if (!recognized) fail(lineNumber, "unknown facing geometry key");
			}
			else if (key == "combine_emitters")
			{
				std::vector<std::string> hashes;
				if (!ParseStringArray(value, hashes) || hashes.empty()) fail(lineNumber, "combine_emitters requires a nonempty tile hash list");
				else for (const auto &hash : hashes) {
					RemasterTileContentId id;
					if (!ParseTileId(hash, id) || std::find(asset->combineEmitters.begin(), asset->combineEmitters.end(), id) != asset->combineEmitters.end())
						fail(lineNumber, "combine_emitters contains an invalid or duplicate tile hash");
					else asset->combineEmitters.push_back(id);
				}
			}
			else if (key == "emission_depth")
			{
				if (!ParseFloat(value, asset->emissionDepth) || asset->emissionDepth < 0 || asset->emissionDepth > 64)
					fail(lineNumber, "emission_depth must be a finite number in [0, 64] source pixels");
			}
			else if (key == "emission_omnidirectional")
			{
				if (!ParseBool(value, asset->emissionOmnidirectional))
					fail(lineNumber, "emission_omnidirectional must be a boolean");
				if (parsed.schemaVersion < 19) fail(lineNumber, "emission_omnidirectional requires schema_version 19");
			}
			else if (key == "emission_rgba")
			{
				std::vector<uint8_t> emission;
				if (!ParseByteArray(value, emission) || emission.size() != 256)
					fail(lineNumber, "emission_rgba must contain exactly 256 values in [0, 255]");
				else
				{
					std::copy(emission.begin(), emission.end(), asset->emissionRgba.begin());
					asset->hasEmission = true;
				}
			}
			else
				fail(lineNumber, "unknown asset key '" + key + "'");
			break;
		case Section::Rule:
			if (!rule)
				break;
			if (key == "tile_hash")
			{
				valid = ParseString(value, rule->tileHash) && ParseTileId(rule->tileHash, rule->tileId);
				rule->hasTileHash = valid;
			}
			else if (key == "asset_group")
			{
				valid = ParseString(value, rule->assetGroup);
				rule->hasAssetGroup = valid;
			}
			else if (key == "material") valid = ParseString(value, rule->material);
			else if (key == "source")
			{
				valid = ParseString(value, stringValue) && ParseSource(stringValue, rule->source);
				rule->hasSource = valid;
			}
			else if (key == "source_index")
			{
				valid = ParseUnsigned(value, unsignedValue) && unsignedValue <= 255;
				if (valid) rule->sourceIndex = static_cast<uint8_t>(unsignedValue);
				rule->hasSourceIndex = valid;
			}
			else if (key == "palette")
			{
				valid = ParseUnsigned(value, unsignedValue) && unsignedValue <= 7;
				if (valid) rule->palette = static_cast<uint8_t>(unsignedValue);
				rule->hasPalette = valid;
			}
			else
			{
				fail(lineNumber, "unknown rule key '" + key + "'");
				break;
			}
			if (!valid)
				fail(lineNumber, "invalid value for '" + key + "'");
			break;
		}
	}

	if (parsed.schemaVersion < 1 || parsed.schemaVersion > 27)
		fail(0, "schema_version must be an integer in [1, 27]");
	if (!parsed.areaLights.empty() && parsed.schemaVersion < 26) fail(0, "area_lights requires schema_version 26");
	for (const auto &entry : parsed.areaLights)
		if (!S9xRemasterValidDirectionalLight(entry.second)) fail(0, "invalid directional light: " + entry.first);
	if(!parsed.obstacleGroups.empty() && parsed.schemaVersion<22)fail(0,"obstacle_groups requires schema_version 22");
	S9xRemasterValidateObstacleGroups(parsed.obstacleGroups,diagnostics);
	for(size_t g=0;g<parsed.obstacleGroups.size();g++)for(size_t m=0;m<parsed.obstacleGroups[g].members.size();m++) {
		const auto &keys=seenKeys[std::to_string(int(Section::ObstacleMember))+":"+std::to_string(g+1)+":"+std::to_string(m+1)];
		for(const char *key:{"tile_hash","x","y","z","hflip","vflip","support_contact"})if(!keys.count(key))fail(0,std::string("obstacle member requires ")+key);
	}
	for (const auto &entry : parsed.assets) {
		if (!entry.second.combineEmitters.empty() && parsed.schemaVersion < 17) fail(0, "combine_emitters requires schema_version 17");
		for (const auto &id : entry.second.combineEmitters)
			if (id == entry.first) fail(0, "combine_emitters must list other tile types, not itself");
	}
	for (const auto &entry : parsed.assets)
		if (entry.second.emissionDepth > 0 && parsed.schemaVersion < 14)
			fail(0, "emission_depth requires schema_version 14");
	if (parsed.schemaVersion == 1 && !parsed.assets.empty())
		fail(0, "assets require schema_version 2");
	if (parsed.schemaVersion < 3)
		for (const auto &entry : parsed.assets)
			if (entry.second.hasEmission)
				fail(0, "emission_rgba requires schema_version 3");
	if (parsed.schemaVersion < 4 && hasLightingSpace)
		fail(0, "lighting_space requires schema_version 4");
	if (parsed.schemaVersion < 5)
		for (const auto &entry : parsed.assets)
			if (entry.second.hasNormals)
				fail(0, "normal_xyz requires schema_version 5");
	if (parsed.schemaVersion < 6)
		for (const auto &entry : parsed.assets)
			if (entry.second.directLightingOppositeFacing)
				fail(0, "direct_lighting_opposite_facing requires schema_version 6");
	if (parsed.schemaVersion < 7 && hasIndirectRoughness)
		fail(0, "indirect_roughness requires schema_version 7");
	if (parsed.schemaVersion < 8 && hasSceneSampling)
		fail(0, "scene contribution and sampling controls require schema_version 8");
	if (parsed.schemaVersion < 9 && hasCameraDirection)
		fail(0, "camera_direction requires schema_version 9");
	if (parsed.schemaVersion < 10 && hasHeightPreviewMultiplier)
		fail(0, "height_preview_multiplier requires schema_version 10");
	if (parsed.schemaVersion < 11)
		for (const auto &entry : parsed.materials)
			if (entry.second.hasDiffuseReflectance)
				fail(0, "diffuse_reflectance requires schema_version 11");
	if (parsed.schemaVersion < 12 && hasReflectanceBoost)
		fail(0, "reflectance_boost requires schema_version 12");
	if (parsed.schemaVersion < 13 && hasUpperFloorHeight)
		fail(0, "upper_floor_height requires schema_version 13");
	if (!std::isfinite(parsed.lightingCoordinateScale) || parsed.lightingCoordinateScale <= 0.0f)
		fail(0, "lighting_space.coordinate_scale must be finite and greater than zero");
	if (!std::isfinite(parsed.indirectRoughness) || parsed.indirectRoughness < 0.0f || parsed.indirectRoughness > 1.0f)
		fail(0, "lighting_space.indirect_roughness must be finite and in [0, 1]");
	if (!std::isfinite(parsed.reflectanceBoost) || parsed.reflectanceBoost < 0.0f || parsed.reflectanceBoost > 8.0f)
		fail(0, "lighting_space.reflectance_boost must be finite and in [0, 8]");
	const float cameraLengthSquared = parsed.cameraDirection[0] * parsed.cameraDirection[0] +
		parsed.cameraDirection[1] * parsed.cameraDirection[1] + parsed.cameraDirection[2] * parsed.cameraDirection[2];
	if (!std::isfinite(parsed.cameraDirection[0]) || !std::isfinite(parsed.cameraDirection[1]) ||
		!std::isfinite(parsed.cameraDirection[2]) || !std::isfinite(cameraLengthSquared) || cameraLengthSquared <= 0.0f)
		fail(0, "lighting_space.camera_direction must be finite and nonzero");
	if (!std::isfinite(parsed.originalSceneContribution) || parsed.originalSceneContribution < 0.0f ||
		parsed.originalSceneContribution > 1.0f)
		fail(0, "lighting_space.original_scene_contribution must be finite and in [0, 1]");
	if (parsed.gameTitle.empty())
		fail(0, "game.title is required");
	if (parsed.romSha256.size() != 64 || parsed.romSha256.find_first_not_of("0123456789abcdef") != std::string::npos)
		fail(0, "game.rom_sha256 must be 64 lowercase hexadecimal digits");
	for (const auto &entry : parsed.materials)
	{
		const RemasterMaterial &item = entry.second;
		if (item.roughness < 0.0f || item.roughness > 1.0f || item.metalness < 0.0f || item.metalness > 1.0f ||
			item.specularLevel < 0.0f || item.specularLevel > 1.0f)
			fail(0, "material '" + item.name + "' has a value outside [0, 1]");
		if (item.hasDiffuseReflectance)
			for (float component : item.diffuseReflectance)
				if (!std::isfinite(component) || component < 0.0f || component > 1.0f)
					fail(0, "material '" + item.name + "' has diffuse_reflectance outside [0, 1]");
		if (item.zMin > item.zMax)
			fail(0, "material '" + item.name + "' has z_min greater than z_max");
	}
	std::map<RemasterTileContentId, std::string> groupMembership;
	for (const auto &entry : parsed.assetGroups)
	{
		const RemasterAssetGroup &item = entry.second;
		if (item.tileIds.empty())
			fail(0, "asset group '" + item.name + "' requires tile_hashes");
		if (!item.material.empty() && !parsed.materials.count(item.material))
			fail(0, "asset group '" + item.name + "' references unknown material '" + item.material + "'");
		for (const RemasterTileContentId &id : item.tileIds)
		{
			auto inserted = groupMembership.emplace(id, item.name);
			if (!inserted.second && inserted.first->second != item.name)
				fail(0, "tile hash belongs to multiple asset groups");
		}
	}
	for (const auto &entry : parsed.assets)
	{
		const RemasterAssetMetadata &item = entry.second;
		if ((item.tileRole == RemasterTileRole::Door || item.hasDoorframeMask) && parsed.schemaVersion < 21)
			fail(0, "door role and doorframe_mask require schema_version 21");
		if (item.hasDoorframeMask && item.tileRole != RemasterTileRole::Door)
			fail(0, "doorframe_mask requires tile_role = door");
		if (!item.tileId.hashVersion)
			fail(0, "asset requires tile_hash");
		for (const auto &facing : item.facingGeometry)
			if (!facing.hasHeight && facing.heightSampling != RemasterHeightSampling::Nearest)
				fail(0, "facing height_sampling requires facing height");
		for (const auto &flip : item.flipGeometry)
			if (!flip.hasHeight && flip.heightSampling != RemasterHeightSampling::Nearest)
				fail(0, "flip height_sampling requires flip height");
		if (!item.hasMaterialSelectors && !item.hasOcclusion && !item.hasHeight && !item.hasNormals && !item.hasEmission && item.emissionDepth == 0 && !item.emissionOmnidirectional && item.combineEmitters.empty() &&
			!item.directLightingOppositeFacing && !item.normalIgnoreTileTransform && !item.geometryByFacing && !item.geometryByFlip &&
			std::none_of(item.flipGeometry.begin(), item.flipGeometry.end(), [](const auto &g) { return g.hasHeight || g.hasNormals; }) &&
			std::none_of(item.facingGeometry.begin(), item.facingGeometry.end(), [](const auto &g) { return g.hasHeight || g.hasNormals; }) && item.tileRole == RemasterTileRole::Unknown && !item.hasTileSemantics)
			fail(0, "asset requires materials, occlusion, height, normal_xyz, emission_rgba, direct_lighting_opposite_facing, or a known tile_role");
		if (!item.hasHeight && item.heightSampling != RemasterHeightSampling::Nearest)
			fail(0, "asset height_sampling requires height");
		if (!item.hasTileSemantics && (!item.tileSemanticNormals.empty() || !item.tileSemanticCorner.empty() ||
			item.tileSemanticContextDependent || !item.tileSemanticNote.empty()))
			fail(0, "tile semantic fields require tile_semantics_authored = true");
		if (item.hasTileSemantics)
		{
			const size_t expected = item.tileRole == RemasterTileRole::Wall ? 1 :
				(item.tileRole == RemasterTileRole::WallCorner ? 2 : 0);
			std::set<std::string> unique;
			for (const auto &normal : item.tileSemanticNormals)
			{
				const bool cardinal = normal == "north" || normal == "east" || normal == "south" || normal == "west";
				const bool diagonal = normal == "north_east" || normal == "south_east" || normal == "south_west" || normal == "north_west";
				if ((!cardinal && !diagonal) || (expected == 2 && !cardinal) || !unique.insert(normal).second)
					fail(0, "invalid or duplicate tile semantic normal");
			}
			if (item.tileSemanticNormals.size() != expected) fail(0, "tile role requires its exact wall normal count");
			if (item.tileRole == RemasterTileRole::WallCorner ?
				(item.tileSemanticCorner != "inside" && item.tileSemanticCorner != "outside") : !item.tileSemanticCorner.empty())
				fail(0, "tile semantic corner must be inside/outside for wall_corner only");
			if (item.tileSemanticNote.size() > 4000) fail(0, "tile semantic note is too long");
		}
		for (const std::string &name : item.materialSelectors)
			if (!name.empty() && !parsed.materials.count(name))
				fail(0, "asset references unknown material '" + name + "'");
	}
	for (RemasterProfileRule &item : parsed.rules)
	{
		if (item.hasTileHash == item.hasAssetGroup)
			fail(item.line, "rule requires exactly one of tile_hash or asset_group");
		if (item.hasAssetGroup && !parsed.assetGroups.count(item.assetGroup))
			fail(item.line, "rule references unknown asset group '" + item.assetGroup + "'");
		if (item.hasSourceIndex && !item.hasSource)
			fail(item.line, "source_index requires source");
		if (item.hasSourceIndex && item.hasSource &&
			((item.source == RemasterSourceType::Background && item.sourceIndex > 3) ||
			 (item.source == RemasterSourceType::Backdrop && item.sourceIndex != 0)))
			fail(item.line, "source_index is outside the source range");
		if (item.material.empty())
		{
			if (item.hasAssetGroup)
			{
				auto group = parsed.assetGroups.find(item.assetGroup);
				if (group != parsed.assetGroups.end())
					item.material = group->second.material;
			}
			if (item.material.empty())
				fail(item.line, "rule has no material");
		}
		if (!item.material.empty() && !parsed.materials.count(item.material))
			fail(item.line, "rule references unknown material '" + item.material + "'");
		item.specificity = static_cast<uint8_t>(1 + item.hasSource + item.hasSourceIndex + item.hasPalette);
	}

	if (!diagnostics.empty())
		return false;
	profile = std::move(parsed);
	return true;
}

inline bool S9xRemasterLoadProfile (const std::string &path, RemasterProfile &profile,
	std::vector<RemasterProfileDiagnostic> &diagnostics)
{
	std::ifstream input(path);
	if (!input)
	{
		diagnostics.clear();
		diagnostics.push_back({ 0, "unable to open profile '" + path + "'" });
		return false;
	}
	std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	return S9xRemasterParseProfile(text, profile, diagnostics);
}

namespace RemasterProfileSerialization
{
	inline std::string Quote (const std::string &value)
	{
		std::string result = "\"";
		for (char character : value)
		{
			if (character == '\n') { result += "\\n"; continue; }
			if (character == '\r') { result += "\\r"; continue; }
			if (character == '\t') { result += "\\t"; continue; }
			if (character == '\\' || character == '"')
				result.push_back('\\');
			result.push_back(character);
		}
		return result + '"';
	}

	inline std::string TileId (const RemasterTileContentId &tileId)
	{
		std::ostringstream text;
		text << "v" << unsigned(tileId.hashVersion) << ':' << unsigned(tileId.bitDepth) << "bpp:"
			<< std::hex << std::setfill('0') << std::setw(16) << tileId.hash;
		return text.str();
	}

	inline const char *SurfaceClass (RemasterSurfaceClass surfaceClass)
	{
		switch (surfaceClass)
		{
		case RemasterSurfaceClass::Floor: return "floor";
		case RemasterSurfaceClass::WallFace: return "wall_face";
		case RemasterSurfaceClass::WallTop: return "wall_top";
		case RemasterSurfaceClass::Character: return "character";
		case RemasterSurfaceClass::Prop: return "prop";
		case RemasterSurfaceClass::Effect: return "effect";
		case RemasterSurfaceClass::UserInterface: return "user_interface";
		case RemasterSurfaceClass::Unclassified:
		default: return "unclassified";
		}
	}

	inline const char *Source (RemasterSourceType source)
	{
		switch (source)
		{
		case RemasterSourceType::Background: return "background";
		case RemasterSourceType::Object: return "object";
		case RemasterSourceType::Backdrop:
		default: return "backdrop";
		}
	}
}

inline bool S9xRemasterSerializeProfile (const RemasterProfile &profile, std::string &text,
	std::vector<RemasterProfileDiagnostic> &diagnostics, bool validate = true)
{
	using namespace RemasterProfileSerialization;
	std::ostringstream output;
	output.imbue(std::locale::classic());
	output << std::setprecision(std::numeric_limits<float>::max_digits10);
	bool hasEmission = false;
	bool hasNormals = false;
	bool hasOppositeFacingDirectLighting = false;
	bool hasFixedNormalDirection = false;
	bool hasFacingGeometry = false;
	bool hasFlipGeometry = false;
	bool hasDiffuseReflectance = false;
	bool hasEmissionDepth = false;
	bool hasCombinedEmitters = false;
	bool hasOmnidirectionalEmission = false;
	for (const auto &entry : profile.assets)
	{
		if (!std::isfinite(entry.second.emissionDepth) || entry.second.emissionDepth < 0 || entry.second.emissionDepth > 64) return false;
		hasEmission |= entry.second.hasEmission;
		hasEmissionDepth |= entry.second.emissionDepth > 0;
		hasOmnidirectionalEmission |= entry.second.emissionOmnidirectional;
		hasCombinedEmitters |= !entry.second.combineEmitters.empty();
		hasNormals |= entry.second.hasNormals;
		hasOppositeFacingDirectLighting |= entry.second.directLightingOppositeFacing;
		hasFixedNormalDirection |= entry.second.normalIgnoreTileTransform;
		hasFacingGeometry |= entry.second.geometryByFacing || std::any_of(entry.second.facingGeometry.begin(), entry.second.facingGeometry.end(), [](const auto &g) { return g.hasHeight || g.hasNormals; });
		hasFlipGeometry |= entry.second.geometryByFlip || std::any_of(entry.second.flipGeometry.begin(), entry.second.flipGeometry.end(), [](const auto &g) { return g.hasHeight || g.hasNormals; });
	}
	for (const auto &entry : profile.materials)
		hasDiffuseReflectance |= entry.second.hasDiffuseReflectance;
	const bool hasSignedHeight = std::any_of(profile.assets.begin(), profile.assets.end(), [](const auto &entry) {
		auto wide = [](const auto &g) { return g.hasHeight && std::any_of(g.height.begin(), g.height.end(), [](int16_t h) { return h < 0 || h > 255; }); };
		return wide(entry.second) || std::any_of(entry.second.facingGeometry.begin(), entry.second.facingGeometry.end(), wide) ||
			std::any_of(entry.second.flipGeometry.begin(), entry.second.flipGeometry.end(), wide);
	});
	const uint32_t requiredSchema = std::max<uint32_t>(hasSignedHeight ? 27 : !profile.areaLights.empty() ? 26 : hasFlipGeometry ? 25 : hasFacingGeometry ? 24 : hasFixedNormalDirection ? 23 : profile.upperFloorHeight > 0 ? 13 :
		(profile.reflectanceBoost > 0.0f ? 12 : (hasDiffuseReflectance ? 11 : 10)),
		hasOppositeFacingDirectLighting ? 6 : (hasNormals ? 5 :
		(hasEmission ? 3 : (profile.assets.empty() ? profile.schemaVersion : 2))));
	const bool hasTileRoles = std::any_of(profile.assets.begin(), profile.assets.end(), [] (const auto &entry) { return entry.second.tileRole != RemasterTileRole::Unknown; });
	const bool hasTileSemantics = std::any_of(profile.assets.begin(), profile.assets.end(), [] (const auto &entry) { return entry.second.hasTileSemantics; });
	const bool hasDoors = std::any_of(profile.assets.begin(), profile.assets.end(), [] (const auto &entry) { return entry.second.tileRole == RemasterTileRole::Door || entry.second.hasDoorframeMask; });
	output << "schema_version = " << std::max(profile.obstacleGroups.empty()?0u:22u, std::max(hasDoors ? 21u : (hasTileSemantics ? 20u : (hasOmnidirectionalEmission ? 19u : (hasTileRoles ? 18u : (hasCombinedEmitters ? 17u : (profile.representativeEmitters ? 16u : 15u))))), std::max(profile.schemaVersion, std::max(hasEmissionDepth ? 14u : 0u, requiredSchema)))) << "\n\n";
	output << "[game]\n";
	output << "title = " << Quote(profile.gameTitle) << "\n";
	output << "rom_sha256 = " << Quote(profile.romSha256) << "\n";
	output << "\n[lighting_space]\n";
	output << "coordinate_scale = " << profile.lightingCoordinateScale << "\n";
	output << "camera_direction = [" << profile.cameraDirection[0] << ", " << profile.cameraDirection[1] << ", " <<
		profile.cameraDirection[2] << "]\n";
	output << "indirect_bounces = " << unsigned(profile.indirectBounceCount) << "\n";
	output << "indirect_roughness = " << profile.indirectRoughness << "\n";
	if (profile.schemaVersion >= 12 || profile.reflectanceBoost > 0.0f)
		output << "reflectance_boost = " << profile.reflectanceBoost << "\n";
	output << "original_scene_contribution = " << profile.originalSceneContribution << "\n";
	output << "height_preview_multiplier = " << unsigned(profile.heightPreviewMultiplier) << "\n";
	if (profile.schemaVersion >= 13 || profile.upperFloorHeight)
		output << "upper_floor_height = " << unsigned(profile.upperFloorHeight) << "\n";
	output << "samples_per_frame = " << unsigned(profile.samplesPerFrame) << "\n";
	output << "emitter_patch_size = " << unsigned(profile.emitterPatchSize) << "\n";
	if (profile.representativeEmitters) output << "representative_emitters = " << unsigned(profile.representativeEmitters) << "\n";
	output << "sample_accumulation = " << (profile.sampleAccumulation ? "true" : "false") << "\n";
	for (const auto &entry : profile.areaLights)
	{
		const auto &light = entry.second;
		if (!S9xRemasterValidAreaKey(entry.first) || !S9xRemasterValidDirectionalLight(light)) return false;
		output << "\n[area_lights." << entry.first << "]\n";
		output << "enabled = " << (light.enabled ? "true" : "false") << "\n";
		output << "direction = [" << light.direction[0] << ", " << light.direction[1] << ", " << light.direction[2] << "]\n";
		output << "color = [" << light.color[0] << ", " << light.color[1] << ", " << light.color[2] << "]\n";
		output << "brightness = " << light.brightness << "\n";
	}
	for (const auto &entry : profile.materials)
	{
		const RemasterMaterial &material = entry.second;
		output << "\n[materials." << entry.first << "]\n";
		output << "surface_class = " << Quote(SurfaceClass(material.surfaceClass)) << "\n";
		if (material.hasDiffuseReflectance)
			output << "diffuse_reflectance = [" << material.diffuseReflectance[0] << ", " <<
				material.diffuseReflectance[1] << ", " << material.diffuseReflectance[2] << "]\n";
		output << "roughness = " << material.roughness << "\n";
		output << "metalness = " << material.metalness << "\n";
		output << "specular_level = " << material.specularLevel << "\n";
		output << "z_min = " << material.zMin << "\n";
		output << "z_max = " << material.zMax << "\n";
		output << "receives_gi = " << (material.receivesGi ? "true" : "false") << "\n";
		output << "casts_shadow = " << (material.castsShadow ? "true" : "false") << "\n";
	}
	for (const auto &entry : profile.assetGroups)
	{
		const RemasterAssetGroup &group = entry.second;
		output << "\n[asset_groups." << entry.first << "]\n";
		output << "tile_hashes = [";
		for (size_t i = 0; i < group.tileIds.size(); i++)
			output << (i ? ", " : "") << Quote(TileId(group.tileIds[i]));
		output << "]\n";
		if (!group.material.empty())
			output << "material = " << Quote(group.material) << "\n";
	}
	for(const auto &group:profile.obstacleGroups) {
		output << "\n[[obstacle_groups]]\nname = " << Quote(group.name) << "\n";
		for(const auto &m:group.members)output << "\n[[obstacle_groups.members]]\ntile_hash = " << Quote(TileId(m.tileId))
			<< "\nx = " << m.x << "\ny = " << m.y << "\nz = " << m.z
			<< "\nhflip = " << (m.hflip?"true":"false") << "\nvflip = " << (m.vflip?"true":"false")
			<< "\nsupport_contact = " << (m.supportContact?"true":"false") << "\n";
	}
	for (const auto &entry : profile.assets)
	{
		const RemasterAssetMetadata &asset = entry.second;
		const bool storeOcclusion = S9xRemasterOcclusionNeedsStorage(asset.hasOcclusion, asset.occlusion);
		// A tile with only default coverage needs no asset table at all.
		if (asset.hasOcclusion && !storeOcclusion && !asset.hasMaterialSelectors && !asset.hasHeight &&
			!asset.hasNormals && !asset.hasEmission && asset.emissionDepth == 0 && !asset.emissionOmnidirectional && asset.combineEmitters.empty() && !asset.directLightingOppositeFacing && !asset.normalIgnoreTileTransform && !asset.geometryByFacing && !asset.geometryByFlip &&
			std::none_of(asset.flipGeometry.begin(), asset.flipGeometry.end(), [](const auto &g) { return g.hasHeight || g.hasNormals; }) &&
			std::none_of(asset.facingGeometry.begin(), asset.facingGeometry.end(), [](const auto &g) { return g.hasHeight || g.hasNormals; }) && !asset.hasTileSemantics && asset.tileRole == RemasterTileRole::Unknown)
			continue;
		output << "\n[[assets]]\n";
		output << "tile_hash = " << Quote(TileId(asset.tileId)) << "\n";
		if (asset.hasMaterialSelectors)
		{
			output << "materials = [";
			for (size_t i = 0; i < asset.materialSelectors.size(); i++)
				output << (i ? ", " : "") << Quote(asset.materialSelectors[i]);
			output << "]\n";
		}
		if (storeOcclusion)
		{
			output << "occlusion = [";
			for (size_t i = 0; i < asset.occlusion.size(); i++)
				output << (i ? ", " : "") << unsigned(asset.occlusion[i]);
			output << "]\n";
		}
		if (asset.hasHeight)
		{
			output << "height = [";
			for (size_t i = 0; i < asset.height.size(); i++)
				output << (i ? ", " : "") << int(asset.height[i]);
			output << "]\n";
			output << "height_sampling = " << Quote(asset.heightSampling == RemasterHeightSampling::Linear ?
				"linear" : "nearest") << "\n";
		}
		if (asset.hasNormals)
		{
			output << "normal_xyz = [";
			for (size_t i = 0; i < asset.normalXyz.size(); i++)
				output << (i ? ", " : "") << unsigned(asset.normalXyz[i]);
			output << "]\n";
		}
		if (asset.directLightingOppositeFacing)
			output << "direct_lighting_opposite_facing = true\n";
		if (asset.normalIgnoreTileTransform)
			output << "normal_ignore_tile_transform = true\n";
		if (asset.geometryByFacing) output << "geometry_by_facing = true\n";
		if (asset.geometryByFlip) output << "geometry_by_flip = true\n";
		const char *facings[] = {"north", "south", "east", "west"};
		const char *flips[] = {"normal", "horz", "vert", "both"};
		for (unsigned f = 0; f < 8; f++)
		{
			const auto &g = f < 4 ? asset.facingGeometry[f] : asset.flipGeometry[f - 4];
			const std::string prefix = f < 4 ? std::string("facing_") + facings[f] + "_" : std::string("flip_") + flips[f - 4] + "_";
			if (g.hasHeight)
			{
				output << prefix << "height = [";
				for (size_t i = 0; i < 64; i++) output << (i ? ", " : "") << int(g.height[i]);
				output << "]\n" << prefix << "height_sampling = " << Quote(g.heightSampling == RemasterHeightSampling::Linear ? "linear" : "nearest") << "\n";
			}
			if (g.hasNormals)
			{
				output << prefix << "normal_xyz = [";
				for (size_t i = 0; i < 192; i++) output << (i ? ", " : "") << unsigned(g.normalXyz[i]);
				output << "]\n";
			}
		}
		if (asset.hasEmission)
		{
			output << "emission_rgba = [";
			for (size_t i = 0; i < asset.emissionRgba.size(); i++)
				output << (i ? ", " : "") << unsigned(asset.emissionRgba[i]);
			output << "]\n";
		}
		if (asset.tileRole != RemasterTileRole::Unknown)
			output << "tile_role = " << Quote(S9xRemasterTileRoleName(asset.tileRole)) << "\n";
		if (asset.hasDoorframeMask)
		{
			std::ostringstream mask;
			mask << std::hex << std::setfill('0') << std::setw(16) << asset.doorframeMask;
			output << "doorframe_mask = " << Quote(mask.str()) << "\n";
		}
		if (asset.hasTileSemantics)
		{
			output << "tile_semantics_authored = true\ntile_semantic_normals = [";
			for (size_t i = 0; i < asset.tileSemanticNormals.size(); i++)
				output << (i ? ", " : "") << Quote(asset.tileSemanticNormals[i]);
			output << "]\ntile_semantic_context_dependent = " << (asset.tileSemanticContextDependent ? "true" : "false") << "\n";
			if (!asset.tileSemanticCorner.empty()) output << "tile_semantic_corner = " << Quote(asset.tileSemanticCorner) << "\n";
			output << "tile_semantic_note = " << Quote(asset.tileSemanticNote) << "\n";
		}
		if (asset.emissionDepth > 0) output << "emission_depth = " << asset.emissionDepth << "\n";
		if (asset.emissionOmnidirectional) output << "emission_omnidirectional = true\n";
		if (!asset.combineEmitters.empty()) {
			output << "combine_emitters = [";
			auto peers = asset.combineEmitters; std::sort(peers.begin(), peers.end());
			for (size_t i = 0; i < peers.size(); i++) output << (i ? ", " : "") << Quote(TileId(peers[i]));
			output << "]\n";
		}
	}
	for (const RemasterProfileRule &rule : profile.rules)
	{
		output << "\n[[rules]]\n";
		if (rule.hasTileHash)
			output << "tile_hash = " << Quote(TileId(rule.tileId)) << "\n";
		else if (rule.hasAssetGroup)
			output << "asset_group = " << Quote(rule.assetGroup) << "\n";
		if (rule.hasSource)
			output << "source = " << Quote(Source(rule.source)) << "\n";
		if (rule.hasSourceIndex)
			output << "source_index = " << unsigned(rule.sourceIndex) << "\n";
		if (rule.hasPalette)
			output << "palette = " << unsigned(rule.palette) << "\n";
		output << "material = " << Quote(rule.material) << "\n";
	}

	const std::string serialized = output.str();
	if (validate)
	{
		RemasterProfile validated;
		if (!S9xRemasterParseProfile(serialized, validated, diagnostics))
			return false;
	}
	text = serialized;
	return true;
}

inline bool S9xRemasterWriteProfile (const RemasterProfile &profile, const std::string &path,
	std::vector<RemasterProfileDiagnostic> &diagnostics)
{
	std::string text;
	if (!S9xRemasterSerializeProfile(profile, text, diagnostics))
		return false;
	const std::string temporaryPath = path + ".tmp";
	std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
	if (!output)
	{
		diagnostics = { { 0, "unable to create temporary profile" } };
		return false;
	}
	output.write(text.data(), static_cast<std::streamsize>(text.size()));
	output.close();
	if (!output.good() || std::rename(temporaryPath.c_str(), path.c_str()) != 0)
	{
		std::remove(temporaryPath.c_str());
		diagnostics = { { 0, "unable to atomically replace profile" } };
		return false;
	}
	return true;
}

inline RemasterProfileMatch S9xRemasterMatchProfile (const RemasterProfile &profile,
	const RemasterProfileMatchContext &context)
{
	RemasterProfileMatch result;
	uint8_t bestSpecificity = 0;
	for (const RemasterProfileRule &rule : profile.rules)
	{
		const RemasterAssetGroup *group = nullptr;
		bool identityMatches = rule.hasTileHash && rule.tileId == context.tileId;
		if (rule.hasAssetGroup)
		{
			auto found = profile.assetGroups.find(rule.assetGroup);
			if (found != profile.assetGroups.end())
			{
				group = &found->second;
				identityMatches = std::find(group->tileIds.begin(), group->tileIds.end(), context.tileId) != group->tileIds.end();
			}
		}
		if (!identityMatches || (rule.hasSource && rule.source != context.source) ||
			(rule.hasSourceIndex && rule.sourceIndex != context.sourceIndex) ||
			(rule.hasPalette && rule.palette != context.palette))
			continue;
		if (result.status == RemasterProfileMatchStatus::NoMatch || rule.specificity > bestSpecificity)
		{
			result.status = RemasterProfileMatchStatus::Matched;
			result.rule = &rule;
			result.assetGroup = group;
			result.material = &profile.materials.find(rule.material)->second;
			result.conflictingRuleLines.assign(1, rule.line);
			bestSpecificity = rule.specificity;
		}
		else if (rule.specificity == bestSpecificity)
		{
			result.status = RemasterProfileMatchStatus::Ambiguous;
			result.material = nullptr;
			result.rule = nullptr;
			result.assetGroup = nullptr;
			result.conflictingRuleLines.push_back(rule.line);
		}
	}
	return result;
}

#endif
