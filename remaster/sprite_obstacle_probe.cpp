// Read-only pinned-room research probe. Derived groups/layers NEVER saved to profile.
#include "remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool v,const char *s){if(!v)throw std::runtime_error(s);}
static std::vector<uint8_t> bytes(const std::string &p){std::ifstream f(p,std::ios::binary);need(bool(f),"missing input");return {std::istreambuf_iterator<char>(f),{}};}
using Part=std::tuple<int,int,RemasterTileContentId,bool,bool>;
static std::set<Part> parts(const RemasterFrame &f) {
	std::set<Part> out;
	for(size_t at=0;at<f.mainPixels.size();at++) {
		const auto &p=f.mainPixels[at];if(!p.instanceId || p.instanceId>f.tileInstances.size() || p.tilePixel>=64)continue;
		const auto &a=f.tileInstances[p.instanceId-1];
		if(a.source!=RemasterSourceType::Object || a.sourceIndex<116 || a.sourceIndex>121)continue;
		int x=int(at%f.width)-(a.hFlip?7-p.tilePixel%8:p.tilePixel%8),y=int(at/f.width)-(a.vFlip?7-p.tilePixel/8:p.tilePixel/8);
		out.emplace(x,y,a.tileId,a.hFlip,a.vFlip);
	}return out;
}
int main(int argc,char **argv){try {
	need(argc>=2,"usage: probe pinned-capture [audit-captures ...]");std::string path=argv[1];
	RemasterFrame f;need(S9xReadRemasterFrame(path,f),"capture parse");
	auto raw=bytes(path+".dungeon-input.bin"),tpl=bytes(path+".dungeon-templates.bin"),meta=bytes(path+".dungeon.json");
	need(raw.size()==90120 && !std::memcmp(raw.data(),"ALTPDI1\0",8),"complete input");
	need(tpl.size()==12884 && !std::memcmp(tpl.data(),"ALTPDT1\0",8),"templates");
	std::string text(meta.begin(),meta.end());
	need(text.find("66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb")!=std::string::npos,"ROM pin");
	auto number=[&](const char *key,int index=0){auto p=text.find(std::string("\"")+key+"\":");need(p!=std::string::npos,"metadata key");p=text.find(':',p)+1;if(text[p]=='[')p++;for(int i=0;i<index;i++)p=text.find(',',p)+1;return std::stoi(text.substr(p));};
	RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
	need(S9xRemasterLoadProfile(path+".profile.toml",profile,errors),"pinned profile");
	// Verify original byte pin, not serialization under a newer profile schema.
	uint64_t digest=UINT64_C(14695981039346656037);for(auto b:bytes(path+".profile.toml"))digest=(digest^b)*UINT64_C(1099511628211);
	std::ostringstream hex;hex<<std::hex<<digest;need(text.find("\"profile_serialized_fnv1a64\":\""+hex.str()+"\"")!=std::string::npos,"profile pin");
	RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=number("room");c.previousRoomIndex=number("previous_room");
	c.linkX=number("link_xy");c.linkY=number("link_xy",1);c.linkPlane=number("plane");c.submodule=number("submodule");c.collisionMode=number("collision_mode");
	c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");
	c.constructedMaps=raw.data()+8;c.collisionAttributes=raw.data()+16392;c.graphics=raw.data()+24584;c.dungeonTemplateData=tpl.data()+8;
	c.groundedEvidence=text.find("\"grounded_evidence\":true")!=std::string::npos;
	need(S9xRemasterReadTerrainEvidence(path+".terrain-input.bin",c),"terrain evidence");
	if(const char *evidence=std::getenv("ALTTP_SPRITE_PRODUCER_EVIDENCE"))
		need(S9xRemasterReadSpriteObstacleEvidence(evidence,c),"sprite producer evidence");
	S9xRemasterDungeonStairsFromAttributes(c);
	size_t matched=0,mismatch=0;
	for(const auto *pixels:{&f.mainPixels,&f.subPixels,&f.scenePixels})for(size_t at=0;at<pixels->size();at++) {
		const auto &p=(*pixels)[at];if(!p.instanceId)continue;const auto &a=f.tileInstances.at(p.instanceId-1);
		if(a.source!=RemasterSourceType::Background || a.sourceIndex>1)continue;
		int x=(c.backgroundScrollX+at%f.width)&511,y=(c.backgroundScrollY+at/f.width)&511;
		int address=(y/8*64+x/8)*2+(a.sourceIndex?0:8192),word=c.constructedMaps[address]|c.constructedMaps[address+1]<<8;
		uint8_t decoded[64];int base=a.sourceIndex?c.graphicsBase:c.bg1GraphicsBase;
		for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++){int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(base+(word&1023)*32+(b/2)*16+yy*2+b%2)]>>(7-xx))&1)<<b;decoded[yy*8+xx]=v;}
		matched++;mismatch+=a.tileId.hash!=S9xRemasterHashTile(4,decoded) || a.hFlip!=bool(word&0x4000) || a.vFlip!=bool(word&0x8000) || p.tilePixel!=((word&0x8000?7-y%8:y%8)*8+(word&0x4000?7-x%8:x%8));
	}
	need(!mismatch,"capture/map mismatch");
	std::cout<<"{\"profile_pin\":\""<<hex.str()<<"\",\"matching_bg_samples\":"<<matched<<",\"layers\":[";
	std::array<std::unique_ptr<RemasterDungeonHeightMap>,2> maps;
	for(int bg=0;bg<2;bg++) {
		auto lc=c;lc.placementBackgroundIndex=bg;auto words=std::make_unique<std::array<uint8_t,16384>>();
		if(!bg){std::copy_n(c.constructedMaps+8192,8192,words->begin());std::copy_n(c.constructedMaps,8192,words->begin()+8192);lc.constructedMaps=words->data();std::swap(lc.graphicsBase,lc.bg1GraphicsBase);}
		if(bg){lc.supportRelations=&maps[0]->authoredSupportRelations;lc.wallSupportRelations=&maps[0]->authoredWallSupportRelations;}
		auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();S9xRemasterDungeonPlacements(lc,profile,*tiles);lc.placements=tiles->data();
		maps[bg]=std::make_unique<RemasterDungeonHeightMap>();auto &m=*maps[bg];need(S9xRemasterBuildDungeonHeightMap(lc,profile.upperFloorHeight,m),"solve");need(bool(m.roomDiagnostics),"runtime diagnostics unavailable");
		if(bg)std::cout<<',';
		std::cout<<"{\"bg\":"<<bg<<",\"required\":["<<m.requiredPlaneCells[0]<<','<<m.requiredPlaneCells[1]<<"],\"missing\":["<<m.unknownPlaneCells[0]<<','<<m.unknownPlaneCells[1]<<"],\"walls_solved\":"<<m.solvedWallCells<<",\"walls_missing\":"<<m.unknownWallCells<<",\"structural_conflicts\":"<<m.structuralConflicts<<",\"equality_conflicts\":"<<m.geometryConflicts.size()<<'}';
	}
	auto observed=parts(f);need(!observed.empty(),"no assembly");const int ox=std::get<0>(*observed.begin()),oy=std::get<1>(*observed.begin());
	int bottom=-10000;for(const auto &p:observed)bottom=std::max(bottom,std::get<1>(p));
	RemasterObstacleGroup group;group.name="capture_research_only";group.spriteAssembly=true;
	for(const auto &p:observed){need((std::get<0>(p)-ox)%8==0 && (std::get<1>(p)-oy)%8==0,"unaligned members");group.members.push_back({std::get<2>(p),(std::get<0>(p)-ox)/8,(std::get<1>(p)-oy)/8,0,std::get<3>(p),std::get<4>(p),std::get<1>(p)==bottom});}
	profile.obstacleGroups.push_back(group);
	std::cout<<"],\"contacts\":[";bool first=true;
	for(const auto &p:observed)if(std::get<1>(p)==bottom)for(int y:{bottom+7,bottom+8})for(int x:{std::get<0>(p),std::get<0>(p)+7}) {
		int cell=(((c.backgroundScrollY+y)&511)/8)*64+((c.backgroundScrollX+x)&511)/8;
		for(int plane=0;plane<2;plane++){const auto &d=maps[1]->roomDiagnostics->cells[plane*4096+cell];if(!first)std::cout<<',';first=false;
		std::cout<<"{\"cell\":["<<cell%64<<','<<cell/64<<"],\"plane\":"<<plane<<",\"floor\":"<<d.floorId<<",\"floor_base\":"<<d.floorBase<<",\"plane_base\":"<<maps[1]->planeOffsets[plane][cell]<<",\"wall\":"<<d.wall<<",\"observed\":"<<d.planeObserved<<",\"conflict\":"<<d.heightConflict<<'}';}
	}
	std::cout<<"],\"captures\":[";
	for(int capture=1;capture<argc;capture++) {
		RemasterFrame audit;need(S9xReadRemasterFrame(argv[capture],audit),"audit capture");
		const bool pinned=capture==1;
		RemasterSpriteObstacleDiagnostics before,after;S9xRemasterApplySpriteObstacles(audit,profile,pinned?&c:nullptr,pinned?maps[1].get():nullptr,&before);
		size_t missing=0;std::set<RemasterTileContentId> ids;for(const auto &m:group.members)ids.insert(m.tileId);
		// A deliberately nonflat TEST placement-local layer proves composition.
		// Does not validate or publish real object local geometry.
		for(auto &a:audit.tileInstances)if(a.source==RemasterSourceType::Object && ids.count(a.tileId)) {
			bool local=a.hasPlacementHeight;for(const auto &md:audit.assetMetadata)if(md.tileId==a.tileId)local|=md.hasHeight;
			if(!local){missing++;a.hasPlacementHeight=true;for(int p=0;p<64;p++)a.placementHeight[p]=int16_t(20-p/8);}
		}
		S9xRemasterApplySpriteObstacles(audit,profile,pinned?&c:nullptr,pinned?maps[1].get():nullptr,&after);
		if(pinned) {
			need(after.matchedAssemblies==1 && after.claimedInstances==192,"pinned assembly regression");
			if(c.hasObstacleActors)need(after.resolvedAssemblies==1 && after.assemblies[0].producerActor==0 && after.assemblies[0].plane==0 && after.assemblies[0].base==50,"producer-owned support regression");
			else need(after.conflictingAssemblies==1 && !after.resolvedAssemblies,"pinned overlapping-plane regression");
			need(missing==192,"pinned missing-local denominator changed");
			if(c.hasObstacleActors) {
				auto alternate=c;alternate.obstacleActors[0].plane=1;
				RemasterSpriteObstacleDiagnostics trial;
				S9xRemasterApplySpriteObstacles(audit,profile,&alternate,maps[1].get(),&trial);
				need(trial.resolvedAssemblies==1 && trial.assemblies[0].base==0 && trial.assemblies[0].plane==1,"producer plane must select independent floor0");
				alternate=c;alternate.obstacleActors[1]=alternate.obstacleActors[0];
				S9xRemasterApplySpriteObstacles(audit,profile,&alternate,maps[1].get(),&trial);
				need(!trial.resolvedAssemblies && trial.conflictingAssemblies==1,"duplicate actor reject");
				alternate=c;alternate.obstacleActors[0].x++;
				S9xRemasterApplySpriteObstacles(audit,profile,&alternate,maps[1].get(),&trial);
				need(!trial.resolvedAssemblies && trial.assemblies[0].producerActor<0,"nearby actor reject");
				alternate=c;alternate.obstacleActors[0].x++;alternate.obstacleActors[0].hasMotion=true;
				alternate.obstacleActors[0].xVelocity=2;alternate.obstacleActors[0].xSubpixel=0;
				S9xRemasterApplySpriteObstacles(audit,profile,&alternate,maps[1].get(),&trial);
				need(trial.resolvedAssemblies==1 && trial.assemblies[0].base==50,"exact pre-move fixed-point anchor");
				alternate=c;alternate.terrainOam[116].name^=1;
				S9xRemasterApplySpriteObstacles(audit,profile,&alternate,maps[1].get(),&trial);
				need(!trial.resolvedAssemblies && trial.assemblies[0].producerActor<0,"overwritten OAM reject");
				alternate=c;alternate.obstacleActors[0].state=0;
				S9xRemasterApplySpriteObstacles(audit,profile,&alternate,maps[1].get(),&trial);
				need(!trial.resolvedAssemblies && trial.assemblies[0].producerActor<0,"inactive actor reject");
				S9xRemasterApplySpriteObstacles(audit,profile,&c,maps[1].get(),&after);
				need(after.resolvedAssemblies==1,"producer recovery");
			}
		}
		for(const auto &a:audit.tileInstances)need(S9xRemasterValidOccurrenceGeometry(a),"invalid occurrence after runtime adapter");
		std::vector<uint8_t> encoded;need(S9xSerializeRemasterFrame(audit,encoded),"runtime capture serialization");
		RemasterFrame decoded;need(S9xDeserializeRemasterFrame(encoded,decoded),"runtime capture roundtrip");
		if(capture>1)std::cout<<',';
		std::cout<<"{\"path\":\""<<argv[capture]<<"\",\"observed\":"<<after.observedTiles<<",\"matched\":"<<after.matchedAssemblies<<",\"claimed\":"<<after.claimedInstances<<",\"unclaimed\":"<<after.unclaimedInstances<<",\"missing_local_records\":"<<missing<<",\"native_resolved\":"<<before.resolvedAssemblies<<",\"test_local_resolved\":"<<after.resolvedAssemblies<<",\"conflicting\":"<<after.conflictingAssemblies<<",\"unresolved\":"<<after.unresolvedAssemblies<<",\"assemblies\":[";
		for(size_t i=0;i<after.assemblies.size();i++){const auto &a=after.assemblies[i];if(i)std::cout<<',';std::cout<<"{\"xy\":["<<a.x<<','<<a.y<<"],\"members\":"<<a.members<<",\"contacts\":"<<a.contacts<<",\"plane\":"<<a.plane<<",\"component\":"<<a.component<<",\"base\":"<<a.base<<",\"status\":"<<int(a.status)<<'}';}
		std::cout<<"]}";
	}
	std::cout<<"]}\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
