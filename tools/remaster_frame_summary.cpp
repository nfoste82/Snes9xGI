#include "../remaster/frame.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

static const char *ownerPath (const RemasterFrame &frame, const RemasterFramePixel &pixel)
{
	if (pixel.owner == 0xffffffffu) return "unsupported";
	if (pixel.owner == 0xfffffffeu) return "forced_blank";
	const uint8_t kind = static_cast<uint8_t>(pixel.owner >> 24);
	if (kind == static_cast<uint8_t>(RemasterSourceType::Backdrop))
		return pixel.instanceId ? "backdrop_with_instance" : "backdrop";
	if (kind != static_cast<uint8_t>(RemasterSourceType::Background) &&
		kind != static_cast<uint8_t>(RemasterSourceType::Object))
		return pixel.owner == 0 ? "unowned" : "owner_sentinel";
	if (!pixel.instanceId) return "supported_owner_missing_instance";
	if (pixel.instanceId > frame.tileInstances.size()) return "instance_out_of_range";
	const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
	if (static_cast<uint8_t>(instance.source) != kind || instance.sourceIndex != ((pixel.owner >> 16) & 0xff) ||
		instance.tileNumber != (pixel.owner & 0x3ff))
		return "owner_instance_mismatch";
	return "linked_tile";
}

static const char *ownerKind (uint32_t owner)
{
	if (owner == 0xffffffffu) return "unsupported";
	if (owner == 0xfffffffeu) return "forced_blank";
	switch (owner >> 24)
	{
	case 1: return "backdrop";
	case 2: return "background";
	case 3: return "object";
	default: return owner ? "sentinel" : "unowned";
	}
}

static void printComposedSummary (const RemasterFrame &frame)
{
	std::map<std::string, uint64_t> counts;
	for (size_t i = 0; i < frame.mainPixels.size(); i++)
	{
		const RemasterFramePixel &main = frame.mainPixels[i];
		const RemasterFramePixel &sub = frame.subPixels[i];
		const uint8_t mainKind = static_cast<uint8_t>(main.owner >> 24);
		if (mainKind == static_cast<uint8_t>(RemasterSourceType::Background) ||
			mainKind == static_cast<uint8_t>(RemasterSourceType::Object))
			counts[main.instanceId ? "main_linked_tile" : "main_supported_owner_missing_instance"]++;
		else if (mainKind == static_cast<uint8_t>(RemasterSourceType::Backdrop) && sub.instanceId)
			counts["subscreen_tile_through_main_backdrop"]++;
		else if (mainKind == static_cast<uint8_t>(RemasterSourceType::Backdrop))
			counts["backdrop_only"]++;
		else if (main.owner == 0xffffffffu)
			counts["unsupported"]++;
		else if (main.owner == 0xfffffffeu)
			counts["forced_blank"]++;
		else
			counts["other_sentinel_or_unowned"]++;
	}
	for (const auto &entry : counts)
		std::cout << "composed path=" << entry.first << " pixels=" << entry.second << "\n";
}

int main (int argc, char **argv)
{
	if (argc != 2 && argc != 4 && argc != 5)
	{
		std::cerr << "Usage: remaster-frame-summary FRAME.s9xrmf [--images PREFIX | --pixel X Y]\n";
		return 2;
	}
	RemasterFrame frame;
	if (!S9xReadRemasterFrame(argv[1], frame))
	{
		std::cerr << "Could not read remaster frame\n";
		return 1;
	}
	struct Summary
	{
		uint64_t pixels = 0;
		int minX = 10000, minY = 10000, maxX = -1, maxY = -1;
		int minEffectiveHeight = 255, maxEffectiveHeight = 0;
	};
	using Key = std::tuple<uint8_t, RemasterSourceType, uint8_t, std::string, std::string, uint64_t>;
	auto sourceName = [] (RemasterSourceType source) {
		switch (source)
		{
		case RemasterSourceType::Background: return "background";
		case RemasterSourceType::Object: return "object";
		default: return "backdrop";
		}
	};
	std::map<Key, Summary> summaries;
	std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> metadata;
	for (const RemasterFrameAssetMetadata &item : frame.assetMetadata)
		metadata[item.tileId] = &item;
	for (uint32_t y = 0; y < frame.height; y++)
		for (uint32_t x = 0; x < frame.width; x++)
		{
			const RemasterFramePixel &pixel = frame.mainPixels[static_cast<size_t>(y) * frame.width + x];
			if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size()) continue;
			const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
			Summary &summary = summaries[Key(instance.heightOffset, instance.source,
				instance.sourceIndex, instance.material, instance.assetGroup, instance.tileId.hash)];
			summary.pixels++;
			summary.minX = std::min(summary.minX, static_cast<int>(x));
			summary.minY = std::min(summary.minY, static_cast<int>(y));
			summary.maxX = std::max(summary.maxX, static_cast<int>(x));
			summary.maxY = std::max(summary.maxY, static_cast<int>(y));
			int effectiveHeight = instance.heightOffset;
			const auto found = metadata.find(instance.tileId);
			if (pixel.tilePixel < 64 && instance.hasPlacementHeight)
				effectiveHeight = std::min(255, effectiveHeight + instance.placementHeight[pixel.tilePixel]);
			else if (found != metadata.end() && found->second->hasHeight && pixel.tilePixel < 64)
				effectiveHeight = std::min(255, effectiveHeight + found->second->height[pixel.tilePixel]);
			summary.minEffectiveHeight = std::min(summary.minEffectiveHeight, effectiveHeight);
			summary.maxEffectiveHeight = std::max(summary.maxEffectiveHeight, effectiveHeight);
		}
	std::cout << "schema=" << frame.schemaVersion << " size=" << frame.width << "x" << frame.height
		<< " instances=" << frame.tileInstances.size() << " profile=" << frame.profileRomSha256 << "\n";
	using OwnerKey = std::tuple<std::string, std::string, unsigned, bool>;
	std::map<OwnerKey, Summary> ownerSummaries;
	for (uint32_t y = 0; y < frame.height; y++) for (uint32_t x = 0; x < frame.width; x++)
	{
		const size_t offset = static_cast<size_t>(y) * frame.width + x;
		for (const RemasterFramePixel *pixel : { &frame.mainPixels[offset], &frame.subPixels[offset] })
		{
			const bool main = pixel == &frame.mainPixels[offset];
			Summary &summary = ownerSummaries[OwnerKey(main ? "main" : "sub", ownerPath(frame, *pixel),
				(pixel->owner >> 16) & 0xff, pixel->instanceId != 0)];
			summary.pixels++;
			summary.minX = std::min(summary.minX, static_cast<int>(x));
			summary.minY = std::min(summary.minY, static_cast<int>(y));
			summary.maxX = std::max(summary.maxX, static_cast<int>(x));
			summary.maxY = std::max(summary.maxY, static_cast<int>(y));
		}
	}
	for (const auto &entry : ownerSummaries)
	{
		const Summary &summary = entry.second;
		std::cout << "ownership screen=" << std::get<0>(entry.first) << " path=" << std::get<1>(entry.first)
			<< " layer=" << std::get<2>(entry.first) << " instance=" << (std::get<3>(entry.first) ? "nonzero" : "zero")
			<< " pixels=" << summary.pixels << " bounds=" << summary.minX << "," << summary.minY << ".."
			<< summary.maxX << "," << summary.maxY << "\n";
	}
	printComposedSummary(frame);
	// Region diagnostics use owner path/layer/link-state as the connectivity label.
	for (int screen = 0; screen < 2; screen++)
	{
		const std::vector<RemasterFramePixel> &pixels = screen ? frame.subPixels : frame.mainPixels;
		std::vector<uint8_t> visited(pixels.size());
		for (uint32_t start = 0; start < pixels.size(); start++)
		{
			if (visited[start]) continue;
			const RemasterFramePixel &seed = pixels[start];
			const std::string path = ownerPath(frame, seed);
			const unsigned layer = (seed.owner >> 16) & 0xff;
			const bool linked = seed.instanceId != 0;
			std::queue<uint32_t> queue;
			queue.push(start); visited[start] = 1;
			Summary region;
			std::map<uint16_t, uint64_t> colors;
			while (!queue.empty())
			{
				const uint32_t offset = queue.front(); queue.pop();
				const uint32_t x = offset % frame.width, y = offset / frame.width;
				region.pixels++; region.minX = std::min(region.minX, static_cast<int>(x));
				region.minY = std::min(region.minY, static_cast<int>(y));
				region.maxX = std::max(region.maxX, static_cast<int>(x));
				region.maxY = std::max(region.maxY, static_cast<int>(y));
				colors[frame.originalRgb555[offset]]++;
				for (const int delta : { -1, 1, -static_cast<int>(frame.width), static_cast<int>(frame.width) })
				{
					const int next = static_cast<int>(offset) + delta;
					if (next < 0 || next >= static_cast<int>(pixels.size()) || visited[next] ||
						(delta == -1 && x == 0) || (delta == 1 && x + 1 == frame.width)) continue;
					const RemasterFramePixel &candidate = pixels[next];
					if (path != ownerPath(frame, candidate) || layer != ((candidate.owner >> 16) & 0xff) ||
						linked != (candidate.instanceId != 0)) continue;
					visited[next] = 1; queue.push(static_cast<uint32_t>(next));
				}
			}
			if (region.pixels >= 16 || path == "supported_owner_missing_instance")
			{
				auto dominant = std::max_element(colors.begin(), colors.end(), [] (const auto &a, const auto &b) {
					return a.second < b.second;
				});
				std::cout << "region screen=" << (screen ? "sub" : "main") << " path=" << path
					<< " layer=" << layer << " pixels=" << region.pixels << " bounds=" << region.minX << ","
					<< region.minY << ".." << region.maxX << "," << region.maxY << " colors=" << colors.size()
					<< " dominant_rgb555=0x" << std::hex << dominant->first << std::dec << ":" << dominant->second << "\n";
			}
		}
	}
	for (const auto &entry : summaries)
	{
		const Summary &summary = entry.second;
		std::cout << "height=" << unsigned(std::get<0>(entry.first))
			<< " source=" << sourceName(std::get<1>(entry.first))
			<< " source_index=" << unsigned(std::get<2>(entry.first))
			<< " material=" << (std::get<3>(entry.first).empty() ? "-" : std::get<3>(entry.first))
			<< " group=" << (std::get<4>(entry.first).empty() ? "-" : std::get<4>(entry.first))
			<< " tile=0x" << std::hex << std::get<5>(entry.first) << std::dec
			<< " effective=" << summary.minEffectiveHeight << ".." << summary.maxEffectiveHeight
			<< " pixels=" << summary.pixels << " bounds=" << summary.minX << "," << summary.minY
			<< ".." << summary.maxX << "," << summary.maxY << "\n";
	}
	if (argc == 5)
	{
		if (std::string(argv[2]) != "--pixel") return 2;
		const unsigned x = static_cast<unsigned>(std::stoul(argv[3]));
		const unsigned y = static_cast<unsigned>(std::stoul(argv[4]));
		if (x >= frame.width || y >= frame.height) return 2;
		const size_t offset = static_cast<size_t>(y) * frame.width + x;
		for (const auto &entry : { std::make_pair("main", &frame.mainPixels[offset]),
			std::make_pair("sub", &frame.subPixels[offset]) })
			std::cout << "pixel screen=" << entry.first << " x=" << x << " y=" << y << " rgb555=0x"
				<< std::hex << frame.originalRgb555[offset] << " owner=0x" << entry.second->owner << std::dec
				<< " kind=" << ownerKind(entry.second->owner) << " layer=" << ((entry.second->owner >> 16) & 0xff)
				<< " path=" << ownerPath(frame, *entry.second) << " instance=" << entry.second->instanceId
				<< " tile_pixel=" << unsigned(entry.second->tilePixel) << "\n";
	}
	if (argc == 4)
	{
		if (std::string(argv[2]) != "--images")
			return 2;
		std::ofstream scene(std::string(argv[3]) + "-scene.ppm", std::ios::binary);
		std::ofstream heights(std::string(argv[3]) + "-heights.ppm", std::ios::binary);
		scene << "P6\n" << frame.width << " " << frame.height << "\n255\n";
		heights << "P6\n" << frame.width << " " << frame.height << "\n255\n";
		for (size_t i = 0; i < frame.mainPixels.size(); i++)
		{
			const uint16_t color = i < frame.originalRgb555.size() ? frame.originalRgb555[i] : 0;
			const char rgb[3] = {
				static_cast<char>(((color >> 10) & 31) * 255 / 31),
				static_cast<char>(((color >> 5) & 31) * 255 / 31),
				static_cast<char>((color & 31) * 255 / 31)
			};
			scene.write(rgb, sizeof(rgb));
			uint8_t height = 0;
			const RemasterFramePixel &pixel = frame.mainPixels[i];
			if (pixel.instanceId && pixel.instanceId <= frame.tileInstances.size())
			{
				const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
				height = instance.heightOffset;
				const auto found = metadata.find(instance.tileId);
				if (pixel.tilePixel < 64 && instance.hasPlacementHeight)
					height = static_cast<uint8_t>(std::min(255,
						static_cast<int>(height) + instance.placementHeight[pixel.tilePixel]));
				else if (found != metadata.end() && found->second->hasHeight && pixel.tilePixel < 64)
					height = static_cast<uint8_t>(std::min(255,
						static_cast<int>(height) + found->second->height[pixel.tilePixel]));
			}
			const char heightRgb[3] = { static_cast<char>(height), static_cast<char>(height), static_cast<char>(height) };
			heights.write(heightRgb, sizeof(heightRgb));
		}
		if (!scene || !heights)
			return 1;
	}
	return 0;
}
