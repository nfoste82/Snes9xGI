#include "../runtime/lighting_trace.h"
#include <cassert>
#include <sstream>
#include <thread>
int main() {
    using namespace RemasterLightingTrace;
    Frame f; f.id=nextId();f.begin=now();
    { Scope a(&f,EndFrame); {Scope b(&f,Room);{Scope c(&f,Solve);}}
      {Scope b(&f,SlotWait);} }
    assert(f.calls[EndFrame]==1 && f.calls[Room]==1 && f.calls[Solve]==1);
    assert(activeScope()==nullptr);
    double total=0;for(double t:f.exclusive){assert(t>=0);total+=t;}
    assert(total<=(now()-f.begin)*1000);
    assert(cpuMs(f)==total-f.exclusive[SlotWait]);
    Frame g;g.id=nextId();assert(g.id!=f.id);
    std::thread worker([f]() mutable {assert(!activeScope()); Scope s(&f,Setup);});worker.join();
    std::ostringstream out;write(out,f,20,20.002,19.990,19.999,1,2,.1,256,224,64,8192,0,4);
    assert(out.str().find("\"gpu_ms\":2")!=std::string::npos);
    assert(out.str().find("\"wall_to_gpu_end_ms\":12")!=std::string::npos);
    Frame off;{Scope no(&off,EndFrame);assert(!activeScope());}assert(cpuMs(off)==0);
}
