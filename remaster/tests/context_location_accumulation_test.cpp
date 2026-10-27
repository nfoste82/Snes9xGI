// Exact legacy oracle plus allocation-work regression (no production counters).
static unsigned publications=0;
#define REMASTER_TEST_CONTEXT_LOCATION_PUBLICATION() (++publications)
#include "../runtime/remaster.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=0;
static bool counting=false;
static size_t allocations=0;
void *operator new(std::size_t n) { if(counting)++allocations; if(auto p=std::malloc(n?n:1))return p; throw std::bad_alloc(); }
void operator delete(void *p) noexcept { std::free(p); }
void *operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void *p) noexcept { ::operator delete(p); }
// Frozen pre-repair implementation, independent of new scratch.
static void reference(RemasterFrame &frame,const std::array<int,4>&sx,const std::array<int,4>&sy) {
    frame.contextLocations.clear(); if(!frame.area.valid())return;
    std::set<uint32_t> conflict;
    for(const auto *pixels:{&frame.mainPixels,&frame.subPixels,&frame.scenePixels})
        for(size_t p=0;p<pixels->size();++p) {
            const auto &pixel=(*pixels)[p];
            if(!pixel.instanceId || pixel.instanceId>frame.tileInstances.size() || pixel.tilePixel>=64)continue;
            const auto &in=frame.tileInstances[pixel.instanceId-1];
            if(in.source!=RemasterSourceType::Background || in.sourceIndex>1)continue;
            int lx=pixel.tilePixel%8,ly=pixel.tilePixel/8;
            if(in.hFlip)lx=7-lx; if(in.vFlip)ly=7-ly;
            const int wx=sx[in.sourceIndex]+int(p%frame.width)-lx;
            const int wy=sy[in.sourceIndex]+int(p/frame.width)-ly;
            RemasterContextLocation at;at.area=frame.area.id;at.background=in.sourceIndex;
            const int mask=frame.area.id<320?511:65535;
            at.x=(wx&mask)/8;at.y=(wy&mask)/8;
            auto inserted=frame.contextLocations.emplace(pixel.instanceId,at);
            if(!inserted.second && (inserted.first->second<at || at<inserted.first->second))conflict.insert(pixel.instanceId);
        }
    for(auto id:conflict)frame.contextLocations.erase(id);
}
static void parity(RemasterFrame &f,const std::array<int,4>&sx,const std::array<int,4>&sy) {
    auto expected=std::make_unique<RemasterFrame>(f);
    reference(*expected,sx,sy);publications=0;S9xRemasterLocateContextTiles(f,sx,sy);
    assert(publications==f.contextLocations.size());
    assert(f.contextLocations.size()==expected->contextLocations.size());
    for(const auto &e:expected->contextLocations) {
        const auto &a=f.contextLocations.at(e.first); assert(!(a<e.second) && !(e.second<a));
    }
    RemasterProfile profile;
    for(const auto &i:f.tileInstances) {
        profile.tileContexts[i.tileId]["zero"].hasHeight=true;
        profile.tileContexts[i.tileId]["other"].hasNormals=true;
    }
    for(const auto &e:expected->contextLocations)profile.contextBindings[e.second]={f.tileInstances[e.first-1].tileId,"zero"};
    S9xRemasterApplyContextBindings(profile,f);S9xRemasterApplyContextBindings(profile,*expected);
    std::vector<uint8_t>a,b;
    assert(S9xSerializeRemasterFrame(f,a));assert(S9xSerializeRemasterFrame(*expected,b));assert(a==b);
}
int main() {
    auto f=std::make_unique<RemasterFrame>();f->width=16;f->height=8;
    f->mainPixels.resize(128);f->subPixels.resize(128);f->scenePixels.resize(128);
    f->originalRgb555.resize(128);f->sceneRgb555.resize(128);
    f->tileInstances.resize(6);
    for(unsigned i=0;i<6;++i) { auto &in=f->tileInstances[i];in.source=RemasterSourceType::Background;in.sourceIndex=i%2;in.tileId={123+i,1,4}; }
    f->tileInstances[4].sourceIndex=2;f->tileInstances[5].source=RemasterSourceType::Object;
    for(int area:{0,319,320,65535})for(int flip=0;flip<4;++flip) {
        f->area.id=area;
        for(auto &in:f->tileInstances){in.hFlip=flip&1;in.vFlip=flip&2;}
        for(unsigned p=0;p<128;++p) {
            auto &pixel=f->mainPixels[p];pixel.instanceId=p%16<8?1:2;
            pixel.tilePixel=((flip&2)?7-p/16:p/16)*8+((flip&1)?7-p%8:p%8);
        }
        f->subPixels=f->scenePixels=f->mainPixels;
        std::array<int,4>sx{{-9,65532,0,0}},sy{{511,-4,0,0}};
        parity(*f,sx,sy);
        assert(f->contextLocations.size()==(f->area.valid()?2u:0u));
        f->scenePixels.back()=f->mainPixels[7];parity(*f,sx,sy);
        if(f->area.valid())assert(!f->contextLocations.count(1));
        f->subPixels[0]=f->mainPixels[8];parity(*f,sx,sy);
        if(f->area.valid())assert(!f->contextLocations.count(2));
        f->mainPixels[0].instanceId=999;f->mainPixels[1].instanceId=0;
        f->mainPixels[2].tilePixel=255;f->mainPixels[3].instanceId=5;f->mainPixels[4].instanceId=6;
        parity(*f,sx,sy);
        sx[0]+=8;f->tileInstances[0].hFlip=!f->tileInstances[0].hFlip;parity(*f,sx,sy);
    }
    f->area={};parity(*f,{},{});assert(f->contextLocations.empty());
    f->area.id=2;f->mainPixels.clear();f->subPixels.clear();f->scenePixels.clear();
    S9xRemasterLocateContextTiles(*f,{},{});assert(f->contextLocations.empty());
    // Dense repeats in three buffers: 192 observations, one occurrence.
    f->width=f->height=8;f->tileInstances.resize(1);f->tileInstances[0].hFlip=f->tileInstances[0].vFlip=false;
    f->mainPixels.resize(64);
    for(unsigned p=0;p<64;++p){f->mainPixels[p].instanceId=1;f->mainPixels[p].tilePixel=p;}
    f->subPixels=f->scenePixels=f->mainPixels;
    allocations=0;publications=0;counting=true;S9xRemasterLocateContextTiles(*f,{},{});counting=false;
    std::cout<<"dense allocations="<<allocations<<" observations=192 occurrences=1"<<std::endl;
    std::cout<<"ordered map publication attempts="<<publications<<std::endl;
    assert(f->contextLocations.size()==1);assert(allocations<=4);assert(publications<=1);
    std::cout<<"PASS exact maps, serialized bindings, transforms, conflicts, invalid inputs and invocation mutations\n";
}
