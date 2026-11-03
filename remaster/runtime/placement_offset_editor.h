#ifndef REMASTER_PLACEMENT_OFFSET_EDITOR_H
#define REMASTER_PLACEMENT_OFFSET_EDITOR_H
#include "placement_offset_debug.h"
#include <climits>
inline bool S9xRemasterPlacementMapsEqual(const std::map<RemasterContextLocation,int16_t> &a,const std::map<RemasterContextLocation,int16_t> &b) {
	if(a.size()!=b.size())return false;auto x=a.begin(),y=b.begin();
	for(;x!=a.end();++x,++y)if(x->first<y->first || y->first<x->first || x->second!=y->second)return false;return true;
}

inline bool S9xRemasterParsePlacementScalar(const std::string &text,int &value) {
	if(text.empty())return false;size_t n=0;bool negative=false;
	if(text[n]=='+' || text[n]=='-'){negative=text[n]=='-';if(++n==text.size())return false;}
	int v=0;for(;n<text.size();n++){if(text[n]<'0'||text[n]>'9')return false;v=v*10+text[n]-'0';if(v>32767)return false;}
	value=negative?-v:v;return true;
}
inline bool S9xRemasterPlacementTargets(const RemasterProfile &p,const RemasterFrame &f,
	const std::set<uint32_t> &ids,std::set<RemasterContextLocation> &out,std::string &error) {
	out.clear();error.clear();
	if(!p.authoredOverworldOffsets){error="Placement mode is disabled; explicitly load an enabled experimental profile.";return false;}
	if(p.romSha256!=f.profileRomSha256 || (!f.placementOffsetCoordinateRom.empty() && p.romSha256!=f.placementOffsetCoordinateRom)){error="Loaded profile does not match this frame ROM.";return false;}
	if(ids.empty()){error="Select world BG2 placements in the scene (Command-click to add).";return false;}
	for(auto id:ids){if(!S9xRemasterPlacementCoordinateVerified(f,id)){error="Unsupported selection: only verified world BG2 coordinates are editable.";out.clear();return false;}out.insert(f.contextLocations.at(id));}
	return true;
}
inline bool S9xRemasterEditPlacementOffsets(RemasterProfile &p,const RemasterFrame &f,
	const std::set<uint32_t> &ids,int value,bool relative,std::string &error) {
	std::set<RemasterContextLocation> targets;if(!S9xRemasterPlacementTargets(p,f,ids,targets,error))return false;
	if(value < -32767 || value > 32767 || (relative && value!=-1 && value!=1)){error="Offset must be an integer in -32767…32767.";return false;}
	std::map<RemasterContextLocation,int16_t> edits;
	for(const auto &at:targets){auto old=p.overworldOffsets.find(at);int now=relative?(old==p.overworldOffsets.end()?0:old->second)+value:value;
		if(now < -32767 || now > 32767){error="Whole selection unchanged: an offset would exceed -32767…32767.";return false;}edits[at]=int16_t(now);}
	for(const auto &e:edits){if(e.second)p.overworldOffsets[e.first]=e.second;else p.overworldOffsets.erase(e.first);}
	p.schemaVersion=std::max<uint32_t>(p.schemaVersion,37);return true;
}
// Edit the combined support offset (scalar + terrain), never local asset relief.
// Pixel is WORLD oriented for every target, independent of artwork flips.
inline bool S9xRemasterEditPlacementPixelHeight(RemasterProfile &p,const RemasterFrame &f,
	const std::set<uint32_t> &ids,unsigned pixel,float value,bool relative,std::string &error) {
	std::set<RemasterContextLocation> targets;
	if(!S9xRemasterPlacementTargets(p,f,ids,targets,error))return false;
	if(pixel>=64 || !std::isfinite(value)){error="Choose a pixel and a finite height offset.";return false;}
	std::map<RemasterContextLocation,RemasterTerrainOffset> edits;
	for(const auto &at:targets) {
		auto found=p.overworldTerrain.find(at);RemasterTerrainOffset t=found==p.overworldTerrain.end()?RemasterTerrainOffset{}:found->second;
		auto scalar=p.overworldOffsets.find(at);const float base=scalar==p.overworldOffsets.end()?0:scalar->second;
		if(!t.hasHeight)t.height.fill(0);
		const float adjustment=relative?t.height[pixel]+value:value-base;
		if(!std::isfinite(adjustment)||std::abs(adjustment)>32767){error="Whole selection unchanged: pixel adjustment exceeds bounds.";return false;}
		t.height[pixel]=adjustment;t.hasHeight=std::any_of(t.height.begin(),t.height.end(),[](float h){return h!=0;});
		edits.emplace(at,t);
	}
	for(const auto &e:edits){if(e.second.identity())p.overworldTerrain.erase(e.first);else p.overworldTerrain[e.first]=e.second;}
	p.schemaVersion=std::max<uint32_t>(p.schemaVersion,38);return true;
}
// Whole coordinate-layer clipboard. Height includes scalar + optional pixel
// adjustment; normal includes uniform or pixel rotation. Other channels survive.
struct RemasterPlacementLayer {
	int16_t scalar = 0;
	RemasterTerrainOffset terrain;
};
inline RemasterPlacementLayer S9xRemasterCopyPlacementLayer(const RemasterProfile &p,const RemasterContextLocation &at) {
	RemasterPlacementLayer layer;auto s=p.overworldOffsets.find(at);if(s!=p.overworldOffsets.end())layer.scalar=s->second;
	auto t=p.overworldTerrain.find(at);if(t!=p.overworldTerrain.end())layer.terrain=t->second;return layer;
}
inline bool S9xRemasterPastePlacementLayer(RemasterProfile &p,const RemasterFrame &f,
	const std::set<uint32_t> &ids,const RemasterPlacementLayer &layer,bool normal,std::string &error) {
	std::set<RemasterContextLocation> targets;if(!S9xRemasterPlacementTargets(p,f,ids,targets,error))return false;
	if(!S9xRemasterValidTerrainOffset(layer.terrain)||layer.scalar < -32767){error="Invalid offset clipboard; whole selection unchanged.";return false;}
	for(const auto &at:targets) {
		auto found=p.overworldTerrain.find(at);RemasterTerrainOffset t=found==p.overworldTerrain.end()?RemasterTerrainOffset{}:found->second;
		if(normal){t.normal=layer.terrain.normal;t.hasPixelNormals=layer.terrain.hasPixelNormals;t.pixelNormals=layer.terrain.pixelNormals;
			bool identity=true;for(unsigned k=0;k<64;k++)identity &= t.normalAt(k)==std::array<float,3>{{0,0,1}};
			if(identity){t.normal={{0,0,1}};t.hasPixelNormals=false;t.pixelNormals={};}}
		else {t.height=layer.terrain.height;t.hasHeight=layer.terrain.hasHeight && std::any_of(t.height.begin(),t.height.end(),[](float h){return h!=0;});
			if(!t.hasHeight)t.height={};
			// A combined zero field clears even a nonzero scalar cancelled by pixels.
			bool zero=true;for(unsigned k=0;k<64;k++)zero &= layer.scalar+(t.hasHeight?t.height[k]:0)==0;
			if(zero){p.overworldOffsets.erase(at);t.hasHeight=false;t.height={};}
			else if(layer.scalar)p.overworldOffsets[at]=layer.scalar;else p.overworldOffsets.erase(at);}
		if(t.identity())p.overworldTerrain.erase(at);else p.overworldTerrain[at]=t;
	}
	p.schemaVersion=std::max<uint32_t>(p.schemaVersion,layer.terrain.hasPixelNormals?39:38);return true;
}
// Current-profile preview only. Never call on immutable recorded evidence.
// Verified identity must ALREADY be in packet; no RAM/filename reconstruction.
inline void S9xRemasterPreviewPlacementOffsets(const RemasterProfile &p,RemasterFrame &f) {
	if(f.placementOffsetCoordinateRom.empty())f.placementOffsetCoordinateRom=f.profileRomSha256;
	const bool enabled=p.authoredOverworldOffsets && p.romSha256==f.placementOffsetCoordinateRom;
	for(uint32_t id=1;id<=f.tileInstances.size();++id) {
		auto &i=f.tileInstances[id-1];
		const bool verified=S9xRemasterPlacementCoordinateVerified(f,id);
		i.savedPlacementCoordinateVerified=verified;
		const bool available=enabled && verified;
		auto at=available?p.overworldOffsets.find(f.contextLocations.at(id)):p.overworldOffsets.end();
		const int value=at==p.overworldOffsets.end()?0:at->second;
		// Only remove our own previous scalar; actor/dungeon/other datums survive.
		if(i.savedPlacementOffsetStatus)i.heightOffset-=i.savedPlacementOffset;
		if(available)i.heightOffset+=value;
		i.savedPlacementOffset=available?value:0;
		i.savedPlacementOffsetStatus=available?(at==p.overworldOffsets.end()?1:2):0;
		i.terrainOffset={};
		if(available){auto t=p.overworldTerrain.find(f.contextLocations.at(id));if(t!=p.overworldTerrain.end())i.terrainOffset=t->second;}
	}
}
#endif
