#ifndef REMASTER_OVERWORLD_ACTOR_SUPPORT_H
#define REMASTER_OVERWORLD_ACTOR_SUPPORT_H
#include "frame.h"

// Sample independently observed BG2 under Link, never the composited OBJ or a
// nearby wall. All reserved-block members share the grounded producer anchor.
inline bool S9xRemasterOverworldLinkSupport(const RemasterFrame &f,
	const RemasterDungeonFloorContext &c,float &height) {
	if(!c.verifiedAlttpRom || c.indoors || f.area.id<320 || f.area.id>321 || !f.width || !f.height)return false;
	const int wx=uint16_t(c.linkX+8),wy=uint16_t(c.linkY+20);
	const int x=int16_t(uint16_t(wx-c.backgroundScrollX)),y=int16_t(uint16_t(wy-c.backgroundScrollY));
	if(x<0 || y<0 || x>=int(f.width) || y>=int(f.height))return false;
	const size_t n=size_t(y)*f.width+x;
	for(unsigned plane:{1u,3u}) {
		if(n>=f.observedBgPixels[plane].size())continue;
		const auto &px=f.observedBgPixels[plane][n];
		if(!px.instanceId || px.instanceId>f.tileInstances.size() || px.tilePixel>=64)continue;
		const auto &i=f.tileInstances[px.instanceId-1];
		const auto at=f.contextLocations.find(px.instanceId);
		if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1 || !i.savedPlacementOffsetStatus ||
			at==f.contextLocations.end() || at->second.area!=f.area.id || at->second.background!=1 ||
			at->second.x!=wx/8 || at->second.y!=wy/8)continue;
		const auto *m=S9xRemasterFrameMetadataForTile(f,i.tileId);
		if(!m || S9xRemasterResolveInstanceTileRole(m->tileRole,i)!=RemasterTileRole::Floor || S9xRemasterInstanceGeometryUnresolved(i))return false;
		const auto g=S9xRemasterResolveFrameGeometry(*m,f,px.instanceId-1);
		height=float(i.heightOffset)+(g.hasHeight?g.height[px.tilePixel]:0);
		if(i.terrainOffset.hasHeight)height+=i.terrainOffset.height[S9xRemasterTerrainPixel(px.tilePixel,i.hFlip,i.vFlip)];
		return std::isfinite(height);
	}
	return false;
}

inline bool S9xRemasterApplyOverworldLinkSupport(RemasterFrame &f,
	const RemasterDungeonFloorContext &c,float *sampled=nullptr) {
	float base=0;if(!S9xRemasterOverworldLinkSupport(f,c,base))return false;
	int block=-1;
	for(const auto &i:f.tileInstances)if(i.source==RemasterSourceType::Object && i.vramAddress>=0x8000 && i.vramAddress<0x8400) {
		const int b=i.sourceIndex>=56 && i.sourceIndex<68?56:i.sourceIndex>=100 && i.sourceIndex<112?100:-1;
		if(b<0)continue;if(block>=0 && block!=b)return false;block=b;
	}
	if(block<0)return false;
	// Keep sub-unit support through the existing float terrain channel. It is a
	// translation only: never rotate Link or replace his authored local relief.
	const int whole=int(std::floor(base));const float fraction=base-whole;
	for(auto &i:f.tileInstances)if(i.source==RemasterSourceType::Object && i.sourceIndex>=block && i.sourceIndex<block+12) {
		i.heightOffset+=whole;
		if(fraction!=0){if(!i.terrainOffset.hasHeight)i.terrainOffset.height.fill(0);for(float &h:i.terrainOffset.height)h+=fraction;i.terrainOffset.hasHeight=true;}
	}
	if(sampled)*sampled=base;return true;
}
#endif
