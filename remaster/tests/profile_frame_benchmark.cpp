// Captured-input CPU benchmark. EndFrame uses the explicitly limited existing
// lighting replay adapter; profile application uses the actual complete packet.
#include "../runtime/remaster.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include "../tools/lighting_replay_input.h"
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
int main(int argc,char **argv) {
    if(argc!=4) throw std::runtime_error("capture iterations output-prefix");
    LightingReplayInput replay(argv[1]);
    std::set<std::tuple<RemasterTileContentId,RemasterSourceType,uint8_t,uint8_t>> keys;
    for(const auto &i:replay.captured->tileInstances) keys.emplace(i.tileId,i.source,i.sourceIndex,i.palette);
    std::cerr<<"instances="<<replay.captured->tileInstances.size()<<" unique_match_contexts="<<keys.size()
        <<" rules="<<replay.profile.rules.size()<<'\n';
    auto f=std::make_unique<RemasterFrame>();
    std::cout<<"iteration,apply_ms,endframe_ms\n";
    for(int n=0;n<std::stoi(argv[2]);++n) {
        *f=*replay.captured;
        auto a=std::chrono::steady_clock::now();
        S9xRemasterApplyProfileToFrame(replay.profile,*f);
        auto b=std::chrono::steady_clock::now();
        if(!n && !S9xWriteRemasterFrame(*f,std::string(argv[3])+".apply.s9xrmf"))throw std::runtime_error("write apply");
        auto c=std::chrono::steady_clock::now();
        replay.run(*f);
        auto d=std::chrono::steady_clock::now();
        if(!n && !S9xWriteRemasterFrame(*f,std::string(argv[3])+".endframe.s9xrmf"))throw std::runtime_error("write endframe");
        std::cout<<n<<','<<std::chrono::duration<double,std::milli>(b-a).count()<<','<<std::chrono::duration<double,std::milli>(d-c).count()<<'\n';
    }
}
