// Offline, capture-driven suggestions for character surface metadata.
// Build: c++ -std=c++17 -O2 -Wall -Wextra -pedantic tools/remaster_geometry.cpp -o /tmp/remaster-geometry
#include "../remaster/frame.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
struct Tile
{
	RemasterTileContentId id;
	std::array<uint8_t, 64> pixels = {};
};

struct Match
{
	const Tile *source = nullptr;
	int dx = 0;
	int dy = 0;
	bool flip = false;
	float score = -1.0f;
	int exact = 0;
};

struct Suggestion
{
	Tile tile;
	RemasterAssetMetadata metadata;
	Match match;
	bool wroteHeight = false;
	bool wroteNormals = false;
};

struct PaletteColors
{
	std::array<std::array<uint8_t, 3>, 16> rgb = {};
	std::array<bool, 16> known = {};
};

bool IsNarrow (const Tile &tile)
{
	int opaque = 0;
	for (int y = 0; y < 8; y++)
	{
		int width = 0;
		for (int x = 0; x < 8; x++)
			if (tile.pixels[y * 8 + x])
			{
				width++;
				opaque++;
			}
		if (width > 2)
			return false;
	}
	return opaque >= 3;
}

std::string Id (const RemasterTileContentId &id)
{
	return RemasterProfileSerialization::TileId(id);
}

bool IsDetailed (const RemasterAssetMetadata &metadata, const Tile &tile)
{
	if (!metadata.hasNormals || !metadata.hasHeight)
		return false;
	std::set<std::array<uint8_t, 3>> normals;
	for (size_t p = 0; p < 64; p++)
		if (tile.pixels[p])
			normals.insert({{ metadata.normalXyz[p * 3], metadata.normalXyz[p * 3 + 1], metadata.normalXyz[p * 3 + 2] }});
	return normals.size() >= 5;
}

bool IsFlat (const RemasterAssetMetadata &metadata, const Tile &tile)
{
	if (!metadata.hasNormals)
		return false;
	std::set<std::array<uint8_t, 3>> normals;
	for (size_t p = 0; p < 64; p++)
		if (tile.pixels[p])
			normals.insert({{ metadata.normalXyz[p * 3], metadata.normalXyz[p * 3 + 1], metadata.normalXyz[p * 3 + 2] }});
	return normals.size() == 1;
}

std::map<RemasterTileContentId, Tile> CollectTiles (const std::vector<RemasterFrame> &frames)
{
	std::map<RemasterTileContentId, Tile> result;
	for (const RemasterFrame &frame : frames)
		for (const RemasterFrameAsset &asset : frame.assets)
		{
			Tile tile;
			tile.id = asset.tileId;
			std::copy(asset.indices, asset.indices + 64, tile.pixels.begin());
			const auto found = result.find(tile.id);
			if (found != result.end() && found->second.pixels != tile.pixels)
				throw std::runtime_error("captured tile hash collision: " + Id(tile.id));
			result[tile.id] = tile;
		}
	return result;
}

Match FindMatch (const Tile &target, const std::vector<const Tile *> &references)
{
	Match best;
	for (const Tile *source : references)
		for (int flip = 0; flip < 2; flip++)
			for (int dy = -2; dy <= 2; dy++)
				for (int dx = -2; dx <= 2; dx++)
				{
					float score = 0.0f;
					int exact = 0, visible = 0;
					for (int y = 0; y < 8; y++)
						for (int x = 0; x < 8; x++)
						{
							const uint8_t value = target.pixels[y * 8 + x];
							if (!value)
								continue;
							visible++;
							const int sx = (flip ? 7 - x : x) + dx, sy = y + dy;
							if (sx < 0 || sx >= 8 || sy < 0 || sy >= 8 || !source->pixels[sy * 8 + sx])
								continue;
							if (value == source->pixels[sy * 8 + sx])
							{
								score += 1.0f;
								exact++;
							}
							else
								score += 0.1f;
						}
					const float coverage = visible ? score / visible : 0.0f;
					// Favor a stable, local correspondence when scores tie.
					const float ranked = coverage - 0.005f * (std::abs(dx) + std::abs(dy));
					if (exact >= 3 && ranked > best.score)
						best = { source, dx, dy, flip != 0, ranked, exact };
				}
	return best;
}

int SourcePixel (const Match &match, const Tile &target, int x, int y)
{
	if (!match.source)
		return -1;
	const int cx = (match.flip ? 7 - x : x) + match.dx, cy = y + match.dy;
	int best = -1, bestDistance = std::numeric_limits<int>::max();
	for (int radius = 0; radius <= 2; radius++)
		for (int oy = -radius; oy <= radius; oy++)
			for (int ox = -radius; ox <= radius; ox++)
			{
				const int sx = cx + ox, sy = cy + oy;
				if (sx < 0 || sx >= 8 || sy < 0 || sy >= 8)
					continue;
				const int p = sy * 8 + sx;
				if (match.source->pixels[p] != target.pixels[y * 8 + x])
					continue;
				const int distance = ox * ox + oy * oy;
				if (distance < bestDistance)
				{
					best = p;
					bestDistance = distance;
				}
			}
	return best;
}

uint8_t EncodeNormal (float component)
{
	return static_cast<uint8_t>(std::max(0, std::min(255,
		static_cast<int>(std::lround(128.0f + 127.0f * component)))));
}

void Suggest (Suggestion &item, const RemasterAssetMetadata &reference, bool replaceFlat)
{
	const Tile &target = item.tile;
	const Match &match = item.match;
	item.wroteHeight = !item.metadata.hasHeight;
	item.wroteNormals = !item.metadata.hasNormals || (replaceFlat && IsFlat(item.metadata, target));
	if (!item.wroteHeight && !item.wroteNormals)
		return;
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++)
		{
			const int p = y * 8 + x;
			if (!target.pixels[p])
				continue;
			const int sourcePixel = SourcePixel(match, target, x, y);
			const int sx = std::max(0, std::min(7, (match.flip ? 7 - x : x) + match.dx));
			const int sy = std::max(0, std::min(7, y + match.dy));
			if (item.wroteHeight)
			{
				// A silhouette pixel is given the nearest corresponding source height;
				// the source row is the fallback for a changed color or small pose shift.
				const int hp = sourcePixel >= 0 ? sourcePixel : sy * 8 + sx;
				item.metadata.height[p] = reference.height[hp];
			}
			if (item.wroteNormals)
			{
				float nx, ny, nz;
				if (sourcePixel >= 0)
				{
					nx = (reference.normalXyz[sourcePixel * 3] - 128.0f) / 127.0f;
					ny = (reference.normalXyz[sourcePixel * 3 + 1] - 128.0f) / 127.0f;
					nz = (reference.normalXyz[sourcePixel * 3 + 2] - 128.0f) / 127.0f;
					if (match.flip)
						nx = -nx;
				}
				else
				{
					int left = 8, right = -1;
					for (int xx = 0; xx < 8; xx++)
						if (target.pixels[y * 8 + xx])
						{
							left = std::min(left, xx);
							right = std::max(right, xx);
						}
					const float halfWidth = std::max(2.0f, (right - left + 1) * 0.5f);
					nx = 0.65f * (x - (left + right) * 0.5f) / halfWidth;
					ny = 0.45f;
					nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
				}
				const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
				if (length > 0.001f)
				{
					item.metadata.normalXyz[p * 3] = EncodeNormal(nx / length);
					item.metadata.normalXyz[p * 3 + 1] = EncodeNormal(ny / length);
					item.metadata.normalXyz[p * 3 + 2] = EncodeNormal(nz / length);
				}
			}
		}
	if (item.wroteHeight)
	{
		item.metadata.hasHeight = true;
		item.metadata.heightSampling = RemasterHeightSampling::Nearest;
	}
	if (item.wroteNormals)
		item.metadata.hasNormals = true;
}

void SuggestNarrow (Suggestion &item, bool replaceFlat)
{
	item.wroteNormals = !item.metadata.hasNormals || (replaceFlat && IsFlat(item.metadata, item.tile));
	if (!item.wroteNormals)
		return;
	for (int y = 0; y < 8; y++)
	{
		int left = 8, right = -1;
		for (int x = 0; x < 8; x++)
			if (item.tile.pixels[y * 8 + x])
			{
				left = std::min(left, x);
				right = std::max(right, x);
			}
		for (int x = left; x <= right; x++)
		{
			const int p = y * 8 + x;
			if (!item.tile.pixels[p])
				continue;
			const float nx = left == right ? 0.0f : (x == left ? -0.25f : 0.25f);
			item.metadata.normalXyz[p * 3] = EncodeNormal(nx);
			item.metadata.normalXyz[p * 3 + 1] = 128;
			item.metadata.normalXyz[p * 3 + 2] = EncodeNormal(std::sqrt(1.0f - nx * nx));
		}
	}
	item.metadata.hasNormals = true;
}

void PrintUsage ()
{
	std::cerr << "Usage: remaster-geometry --profile PROFILE --capture FRAME [--capture FRAME ...]"
		" --group NAME --output CANDIDATE [--palette 0..7] [--replace-flat] [--preview PPM]\n"
		"Uses detailed authored tiles as references. --palette adds captured object tiles to the group.\n";
}

void Preview (const std::string &path, const std::vector<Suggestion> &suggestions,
	const PaletteColors &palette)
{
	const int scale = 4, tileWidth = 8 * scale, width = tileWidth * 3;
	const int height = static_cast<int>(suggestions.size()) * 8 * scale;
	std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3, 0);
	for (size_t row = 0; row < suggestions.size(); row++)
		for (int p = 0; p < 64; p++)
			for (int yy = 0; yy < scale; yy++)
				for (int xx = 0; xx < scale; xx++)
				{
					const int x = (p % 8) * scale + xx, y = (static_cast<int>(row) * 8 + p / 8) * scale + yy;
					const uint8_t index = suggestions[row].tile.pixels[p];
					const std::array<uint8_t, 3> art = index && palette.known[index] ? palette.rgb[index] :
						std::array<uint8_t, 3>{{ static_cast<uint8_t>(index ? 30 + index * 13 : 15),
							static_cast<uint8_t>(index ? 40 + index * 7 : 15),
							static_cast<uint8_t>(index ? 200 - index * 9 : 15) }};
					const uint8_t grey = index ? static_cast<uint8_t>(std::min(255,
						static_cast<int>(suggestions[row].metadata.height[p]) * 11)) : 15;
					const uint8_t norm[3] = { suggestions[row].metadata.normalXyz[p * 3],
						suggestions[row].metadata.normalXyz[p * 3 + 1], suggestions[row].metadata.normalXyz[p * 3 + 2] };
					for (int panel = 0; panel < 3; panel++)
						for (int c = 0; c < 3; c++)
							rgb[(static_cast<size_t>(y) * width + x + panel * tileWidth) * 3 + c] =
								panel == 0 ? art[c] : panel == 1 ? grey : (index ? norm[c] : 15);
				}
	std::ofstream output(path, std::ios::binary);
	output << "P6\n" << width << " " << height << "\n255\n";
	output.write(reinterpret_cast<const char *>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
	if (!output)
		throw std::runtime_error("unable to write preview");
}
} // namespace

int main (int argc, char **argv)
{
	try
	{
		std::string profilePath, groupName, outputPath, previewPath;
		std::vector<std::string> capturePaths;
		bool replaceFlat = false;
		int selectedPalette = -1;
		for (int i = 1; i < argc; i++)
		{
			const std::string option = argv[i];
			if (option == "--replace-flat")
				replaceFlat = true;
			else if (i + 1 < argc && option == "--profile")
				profilePath = argv[++i];
			else if (i + 1 < argc && option == "--group")
				groupName = argv[++i];
			else if (i + 1 < argc && option == "--output")
				outputPath = argv[++i];
			else if (i + 1 < argc && option == "--preview")
				previewPath = argv[++i];
			else if (i + 1 < argc && option == "--capture")
				capturePaths.push_back(argv[++i]);
			else if (i + 1 < argc && option == "--palette")
			{
				const std::string value = argv[++i];
				if (value.size() != 1 || value[0] < '0' || value[0] > '7')
					throw std::runtime_error("--palette must be an integer from 0 through 7");
				selectedPalette = value[0] - '0';
			}
			else
			{
				PrintUsage();
				return 2;
			}
		}
		if (profilePath.empty() || groupName.empty() || outputPath.empty() || capturePaths.empty() || outputPath == profilePath)
		{
			PrintUsage();
			return 2;
		}
		RemasterProfile profile;
		std::vector<RemasterProfileDiagnostic> diagnostics;
		if (!S9xRemasterLoadProfile(profilePath, profile, diagnostics))
			throw std::runtime_error(diagnostics.empty() ? "invalid profile" : diagnostics.front().message);
		const auto group = profile.assetGroups.find(groupName);
		if (group == profile.assetGroups.end())
			throw std::runtime_error("profile group not found: " + groupName);
		std::vector<RemasterFrame> frames(capturePaths.size());
		for (size_t i = 0; i < frames.size(); i++)
		{
			if (!S9xReadRemasterFrame(capturePaths[i], frames[i]))
				throw std::runtime_error("unable to read capture: " + capturePaths[i]);
			if (!frames[i].profileRomSha256.empty() && frames[i].profileRomSha256 != profile.romSha256)
				throw std::runtime_error("capture ROM does not match profile: " + capturePaths[i]);
		}
		const auto tiles = CollectTiles(frames);
		std::set<RemasterTileContentId> paletteTiles;
		for (const RemasterFrame &frame : frames)
		{
			std::set<RemasterTileContentId> framePaletteTiles;
			for (const RemasterFrameTileInstance &instance : frame.tileInstances)
				if (instance.source == RemasterSourceType::Object && instance.palette == selectedPalette)
					framePaletteTiles.insert(instance.tileId);
			// The representative scene has instances for its first frame only.
			// Capture-local tracks connect those objects to later decoded variants.
			for (const RemasterFrameAssetGroup &track : frame.assetGroups)
				if (track.name.compare(0, 18, "capture_animation_") == 0 &&
					std::any_of(track.tileIds.begin(), track.tileIds.end(),
						[&] (const RemasterTileContentId &id) { return framePaletteTiles.count(id) != 0; }))
					framePaletteTiles.insert(track.tileIds.begin(), track.tileIds.end());
			paletteTiles.insert(framePaletteTiles.begin(), framePaletteTiles.end());
		}
		PaletteColors previewColors;
		std::array<std::array<uint64_t, 3>, 16> colorSums = {};
		std::array<uint64_t, 16> colorCounts = {};
		for (const RemasterFrame &frame : frames)
			for (const RemasterFrameArtworkColors &artwork : frame.artworkColors)
			{
				if (!paletteTiles.count(artwork.tileId))
					continue;
				const auto tile = tiles.find(artwork.tileId);
				if (tile == tiles.end())
					continue;
				for (int p = 0; p < 64; p++)
				{
					if (!(artwork.visiblePixels & (uint64_t(1) << p)))
						continue;
					const uint8_t index = tile->second.pixels[p];
					if (!index || index >= 16)
						continue;
					const uint16_t rgb555 = artwork.rgb555[p];
					colorSums[index][0] += ((rgb555 >> 10) & 31) * 255 / 31;
					colorSums[index][1] += ((rgb555 >> 5) & 31) * 255 / 31;
					colorSums[index][2] += (rgb555 & 31) * 255 / 31;
					colorCounts[index]++;
				}
			}
		for (int index = 1; index < 16; index++)
			if (colorCounts[index])
			{
				previewColors.known[index] = true;
				for (int c = 0; c < 3; c++)
					previewColors.rgb[index][c] = static_cast<uint8_t>(colorSums[index][c] / colorCounts[index]);
			}
		std::set<RemasterTileContentId> candidateIds(group->second.tileIds.begin(), group->second.tileIds.end());
		if (selectedPalette >= 0)
			for (const RemasterTileContentId &id : paletteTiles)
			{
				bool inAnotherGroup = false;
				for (const auto &other : profile.assetGroups)
					if (other.first != groupName && std::find(other.second.tileIds.begin(),
						other.second.tileIds.end(), id) != other.second.tileIds.end())
						inAnotherGroup = true;
				if (!inAnotherGroup)
					candidateIds.insert(id);
			}
		std::vector<const Tile *> references;
		for (const auto &entry : profile.assets)
		{
			const auto tile = tiles.find(entry.first);
			if (tile != tiles.end() && IsDetailed(entry.second, tile->second) &&
				(selectedPalette < 0 || paletteTiles.count(entry.first)))
				references.push_back(&tile->second);
		}
		if (references.empty())
			throw std::runtime_error("no captured tile has detailed authored heights and normals");
		std::vector<Suggestion> suggestions;
		int skipped = 0;
		for (const RemasterTileContentId &id : candidateIds)
		{
			const auto tile = tiles.find(id);
			if (tile == tiles.end())
			{
				std::cout << Id(id) << " missing artwork; capture this animation before generation\n";
				skipped++;
				continue;
			}
			Suggestion item;
			item.tile = tile->second;
			const auto existing = profile.assets.find(id);
			if (existing != profile.assets.end())
				item.metadata = existing->second;
			item.metadata.tileId = id;
			if ((item.metadata.hasHeight && item.metadata.hasNormals && !replaceFlat) ||
				(item.metadata.hasHeight && item.metadata.hasNormals && !IsFlat(item.metadata, item.tile)))
				continue;
			int opaque = 0;
			for (uint8_t p : item.tile.pixels)
				opaque += p != 0;
			if (opaque < 4 || (item.metadata.hasHeight &&
				*std::max_element(item.metadata.height.begin(), item.metadata.height.end()) == 0))
				continue; // empty/shadow tile
			item.match = FindMatch(item.tile, references);
			if ((!item.match.source || item.match.score < 0.33f) && IsNarrow(item.tile) && item.metadata.hasHeight)
			{
				SuggestNarrow(item, replaceFlat);
				if (item.wroteNormals)
					std::cout << Id(id) << " narrow surface: camera-facing blade normal\n";
			}
			else if (!item.match.source || item.match.score < 0.33f)
			{
				std::cout << Id(id) << " low confidence; left unchanged\n";
				skipped++;
				continue;
			}
			else
			{
				const auto ref = profile.assets.find(item.match.source->id);
				Suggest(item, ref->second, replaceFlat);
			}
			if (!item.wroteHeight && !item.wroteNormals)
				continue;
			if (item.match.source && item.match.score >= 0.33f)
				std::cout << Id(id) << " <= " << Id(item.match.source->id)
					<< " score=" << item.match.score << " exact=" << item.match.exact
					<< " height=" << item.wroteHeight << " normals=" << item.wroteNormals << "\n";
			profile.assets[id] = item.metadata;
			if (std::find(profile.assetGroups[groupName].tileIds.begin(),
				profile.assetGroups[groupName].tileIds.end(), id) == profile.assetGroups[groupName].tileIds.end())
				profile.assetGroups[groupName].tileIds.push_back(id);
			suggestions.push_back(item);
		}
		if (suggestions.empty())
			throw std::runtime_error("no eligible, confidently matched tiles; profile unchanged");
		std::sort(profile.assetGroups[groupName].tileIds.begin(), profile.assetGroups[groupName].tileIds.end());
		if (!S9xRemasterWriteProfile(profile, outputPath, diagnostics))
			throw std::runtime_error(diagnostics.empty() ? "unable to write profile" : diagnostics.front().message);
		if (!previewPath.empty())
			Preview(previewPath, suggestions, previewColors);
		std::cout << "Wrote " << suggestions.size() << " tile suggestions; " << skipped
			<< " lacked artwork or a confident match. Candidate: " << outputPath << "\n";
		return 0;
	}
	catch (const std::exception &error)
	{
		std::cerr << "remaster-geometry: " << error.what() << "\n";
		return 1;
	}
}
