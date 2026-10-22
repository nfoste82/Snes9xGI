// Authored dynamic OBJ footprints. No room-specific coordinates or hash lists.
#ifndef _REMASTER_SPRITE_OBSTACLE_H_
#define _REMASTER_SPRITE_OBSTACLE_H_
#include "frame.h"
#include <set>

// Additive producer sidecar; older terrain observations cannot attest these actors.
inline bool S9xRemasterReadSpriteObstacleEvidence(const std::string &path,RemasterDungeonFloorContext &c)
{
	std::ifstream f(path,std::ios::binary);if(!f)return false;
	std::vector<uint8_t> b{std::istreambuf_iterator<char>(f),{}};
	const bool v2=b.size()==204 && !std::memcmp(b.data(),"ALTPSA2\0",8);
	if(!v2 && (b.size()!=140 || std::memcmp(b.data(),"ALTPSA1\0",8)))return false;
	auto word=[&](size_t p){return uint16_t(b[p]|b[p+1]<<8);};
	if(word(8)!=c.actorScrollX || word(10)!=c.actorScrollY)return false;
	for(size_t i=0;i<16;i++) {
		size_t p=12+i*(v2?12:8);auto &a=c.obstacleActors[i];
		a={word(p),word(p+2),b[p+4],b[p+5],b[p+6],b[p+7]};
		if(v2){a.xSubpixel=b[p+8];a.xVelocity=b[p+9];a.yVelocity=b[p+10];a.hasMotion=b[p+11]==1;}
	}
	c.hasObstacleActors=true;return true;
}

struct RemasterSpriteObstacleDiagnostics
{
	struct Assembly {
		std::string group;
		int x=0,y=0,transform=0,plane=-1,component=-1,base=255;
		size_t members=0,instances=0,contacts=0;
		size_t missingLocalInstances=0;
		uint8_t supportPlaneMask=0;
		int producerActor=-1,producerPlane=-1;
		RemasterOccurrenceGeometryStatus status=RemasterOccurrenceGeometryStatus::Unresolved;
	};
	uint32_t observedTiles=0,invalidInstances=0,incompleteCandidates=0;
	uint32_t matchedAssemblies=0,resolvedAssemblies=0,conflictingAssemblies=0,unresolvedAssemblies=0;
	uint32_t claimedInstances=0,unclaimedInstances=0;
	std::vector<Assembly> assemblies;
};

// Retain just movement-floor evidence, not the large placement/solver scratch.
// This lets paused painting reapply local layers without borrowing a newer room.
struct RemasterSpriteObstacleSupport
{
	struct Cell { int component=-1,base=255,planeBase=255; bool observed=false,wall=false,conflict=false; };
	RemasterDungeonFloorContext context;
	std::array<Cell,8192> cells;
};

// Requires runtime movement-floor diagnostics, not artwork offsets (which may
// describe walls). Compiled legacy maps without this evidence remain unresolved.
// This runs AFTER the legacy OBJ floor/ramp adapters and replaces their datum
// for every claimed record. Existing placement-local arrays remain immutable.
// Limitations: settled current-room grounded assemblies only; no inferred actor
// plane, airborne displacement, outgoing-room support or wholly occluded member.
// Every declared 8x8 member needs an owned pixel. Contact evidence is restricted
// to its transformed underside and the immediately outside pixel row, not a
// nearest-floor search. A future producer adapter can disambiguate overlapping
// gameplay planes; equal bases alone cannot establish ownership. Diagnostics
// describe visible OBJ claims, never complete-room/corpus height approval.
inline void S9xRemasterApplySpriteObstacles(RemasterFrame &frame,const RemasterProfile &profile,
	const RemasterDungeonFloorContext *context,const RemasterDungeonHeightMap *map,
	RemasterSpriteObstacleDiagnostics *diagnostics=nullptr)
{
	RemasterSpriteObstacleDiagnostics out;
	std::set<RemasterTileContentId> identities;
	for(const auto &g:profile.obstacleGroups)if(g.spriteAssembly)
		for(const auto &m:g.members)identities.insert(m.tileId);
	if(identities.empty()){if(diagnostics)*diagnostics=out;return;}
	if(context && map && context->verifiedAlttpRom && context->indoors && !context->submodule &&
		map->roomIndex==context->roomIndex && map->hasPlaneOffsets && map->roomDiagnostics) {
		auto support=std::make_shared<RemasterSpriteObstacleSupport>();support->context=*context;
		// These are invocation-local pointers; retained support never dereferences them.
		auto &c=support->context;c.constructedMaps=nullptr;c.collisionAttributes=nullptr;c.graphics=nullptr;
		c.movementCache=nullptr;
		c.retainedOutgoingMaps=nullptr;
		c.dungeonTemplateData=nullptr;c.placements=nullptr;c.supportRelations=nullptr;c.wallSupportRelations=nullptr;
		for(size_t i=0;i<8192;i++) {
			const auto &v=map->roomDiagnostics->cells[i];
			support->cells[i]={v.floorId,v.floorBase,map->planeOffsets[i/4096][i%4096],v.planeObserved,v.wall,v.heightConflict};
		}
		frame.spriteObstacleSupport=std::move(support);
	} else if(context || map) frame.spriteObstacleSupport.reset();
	// A null context/map is the paused/replay refresh contract. No snapshot means
	// no support, never a Link-plane or base-zero fallback.
	const auto support=frame.spriteObstacleSupport;
	context=support?&support->context:nullptr;
	struct Tile { int x,y; RemasterTileContentId id; bool h,v; std::vector<size_t> instances; };
	using Key=std::tuple<int,int,RemasterTileContentId,bool,bool>;
	std::map<Key,size_t> index;
	std::map<RemasterTileContentId,const RemasterFrameAssetMetadata *> metadata;
	for(const auto &m:frame.assetMetadata)metadata[m.tileId]=&m;
	std::vector<bool> localSources(frame.tileInstances.size(),false);
	for(size_t i=0;i<frame.tileInstances.size();i++) {
		auto &a=frame.tileInstances[i];if(a.source!=RemasterSourceType::Object || !identities.count(a.tileId))continue;
		// Re-evaluate source availability independently of last invocation's
		// unresolved output, while retaining the exact flip/facing/context selection.
		const auto status=a.geometryStatus;a.geometryStatus=RemasterOccurrenceGeometryStatus::Inherit;
		const auto found=metadata.find(a.tileId);
		localSources[i]=S9xRemasterResolveFrameGeometry(found==metadata.end()?RemasterFrameAssetMetadata{}:*found->second,frame,i).hasHeight;
		a.geometryStatus=status;
	}
	std::vector<Tile> tiles;
	std::vector<int> occurrence(frame.tileInstances.size(),-1);
	std::vector<bool> invalid(frame.tileInstances.size(),false),seen(frame.tileInstances.size(),false);
	// Scanline renderer records are observations, not separate 8x8 objects.
	// Recover each origin from the owned canonical pixel, including clipped ink.
	for(const auto *pixels:{&frame.mainPixels,&frame.subPixels})for(size_t p=0;p<pixels->size();p++) {
		const auto &px=(*pixels)[p];
		if(!frame.width || p>=size_t(frame.width)*frame.height || !px.instanceId ||
			px.instanceId>frame.tileInstances.size())continue;
		const size_t i=px.instanceId-1;const auto &a=frame.tileInstances[i];
		if(a.source!=RemasterSourceType::Object || !identities.count(a.tileId))continue;
		seen[i]=true;
		if(px.tilePixel>=64){invalid[i]=true;continue;}
		const int x=int(p%frame.width)-(a.hFlip?7-px.tilePixel%8:px.tilePixel%8);
		const int y=int(p/frame.width)-(a.vFlip?7-px.tilePixel/8:px.tilePixel/8);
		const Key key{x,y,a.tileId,a.hFlip,a.vFlip};
		auto found=index.find(key);
		if(found==index.end()) {size_t n=tiles.size();tiles.push_back({x,y,a.tileId,a.hFlip,a.vFlip,{}});found=index.emplace(key,n).first;}
		const int n=int(found->second);
		if(occurrence[i]>=0 && occurrence[i]!=n)invalid[i]=true;
		else if(occurrence[i]<0){occurrence[i]=n;tiles[n].instances.push_back(i);}
	}
	out.observedTiles=uint32_t(tiles.size());
	for(size_t i=0;i<invalid.size();i++)if(invalid[i])out.invalidInstances++;
	struct Claim { size_t group; int x,y,flip; std::vector<size_t> tiles; std::vector<int> z; std::vector<bool> contacts; bool overlap=false; };
	std::vector<Claim> claims;
	for(size_t gi=0;gi<profile.obstacleGroups.size();gi++) {
		const auto &g=profile.obstacleGroups[gi];if(!g.spriteAssembly || g.members.empty())continue;
		std::set<std::tuple<int,int,int>> attempted;
		for(const auto &t:tiles)for(const auto &seed:g.members)if(t.id==seed.tileId)
		for(int flip=0;flip<4;flip++) {
			const bool hf=flip&1,vf=flip&2;
			if(t.h!=(seed.hflip!=hf) || t.v!=(seed.vflip!=vf))continue;
			const int ox=t.x-(hf?-seed.x:seed.x)*8,oy=t.y-(vf?-seed.y:seed.y)*8;
			if(!attempted.emplace(ox,oy,flip).second)continue;
			Claim c{gi,ox,oy,flip,{},{},{},false};bool complete=true;
			for(const auto &m:g.members) {
				auto it=index.find(Key{ox+(hf?-m.x:m.x)*8,oy+(vf?-m.y:m.y)*8,m.tileId,m.hflip!=hf,m.vflip!=vf});
				if(it==index.end()){complete=false;break;}
				const auto &part=tiles[it->second];
				if(part.instances.empty())complete=false;
				int slot=-1;
				for(size_t i:part.instances) {
					if(invalid[i])complete=false;
					const int current=frame.tileInstances[i].sourceIndex;
					if(slot>=0 && slot!=current)complete=false;
					slot=current;
				}
				c.tiles.push_back(it->second);c.z.push_back(m.z);c.contacts.push_back(m.supportContact);
			}
			if(!complete){out.incompleteCandidates++;continue;}
			// Equivalent H symmetry is safe only with identical member semantics.
			bool duplicate=false;
			for(const auto &prior:claims)if(prior.group==gi && ((prior.flip^flip)&2)==0 && prior.tiles.size()==c.tiles.size()) {
				bool same=true;
				for(size_t k=0;k<c.tiles.size();k++) {
					auto it=std::find(prior.tiles.begin(),prior.tiles.end(),c.tiles[k]);
					if(it==prior.tiles.end()){same=false;break;}
					const size_t j=size_t(it-prior.tiles.begin());
					if(prior.z[j]!=c.z[k] || prior.contacts[j]!=c.contacts[k])same=false;
				}
				if(same){duplicate=true;break;}
			}
			if(!duplicate)claims.push_back(std::move(c));
		}
	}
	// Reject the entire candidate when any member rectangle overlaps a different
	// candidate, including competing identities at the same screen location.
	for(size_t a=0;a<claims.size();a++)for(size_t b=a+1;b<claims.size();b++)
		for(size_t i:claims[a].tiles)for(size_t j:claims[b].tiles)
			if(std::abs(tiles[i].x-tiles[j].x)<8 && std::abs(tiles[i].y-tiles[j].y)<8)
				claims[a].overlap=claims[b].overlap=true;
	for(auto &c:claims)for(size_t j=0;j<tiles.size();j++)
		if(std::find(c.tiles.begin(),c.tiles.end(),j)==c.tiles.end())
			for(size_t i:c.tiles)if(std::abs(tiles[i].x-tiles[j].x)<8 && std::abs(tiles[i].y-tiles[j].y)<8)c.overlap=true;
	std::vector<bool> claimed(frame.tileInstances.size(),false);
	auto unresolved=[&](RemasterFrameTileInstance &a,RemasterOccurrenceGeometryStatus status) {
		a.geometryStatus=status;a.heightOffset=0;a.placementTileRole=RemasterTileRole::Unknown;
		// Capture contract forbids active occurrence normal/occlusion overrides
		// on unresolved geometry. Preserve array bytes, but explicitly deactivate
		// these layers; a subsequent profile/context application must restore flags.
		a.hasPlacementNormals=false;a.hasPlacementOcclusion=false;a.placementNormalIgnoreTileTransform=false;
	};
	for(const auto &c:claims) {
		RemasterSpriteObstacleDiagnostics::Assembly d;
		d.group=profile.obstacleGroups[c.group].name;d.x=c.x;d.y=c.y;d.transform=c.flip;d.members=c.tiles.size();
		bool conflict=c.overlap,unknown=false;
		// A sparse identity-only selection can match while repeated occurrences
		// underneath it were omitted. Never let an interior "contact" vote as a
		// floor edge. Completion belongs to explicit editor review, not runtime
		// identity flood-fill (which could merge two adjacent objects).
		bool incompleteBoundary=false;
		for(size_t k=0;k<c.tiles.size();k++)if(c.contacts[k]) {
			const auto &t=tiles[c.tiles[k]];
			for(const auto &other:tiles)if(other.x==t.x && other.y==t.y+((c.flip&2)?-8:8)) incompleteBoundary=true;
		}
		// USA Sprite_EE_MovableMantle -> MovableMantle_Draw (9A:FCB3).
		// Match the full six-entry producer signature and actor-relative origin,
		// not a nearby actor, slot number, hash or PPU priority. Future producer
		// families may provide their own exact footprints through this boundary.
		if(context && context->verifiedAlttpRom && context->hasObstacleActors && context->hasTerrainActors) {
			static const int dx[6]={32,16,0,32,16,0},dy[6]={16,16,16,0,0,0};
			static const uint8_t names[6]={0x2c,0x2e,0x2c,0x0c,0x0e,0x0c},flags[6]={0x71,0x31,0x31,0x71,0x31,0x31};
			for(int actor=0;actor<16;actor++) {
				const auto &a=context->obstacleActors[actor];
				if(a.type!=0xee || a.state!=9 || a.z || a.plane>1)continue;
				std::set<uint16_t> anchors{a.x};
				// EE draws first, then pushes right via Sprite_MoveXY with xVel2.
				// Invert exactly ONE fixed-point movement, not +/-pixel tolerance.
				// Final OAM must prove which pre/post integer anchor actually drew.
				if(a.hasMotion && a.xVelocity==2 && !a.yVelocity)
					anchors.insert(uint16_t(((uint32_t(a.x)<<8)+a.xSubpixel-32)>>8));
				for(uint16_t anchor:anchors) {
				const int sx=int16_t(uint16_t(anchor-context->actorScrollX)),sy=int16_t(uint16_t(a.y-context->actorScrollY));
				std::set<int> slots;bool exact=true;
				for(int part=0;part<6;part++) {
					int count=0,slot=-1;
					for(int j=0;j<128;j++){const auto &o=context->terrainOam[j];
						if(o.x==((sx+dx[part])&511) && o.y==uint8_t(sy+dy[part]) && o.name==names[part] && o.flags==flags[part] && o.size==2){count++;slot=j;}}
					if(count!=1)exact=false;else slots.insert(slot);
				}
				std::set<std::tuple<int,int,int>> children;
				for(size_t tile:c.tiles) {
					const auto &t=tiles[tile];
					if(t.x<sx || t.x>=sx+48 || t.y<sy || t.y>=sy+32 || (t.x-sx)%8 || (t.y-sy)%8)exact=false;
					for(size_t i:t.instances) {
						const auto &instance=frame.tileInstances[i];const int slot=instance.sourceIndex;
						if(!slots.count(slot)){exact=false;continue;}
						const auto &o=context->terrainOam[slot];
						const int xx=t.x-int(o.x),yy=t.y-int(o.y);
						const int expected=(o.name|((o.flags&1)<<8))+(instance.hFlip?(8-xx)/8:xx/8)+(instance.vFlip?(8-yy)/8:yy/8)*16;
						if((xx!=0 && xx!=8) || (yy!=0 && yy!=8) || instance.tileNumber!=expected || instance.hFlip!=bool(o.flags&64) || instance.vFlip!=bool(o.flags&128) || instance.palette!=((o.flags>>1)&7))exact=false;
						children.emplace(slot,xx,yy);
					}
				}
				if(children.size()!=24 || c.tiles.size()!=24)exact=false;
				if(exact) {
					if(d.producerActor>=0){conflict=true;d.producerPlane=-1;break;}
					d.producerActor=actor;d.producerPlane=a.plane;
				}
				}
			}
		}
		for(size_t tile:c.tiles)for(size_t i:tiles[tile].instances)
			if(!localSources[i])d.missingLocalInstances++;
		struct Support {int component=-1,base=255;bool valid=false,unknown=false,conflict=false;};
		Support planes[2];
		d.contacts=size_t(std::count(c.contacts.begin(),c.contacts.end(),true));
		const bool evidence=bool(support);
		for(int plane=0;plane<2;plane++) {
			auto &s=planes[plane];bool all=true;
			if(d.producerPlane>=0 && d.producerPlane!=plane)continue;
			for(size_t k=0;k<c.tiles.size();k++)if(c.contacts[k]) {
				if(!evidence){all=false;continue;}
				const auto &t=tiles[c.tiles[k]];bool witnessed=false;
				const int sy=t.y+((c.flip&2)?0:7),outside=sy+((c.flip&2)?-1:1);
				std::set<int> cells;
				for(int y:{sy,outside})for(int edgeX=0;edgeX<8;edgeX++) {
					const int x=t.x+edgeX;
					if(x<0 || y<0 || x>=int(frame.width) || y>=int(frame.height))continue;
					const int cell=(((context->backgroundScrollY+y)&511)/8)*64+((context->backgroundScrollX+x)&511)/8;
					if(!cells.insert(cell).second)continue;
					const auto &v=support->cells[plane*4096+cell];
					if(!v.observed || v.wall || v.component<0)continue;
					if(v.conflict){s.conflict=true;continue;}
					const int base=v.planeBase;
					if(base==255 || v.base==255){s.unknown=true;continue;}
					if(base!=v.base || (s.component>=0 && (s.component!=v.component || s.base!=base)))s.conflict=true;
					s.component=v.component;s.base=base;witnessed=true;
				}
				if(!witnessed)all=false;
			}
			s.valid=all && d.contacts && s.component>=0 && !s.unknown && !s.conflict;
			conflict|=s.conflict;unknown|=s.unknown;
		}
		if(planes[0].valid && planes[1].valid)conflict=true; // no implicit Link-plane choice
		d.supportPlaneMask=uint8_t(planes[0].valid | (planes[1].valid<<1));
		const int plane=planes[0].valid?0:planes[1].valid?1:-1;
		if(conflict)d.status=RemasterOccurrenceGeometryStatus::Conflict;
		else if(plane>=0 && !unknown && !incompleteBoundary) {
			d.status=RemasterOccurrenceGeometryStatus::Resolved;d.plane=plane;
			d.component=planes[plane].component;d.base=planes[plane].base;
		}
		if(d.status==RemasterOccurrenceGeometryStatus::Resolved)for(int z:c.z) {
			const int64_t offset=int64_t(d.base)+int64_t(z)*8;
			if(offset < -32767 || offset > 32767)d.status=RemasterOccurrenceGeometryStatus::Unresolved;
		}
		for(size_t k=0;k<c.tiles.size();k++)for(size_t i:tiles[c.tiles[k]].instances) {
			auto &a=frame.tileInstances[i];claimed[i]=true;d.instances++;
			a.geometryStatus=d.status;
			if(d.status==RemasterOccurrenceGeometryStatus::Resolved) {
				a.placementTileRole=RemasterTileRole::Obstacle;
				const int64_t offset=int64_t(d.base)+int64_t(c.z[k])*8;
				a.heightOffset=int32_t(offset); // whole-assembly range checked above
			} else unresolved(a,d.status);
			// Never replace placementHeight, asset height, normals or occlusion.
		}
		out.matchedAssemblies++;
		if(d.status==RemasterOccurrenceGeometryStatus::Resolved)out.resolvedAssemblies++;
		else if(d.status==RemasterOccurrenceGeometryStatus::Conflict)out.conflictingAssemblies++;
		else out.unresolvedAssemblies++;
		out.assemblies.push_back(std::move(d));
	}
	for(size_t i=0;i<seen.size();i++)if(seen[i]) {
		if(claimed[i])out.claimedInstances++;
		else {out.unclaimedInstances++;unresolved(frame.tileInstances[i],RemasterOccurrenceGeometryStatus::Unresolved);}
	}
	if(const char *path=std::getenv("S9X_REMASTER_SPRITE_OBSTACLE_TRACE")) {
		std::ofstream trace(path,std::ios::app);
		for(const auto &a:out.assemblies)trace<<a.group<<" xy="<<a.x<<','<<a.y<<" status="<<int(a.status)
			<<" actor="<<a.producerActor<<" plane="<<a.producerPlane<<" mask="<<int(a.supportPlaneMask)
			<<" base="<<a.base<<" observed="<<out.observedTiles<<" invalid="<<out.invalidInstances<<'\n';
		if(context) {trace<<"camera="<<context->actorScrollX<<','<<context->actorScrollY<<" bg="<<context->backgroundScrollX<<','<<context->backgroundScrollY;
			for(const auto &a:context->obstacleActors)if(a.type==0xee)trace<<" actorxy="<<a.x<<','<<a.y<<" sub="<<int(a.xSubpixel);
			for(int j=0;j<128;j++){const auto &o=context->terrainOam[j];if(o.name==0x0e)trace<<" oam="<<j<<':'<<o.x<<','<<int(o.y)<<','<<int(o.flags)<<','<<int(o.size);}
			trace<<'\n';}
	}
	if(diagnostics)*diagnostics=std::move(out);
}
#endif
