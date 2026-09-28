#include "../remaster/frame.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <tuple>

int main (int argc, char **argv)
{
	if (argc != 2 && argc != 4)
	{
		std::cerr << "Usage: remaster-frame-summary FRAME.s9xrmf [--images PREFIX]\n";
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
			if (found != metadata.end() && found->second->hasHeight && pixel.tilePixel < 64)
				effectiveHeight = std::min(255, effectiveHeight + found->second->height[pixel.tilePixel]);
			summary.minEffectiveHeight = std::min(summary.minEffectiveHeight, effectiveHeight);
			summary.maxEffectiveHeight = std::max(summary.maxEffectiveHeight, effectiveHeight);
		}
	std::cout << "schema=" << frame.schemaVersion << " size=" << frame.width << "x" << frame.height
		<< " instances=" << frame.tileInstances.size() << " profile=" << frame.profileRomSha256 << "\n";
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
				if (found != metadata.end() && found->second->hasHeight && pixel.tilePixel < 64)
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
