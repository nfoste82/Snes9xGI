// Independent expected door-mask values: pixel-dependent branches stay fresh.
#include "../runtime/frame.h"
// The capture corpus probe supplies the full consumer differential. This fixture
// independently stresses the pixel-dependent branch which must never be memoized.
#include <cassert>
int main() {
    for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++)for(int conflict=0;conflict<2;conflict++) {
        auto m=std::make_unique<RemasterDungeonHeightMap>();m->roomIndex=7;m->hasRoomGeometry=true;
        m->offsets.fill(24);
        m->doorframeAttachments.push_back({0,0,40,UINT64_C(0x0f0f0f0f0f0f0f0f),1,true});
        m->doorframeAttachments[0].openingBase=24;
        if(conflict){m->doorframeAttachments.push_back(m->doorframeAttachments[0]);m->doorframeAttachments.back().base=48;}
        RemasterDungeonFloorContext c;c.roomIndex=7;
        RemasterFrame f;f.width=16;f.height=8;f.mainPixels.resize(128);f.subPixels.resize(128);
        RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=bg;i.hFlip=flip&1;i.vFlip=flip&2;
        f.tileInstances.push_back(i);
        RemasterFrameAssetMetadata a;a.hasHeight=true;a.tileRole=RemasterTileRole::Doorway;
        for(int y=0;y<8;y++)for(int x=0;x<8;x++)a.height[y*8+x]=y;
        f.assetMetadata.push_back(a);
        for(int n=0;n<128;n++){f.mainPixels[n].instanceId=1;f.mainPixels[n].tilePixel=((flip&2)?7-n/16:n/16)*8+((flip&1)?7-n%8:n%8);}
        S9xRemasterApplyDungeonHeightMap(f,c,*m,nullptr,nullptr,bg);
        for(int n=0;n<8*16;n++) {
            auto &p=f.mainPixels[n];auto &inst=f.tileInstances[p.instanceId-1];
            const bool masked=(m->doorframeAttachments[0].mask>>p.tilePixel)&1;
            if(n%16<8)assert(inst.heightOffset==(conflict && masked?0:masked?40:24-a.height[p.tilePixel]));
            else assert(inst.heightOffset==0); // no attachment: Doorway is not ordinary wall
        }
        assert(f.assetMetadata[0].height==a.height);
    }
}
