// Compile the actual production renderer, discarding unrelated frontend symbols.
#define main lightingCriticalProbeMain
#include "../tools/lighting_critical_path_probe.mm"
#undef main
int main() { @autoreleasepool { try {
    metalDevice=MTLCreateSystemDefaultDevice();
    if(!metalDevice)throw std::runtime_error("Metal device");
    slotRegression();return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;} } }
