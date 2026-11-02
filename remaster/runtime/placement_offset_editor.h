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
	for(const auto &e:edits)p.overworldOffsets[e.first]=e.second; // explicit zero preserved
	p.schemaVersion=std::max<uint32_t>(p.schemaVersion,37);return true;
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
	}
}
#endif
