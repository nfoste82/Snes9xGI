#ifndef REMASTER_OFFSCREEN_EMITTERS_H
#define REMASTER_OFFSCREEN_EMITTERS_H

#include "frame.h"
#include <map>
#include <cmath>

struct RemasterRoomEmitterDiagnostics
{
	uint32_t placements = 0, lights = 0, visiblePixels = 0, unknownBases = 0;
};

// Pure completed-frame adapter. WRAM has two row-major 64x64 maps (BG2 then
// BG1); PPU screen blocks are streamed and cannot establish complete coverage.
// Scratch is invocation-local: no room/animation/profile state survives a load,
// defrost, transition or editor update. hashTile is the canonical v1 core hash.
template<class HashTile>
inline RemasterRoomEmitterDiagnostics S9xRemasterAppendOffscreenRoomLights(
	RemasterFrame &frame, const RemasterDungeonFloorContext &c, const RemasterProfile &profile,
	const RemasterDungeonHeightMap *bg2, const RemasterDungeonHeightMap *bg1, HashTile hashTile)
{
	RemasterRoomEmitterDiagnostics d;
	if (!c.verifiedAlttpRom || !c.indoors || c.module != 7 || c.submodule ||
		c.constructionPhase || c.collisionMode || c.roomIndex >= 320 || !c.constructedMaps || !c.graphics ||
		frame.width != 256 || !frame.height || frame.height > 256) return d;
	// Only physical flame/volume sources can be represented by a sphere. A
	// directional painted surface needs the planar source transport contract.
	if (std::none_of(profile.assets.begin(), profile.assets.end(), [](const auto &a) {
		return a.second.hasEmission && (a.second.emissionOmnidirectional || a.second.emissionDepth > 0);
	})) return d;
	std::map<uint16_t, RemasterTileContentId> identities;
	struct Source {
		int layer, cell;
		const RemasterAssetMetadata *asset;
		float power = 0, sx = 0, sy = 0, sz = 0;
		unsigned count = 0;
		std::array<float,3> color = {};
	};
	std::vector<Source> sources;
	const int scrollX = c.backgroundScrollX & 511, scrollY = c.backgroundScrollY & 511;
	for (int layer : {1, 0}) for (int cell = 0; cell < 4096; cell++)
	{
		const size_t at = (layer == 1 ? 0 : 8192) + cell * 2;
		const uint16_t word = c.constructedMaps[at] | (c.constructedMaps[at+1] << 8);
		const uint16_t base = layer == 1 || c.bg1GraphicsBase == 0xffff ? c.graphicsBase : c.bg1GraphicsBase;
		// Mirrored BG1/BG2 room-loader writes describe one physical fixture.
		if (!layer && base == c.graphicsBase && word ==
			(c.constructedMaps[cell*2] | (c.constructedMaps[cell*2+1] << 8))) continue;
		const uint16_t address = static_cast<uint16_t>(base + (word & 1023) * 32);
		auto found = identities.find(address);
		if (found == identities.end())
		{
			uint8_t pixels[64];
			for (int y=0; y<8; y++) for (int x=0; x<8; x++)
			{
				uint8_t index=0;
				for (int p=0; p<4; p++) index |= ((c.graphics[static_cast<uint16_t>(address + p/2*16 + y*2 + p%2)] >> (7-x)) & 1) << p;
				pixels[y*8+x]=index;
			}
			found=identities.emplace(address,RemasterTileContentId{hashTile(4,pixels),1,4}).first;
		}
		auto asset=profile.assets.find(found->second);
		if (asset==profile.assets.end() || !asset->second.hasEmission ||
			(!asset->second.emissionOmnidirectional && asset->second.emissionDepth <= 0)) continue;
		const auto &a=asset->second;
		d.placements++;
		const auto *map=layer==1 ? bg2 : bg1;
		if (!map) { d.unknownBases++; continue; }
		int32_t datum=0;
		if (map)
		{
			if (map->roomIndex!=c.roomIndex || map->offsets[cell]==255 ||
				(map->producerHashes[cell] && (map->producerHashes[cell]!=found->second.hash ||
				map->producerFlips[cell]!=(uint8_t(bool(word&0x4000)) | (uint8_t(bool(word&0x8000))<<1)))))
			{ d.unknownBases++; continue; }
			datum=map->offsets[cell];
		}
		float power=0, sx=0, sy=0, sz=0; unsigned count=0;
		std::array<float,3> color={};
		for (int y=0; y<8; y++) for (int x=0; x<8; x++)
		{
			const int pixel=(word&0x8000 ? 7-y : y)*8+(word&0x4000 ? 7-x : x);
			const auto *e=a.emissionRgba.data()+pixel*4;
			if (!e[3] || !(e[0] || e[1] || e[2])) continue;
			const int px=cell%64*8+x-scrollX, py=cell/64*8+y-scrollY;
			// Visible/occluded in-viewport sources remain under the authoritative
			// pixel path. Never relight a fixture hidden behind an on-screen wall.
			if (px>=0 && py>=0 && px<int(frame.width) && py<int(frame.height))
			{ d.visiblePixels++; continue; }
			const float w=e[3];
			power+=w; sx+=(px+0.5f)*w; sy+=(py+0.5f)*w;
			sz+=((datum+(a.hasHeight ? a.height[pixel] : 0))*frame.lightingCoordinateScale/255.0f + a.emissionDepth*0.5f)*w;
			for (int k=0;k<3;k++) color[k]+=std::pow(e[k]/255.0f,2.2f)*w;
			count++;
		}
		if (!count) continue;
		sources.push_back({layer, cell, &a, power, sx, sy, sz, count, color});
	}
	// The visible path honors these links in representative mode. Use the same
	// opt-in contract for missing pixels, before losing placement/hash identity.
	// Never combine across BG planes, room edges or disconnected occurrences.
	std::vector<size_t> parent(sources.size());
	std::map<std::pair<int,int>,size_t> placements;
	for (size_t i = 0; i < sources.size(); i++) {
		parent[i] = i;
		placements[{sources[i].layer, sources[i].cell}] = i;
	}
	auto root = [&](size_t i) { while (parent[i] != i) i = parent[i]; return i; };
	if (frame.representativeEmitters) for (size_t i = 0; i < sources.size(); i++) {
		const auto &s = sources[i];
		for (int delta : {1, 64}) {
			if ((delta == 1 && s.cell % 64 == 63) || s.cell + delta >= 4096) continue;
			auto next = placements.find({s.layer, s.cell + delta});
			if (next == placements.end()) continue;
			const auto &a = *s.asset, &b = *sources[next->second].asset;
			if (a.emissionOmnidirectional != b.emissionOmnidirectional ||
				std::find(a.combineEmitters.begin(), a.combineEmitters.end(), b.tileId) == a.combineEmitters.end() ||
				std::find(b.combineEmitters.begin(), b.combineEmitters.end(), a.tileId) == b.combineEmitters.end()) continue;
			size_t ra = root(i), rb = root(next->second);
			parent[std::max(ra,rb)] = std::min(ra,rb);
		}
	}
	for (size_t i = 0; i < sources.size(); i++) {
		const size_t r = root(i);
		if (r == i) continue;
		auto &a = sources[r]; const auto &b = sources[i];
		a.power += b.power; a.sx += b.sx; a.sy += b.sy; a.sz += b.sz; a.count += b.count;
		for (unsigned k = 0; k < 3; k++) a.color[k] += b.color[k];
	}
	for (size_t i = 0; i < sources.size(); i++) {
		if (root(i) != i) continue;
		const auto &s = sources[i];
		RemasterFrameLight light;
		light.x=s.sx/s.power; light.y=s.sy/s.power; light.z=s.sz/s.power;
		// Equal emitting area: sphere area = 4*pi*r^2. This is a physical
		// representative of missing flame pixels, not a 96px influence radius.
		light.radius=std::sqrt(s.count/(4.0f*3.14159265358979323846f));
		light.intensity=s.power/s.count;
		light.red=std::pow(s.color[0]/s.power,1.0f/2.2f);
		light.green=std::pow(s.color[1]/s.power,1.0f/2.2f);
		light.blue=std::pow(s.color[2]/s.power,1.0f/2.2f);
		frame.lights.push_back(light); d.lights++;
	}
	return d;
}

#endif
