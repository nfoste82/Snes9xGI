#ifndef REMASTER_PLACEMENT_OFFSET_DEBUG_H
#define REMASTER_PLACEMENT_OFFSET_DEBUG_H
#include "frame.h"

inline bool S9xRemasterPlacementCoordinateVerified(const RemasterFrame &f, uint32_t id) {
	if(!id || id>f.tileInstances.size() || f.area.id<320 || f.area.id>321) return false;
	const auto &i=f.tileInstances[id-1];
	const auto at=f.contextLocations.find(id);
	return i.source==RemasterSourceType::Background && i.sourceIndex==1 && (i.savedPlacementOffsetStatus>0 || i.savedPlacementCoordinateVerified) &&
		at!=f.contextLocations.end() && at->second.area==f.area.id && at->second.background==1 &&
		at->second.x<=8191 && at->second.y<=8191;
}
inline bool S9xRemasterSavedPlacementApplicable(const RemasterFrame &f, uint32_t id) {
	return S9xRemasterPlacementCoordinateVerified(f,id) && f.tileInstances[id-1].savedPlacementOffsetStatus>0;
}
// Click and offset-category highlight share this exact visible occurrence policy.
inline uint32_t S9xRemasterPlacementOffsetPick(const RemasterFrame &f,size_t n) {
	if(n>=f.mainPixels.size() || f.profileRomSha256!="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb")return 0;
	const auto owner=f.mainPixels[n].owner;
	if(RemasterSourceType(owner>>24)==RemasterSourceType::Background && ((owner>>16)&255)==2)return 0;
	const auto p=S9xRemasterSelectVisibleOccurrence(f,n,true,true);
	return (p.owner&0xffff0000u)==0x02010000u && S9xRemasterPlacementCoordinateVerified(f,p.instanceId)?p.instanceId:0;
}
// Latest added selected ID wins; on removal choose lowest remaining supported ID.
inline uint32_t S9xRemasterPlacementRepresentative(const RemasterFrame &f,const std::set<uint32_t> &ids,uint32_t preferred) {
	if(ids.count(preferred) && S9xRemasterPlacementCoordinateVerified(f,preferred))return preferred;
	for(auto id:ids)if(S9xRemasterPlacementCoordinateVerified(f,id))return id;
	return 0;
}
// Inspector actions only, not an off-mode per-frame scan. Visibility is defined
// by the same main/sub/scene truth used by click and highlight, not by membership
// in tileInstances (which also contains wholly hidden/wrapped occurrences).
inline void S9xRemasterRefreshPlacementSelection(const RemasterFrame &f,std::set<uint32_t> &ids,
	const std::set<RemasterTileContentId> *types=nullptr) {
	std::set<uint32_t> visible;
	for(size_t n=0;n<f.mainPixels.size();++n) {
		const auto id=S9xRemasterPlacementOffsetPick(f,n);
		if(id && (types?types->count(f.tileInstances[id-1].tileId):ids.count(id)))visible.insert(id);
	}
	ids.swap(visible);
}
inline uint32_t S9xRemasterPickPlacementOffset(const RemasterFrame &f,size_t n,bool extend,std::set<uint32_t> &ids,
	std::set<RemasterTileContentId> *types=nullptr) {
	const auto id=S9xRemasterPlacementOffsetPick(f,n);
	if(types) {
		if(!extend)types->clear();
		if(id){const auto tile=f.tileInstances[id-1].tileId;if(extend && types->count(tile))types->erase(tile);else types->insert(tile);}
		S9xRemasterRefreshPlacementSelection(f,ids,types);
		return S9xRemasterPlacementRepresentative(f,ids,id);
	}
	if(!extend)ids.clear();
	if(id){if(extend && ids.count(id))ids.erase(id);else ids.insert(id);}
	return S9xRemasterPlacementRepresentative(f,ids,id);
}
inline RemasterFrameAssetMetadata S9xRemasterPlacementLocalGeometry(const RemasterFrame &f,uint32_t id) {
	if(!S9xRemasterPlacementCoordinateVerified(f,id))return {};
	const auto *m=S9xRemasterFrameMetadataForTile(f,f.tileInstances[id-1].tileId);
	const auto &i=f.tileInstances[id-1];
	auto g=m?(m->geometryByFlip?*m:S9xRemasterResolveFacingGeometry(*m,i.geometryFacing)):RemasterFrameAssetMetadata{};
	if(m && m->geometryByFlip){const auto &v=m->flipGeometry[S9xRemasterGeometryFlipIndex(i.hFlip,i.vFlip)];
		g.height=v.height;g.hasHeight=v.hasHeight;g.heightSampling=v.heightSampling;}
	const auto context=f.contextGeometry.find(id);
	if(context!=f.contextGeometry.end()){g.height=context->second.height;g.hasHeight=context->second.hasHeight;g.heightSampling=context->second.heightSampling;}
	// Resolve the full local normal alternatives too; terrain stays separate.
	if(m){auto resolved=S9xRemasterResolveFrameGeometry(*m,f,id-1);g.normalXyz=resolved.normalXyz;g.hasNormals=resolved.hasNormals;g.normalIgnoreTileTransform=resolved.normalIgnoreTileTransform;}
	return g;
}

// Same observed visible occurrence policy as lighting, but NO metadata-channel
// fallback: a missing local height/normal must never elect another coordinate.
// BG3 HUD/OBJ are deliberately masked purple rather than pretending to be zero.
inline void S9xRemasterPlacementOffsetField(const RemasterFrame &f, bool useScene, std::vector<float> &out) {
	out.assign(f.mainPixels.size()*2,0);
	const bool alttp=f.profileRomSha256=="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
	if(!alttp) return;
	for(size_t n=0;n<f.mainPixels.size();++n) {
		const auto owner=f.mainPixels[n].owner;
		if(RemasterSourceType(owner>>24)==RemasterSourceType::Background && ((owner>>16)&255)==2)continue;
		const auto p=S9xRemasterSelectVisibleOccurrence(f,n,useScene,alttp);
		if(RemasterSourceType(p.owner>>24)!=RemasterSourceType::Background || ((p.owner>>16)&255)!=1 ||
			!S9xRemasterSavedPlacementApplicable(f,p.instanceId))continue;
		const auto &i=f.tileInstances[p.instanceId-1];
		out[n*2]=float(i.savedPlacementOffset);
		if(i.terrainOffset.hasHeight)out[n*2]+=i.terrainOffset.height[S9xRemasterTerrainPixel(p.tilePixel,i.hFlip,i.vFlip)];
		out[n*2+1]=float(i.savedPlacementOffsetStatus);
	}
}

inline std::string S9xRemasterSavedPlacementDescription(const RemasterFrame &f,uint32_t id) {
	if(!S9xRemasterSavedPlacementApplicable(f,id))return "Saved placement offset: not applicable / unavailable in this frame";
	const auto &at=f.contextLocations.at(id); const auto &i=f.tileInstances[id-1];
	std::ostringstream s; s<<"Saved placement: "<<(at.area==320?"Light":"Dark")<<" BG2 world tile ("<<at.x<<","<<at.y
		<<") offset "<<(i.savedPlacementOffset>=0?"+":"")<<i.savedPlacementOffset
		<<(i.savedPlacementOffsetStatus==2?" (explicit entry)":" (missing entry; default zero)")<<" [frame evidence; read only]";
	return s.str();
}
#endif
