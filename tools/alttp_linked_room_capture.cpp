/* Convert the reference PPU's linked room export into a profile-aware
 * RemasterFrame. This is an inspection capture, not an emulated Snes9x frame:
 * BG1/BG2 owner links are reconstructed from the winning PPU priority word;
 * sprites and the HUD retain their painted color but have no owner link. */
#include "../remaster/remaster.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <tuple>

namespace {
constexpr int width = 256, height = 224;

struct Reader {
  std::vector<uint8_t> bytes;
  size_t at = 0;
  explicit Reader(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open raw room export");
    bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
  }
  uint16_t u16() {
    if (at + 2 > bytes.size()) throw std::runtime_error("truncated raw room export");
    uint16_t value = bytes[at] | static_cast<uint16_t>(bytes[at + 1]) << 8;
    at += 2;
    return value;
  }
};

struct Layer {
  uint16_t hscroll, vscroll, mapAddress, tileAddress, flags;
};

struct RawPixel {
  uint16_t color, main, sub;
};

struct Raw {
  uint16_t room, viewportX, viewportY;
  Layer layers[height][3] = {};
  RawPixel pixels[height][width] = {};
  uint16_t vram[0x8000] = {};
  uint16_t cgram[0x100] = {};
};

struct HeightMap {
  uint16_t room = 0xffff;
  std::vector<uint8_t> offsets;
};

HeightMap readHeightMap(const std::string &path) {
  Reader reader(path);
  if (reader.bytes.size() != 8 + 2 + 64 * 64 ||
      std::memcmp(reader.bytes.data(), "ALTPHM1\0", 8))
    throw std::runtime_error("invalid room height proposal map");
  reader.at = 8;
  HeightMap map;
  map.room = reader.u16();
  map.offsets.assign(reader.bytes.begin() + 10, reader.bytes.end());
  return map;
}

Raw readRaw(const std::string &path) {
  Reader reader(path);
  const char magic[] = "ALTPPD1\0";
  if (reader.bytes.size() < 8 || std::memcmp(reader.bytes.data(), magic, 8))
    throw std::runtime_error("not a linked room export");
  reader.at = 8;
  Raw raw;
  raw.room = reader.u16(); raw.viewportX = reader.u16(); raw.viewportY = reader.u16();
  if (raw.room >= 320 || raw.viewportX > 256 || raw.viewportY > 288)
    throw std::runtime_error("invalid raw room or viewport");
  for (int y = 0; y < height; y++) {
    for (Layer &layer : raw.layers[y]) {
      layer.hscroll = reader.u16(); layer.vscroll = reader.u16();
      layer.mapAddress = reader.u16(); layer.tileAddress = reader.u16();
      layer.flags = reader.u16();
    }
    for (RawPixel &pixel : raw.pixels[y]) {
      pixel.color = reader.u16(); pixel.main = reader.u16(); pixel.sub = reader.u16();
    }
  }
  for (uint16_t &word : raw.vram) word = reader.u16();
  for (uint16_t &color : raw.cgram) color = reader.u16();
  if (reader.at != reader.bytes.size()) throw std::runtime_error("trailing raw room data");
  return raw;
}

uint16_t mapWord(const Raw &raw, const Layer &layer, unsigned x, unsigned y) {
  const unsigned sx = x + layer.hscroll, sy = y + 1 + layer.vscroll;
  const unsigned row = ((sy >> 3) & 31) << 5;
  const unsigned ybank = ((sy & 0x100) && (layer.flags & 2)) ?
    ((layer.flags & 1) ? 0x800 : 0x400) : 0;
  const unsigned xbank = ((sx & 0x100) && (layer.flags & 1)) ? 0x400 : 0;
  const unsigned offset = (layer.mapAddress + row + ybank + xbank + ((sx >> 3) & 31)) & 0x7fff;
  return raw.vram[offset];
}

void decodeTile(const Raw &raw, uint16_t address, uint8_t *indices) {
  for (int y = 0; y < 8; y++) {
    uint16_t lo = raw.vram[(address + y) & 0x7fff];
    uint16_t hi = raw.vram[(address + y + 8) & 0x7fff];
    for (int x = 0; x < 8; x++) {
      const int bit = 7 - x;
      indices[y * 8 + x] = ((lo >> bit) & 1) | ((lo >> (bit + 8)) & 1) << 1 |
        ((hi >> bit) & 1) << 2 | ((hi >> (bit + 8)) & 1) << 3;
    }
  }
}

struct CellKey {
  int layer, x, y;
  uint16_t word, address;
  bool operator<(const CellKey &other) const {
    return std::tie(layer, x, y, word, address) <
      std::tie(other.layer, other.x, other.y, other.word, other.address);
  }
};

RemasterFrame makeFrame(const Raw &raw, const RemasterProfile &profile,
                        const HeightMap *heightMap = nullptr) {
  if (heightMap && (heightMap->room != raw.room || heightMap->offsets.size() != 64 * 64))
    throw std::runtime_error("height proposal map belongs to another room");
  RemasterFrame frame;
  frame.width = width; frame.height = height;
  frame.originalRgb555.reserve(width * height);
  frame.mainPixels.reserve(width * height);
  frame.subPixels.reserve(width * height);
  std::map<RemasterTileContentId, RemasterFrameAsset> assets;
  std::map<CellKey, uint32_t> instances;
  std::vector<uint16_t> instanceCells;
  uint32_t linked = 0, hud = 0, sprites = 0, backdrop = 0;
  auto link = [&](const RawPixel &rawPixel, const Layer *layers, int x, int y, bool sub) {
    RemasterFramePixel pixel;
    const uint16_t z = sub ? rawPixel.sub : rawPixel.main;
    const int priority = z >> 12;
    int layerIndex = -1;
    if (priority == 8 || priority == 12) layerIndex = 0;
    if (priority == 7 || priority == 11) layerIndex = 1;
    if (layerIndex < 0) {
      if (!sub) {
        if (priority == 1 || priority == 3 || priority == 15) hud++;
        else if (priority == 2 || priority == 6 || priority == 10 || priority == 14) sprites++;
        else backdrop++;
      }
      return pixel;
    }
    const Layer &layer = layers[layerIndex];
    if (!(layer.flags & (sub ? 8 : 4))) return pixel;
    const uint16_t word = mapWord(raw, layer, x, y);
    const unsigned sx = x + layer.hscroll, sy = y + 1 + layer.vscroll;
    const uint16_t address = static_cast<uint16_t>((layer.tileAddress + (word & 0x3ff) * 16) & 0x7fff);
    const int tx = static_cast<int>(sx & 7), ty = static_cast<int>(sy & 7);
    const uint8_t sourceX = (word & 0x4000) ? 7 - tx : tx;
    const uint8_t sourceY = (word & 0x8000) ? 7 - ty : ty;
    const uint8_t sourcePixel = sourceY * 8 + sourceX;
    uint8_t indices[64];
    decodeTile(raw, address, indices);
    if (!indices[sourcePixel] || ((z & 15) != indices[sourcePixel])) return pixel;
    const RemasterTileContentId id = { S9xRemasterHashTile(4, indices), 1, 4 };
    const CellKey key = {layerIndex, static_cast<int>(sx >> 3), static_cast<int>(sy >> 3), word, address};
    auto found = instances.find(key);
    if (found == instances.end()) {
      RemasterFrameAsset asset; asset.tileId = id;
      std::copy(indices, indices + 64, asset.indices);
      assets.emplace(id, asset);
      RemasterFrameTileInstance instance;
      instance.tileId = id;
      instance.source = RemasterSourceType::Background;
      instance.sourceIndex = static_cast<uint8_t>(layerIndex);
      instance.tileNumber = word & 0x3ff;
      instance.vramAddress = address;
      instance.palette = (word >> 10) & 7;
      instance.ppuPriority = (word & 0x2000) ? 1 : 0;
      instance.hFlip = (word & 0x4000) != 0;
      instance.vFlip = (word & 0x8000) != 0;
      frame.tileInstances.push_back(instance);
      instanceCells.push_back(static_cast<uint16_t>(((key.y & 63) << 6) | (key.x & 63)));
      found = instances.emplace(key, static_cast<uint32_t>(frame.tileInstances.size())).first;
    }
    pixel.owner = (2u << 24) | (static_cast<uint32_t>(layerIndex) << 16) | word;
    pixel.instanceId = found->second;
    pixel.tilePixel = sourcePixel;
    if (!sub) linked++;
    return pixel;
  };
  for (int y = 0; y < height; y++) for (int x = 0; x < width; x++) {
    const RawPixel &pixel = raw.pixels[y][x];
    frame.originalRgb555.push_back(pixel.color);
    frame.mainPixels.push_back(link(pixel, raw.layers[y], x, y, false));
    frame.subPixels.push_back(link(pixel, raw.layers[y], x, y, true));
  }
  for (const auto &entry : assets) frame.assets.push_back(entry.second);
  S9xRemasterApplyProfileToFrame(profile, frame);
  RemasterDungeonFloorContext floor;
  floor.verifiedAlttpRom = true;
  floor.indoors = true;
  floor.roomIndex = raw.room;
  floor.backgroundScrollX = raw.viewportX;
  floor.backgroundScrollY = raw.viewportY;
  if (heightMap) {
    for (size_t i = 0; i < frame.tileInstances.size(); i++) {
      RemasterFrameTileInstance &instance = frame.tileInstances[i];
      if (instance.source != RemasterSourceType::Background || instance.sourceIndex != 1)
        continue;
      const uint8_t offset = heightMap->offsets[instanceCells[i]];
      if (offset != 255)
        instance.heightOffset = static_cast<uint8_t>(std::min(255,
          static_cast<int>(instance.heightOffset) + offset));
    }
  } else {
    S9xRemasterApplyDungeonFloorHeight(frame, floor, profile.upperFloorHeight);
  }
  unsigned matched = 0, unmatched = 0, ambiguous = 0, raised = 0;
  for (const auto &instance : frame.tileInstances) {
    if (instance.matchStatus == RemasterProfileMatchStatus::Matched) matched++;
    else if (instance.matchStatus == RemasterProfileMatchStatus::Ambiguous) ambiguous++;
    else unmatched++;
    if (instance.heightOffset) raised++;
  }
  unsigned withHeight = 0, withNormals = 0, withOcclusion = 0, withEmission = 0;
  for (const auto &asset : frame.assetMetadata) {
    withHeight += asset.hasHeight;
    withNormals += asset.hasNormals;
    withOcclusion += asset.hasOcclusion;
    withEmission += asset.hasEmission;
  }
  std::cerr << "room=0x" << std::hex << raw.room << std::dec
    << " viewport=" << raw.viewportX << "," << raw.viewportY
    << " linked_bg_pixels=" << linked << " hud_pixels=" << hud
    << " sprite_pixels=" << sprites << " backdrop_pixels=" << backdrop
    << " assets=" << frame.assets.size() << " instances=" << frame.tileInstances.size()
    << " matched=" << matched << " unmatched=" << unmatched << " ambiguous=" << ambiguous
    << " raised=" << raised << " height_assets=" << withHeight
    << " normal_assets=" << withNormals << " occlusion_assets=" << withOcclusion
    << " emission_assets=" << withEmission << "\n";
  return frame;
}

void writePreview(const RemasterFrame &frame, const std::string &path) {
  std::ofstream output(path, std::ios::binary);
  if (!output) throw std::runtime_error("cannot open preview PPM");
  output << "P6\n" << frame.width << " " << frame.height << "\n255\n";
  for (uint16_t color : frame.originalRgb555) {
    uint8_t rgb[3];
    for (int i = 0; i < 3; i++) {
      int value = (color >> (i * 5)) & 31;
      rgb[i] = static_cast<uint8_t>((value << 3) | (value >> 2));
    }
    output.write(reinterpret_cast<const char *>(rgb), sizeof(rgb));
  }
  if (!output) throw std::runtime_error("cannot write preview PPM");
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 4 && argc != 5 && argc != 7) {
    std::cerr << "Usage: alttp-linked-room-capture RAW PROFILE.toml OUTPUT.s9xrmf [PREVIEW.ppm [--height-map MAP.bin]]\n";
    return 2;
  }
  try {
    RemasterProfile profile;
    std::vector<RemasterProfileDiagnostic> diagnostics;
    if (!S9xRemasterLoadProfile(argv[2], profile, diagnostics)) {
      for (const auto &diagnostic : diagnostics)
        std::cerr << diagnostic.line << ": " << diagnostic.message << "\n";
      throw std::runtime_error("profile did not load");
    }
    HeightMap heightMap;
    if (argc == 7) {
      if (std::strcmp(argv[5], "--height-map"))
        throw std::runtime_error("expected --height-map after preview path");
      heightMap = readHeightMap(argv[6]);
    }
    RemasterFrame frame = makeFrame(readRaw(argv[1]), profile,
                                   argc == 7 ? &heightMap : nullptr);
    if (argc >= 5) writePreview(frame, argv[4]);
    if (!S9xWriteRemasterFrame(frame, argv[3]))
      throw std::runtime_error("could not write inspection capture");
    RemasterFrame check;
    if (!S9xReadRemasterFrame(argv[3], check) ||
        check.originalRgb555 != frame.originalRgb555 ||
        check.assets.size() != frame.assets.size() ||
        check.assetMetadata.size() != frame.assetMetadata.size() ||
        check.tileInstances.size() != frame.tileInstances.size() ||
        check.mainPixels.size() != frame.mainPixels.size())
      throw std::runtime_error("inspection capture failed readback");
    for (size_t i = 0; i < frame.mainPixels.size(); i++)
      if (frame.mainPixels[i].instanceId != check.mainPixels[i].instanceId ||
          frame.mainPixels[i].tilePixel != check.mainPixels[i].tilePixel)
        throw std::runtime_error("inspection capture lost pixel links");
    for (size_t i = 0; i < frame.tileInstances.size(); i++)
      if (frame.tileInstances[i].heightOffset != check.tileInstances[i].heightOffset)
        throw std::runtime_error("inspection capture lost placement heights");
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
  return 0;
}
