#include "sprite_obstacle.h"
#include "lighting_field_cache.h"
#include <cassert>
#include <iostream>

namespace {
RemasterTileContentId id(uint64_t hash) {RemasterTileContentId v;v.hash=hash;v.hashVersion=1;v.bitDepth=4;return v;}
struct Fixture {
	RemasterFrame frame;
	RemasterProfile profile;
	RemasterDungeonFloorContext context;
	std::unique_ptr<RemasterDungeonHeightMap> map=std::make_unique<RemasterDungeonHeightMap>();
	std::shared_ptr<RemasterRoomDiagnostics> rooms=std::make_shared<RemasterRoomDiagnostics>();
	RemasterSpriteObstacleDiagnostics diagnostics;
	Fixture(int x=24,int y=24,int flip=0) {
		frame.width=128;frame.height=96;frame.mainPixels.resize(128*96);
		context.verifiedAlttpRom=true;context.indoors=true;context.roomIndex=0x51;
		context.linkPlane=1; // deliberately NOT support-plane evidence
		map->roomIndex=0x51;map->hasPlaneOffsets=true;map->roomDiagnostics=rooms;
		for(auto &p:map->planeOffsets)p.fill(255);
		RemasterObstacleGroup g;g.name="moving";g.spriteAssembly=true;
		// A repeated hash on opposite ends, and a noncontact head at z=1.
		g.members={{id(1),0,0,0,false,false,true},{id(2),1,-1,1,true,false,false},
			{id(1),2,0,-1,false,false,true}};
		profile.obstacleGroups.push_back(g);
		for(int n=1;n<=2;n++) {RemasterFrameAssetMetadata m;m.tileId=id(n);m.hasHeight=true;
			for(int p=0;p<64;p++)m.height[p]=int16_t(p-17);
			frame.assetMetadata.push_back(m);}
		int slot=116;
		for(const auto &m:g.members) {
			const int tx=x+((flip&1)?-m.x:m.x)*8,ty=y+((flip&2)?-m.y:m.y)*8;
			// Two records per tile model separate scanline draw contexts.
			for(int half=0;half<2;half++) {
				RemasterFrameTileInstance a;a.source=RemasterSourceType::Object;a.sourceIndex=slot;
				a.tileId=m.tileId;a.hFlip=m.hflip!=bool(flip&1);a.vFlip=m.vflip!=bool(flip&2);
				a.heightOffset=50; // wrong existing per-OAM wall/background fallback
				a.hasPlacementHeight=true;for(int p=0;p<64;p++)a.placementHeight[p]=int16_t(100-p);
				frame.tileInstances.push_back(a);
				for(int yy=half*4;yy<half*4+4;yy++)for(int xx=0;xx<8;xx++) {
					auto &px=frame.mainPixels[(ty+yy)*128+tx+xx];px.instanceId=uint32_t(frame.tileInstances.size());
					px.tilePixel=uint8_t((a.vFlip?7-yy:yy)*8+(a.hFlip?7-xx:xx));
				}
			}
			slot++;
		}
	}
	void support(int plane,int base,int component=7) {
		for(int cell=0;cell<4096;cell++) {
			auto &d=rooms->cells[plane*4096+cell];d.floorId=component;d.floorBase=base;
			d.planeObserved=true;map->planeOffsets[plane][cell]=base;
		}
	}
	void apply(){S9xRemasterApplySpriteObstacles(frame,profile,&context,map.get(),&diagnostics);}
	void resolved(int base) {
		apply();assert(diagnostics.resolvedAssemblies==1);assert(diagnostics.claimedInstances==6);
		assert(diagnostics.observedTiles==3);assert(diagnostics.assemblies[0].plane==0);
		for(size_t i=0;i<6;i++) {
			const auto &a=frame.tileInstances[i];const int z=profile.obstacleGroups[0].members[i/2].z;
			assert(a.heightOffset==base+z*8);assert(a.geometryStatus==RemasterOccurrenceGeometryStatus::Resolved);
			for(int p=0;p<64;p++)assert(a.placementHeight[p]==100-p);
		}
		for(const auto &m:frame.assetMetadata)for(int p=0;p<64;p++)assert(m.height[p]==p-17);
	}
};
}

int main(int argc,char **argv) {
	{Fixture f;f.support(0,50);
		// Independent scene draw records and stock records observe the same OAM
		// assembly. EndFrame swaps these planes; paused refresh does not.
		f.frame.scenePixels=f.frame.mainPixels;
		const size_t originalCount=f.frame.tileInstances.size();
		for(size_t i=0;i<originalCount;i++)f.frame.tileInstances.push_back(f.frame.tileInstances[i]);
		for(auto &p:f.frame.scenePixels)if(p.instanceId)p.instanceId+=uint32_t(originalCount);
		std::swap(f.frame.mainPixels,f.frame.scenePixels);f.apply();
		assert(f.diagnostics.claimedInstances==12 && f.diagnostics.resolvedAssemblies==1);
		const auto resolved=f.frame.tileInstances;
		std::swap(f.frame.mainPixels,f.frame.scenePixels);f.apply();
		assert(f.diagnostics.claimedInstances==12 && f.diagnostics.resolvedAssemblies==1);
		for(size_t i=0;i<resolved.size();i++) {
			assert(f.frame.tileInstances[i].heightOffset==resolved[i].heightOffset);
			assert(f.frame.tileInstances[i].geometryStatus==resolved[i].geometryStatus);
		}
	}
	for(bool conflict:{false,true}) {
		Fixture f;f.frame.originalRgb555.resize(f.frame.width*f.frame.height);
		f.frame.subPixels.resize(f.frame.width*f.frame.height);
		if(conflict){f.support(0,0);f.support(1,50);}
		for(auto &a:f.frame.tileInstances) {
			a.geometryStatus=RemasterOccurrenceGeometryStatus::Resolved;a.placementTileRole=RemasterTileRole::Obstacle;
			a.hasPlacementNormals=a.hasPlacementOcclusion=true;a.placementNormalIgnoreTileTransform=true;
			a.placementNormalXyz.fill(123);a.placementOcclusion.fill(234);
		}
		f.apply();
		for(const auto &a:f.frame.tileInstances) {
			assert(S9xRemasterValidOccurrenceGeometry(a));assert(a.placementTileRole==RemasterTileRole::Unknown);
			assert(!a.hasPlacementNormals && !a.hasPlacementOcclusion && !a.placementNormalIgnoreTileTransform);
			assert(a.placementNormalXyz[0]==123 && a.placementOcclusion[0]==234);
		}
		std::vector<uint8_t> encoded;assert(S9xSerializeRemasterFrame(f.frame,encoded));
		RemasterFrame decoded;assert(S9xDeserializeRemasterFrame(encoded,decoded));
		for(const auto &a:decoded.tileInstances)assert(S9xRemasterValidOccurrenceGeometry(a));
	}
	for(int base:{0,50,-8})for(int flip=0;flip<4;flip++)for(int x:{24,61}) {
		Fixture f(x,24,flip);f.support(0,base);f.resolved(base);
		// A second application cannot accumulate either floor or member z.
		f.resolved(base);
	}
	{Fixture f;f.support(0,0);f.support(1,50);f.apply();assert(f.diagnostics.conflictingAssemblies==1);
		for(const auto &a:f.frame.tileInstances)assert(a.geometryStatus==RemasterOccurrenceGeometryStatus::Conflict);}
	{Fixture f;f.support(0,0);f.support(1,0);f.apply();assert(f.diagnostics.conflictingAssemblies==1);}
	{Fixture f;f.support(0,50);const auto pixels=f.frame.mainPixels;
		for(size_t i=0;i<6;i++) {auto a=f.frame.tileInstances[i];a.sourceIndex+=8;f.frame.tileInstances.push_back(a);}
		for(size_t p=0;p<pixels.size();p++)if(pixels[p].instanceId) {
			auto px=pixels[p];px.instanceId+=6;f.frame.mainPixels[p+40]=px;
		}
		f.apply();assert(f.diagnostics.resolvedAssemblies==2);assert(f.diagnostics.claimedInstances==12);}
	{Fixture f;f.apply();assert(f.diagnostics.unresolvedAssemblies==1);}
	// Sparse motif contact with an omitted owned occurrence directly underneath:
	// even a valid single-plane map cannot turn this interior edge into support.
	{Fixture f;f.support(0,0);auto a=f.frame.tileInstances[0];a.sourceIndex=60;
		f.frame.tileInstances.push_back(a);
		for(int y=32;y<40;y++)for(int x=24;x<32;x++) {
			auto &p=f.frame.mainPixels[y*128+x];p.instanceId=7;p.tilePixel=(y-32)*8+x-24;
		}
		f.apply();assert(!f.diagnostics.resolvedAssemblies && f.diagnostics.unresolvedAssemblies==1);
		assert(f.diagnostics.unclaimedInstances==1);
	}
	{Fixture f;f.support(0,0);f.rooms->cells[3*64+3].heightConflict=true;f.apply();
		assert(f.diagnostics.conflictingAssemblies==1);}
	{Fixture f;f.support(0,0);f.map->planeOffsets[0][3*64+3]=255;f.apply();
		assert(f.diagnostics.unresolvedAssemblies==1);}
	{Fixture f;f.support(0,0);f.rooms->cells[3*64+5].floorId=8;f.apply();
		assert(f.diagnostics.conflictingAssemblies==1);}
	{Fixture f;f.support(0,0);f.map->roomDiagnostics.reset();f.apply();
		assert(f.diagnostics.unresolvedAssemblies==1);}
	// A wall artwork datum of50 cannot elevate a contact grounded at0 on the
	// immediately adjacent outside floor row. Heads do not vote on support.
	{Fixture f;f.support(0,0);for(int x=3;x<=5;x++) {
		auto &d=f.rooms->cells[3*64+x];d.wall=true;d.floorId=-1;d.floorBase=50;
		f.map->offsets[3*64+x]=50;f.map->planeOffsets[0][3*64+x]=50;
	} f.resolved(0);}
	// Full footprint required; partial metadata or one lost member is not rescued.
	{Fixture f;f.support(0,0);for(auto &p:f.frame.mainPixels)if(p.instanceId==3 || p.instanceId==4)p.instanceId=0;
		f.apply();assert(!f.diagnostics.resolvedAssemblies);assert(f.diagnostics.unclaimedInstances==4);}
	{Fixture f;f.support(0,0);f.frame.assetMetadata.clear();for(auto &a:f.frame.tileInstances)a.hasPlacementHeight=false;
		f.apply();assert(f.diagnostics.resolvedAssemblies==1);assert(f.diagnostics.assemblies[0].missingLocalInstances==6);}
	// Progressive authoring: support is shared, local sources are independent.
	// Normals-only members remain visible without inventing a height0 shell.
	for(int base:{0,50,-8})for(int flip=0;flip<4;flip++) {
		Fixture f(24,24,flip);f.support(0,base);
		for(auto &a:f.frame.tileInstances)a.hasPlacementHeight=false;
		f.frame.assetMetadata[0].hasHeight=false;
		for(auto &m:f.frame.assetMetadata){m.hasNormals=true;m.normalXyz.fill(128);}
		f.apply();assert(f.diagnostics.resolvedAssemblies==1);
		assert(f.diagnostics.assemblies[0].missingLocalInstances==4);
		RemasterLightingFieldCache cache;cache.update(f.frame,false);
		for(size_t p=0;p<f.frame.mainPixels.size();p++)if(f.frame.mainPixels[p].instanceId) {
			const auto i=f.frame.mainPixels[p].instanceId-1;
			assert(cache.authoredNormals[p]);assert(bool(cache.height[p*2+1])==(i/2==1));
		}
		// Paint/save/metadata sync reuses only this frame's attested support.
		f.frame.assetMetadata[0].hasHeight=true;f.frame.assetMetadata[0].height.fill(23);
		S9xRemasterApplySpriteObstacles(f.frame,f.profile,nullptr,nullptr,&f.diagnostics);
		assert(f.diagnostics.resolvedAssemblies==1 && !f.diagnostics.assemblies[0].missingLocalInstances);
		cache.update(f.frame,false);
		for(size_t p=0;p<f.frame.mainPixels.size();p++)if(f.frame.mainPixels[p].instanceId)assert(cache.height[p*2+1]);
		// Losing live support revokes the snapshot; replay cannot revive it.
		f.map->roomDiagnostics.reset();f.apply();assert(!f.frame.spriteObstacleSupport);
		S9xRemasterApplySpriteObstacles(f.frame,f.profile,nullptr,nullptr,&f.diagnostics);
		assert(f.diagnostics.unresolvedAssemblies==1);
	}
	// An unresolved gameplay plane does not invalidate local OBJ directions,
	// but must never expose absolute heights or shadow geometry at guessed0.
	{Fixture f;f.support(0,0);f.support(1,50);
		for(auto &m:f.frame.assetMetadata){m.hasNormals=true;m.normalXyz.fill(128);}
		RemasterTileContext local;local.hasHeight=local.hasNormals=true;local.height.fill(17);local.normalXyz.fill(128);
		f.frame.contextGeometry[1]=local;
		f.apply();assert(f.diagnostics.conflictingAssemblies==1);
		RemasterLightingFieldCache cache;cache.update(f.frame,false);
		for(size_t p=0;p<f.frame.mainPixels.size();p++)if(f.frame.mainPixels[p].instanceId) {
			assert(cache.authoredNormals[p]);assert(!cache.height[p*2+1]);assert(!cache.samples[p].known);
		}
	}
	// Variant-only local layers and exact named context selection are sources,
	// including after a previous unresolved invocation (no sticky suppression).
	for(int variant=0;variant<3;variant++) {
		Fixture f;f.support(0,0);
		for(auto &a:f.frame.tileInstances){a.hasPlacementHeight=false;a.geometryFacing=2;}
		for(auto &m:f.frame.assetMetadata) {
			m.hasHeight=false;
			if(variant==0){m.geometryByFlip=true;for(auto &g:m.flipGeometry){g.hasHeight=true;g.height.fill(9);}}
			if(variant==1){m.geometryByFacing=true;m.facingGeometry[1].hasHeight=true;m.facingGeometry[1].height.fill(11);}
		}
		if(variant==2)for(size_t i=0;i<f.frame.tileInstances.size();i++) {
			RemasterTileContext context;context.hasHeight=true;context.height.fill(13);f.frame.contextGeometry[uint32_t(i+1)]=context;
		}
		f.apply();assert(f.diagnostics.resolvedAssemblies==1);
		for(auto &a:f.frame.tileInstances)a.geometryStatus=RemasterOccurrenceGeometryStatus::Unresolved;
		f.apply();assert(f.diagnostics.resolvedAssemblies==1);
		for(size_t i=0;i<f.frame.tileInstances.size();i++) {
			const auto &a=f.frame.tileInstances[i];const auto *m=S9xRemasterFrameMetadataForTile(f.frame,a.tileId);
			const auto effective=S9xRemasterResolveFrameGeometry(*m,f.frame,i);
			assert(effective.hasHeight && effective.height[0]==(variant==0?9:variant==1?11:13));
		}
	}
	// A record spanning two origins is not allowed to claim either occurrence.
	{Fixture f;f.support(0,0);auto &p=f.frame.mainPixels[70*128+70];p.instanceId=1;p.tilePixel=0;
		f.apply();assert(f.diagnostics.invalidInstances==1);assert(!f.diagnostics.resolvedAssemblies);}
	// Conflicting footprints reject all members, not only the shared tail.
	{Fixture f;f.support(0,0);auto g=f.profile.obstacleGroups[0];g.name="ambiguous";g.members[1].z=2;
		f.profile.obstacleGroups.push_back(g);f.apply();assert(f.diagnostics.conflictingAssemblies==2);
		for(const auto &a:f.frame.tileInstances)assert(a.geometryStatus==RemasterOccurrenceGeometryStatus::Conflict);}
	// An overlapping OAM slot with identical art is not a scanline duplicate.
	{Fixture f;f.support(0,0);auto a=f.frame.tileInstances[0];a.sourceIndex=20;f.frame.tileInstances.push_back(a);
		f.frame.subPixels=f.frame.mainPixels;for(auto &p:f.frame.subPixels)if(p.instanceId==1)p.instanceId=7;
		f.apply();assert(!f.diagnostics.resolvedAssemblies);}
	// BG-only authoring is not consumed by the dynamic matcher.
	{Fixture f;f.profile.obstacleGroups[0].spriteAssembly=false;f.apply();
		assert(!f.diagnostics.observedTiles);assert(f.frame.tileInstances[0].heightOffset==50);}
	if(argc==2) {
		// Capture-derived footprint is test-only, never profile authorship. This
		// verifies actual OBJ record/origin handling, NOT room floor correctness.
		RemasterFrame frame;assert(S9xReadRemasterFrame(argv[1],frame));
		using K=std::tuple<int,int,RemasterTileContentId,bool,bool>;
		std::set<K> parts;size_t records=0;
		for(size_t p=0;p<frame.mainPixels.size();p++) {
			const auto &px=frame.mainPixels[p];if(!px.instanceId || px.instanceId>frame.tileInstances.size() || px.tilePixel>=64)continue;
			const auto &a=frame.tileInstances[px.instanceId-1];
			if(a.source!=RemasterSourceType::Object || a.sourceIndex<116 || a.sourceIndex>121)continue;
			int x=int(p%frame.width)-(a.hFlip?7-px.tilePixel%8:px.tilePixel%8);
			int y=int(p/frame.width)-(a.vFlip?7-px.tilePixel/8:px.tilePixel/8);
			parts.emplace(x,y,a.tileId,a.hFlip,a.vFlip);
		}
		assert(!parts.empty());const int ox=std::get<0>(*parts.begin()),oy=std::get<1>(*parts.begin());
		RemasterObstacleGroup group;group.name="capture_test_only";group.spriteAssembly=true;
		int bottom=-10000;for(const auto &k:parts)bottom=std::max(bottom,std::get<1>(k));
		for(const auto &k:parts) {
			assert((std::get<0>(k)-ox)%8==0 && (std::get<1>(k)-oy)%8==0);
			group.members.push_back({std::get<2>(k),(std::get<0>(k)-ox)/8,(std::get<1>(k)-oy)/8,0,
				std::get<3>(k),std::get<4>(k),std::get<1>(k)==bottom});
		}
		RemasterProfile profile;profile.obstacleGroups.push_back(group);
		for(const auto &a:frame.tileInstances)if(a.source==RemasterSourceType::Object && a.sourceIndex>=116 && a.sourceIndex<=121)records++;
		RemasterSpriteObstacleDiagnostics d;
		S9xRemasterApplySpriteObstacles(frame,profile,nullptr,nullptr,&d);
		assert(d.matchedAssemblies==1 && d.unresolvedAssemblies==1 && !d.resolvedAssemblies);
		for(const auto &a:frame.tileInstances)assert(S9xRemasterValidOccurrenceGeometry(a));
		std::vector<uint8_t> encoded;assert(S9xSerializeRemasterFrame(frame,encoded));
		RemasterFrame decoded;assert(S9xDeserializeRemasterFrame(encoded,decoded));
		std::cout<<"capture tiles="<<parts.size()<<" OAM records="<<records
			<<" claimed="<<d.claimedInstances<<" unresolved(no pinned support)="<<d.unresolvedAssemblies<<'\n';
	}
	std::cout<<"sprite obstacle regressions passed\n";
}
