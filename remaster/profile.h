/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REMASTER_PROFILE_H_
#define _REMASTER_PROFILE_H_

#include "types.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
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
	float roughness = 0.8f;
	float metalness = 0.0f;
	float specularLevel = 0.25f;
	float zMin = 0.0f;
	float zMax = 0.0f;
	bool receivesGi = true;
	bool castsShadow = false;
};

struct RemasterAssetGroup
{
	std::string name;
	std::vector<RemasterTileContentId> tileIds;
	std::string material;
};

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

struct RemasterProfile
{
	uint32_t schemaVersion = 0;
	std::string gameTitle;
	std::string romSha256;
	std::map<std::string, RemasterMaterial> materials;
	std::map<std::string, RemasterAssetGroup> assetGroups;
	std::vector<RemasterProfileRule> rules;
};

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
				if (++i + 1 >= value.size() || (value[i] != '\\' && value[i] != '"'))
					return false;
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
}

inline bool S9xRemasterParseProfile (const std::string &text, RemasterProfile &profile,
	std::vector<RemasterProfileDiagnostic> &diagnostics)
{
	using namespace RemasterProfileParsing;
	enum class Section { Root, Game, Material, AssetGroup, Rule };
	Section section = Section::Root;
	RemasterProfile parsed;
	RemasterMaterial *material = nullptr;
	RemasterAssetGroup *group = nullptr;
	RemasterProfileRule *rule = nullptr;
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
			rule = nullptr;
			if (line == "[game]")
				section = Section::Game;
			else if (line == "[[rules]]")
			{
				section = Section::Rule;
				parsed.rules.emplace_back();
				rule = &parsed.rules.back();
				rule->line = lineNumber;
			}
			else
			{
				std::string name;
				if (parseNamedSection(line, "[materials.", lineNumber, name))
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
		if (material)
			keyScope += ":" + material->name;
		if (group)
			keyScope += ":" + group->name;
		if (rule)
			keyScope += ":" + std::to_string(parsed.rules.size());
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
		case Section::Material:
			if (!material)
				break;
			if (key == "surface_class")
			{
				if (!ParseString(value, stringValue) || !ParseSurfaceClass(stringValue, material->surfaceClass))
					fail(lineNumber, "invalid surface_class");
			}
			else if (key == "roughness") valid = ParseFloat(value, material->roughness);
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

	if (parsed.schemaVersion != 1)
		fail(0, "schema_version must be 1");
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
