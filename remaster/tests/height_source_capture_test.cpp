// Build a second binary with HEIGHT_REPLAY_HEADER pointing at the frozen startup
// context adapter. Compare every packet and complete map inventory externally.
#define main context_location_benchmark_main
#ifndef HEIGHT_REPLAY_HEADER
#define HEIGHT_REPLAY_HEADER "context_location_benchmark.cpp"
#endif
#include HEIGHT_REPLAY_HEADER
#undef main
#include "dungeon_geometry_cache_inventory.h"
int main(int argc,char **argv) {
    if(argc!=3)throw std::runtime_error("capture output-prefix");
    ContextReplay replay(argv[1]);auto &s=S9xRemasterState();
    auto f=std::make_unique<RemasterFrame>();
    std::array<int,4> sx{{replay.context.backgroundScrollX,replay.context.backgroundScrollX,0,0}};
    std::array<int,4> sy{{replay.context.backgroundScrollY,replay.context.backgroundScrollY,0,0}};
    int count=0;
    auto save=[&](const std::string &label) {
        replay.runLocated(*f,sx,sy);std::string path=std::string(argv[2])+"-"+std::to_string(count++)+"-"+label;
        if(!S9xWriteRemasterFrame(*f,path+".s9xrmf"))throw std::runtime_error("packet");
        std::ofstream out(path+".inventory");out<<dungeonMapInventory(s.runtimeDungeonHeightMap)<<dungeonMapInventory(s.runtimeDungeonBg1HeightMap);
        std::cout<<label<<" instances="<<f->tileInstances.size()<<"\n";
    };
    save("cold");save("warm");
    auto p=s.activeProfile;S9xRemasterSetProfile(p);save("profile");
    replay.context.linkPlane^=1;save("plane");replay.context.linkPlane^=1;
    for(int offset:{16392,16392+4096}){replay.input[offset]^=1;save("collision");replay.input[offset]^=1;}
    replay.input[9]^=0x20;save("priority");replay.input[9]^=0x20;
    replay.input[9]^=0x44;save("palette-flip");replay.input[9]^=0x44;
    replay.templates[8]^=1;save("template");replay.templates[8]^=1;
    const int tile=(replay.input[8]|replay.input[9]<<8)&1023;
    const auto address=uint16_t(replay.context.graphicsBase+tile*32);
    for(int n=0;n<4;++n){replay.input[24584+address]^=1;save("alternating-referenced-graphics");}
    // A graphics byte outside all referenced BG tile ranges.
    std::array<bool,65536> referenced={};
    for(int cell=0;cell<8192;++cell) {
        int word=replay.input[8+cell*2]|replay.input[9+cell*2]<<8;
        for(int k=0;k<32;++k)referenced[uint16_t(replay.context.graphicsBase+(word&1023)*32+k)]=true;
    }
    for(int at=0;at<65536;++at)if(!referenced[at]){replay.input[24584+at]^=1;save("unreferenced-graphics");replay.input[24584+at]^=1;break;}
    p.upperFloorHeight+=1;S9xRemasterSetProfile(p);save("rise");
    replay.context.terrainActors[0].x+=1;save("actor");
    for(int n=0;n<4;++n){sx[0]+=8;sx[1]+=8;save("scroll");}
    S9xRemasterInvalidateDungeonContinuity();save("reset");save("reset-warm");
    replay.context.roomIndex^=1;save("room");replay.context.roomIndex^=1;
    replay.context.submodule=2;save("transition");replay.context.submodule=0;save("settled");
    std::cout<<"PASS states="<<count<<" controlled-scroll, not full live parity\n";
}
