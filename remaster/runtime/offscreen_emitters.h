#ifndef REMASTER_OFFSCREEN_EMITTERS_H
#define REMASTER_OFFSCREEN_EMITTERS_H

#include "frame.h"
#include "held_lamp.h"
#include <map>
#include <cmath>

struct RemasterRoomEmitterDiagnostics
{
	uint32_t placements = 0, lights = 0, visiblePixels = 0, unknownBases = 0, hiddenPixels = 0;
};

// USA Module07_08/10 only move Link/camera/plane and animate the straight
// intra-room stair. Unlike room construction, they do not redraw the maps or
// replace collision arrays. Keep physical fixtures alive while traversing;
// each source still requires current room and exact producer identity below.
inline bool S9xRemasterRoomEmitterLifecycle(const RemasterDungeonFloorContext &c)
{
	if(c.module != 7)return false;
	// All eight USA subtile handlers retain this same constructed room. Never
	// retain historical identities: enumerate current maps/VRAM/palette below.
	if(c.submodule==1)return c.constructionPhase<8;
	if(c.submodule==5)return true; // final subtile ControlShutters, no loader
	// WRAM $B0 is subsubmodule_index, reused by each module handler. In
	// Module07_08/10, 0 initializes and 1 climbs; neither constructs the room.
	// Other stair phases are not present in the verified two-entry dispatch.
	if(c.submodule == 8 || c.submodule == 16) {
		return c.constructionPhase <= 1;
	}
	return c.submodule == 0 && c.constructionPhase == 0;
}

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
	if (!c.verifiedAlttpRom || !c.indoors || !S9xRemasterRoomEmitterLifecycle(c) ||
		c.collisionMode || c.roomIndex >= 320 || !c.constructedMaps || !c.graphics ||
		frame.width != 256 || !frame.height || frame.height > 256) return d;
	// All missing pixels retain the same planar/depth source contract.
	if (std::none_of(profile.assets.begin(), profile.assets.end(), [](const auto &a) {
		return a.second.hasEmission;
	})) return d;
	std::map<uint16_t, RemasterTileContentId> identities;
	// During same-room scrolling retain the conservative whole-room union.
	// Missing/contradictory settled evidence likewise preserves legacy output.
	const auto compartment=(c.submodule==0 && c.constructionPhase==0) ?
		S9xRemasterDungeonCompartment(c.layoutEvidence,c.backgroundScrollX,c.backgroundScrollY) : RemasterDungeonCompartment{};
	struct Candidate { int layer,cell; uint16_t word,address; RemasterTileContentId id; const RemasterAssetMetadata *asset; };
	std::vector<Candidate> candidates;
	std::array<std::array<uint8_t,4096>,2> emittingCells={}, admittedCells={};
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
		if (asset==profile.assets.end() || !asset->second.hasEmission) continue;
		const auto &a=asset->second;
		auto emissionAt = [&](int pixel) {
			std::array<uint8_t,4> e; std::copy_n(a.emissionRgba.data()+pixel*4,4,e.begin());
			if (!(a.emissionColorMask & (UINT64_C(1)<<pixel))) {
				unsigned index=0, y=pixel/8, x=pixel%8;
				for(unsigned p=0;p<4;p++) index |= ((c.graphics[uint16_t(address+p/2*16+y*2+p%2)]>>(7-x))&1)<<p;
				const uint16_t rgb=c.emitterPalette && index ? S9xRemasterPaletteRgb555(c.emitterPalette[((word>>10)&7)*16+index]) : 0;
				for(unsigned k=0;k<3;k++) e[k]=uint8_t(((((rgb>>(10-k*5))&31)*527)+23)>>6);
			}
			return e;
		};
		// Explicit all-zero emission arrays are common on non-emitting art.
		// They are not source placements or unknown source bases. In particular,
		// counting them hid the small set of genuinely unsupported room fixtures.
		bool emitting=false;
		for(int pixel=0;pixel<64;pixel++) {
			const auto e=emissionAt(pixel);
			if(e[3] && (e[0] || e[1] || e[2])) { emitting=true; break; }
		}
		if(!emitting)continue;
		candidates.push_back({layer,cell,word,address,found->second,&a});
		emittingCells[layer][cell]=1;
	}
	// This is only conservative admission, NOT representative grouping. Keep an
	// entire cardinally connected emitting footprint if any tile touches the
	// active compartment. No reciprocal/self links or authored arrays change.
	for(int layer=0;layer<2;layer++) {
		std::vector<int> queue;
		for(int cell=0;cell<4096;cell++)if(emittingCells[layer][cell] && compartment.containsTile(cell%64*8,cell/64*8)) {
			admittedCells[layer][cell]=1;queue.push_back(cell);
		}
		for(size_t k=0;k<queue.size();k++) {
			int cell=queue[k];
			for(int next:{cell%64?cell-1:-1,cell%64<63?cell+1:-1,cell>=64?cell-64:-1,cell<4032?cell+64:-1})
				if(next>=0 && emittingCells[layer][next] && !admittedCells[layer][next]){admittedCells[layer][next]=1;queue.push_back(next);}
		}
	}
	for(const auto &candidate:candidates) {
		const int layer=candidate.layer,cell=candidate.cell;
		if(!admittedCells[layer][cell])continue;
		const auto word=candidate.word,address=candidate.address;
		const auto &a=*candidate.asset;
		auto emissionAt = [&](int pixel) {
			std::array<uint8_t,4> e;std::copy_n(a.emissionRgba.data()+pixel*4,4,e.begin());
			if(!(a.emissionColorMask & (UINT64_C(1)<<pixel))) {
				unsigned index=0,y=pixel/8,x=pixel%8;
				for(unsigned p=0;p<4;p++)index|=((c.graphics[uint16_t(address+p/2*16+y*2+p%2)]>>(7-x))&1)<<p;
				const uint16_t rgb=c.emitterPalette && index?S9xRemasterPaletteRgb555(c.emitterPalette[((word>>10)&7)*16+index]):0;
				for(unsigned k=0;k<3;k++)e[k]=uint8_t(((((rgb>>(10-k*5))&31)*527)+23)>>6);
			}
			return e;
		};
		d.placements++;
		const auto *map=layer==1 ? bg2 : bg1;
		if (!map) { d.unknownBases++; continue; }
		int32_t datum=0;
		if (map)
		{
			// offsets is the solved visible artwork base, not Link's support.
			// planeOffsets may describe a floor UNDER an unresolved wall/overlay;
			// neither the active plane nor a nearby solved cell proves ownership.
			if (map->roomIndex!=c.roomIndex || map->offsets[cell]==255 ||
				(map->producerHashes[cell] && (map->producerHashes[cell]!=candidate.id.hash ||
				map->producerFlips[cell]!=(uint8_t(bool(word&0x4000)) | (uint8_t(bool(word&0x8000))<<1)))))
			{ d.unknownBases++; continue; }
			datum=map->offsets[cell];
		}
		const std::array<int16_t,64> *placementLocal=nullptr;
		for(const auto &preview:map->localHeightPreviews)if(preview.cell==cell){placementLocal=&preview.height;break;}
		for (int y=0; y<8; y++) for (int x=0; x<8; x++)
		{
			const int pixel=(word&0x8000 ? 7-y : y)*8+(word&0x4000 ? 7-x : x);
			const auto e=emissionAt(pixel);
			if (!e[3] || !(e[0] || e[1] || e[2])) continue;
			const int px=cell%64*8+x-scrollX, py=cell/64*8+y-scrollY;
			// World-covered pixels remain with the authoritative pixel path. HUD
			// ink suppresses that path's transport, so supply only those missing
			// pixels from the current room placement, just like offscreen pixels.
			if (px>=0 && py>=0 && px<int(frame.width) && py<int(frame.height))
			{
				if (!S9xRemasterHeldLampHudPixel(frame, size_t(py)*frame.width+px))
				{ d.visiblePixels++; continue; }
			}
			// Preserve the visible pixel source contract at the viewport boundary:
			// exact signed XY, placement-local Z, authored normal/depth and power.
			// The existing source-only deterministic depth transport consumes it.
			{
				RemasterFrameHiddenEmitter hidden;
				hidden.x=px+0.5f; hidden.y=py+0.5f;
				int local=placementLocal ? (*placementLocal)[pixel] : a.hasHeight ? a.height[pixel] : 0;
				// Same existing placement-local source consumed by visible tilePixel.
				hidden.z=(datum+local)*frame.lightingCoordinateScale/255.0f;
				hidden.depth=a.emissionDepth; hidden.omnidirectional=a.emissionOmnidirectional;
				std::copy(e.begin(),e.end(),hidden.emission.begin());
				if (a.hasNormals) {
					float length=0;
					for (int k=0;k<3;k++) {
						hidden.normal[k]=a.normalXyz[pixel*3+k]/127.5f-1;
						if ((k==0 && (word&0x4000)) || (k==1 && (word&0x8000))) hidden.normal[k]=-hidden.normal[k];
						length+=hidden.normal[k]*hidden.normal[k];
					}
					length=std::sqrt(length);
					if (length>0.0001f) for(auto &n:hidden.normal)n/=length;
					else hidden.normal={{0,0,1}};
				}
				frame.hiddenEmitters.push_back(hidden); d.hiddenPixels++;
			}
		}
	}
	return d;
}

#endif
