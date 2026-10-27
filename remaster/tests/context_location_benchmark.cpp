// Captured pixels/area; controlled BG scroll (same captured hint for both BGs).
// Not a full live replay: native sidecars do not pin independent BG2 scroll.
#include "../runtime/remaster.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include "../tools/lighting_replay_input.h"
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=0;
struct ContextReplay : LightingReplayInput {
    using LightingReplayInput::LightingReplayInput;
    void runLocated(RemasterFrame &f,const std::array<int,4>&sx,const std::array<int,4>&sy) {
        auto &s=S9xRemasterState();const auto &p=*captured;
        S9xRemasterBeginFrame(p.width*p.height,p.width,p.width,p.height);
        s.tileInstances=p.tileInstances;for(auto &i:s.tileInstances){i.heightOffset=0;i.hasPlacementHeight=false;}
        for(const auto &a:p.assets){auto &t=s.observedTiles[a.tileId.hash];t.hash=a.tileId.hash;t.bitDepth=a.tileId.bitDepth;std::copy_n(a.indices,64,t.indices);}
        for(size_t i=0;i<p.mainPixels.size();++i) {
            s.mainOwners[i]=p.mainPixels[i].owner;s.subOwners[i]=p.subPixels[i].owner;
            s.mainInstanceIds[i]=p.mainPixels[i].instanceId;s.subInstanceIds[i]=p.subPixels[i].instanceId;
            s.mainTilePixels[i]=p.mainPixels[i].tilePixel;s.subTilePixels[i]=p.subPixels[i].tilePixel;
        }
        s.scenePixels=p.scenePixels;s.sceneRgb=p.sceneRgb555;
        S9xRemasterEndFrame(p.originalRgb555.data(),p.width,p.width,p.height,&f,&context,nullptr,&p.area,0,nullptr,&sx,&sy);
    }
};
int main(int argc,char **argv) {
    if(argc!=5)throw std::runtime_error("capture iterations stage|endframe output-prefix");
    ContextReplay replay(argv[1]);
    std::array<int,4>sx{{replay.context.backgroundScrollX,replay.context.backgroundScrollX,0,0}};
    std::array<int,4>sy{{replay.context.backgroundScrollY,replay.context.backgroundScrollY,0,0}};
    auto f=std::make_unique<RemasterFrame>(*replay.captured);
    bool stage=std::string(argv[3])=="stage";
    // Cold sample starts with no location map; hot samples include clearing the
    // prior map. All scratch initialization/publication/destruction is timed.
    f->contextLocations.clear();
    std::cout<<"iteration,ms,locations\n";
    for(int n=0;n<std::stoi(argv[2]);++n) {
        const auto a=std::chrono::steady_clock::now();
        if(stage)S9xRemasterLocateContextTiles(*f,sx,sy);else replay.runLocated(*f,sx,sy);
        const auto b=std::chrono::steady_clock::now();
        std::cout<<n<<','<<std::chrono::duration<double,std::milli>(b-a).count()<<','<<f->contextLocations.size()<<'\n';
        if(!n && !S9xWriteRemasterFrame(*f,std::string(argv[4])+".s9xrmf"))throw std::runtime_error("write packet");
    }
    std::cerr<<"captured_area="<<replay.captured->area.id<<" controlled_scroll="<<sx[0]<<','<<sy[0]
        <<" instances="<<f->tileInstances.size()<<" full-live-parity=false\n";
}
