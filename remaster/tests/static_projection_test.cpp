// Retained descriptor consumer versus the unchanged uncached production path.
#include "../runtime/frame.h"
#include <cassert>
#include <iostream>
static void equal(const RemasterFrame &a,const RemasterFrame &b) {
    assert(a.tileInstances.size()==b.tileInstances.size());
    for(size_t n=0;n<a.tileInstances.size();++n) {
        const auto &x=a.tileInstances[n],&y=b.tileInstances[n];
        assert(x.tileId==y.tileId && x.heightOffset==y.heightOffset && x.hasPlacementHeight==y.hasPlacementHeight && x.placementHeight==y.placementHeight);
    }
    auto pixels=[](const auto &x,const auto &y){assert(x.size()==y.size());for(size_t n=0;n<x.size();++n)assert(x[n].instanceId==y[n].instanceId && x[n].tilePixel==y[n].tilePixel && x[n].owner==y[n].owner);};
    pixels(a.mainPixels,b.mainPixels);pixels(a.subPixels,b.subPixels);
    for(int n=0;n<2;++n)pixels(a.observedBgPixels[n],b.observedBgPixels[n]);
}
int main() {
    for(int bg=0;bg<2;++bg)for(int flip=0;flip<4;++flip)for(int mode=0;mode<7;++mode) {
        auto map=std::make_unique<RemasterDungeonHeightMap>();map->roomIndex=7;map->hasRoomGeometry=mode!=6;
        map->offsets.fill(24);map->structuralPlaneOffsets[0].fill(24);map->structuralPlaneOffsets[1].fill(255);
        map->mixedFloorBases[0].fill(mode==1?32:255);
        if(mode==2 || mode==3){map->doorframeAttachments.push_back({0,0,40,UINT64_C(0x0f0f0f0f0f0f0f0f),1,true,24});if(mode==3)map->doorframeAttachments.push_back({0,1,48,UINT64_MAX,2,true,24});}
        if(mode==4){map->localHeightPreviews.push_back({0,{}});map->localHeightPreviews[0].height.fill(-8);map->producerHashes[0]=3;}
        if(mode==5){map->offsets[0]=255;map->fallbackDirections[1]=1;}
        RemasterDungeonFloorContext c;c.roomIndex=7;
        auto input=std::make_unique<RemasterFrame>();input->width=16;input->height=8;input->mainPixels.resize(128);input->subPixels.resize(128);input->observedBgPixels[bg].resize(128);
        RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=bg;i.hFlip=flip&1;i.vFlip=flip&2;i.tileId.hash=3;
        input->tileInstances.push_back(i);i.tileId.hash=4;i.assetGroup="obstacles";input->tileInstances.push_back(i);
        RemasterFrameAssetMetadata m;m.tileId=input->tileInstances[0].tileId;m.hasHeight=true;m.tileRole=mode==2||mode==3?RemasterTileRole::Doorway:RemasterTileRole::Floor;
        for(int n=0;n<64;++n)m.height[n]=n/8;input->assetMetadata.push_back(m);
        for(auto *p:{&input->mainPixels,&input->subPixels,&input->observedBgPixels[bg]})for(int n=0;n<128;++n){(*p)[n].instanceId=(n%13==0)?2:1;(*p)[n].tilePixel=((flip&2)?7-n/16:n/16)*8+((flip&1)?7-n%8:n%8);}
        auto cache=std::make_unique<RemasterDungeonProjectionCache>();
        uint64_t previous=0;
        for(int pass=0;pass<3;++pass) {
            auto a=std::make_unique<RemasterFrame>(*input),b=std::make_unique<RemasterFrame>(*input);
            RemasterDungeonHeightApplyDiagnostics x,y;
            S9xRemasterApplyDungeonHeightMap(*a,c,*map,nullptr,&x,bg);
            S9xRemasterApplyDungeonHeightMap(*b,c,*map,nullptr,&y,bg,cache.get());equal(*a,*b);
            assert(x.destinationPixels==y.destinationPixels && x.destinationUnknownPixels==y.destinationUnknownPixels && x.destinationRaisedPixels==y.destinationRaisedPixels);
            // Overlapping different identities intentionally replace the single
            // bounded cell slot; only unique producer variants promise hits.
            assert(cache->builds>=previous);previous=cache->builds;
            // Selected local geometry changed without a different tile identity.
            if(pass==1)input->assetMetadata[0].height.fill(0);
        }
        cache->invalidate();map->offsets[0]=-24;
        auto a=*input,b=*input;S9xRemasterApplyDungeonHeightMap(a,c,*map,nullptr,nullptr,bg);S9xRemasterApplyDungeonHeightMap(b,c,*map,nullptr,nullptr,bg,cache.get());equal(a,b);
    }
    std::cout<<"PASS static projection BG/HV/mixed/frames/conflicts/preview/unknown/legacy and selected zero\n";
}
