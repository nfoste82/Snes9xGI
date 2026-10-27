// Actual controlled EndFrame adapter; independent BG2 scroll is unavailable.
#define main context_location_benchmark_main
#include "context_location_benchmark.cpp"
#undef main
int main(int argc,char **argv) {
    if(argc!=4)throw std::runtime_error("capture iterations output-prefix");
    setenv("S9X_REMASTER_LIGHTING_TRACE","1",1);
    ContextReplay replay(argv[1]);
    std::array<int,4> sx{{replay.context.backgroundScrollX,replay.context.backgroundScrollX,0,0}};
    std::array<int,4> sy{{replay.context.backgroundScrollY,replay.context.backgroundScrollY,0,0}};
    auto f=std::make_unique<RemasterFrame>();
    std::cout<<"iteration,ms,apply,room,visible,finalize,decisions,builds,hits,misses,visits\n";
    for(int n=0;n<std::stoi(argv[2]);++n) {
        auto a=std::chrono::steady_clock::now();replay.runLocated(*f,sx,sy);
        auto b=std::chrono::steady_clock::now();
        uint64_t builds=0,hits=0,misses=0,visits=0;
#ifndef ALTTP_STATIC_BASELINE
        for(auto &p:S9xRemasterState().dungeonProjection)if(p){builds+=p->builds;hits+=p->hits;misses+=p->misses;visits+=p->dynamicVisits;}
#endif
        const auto &t=f->lightingTrace;
        std::cout<<n<<','<<std::chrono::duration<double,std::milli>(b-a).count()<<','
          <<t.exclusive[RemasterLightingTrace::Apply]<<','<<t.exclusive[RemasterLightingTrace::Room]<<','
          <<t.exclusive[RemasterLightingTrace::Visible]<<','<<t.exclusive[RemasterLightingTrace::Finalize]<<','
          <<t.projectionDecisions<<','<<builds<<','<<hits<<','<<misses<<','<<visits<<'\n';
        if(!n && !S9xWriteRemasterFrame(*f,std::string(argv[3])+".s9xrmf"))throw std::runtime_error("packet");
    }
}
