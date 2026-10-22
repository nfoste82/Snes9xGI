// Complete capture evidence -> retained movable-obstacle floor support.
#ifndef _REMASTER_SPRITE_OBSTACLE_REPLAY_H_
#define _REMASTER_SPRITE_OBSTACLE_REPLAY_H_
#include "remaster.h"

inline bool S9xRemasterRestoreSpriteObstacleSupport(const std::string &path, RemasterFrame &frame,
	const RemasterProfile &profile)
{
	frame.spriteObstacleSupport.reset();
	if(std::none_of(profile.obstacleGroups.begin(),profile.obstacleGroups.end(),
		[](const RemasterObstacleGroup &g){return g.spriteAssembly;}))return false;
	auto read=[](const std::string &p){std::ifstream f(p,std::ios::binary);return std::vector<uint8_t>{std::istreambuf_iterator<char>(f),{}};};
	const auto input=read(path+".dungeon-input.bin"),templates=read(path+".dungeon-templates.bin"),json=read(path+".dungeon.json"),pinned=read(path+".profile.toml");
	if(input.size()!=90120 || std::memcmp(input.data(),"ALTPDI1\0",8) ||
		templates.size()!=12884 || std::memcmp(templates.data(),"ALTPDT1\0",8) || pinned.empty())return false;
	const std::string text(json.begin(),json.end());
	const std::string rom="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
	if(frame.profileRomSha256!=rom || profile.romSha256!=rom || text.find("\""+rom+"\"")==std::string::npos)return false;
	uint64_t digest=UINT64_C(14695981039346656037);for(uint8_t b:pinned)digest=(digest^b)*UINT64_C(1099511628211);
	std::ostringstream hex;hex<<std::hex<<digest;
	if(text.find("\"profile_serialized_fnv1a64\":\""+hex.str()+"\"")==std::string::npos)return false;
	auto number=[&](const char *key,int index=0)->int {
		auto p=text.find(std::string("\"")+key+"\":");if(p==std::string::npos)throw std::runtime_error("missing capture state");
		p=text.find(':',p)+1;while(p<text.size() && std::isspace(static_cast<unsigned char>(text[p])))p++;
		if(p<text.size() && text[p]=='[')p++;
		for(int i=0;i<index;i++){p=text.find(',',p);if(p==std::string::npos)throw std::runtime_error("capture state array");p++;}
		return std::stoi(text.substr(p));
	};
	try {
		RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;
		c.roomIndex=number("room");c.previousRoomIndex=number("previous_room");
		c.submodule=number("submodule");c.collisionMode=number("collision_mode");
		if(c.submodule || c.collisionMode || !frame.area.valid() || frame.area.id!=c.roomIndex)return false;
		c.linkX=number("link_xy");c.linkY=number("link_xy",1);c.linkPlane=number("plane");
		if(c.linkPlane>1)return false;
		c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);
		c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");
		c.groundedEvidence=text.find("\"grounded_evidence\":true")!=std::string::npos;
		c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=templates.data()+8;
		if(!S9xRemasterReadTerrainEvidence(path+".terrain-input.bin",c) ||
			!S9xRemasterReadSpriteObstacleEvidence(path+".sprite-input.bin",c))return false;
		// Reject stale camera/maps/graphics instead of silently binding unrelated
		// sidecars. Check every available BG observation, not a viewport sample.
		size_t observed=0;
		for(const auto *pixels:{&frame.mainPixels,&frame.subPixels,&frame.scenePixels})for(size_t at=0;at<pixels->size();at++) {
			const auto &p=(*pixels)[at];if(!p.instanceId || p.instanceId>frame.tileInstances.size())continue;
			const auto &a=frame.tileInstances[p.instanceId-1];if(a.source!=RemasterSourceType::Background || a.sourceIndex>1)continue;
			const int x=(c.backgroundScrollX+at%frame.width)&511,y=(c.backgroundScrollY+at/frame.width)&511;
			const int address=(y/8*64+x/8)*2+(a.sourceIndex?0:8192),word=c.constructedMaps[address]|c.constructedMaps[address+1]<<8;
			const int base=a.sourceIndex || c.bg1GraphicsBase==0xffff?c.graphicsBase:c.bg1GraphicsBase;
			uint8_t decoded[64];for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++) {
				int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(base+(word&1023)*32+b/2*16+yy*2+b%2)]>>(7-xx))&1)<<b;
				decoded[yy*8+xx]=v;
			}
			if(a.tileId.hash!=S9xRemasterHashTile(4,decoded) || a.hFlip!=bool(word&0x4000) || a.vFlip!=bool(word&0x8000) ||
				p.tilePixel!=((word&0x8000?7-y%8:y%8)*8+(word&0x4000?7-x%8:x%8)))return false;
			observed++;
		}
		if(!observed)return false;
		S9xRemasterDungeonStairsFromAttributes(c);
		std::array<std::unique_ptr<RemasterDungeonHeightMap>,2> maps;
		for(int bg=0;bg<2;bg++) {
			auto lc=c;lc.placementBackgroundIndex=bg;
			auto words=std::make_unique<std::array<uint8_t,16384>>();
			if(!bg){std::copy_n(c.constructedMaps+8192,8192,words->begin());std::copy_n(c.constructedMaps,8192,words->begin()+8192);lc.constructedMaps=words->data();std::swap(lc.graphicsBase,lc.bg1GraphicsBase);}
			else {lc.supportRelations=&maps[0]->authoredSupportRelations;lc.wallSupportRelations=&maps[0]->authoredWallSupportRelations;}
			auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
			S9xRemasterDungeonPlacements(lc,profile,*tiles);lc.placements=tiles->data();
			maps[bg]=std::make_unique<RemasterDungeonHeightMap>();
			if(!S9xRemasterBuildDungeonHeightMap(lc,profile.upperFloorHeight,*maps[bg]))return false;
		}
		S9xRemasterApplySpriteObstacles(frame,profile,&c,maps[1].get());
		return bool(frame.spriteObstacleSupport);
	} catch(const std::exception &) { return false; }
}
#endif
