#ifndef REMASTER_BENCHMARK_ROUTE_H
#define REMASTER_BENCHMARK_ROUTE_H

// Benchmark-only deterministic input script. No gameplay state writes: steps
// feed the normal joypad reporting path, indexed by measured emulated frame.
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <limits>

struct RemasterBenchmarkRouteStep { uint32_t frames=0, buttons=0; };
inline bool S9xRemasterParseBenchmarkRoute(const std::string &text,
    std::vector<RemasterBenchmarkRouteStep> &result)
{
    std::vector<RemasterBenchmarkRouteStep> parsed;
    std::istringstream input(text);std::string token;uint64_t total=0;
    if(text.empty() || text.back()==',')return false;
    while(std::getline(input,token,',')) {
        const auto colon=token.find(':');
        if(colon==std::string::npos || !colon || colon+1==token.size())return false;
        uint64_t frames=0;
        for(size_t i=0;i<colon;i++) {
            if(token[i]<'0' || token[i]>'9')return false;
            frames=frames*10+unsigned(token[i]-'0');if(frames>36000)return false;
        }
        if(!frames || (total+=frames)>36000)return false;
        uint32_t buttons=0;std::istringstream chord(token.substr(colon+1));std::string name;
        if(token.back()=='+')return false;
        while(std::getline(chord,name,'+')) {
            uint32_t flag=0;
            if(name=="none") {if(token.substr(colon+1)!="none")return false;}
            else if(name=="left")flag=0x0200;
            else if(name=="right")flag=0x0100;
            else if(name=="up")flag=0x0800;
            else if(name=="down")flag=0x0400;
            else if(name=="a")flag=0x0080;
            else if(name=="b")flag=0x8000;
            else if(name=="x")flag=0x0040;
            else if(name=="y")flag=0x4000;
            else return false;
            if(flag && (buttons&flag))return false;
            buttons|=flag;
        }
        if((buttons&0x0300)==0x0300 || (buttons&0x0c00)==0x0c00)return false;
        parsed.push_back({uint32_t(frames),buttons});
    }
    result=std::move(parsed);return !result.empty();
}
inline uint32_t S9xRemasterBenchmarkRouteButtons(
    const std::vector<RemasterBenchmarkRouteStep> &steps,uint32_t frame)
{
    for(const auto &step:steps){if(frame<step.frames)return step.buttons;frame-=step.frames;}
    return 0;
}
#endif
