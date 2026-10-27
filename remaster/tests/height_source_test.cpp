// Allocation/work regression plus full-source differential. Run in the frozen
// startup tree with HEIGHT_SOURCE_BASELINE to record the behavioral failure.
#include "../runtime/frame.h"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include "height_selection_reference.h"
static size_t allocatedBytes=0;
void *operator new(size_t n) { allocatedBytes+=n; if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p,size_t) noexcept { std::free(p); }
static void require(bool ok,const char *s){if(!ok)throw std::runtime_error(s);}
int main() {
    auto storage=std::make_unique<RemasterFrame>();auto &f=*storage;
    f.width=8;f.height=8;
    f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
    f.assetMetadata.resize(4);
    for(size_t a=0;a<4;++a) {
        auto &m=f.assetMetadata[a];m.tileId.hash=a+1;m.hasHeight=a!=0;
        m.height.fill(int(a)*3);m.geometryByFacing=true;m.geometryByFlip=a&1;
        for(int v=0;v<4;++v) {
            m.facingGeometry[v].hasHeight=v&1;m.facingGeometry[v].height.fill(20+v);
            m.flipGeometry[v].hasHeight=v&1;m.flipGeometry[v].height.fill(30+v);
        }
        // Unrelated dynamic strings/arrays must never be materialized by a
        // height-only consumer, even for thousands of equal asset occurrences.
        for(auto &s:m.materialSelectors)s=std::string(80,'m');
    }
    f.tileInstances.resize(1024);
    for(size_t n=0;n<f.tileInstances.size();++n) {
        auto &i=f.tileInstances[n];i.tileId.hash=n%5+1; // also missing asset
        i.source=n%7==0?RemasterSourceType::Object:RemasterSourceType::Background;
        i.sourceIndex=n%3;i.geometryFacing=n%6;i.hFlip=n&1;i.vFlip=n&2;
        i.geometryStatus=RemasterOccurrenceGeometryStatus(n%4);
        i.hasPlacementHeight=n%3==0;i.placementHeight.fill(n%2?0:45);
        if(n%5==0){auto &c=f.contextGeometry[n+1];c.hasHeight=n%2;c.height.fill(n%3?0:60);}
    }
    const auto before=allocatedBytes;
#ifdef HEIGHT_SOURCE_BASELINE
    RemasterFrameChannelResolver compact(f);
#else
    RemasterFrameHeightResolver compact(f);
#endif
    const size_t bytes=allocatedBytes-before;
    std::cout<<"instances="<<f.tileInstances.size()<<" resolver_allocated_bytes="<<bytes<<std::endl;
    require(bytes<f.tileInstances.size()*256,"height consumer materializes full metadata per instance");
#ifndef HEIGHT_SOURCE_BASELINE
    RemasterFrameChannelResolver original(f);
    for(size_t n=0;n<f.tileInstances.size();++n) {
        require(compact.effective[n].hasHeight==original.effective[n].hasHeight,"presence precedence");
        if(compact.effective[n].hasHeight)require(*compact.effective[n].height==original.effective[n].height,"local source precedence");
    }
    f.mainPixels.resize(64);f.subPixels.resize(64);f.scenePixels.resize(64);
    for(auto &v:f.observedBgPixels)v.resize(64);
    for(auto &v:f.observedBgDepth)v.resize(64);
    auto pixel=[&](size_t id,unsigned k) {RemasterFramePixel p;p.instanceId=id;p.tilePixel=k;
        const auto &i=f.tileInstances[id-1];p.owner=(uint32_t(i.source)<<24)|(uint32_t(i.sourceIndex)<<16);return p;};
    // Deterministic exhaustive cross-product of present/missing/zero sources,
    // both BGs, H/V, HUD, OBJ boundaries, observed depth ties and scene clones.
    size_t comparisons=0;uint32_t rng=123;
    for(int pass=0;pass<256;++pass) {
        auto next=[&](){rng=rng*1664525+1013904223;return rng;};
        for(size_t k=0;k<64;++k) {
            auto sample=[&](){unsigned r=next();return r%11?pixel(1+(r>>8)%1024,r%67):RemasterFramePixel{};};
            f.mainPixels[k]=sample();f.subPixels[k]=sample();f.scenePixels[k]=pass&1?sample():f.mainPixels[k];
            for(int b=0;b<4;++b){f.observedBgPixels[b][k]=sample();f.observedBgDepth[b][k]=next()%4;}
            for(bool scene:{false,true}) {
                const auto a=heightSelectionReference(f,k,scene,original.metadata),b=compact.selectHeight(f,k,scene);
                auto equal=[](const RemasterFramePixel &a,const RemasterFramePixel &b){return a.owner==b.owner&&a.instanceId==b.instanceId&&a.tilePixel==b.tilePixel;};
                require(equal(a.visible,b.visible)&&equal(a.donors[0],b.donors[0]),"selection differential");++comparisons;
            }
        }
    }
    std::cout<<"source_and_selection_comparisons="<<comparisons<<" PASS\n";
#endif
}
