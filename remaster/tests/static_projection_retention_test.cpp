#define main context_location_benchmark_main
#include "context_location_benchmark.cpp"
#undef main
#include "dungeon_geometry_cache_inventory.h"
static void require(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static std::vector<uint8_t> packet(const RemasterFrame &f) {
    const std::string path="build/static-projection-test-packet.s9xrmf";
    require(S9xWriteRemasterFrame(f,path),"write");std::ifstream in(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(in),{}};
}
int main(int argc,char **argv) {
    require(argc==2,"capture required");ContextReplay replay(argv[1]);
    std::array<int,4>sx{{replay.context.backgroundScrollX,replay.context.backgroundScrollX,0,0}};
    std::array<int,4>sy{{replay.context.backgroundScrollY,replay.context.backgroundScrollY,0,0}};
    auto a=std::make_unique<RemasterFrame>(),b=std::make_unique<RemasterFrame>();
    replay.runLocated(*a,sx,sy);replay.runLocated(*b,sx,sy);
#ifdef ALTTP_STATIC_BASELINE
    require(false,"baseline has no persistent room-cell descriptors");
#else
    auto &s=S9xRemasterState();
    auto builds=[&]{uint64_t n=0;for(auto &p:s.dungeonProjection)if(p)n+=p->builds;return n;};
    const auto count=builds();require(count>0,"production retained builds");
    for(int n=0;n<4;n++){replay.runLocated(*b,sx,sy);require(builds()==count,"warm ZERO rebuilds");require(packet(*a)==packet(*b),"warm exact bytes");}
    // Controlled scroll reprojects retained room cells; actors remain dynamic.
    sx[0]+=8;sx[1]+=8;replay.runLocated(*b,sx,sy);
    require(builds()>=count,"scroll accounting");
    // Force the uncached production path without changing any source/geometry.
    auto cached=packet(*b);
    for(auto &p:s.dungeonProjection)if(p)p->invalidate();
    replay.runLocated(*b,sx,sy);require(cached==packet(*b),"scroll cold parity");
    auto profile=s.activeProfile;S9xRemasterSetProfile(profile);
    replay.runLocated(*b,sx,sy);require(cached==packet(*b),"equal profile publication parity");
    const auto after=builds();require(after>count,"publication rebuilds");
    S9xRemasterInvalidateDungeonContinuity();replay.runLocated(*b,sx,sy);
    require(cached==packet(*b),"session reset parity");require(builds()>after,"reset rebuilds");
    auto parity=[&](const char *label){
        replay.runLocated(*a,sx,sy);const auto bytes=packet(*a);
        const auto maps=dungeonMapInventory(s.runtimeDungeonHeightMap)+dungeonMapInventory(s.runtimeDungeonBg1HeightMap);
        // Uncached production application is selected by transition-ineligible
        // diagnostic switch in the separate reference binary; here cold rebuild
        // and warm reuse must agree, including complete map inventories.
        for(auto &p:s.dungeonProjection)if(p)p->invalidate();
        replay.runLocated(*b,sx,sy);require(bytes==packet(*b),label);
        require(maps==dungeonMapInventory(s.runtimeDungeonHeightMap)+dungeonMapInventory(s.runtimeDungeonBg1HeightMap),"map inventory");
    };
    replay.context.linkPlane^=1;parity("plane");replay.context.linkPlane^=1;
    replay.input[16392]^=1;parity("collision plane0");replay.input[16392]^=1;
    replay.input[16392+4096]^=1;parity("collision plane1");replay.input[16392+4096]^=1;
    replay.input[8+1]^=0x20;parity("priority word");replay.input[8+1]^=0x20;
    replay.input[8+1]^=0x44;parity("palette flip word");replay.input[8+1]^=0x44;
    const int tile=(replay.input[8]|replay.input[9]<<8)&1023;
    const auto address=uint16_t(replay.context.graphicsBase+tile*32);
    replay.input[24584+address]^=1;parity("referenced graphics");replay.input[24584+address]^=1;
    replay.templates[8]^=1;parity("template");replay.templates[8]^=1;
    profile.upperFloorHeight+=1;S9xRemasterSetProfile(profile);parity("rise publication");
    replay.context.terrainActors[0].x+=1;parity("actor motion");
    std::cout<<"PASS retained="<<count<<" warm rebuilds=0 publication/reset parity\n";
#endif
}
