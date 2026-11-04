#include "../runtime/benchmark_route.h"
#include <cassert>
#include <iostream>
int main() {
 std::vector<RemasterBenchmarkRouteStep> steps;
 assert(S9xRemasterParseBenchmarkRoute("2:right,1:a+up,3:none,2:left",steps));
 assert(S9xRemasterBenchmarkRouteButtons(steps,0)==0x100);
 assert(S9xRemasterBenchmarkRouteButtons(steps,1)==0x100);
 assert(S9xRemasterBenchmarkRouteButtons(steps,2)==0x880);
 assert(S9xRemasterBenchmarkRouteButtons(steps,3)==0);
 assert(S9xRemasterBenchmarkRouteButtons(steps,6)==0x200);
 assert(S9xRemasterBenchmarkRouteButtons(steps,8)==0);
 for(const char *bad:{"","0:left","-1:left","1:unknown","1:left+right","1:up+down",
    "1:a+a","1:none+a","1:right,","1:right+","1:right,,1:left","999999999999999999:right"}) {
  const auto before=steps.size();assert(!S9xRemasterParseBenchmarkRoute(bad,steps));assert(steps.size()==before);
 }
 std::cout<<"PASS benchmark route bounds, chords, normal joypad bits and frame boundaries\n";
}
