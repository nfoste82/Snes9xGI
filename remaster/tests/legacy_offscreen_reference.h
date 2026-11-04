#ifndef REMASTER_LEGACY_OFFSCREEN_REFERENCE_H
#define REMASTER_LEGACY_OFFSCREEN_REFERENCE_H

// Historical sphere approximation for old tests only. Source discovery, signed
// placement bases and provenance are independently exercised by production tests.
#include "../runtime/offscreen_emitters.h"

template<class HashTile>
inline RemasterRoomEmitterDiagnostics legacyAppendOffscreenRoomLights(
	RemasterFrame &frame, const RemasterDungeonFloorContext &c, const RemasterProfile &profile,
	const RemasterDungeonHeightMap *bg2, const RemasterDungeonHeightMap *bg1, HashTile hashTile)
{
	// Obtain current exact source records, then apply only the retired grouping
	// policy. Never route EndFrame or the application through this approximation.
	const size_t first=frame.hiddenEmitters.size();
	auto diagnostics=S9xRemasterAppendOffscreenRoomLights(frame,c,profile,bg2,bg1,hashTile);
	struct Source {
		int layer,cell;
		const RemasterAssetMetadata *asset;
		float power=0,x=0,y=0,z=0;
		unsigned count=0;
		std::array<float,3> color={};
	};
	std::vector<Source> sources;
	std::map<std::pair<int,int>,size_t> placements;
	std::vector<RemasterFrameHiddenEmitter> covered;
	for(size_t n=first;n<frame.hiddenEmitters.size();++n) {
		const auto &e=frame.hiddenEmitters[n];
		if(e.x>=0 && e.y>=0 && e.x<frame.width && e.y<frame.height) {covered.push_back(e);continue;}
		if(!e.omnidirectional && e.depth<=0)continue;
		const int x=int(std::floor(e.x))+(c.backgroundScrollX&511);
		const int y=int(std::floor(e.y))+(c.backgroundScrollY&511);
		const int cell=y/8*64+x/8;
		// Exact source ordering is BG2 then BG1. Match the complete source
		// identity/transform to recover its historical grouping selector.
		int layer=-1;const RemasterAssetMetadata *asset=nullptr;
		for(int bg:{1,0}) {
			const size_t at=(bg==1?0:8192)+cell*2;
			const uint16_t word=c.constructedMaps[at]|c.constructedMaps[at+1]<<8;
			const uint16_t base=bg==1 || c.bg1GraphicsBase==0xffff?c.graphicsBase:c.bg1GraphicsBase;
			if(!bg && base==c.graphicsBase && word==(c.constructedMaps[cell*2]|c.constructedMaps[cell*2+1]<<8))continue;
			const uint16_t address=uint16_t(base+(word&1023)*32);
			uint8_t pixels[64]={};
			for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)for(int p=0;p<4;p++)
				pixels[yy*8+xx]|=((c.graphics[uint16_t(address+p/2*16+yy*2+p%2)]>>(7-xx))&1)<<p;
			const auto found=profile.assets.find({hashTile(4,pixels),1,4});
			if(found==profile.assets.end() || !found->second.hasEmission)continue;
			const auto *map=bg==1?bg2:bg1;
			if(!map || map->offsets[cell]==255)continue;
			layer=bg;asset=&found->second;break;
		}
		if(!asset)continue;
		const auto key=std::make_pair(layer,cell);
		auto at=placements.find(key);
		if(at==placements.end()) {at=placements.emplace(key,sources.size()).first;sources.push_back({layer,cell,asset});}
		auto &s=sources[at->second];const float w=e.emission[3];
		s.power+=w;s.x+=e.x*w;s.y+=e.y*w;s.z+=(e.z+e.depth*0.5f)*w;s.count++;
		for(int k=0;k<3;k++)s.color[k]+=std::pow(e.emission[k]/255.0f,2.2f)*w;
	}
	frame.hiddenEmitters.resize(first);
	frame.hiddenEmitters.insert(frame.hiddenEmitters.end(),covered.begin(),covered.end());
	diagnostics.hiddenPixels=uint32_t(covered.size());
	std::vector<size_t> parent(sources.size());
	for(size_t i=0;i<parent.size();i++)parent[i]=i;
	auto root=[&](size_t i){while(parent[i]!=i)i=parent[i];return i;};
	if(frame.representativeEmitters)for(size_t i=0;i<sources.size();i++)for(int delta:{1,64}) {
		const auto &s=sources[i];if((delta==1 && s.cell%64==63) || s.cell+delta>=4096)continue;
		const auto next=placements.find({s.layer,s.cell+delta});if(next==placements.end())continue;
		const auto &a=*s.asset,&b=*sources[next->second].asset;
		if(a.emissionOmnidirectional!=b.emissionOmnidirectional ||
			std::find(a.combineEmitters.begin(),a.combineEmitters.end(),b.tileId)==a.combineEmitters.end() ||
			std::find(b.combineEmitters.begin(),b.combineEmitters.end(),a.tileId)==b.combineEmitters.end())continue;
		const size_t ra=root(i),rb=root(next->second);parent[std::max(ra,rb)]=std::min(ra,rb);
	}
	for(size_t i=0;i<sources.size();i++) {
		const auto r=root(i);if(r==i)continue;
		auto &a=sources[r];const auto &b=sources[i];
		a.power+=b.power;a.x+=b.x;a.y+=b.y;a.z+=b.z;a.count+=b.count;
		for(int k=0;k<3;k++)a.color[k]+=b.color[k];
	}
	for(size_t i=0;i<sources.size();i++)if(root(i)==i) {
		const auto &s=sources[i];RemasterFrameLight light;
		light.x=s.x/s.power;light.y=s.y/s.power;light.z=s.z/s.power;
		light.radius=std::sqrt(s.count/(4.0f*3.14159265358979323846f));light.intensity=s.power/s.count;
		light.red=std::pow(s.color[0]/s.power,1.0f/2.2f);
		light.green=std::pow(s.color[1]/s.power,1.0f/2.2f);
		light.blue=std::pow(s.color[2]/s.power,1.0f/2.2f);
		frame.lights.push_back(light);diagnostics.lights++;
	}
	return diagnostics;
}

#endif
