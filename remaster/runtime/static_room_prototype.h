#ifndef S9X_STATIC_ROOM_PROTOTYPE_H
#define S9X_STATIC_ROOM_PROTOTYPE_H
// Isolated, synchronous prototype: one scene for both 512x512 BG planes.
// Dynamic actor/sprite processing and emission remain outside this cache.
struct RemasterStaticRoomScene {
	uint16_t room=0xffff;
	uint64_t profile=0,rom=0;
	bool valid=false;
	std::array<uint8_t,16384> words;
	std::array<uint8_t,8192> collision;
	RemasterFrame geometry;
	struct Local {std::array<int16_t,64> height;std::array<uint8_t,192> normals;RemasterTileRole role=RemasterTileRole::Unknown;bool hasHeight=false,hasNormals=false,ignoreTransform=false;};
	std::vector<Local> local;
	RemasterVisibleFaceTemplateCache faces;
	bool prepare(const RemasterDungeonFloorContext &c,const RemasterProfile &p,uint64_t generation,
		const RemasterDungeonHeightMap &m0,const RemasterDungeonHeightMap &m1,
		const RemasterDungeonFloorContext::Placement *t0,const RemasterDungeonFloorContext::Placement *t1) {
		if(!S9xRemasterStaticRoomEnabled() || !c.verifiedAlttpRom || !c.indoors || c.module!=7 || c.submodule ||
			!c.constructedMaps || !c.collisionAttributes || !m0.roomDiagnostics || !m1.roomDiagnostics)return false;
		if(valid && room==c.roomIndex && profile==generation && rom==c.romIdentity &&
			!std::memcmp(words.data(),c.constructedMaps,words.size()) &&
			!std::memcmp(collision.data(),c.collisionAttributes,collision.size()))return true;
		const double start=RemasterLightingTrace::now();
		geometry={};auto &f=geometry;f.width=512;f.height=1024;f.area.id=c.roomIndex;
		f.mainPixels.resize(512*1024);f.subPixels.resize(f.mainPixels.size());
		const RemasterDungeonFloorContext::Placement *tiles[]={t0,t1};
		for(int bg=0;bg<2;bg++)for(int cell=0;cell<4096;cell++) {
			const auto &t=tiles[bg][cell];int at=(bg?0:8192)+cell*2,word=c.constructedMaps[at]|c.constructedMaps[at+1]<<8;
			RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=bg;
			i.tileId={t.tileHash,1,4};i.tileNumber=word&1023;i.palette=(word>>10)&7;i.hFlip=t.hflip;i.vFlip=t.vflip;
			f.tileInstances.push_back(i);const uint32_t id=uint32_t(f.tileInstances.size());
			for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
				auto &px=f.mainPixels[(bg*512+cell/64*8+y)*512+cell%64*8+x];px.instanceId=id;
				px.tilePixel=(t.vflip?7-y:y)*8+(t.hflip?7-x:x);px.owner=0x02000000|(bg<<16);
			}
		}
		S9xRemasterApplyProfileToFrame(p,f);
		auto context=c;context.backgroundScrollX=context.backgroundScrollY=0;
		S9xRemasterApplyDungeonHeightMap(f,context,m0,nullptr,nullptr,0);
		S9xRemasterApplyDungeonHeightMap(f,context,m1,nullptr,nullptr,1);
		f.scenePixels=f.mainPixels;
		S9xRemasterApplyVisibleDungeonFaces(f,context,t0,t1,m0,m1,&faces,generation);
		local.clear();local.resize(f.tileInstances.size());
		std::map<RemasterTileContentId,const RemasterFrameAssetMetadata*> metadata;
		for(const auto &m:f.assetMetadata)metadata[m.tileId]=&m;
		for(size_t n=0;n<f.tileInstances.size();n++) {
			auto m=metadata.find(f.tileInstances[n].tileId);if(m==metadata.end())continue;
			const auto g=S9xRemasterResolveFrameGeometry(*m->second,f,n);auto &l=local[n];
			l.height=g.height;l.normals=g.normalXyz;l.role=g.tileRole;l.hasHeight=g.hasHeight;l.hasNormals=g.hasNormals;l.ignoreTransform=g.normalIgnoreTileTransform;
		}
		std::copy_n(c.constructedMaps,words.size(),words.begin());std::copy_n(c.collisionAttributes,collision.size(),collision.begin());
		room=c.roomIndex;rom=c.romIdentity;profile=generation;valid=true;
		std::fprintf(stderr,"STATIC_ROOM scene room=%u build_ms=%.3f instances=%zu\n",unsigned(room),(RemasterLightingTrace::now()-start)*1000,f.tileInstances.size());
		return true;
	}
	void publish(RemasterFrame &f,const RemasterDungeonFloorContext &c) {
		std::unordered_map<uint64_t,uint32_t> clones;
		const size_t original=f.tileInstances.size();
		auto raster=[&](std::vector<RemasterFramePixel> &pixels) {
			for(size_t n=0;n<pixels.size();n++) {
				auto &px=pixels[n];if(!px.instanceId || px.instanceId>original || px.tilePixel>=64)continue;
				const auto &in=f.tileInstances[px.instanceId-1];if(in.source!=RemasterSourceType::Background || in.sourceIndex>1)continue;
				// Movable obstacle overrides are actor-owned and remain per frame.
				if(in.hasPlacementHeight || in.hasPlacementNormals || f.contextGeometry.count(px.instanceId))continue;
				int x=(c.backgroundScrollX+n%f.width)&511,y=(c.backgroundScrollY+n/f.width)&511;
				const auto &sp=geometry.scenePixels[(in.sourceIndex*512+y)*512+x];
				if(!sp.instanceId)continue;const auto &s=geometry.tileInstances[sp.instanceId-1];
				if(s.tileNumber!=in.tileNumber || s.hFlip!=in.hFlip || s.vFlip!=in.vFlip || sp.tilePixel!=px.tilePixel)continue;
				uint64_t key=(uint64_t(px.instanceId)<<32)|sp.instanceId;auto found=clones.find(key);
				if(found==clones.end()) {
					const auto &l=local[sp.instanceId-1];
					auto copy=in;copy.shiftPlacementId=sp.instanceId;copy.heightOffset=s.heightOffset;copy.placementHeight=l.height;copy.hasPlacementHeight=l.hasHeight;
					copy.geometryStatus=s.geometryStatus;copy.visualClaim=s.visualClaim;copy.placementTileRole=s.placementTileRole;
					if(copy.geometryStatus==RemasterOccurrenceGeometryStatus::Inherit) {copy.geometryStatus=RemasterOccurrenceGeometryStatus::Resolved;copy.placementTileRole=l.role;}
					copy.placementNormalXyz=l.normals;copy.hasPlacementNormals=l.hasNormals;
					copy.placementNormalIgnoreTileTransform=l.ignoreTransform;
					f.tileInstances.push_back(copy);found=clones.emplace(key,uint32_t(f.tileInstances.size())).first;
				}
				px.instanceId=found->second;
			}
		};
		raster(f.scenePixels);raster(f.mainPixels);raster(f.subPixels);for(auto &r:f.observedBgPixels)raster(r);
	}
};
#endif
