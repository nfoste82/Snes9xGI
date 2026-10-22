#include "remaster.h"
#include <cassert>
#include <iostream>

int main()
{
	for(int rise:{-19,0,37,50,83})for(int flip=0;flip<4;flip++)for(int mode=0;mode<3;mode++) {
		auto map=std::make_unique<RemasterDungeonHeightMap>();
		map->roomIndex=7;map->offsets.fill(255);
		const uint64_t mask=UINT64_C(0x0f0f0f0f0f0f0f0f);
		map->doorframeAttachments.push_back({0,0,rise+8,mask,0x2716,true,mode==2?255:rise});
		map->doorframeAttachments.push_back({0,1,-90,mask,0x2716,mode==1,-90});
		RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
		RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
		RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;instance.sourceIndex=1;
		instance.hFlip=flip&1;instance.vFlip=flip&2;instance.heightOffset=3;f.tileInstances.push_back(instance);
		RemasterFrameAssetMetadata a;a.tileRole=RemasterTileRole::Door;a.hasHeight=a.hasDoorframeMask=true;a.doorframeMask=mask;
		for(int k=0;k<64;k++)a.height[k]=(mask>>k&1)?7-k/8:0;
		f.assetMetadata.push_back(a);
		for(auto *pixels:{&f.mainPixels,&f.subPixels})for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
			auto &p=(*pixels)[y*8+x];p.instanceId=1;p.tilePixel=(instance.vFlip?7-y:y)*8+(instance.hFlip?7-x:x);
		}
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto *pixels:{&f.mainPixels,&f.subPixels})for(const auto &p:*pixels) {
			const auto &placed=f.tileInstances[p.instanceId-1];
			const bool marked=mask&(UINT64_C(1)<<p.tilePixel);
			const int offset=mode==1?0:marked?rise+8:mode==2?0:rise;
			assert(!placed.hasPlacementHeight);
			assert(placed.heightOffset+a.height[p.tilePixel]==3+a.height[p.tilePixel]+offset);
		}
		assert(f.assetMetadata[0].height==a.height);
	}
	std::cout<<"PASS doorway landing heights, signed bases, H/V, both screens, hidden/conflicting planes and missing landing\n";
}
