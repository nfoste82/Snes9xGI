// Actual own051 negative/positive source denominator; no invented movement trace.
#define main captureFixtureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
#include "lighting_field_cache.h"
#include "sphere_samples.h"
int main(int argc,char **argv){try {
 check(argc==3,"usage: own051 output-prefix");check(captureFixtureMain(argc,argv)==0,"actual production input replay");
 auto original=std::make_unique<RemasterFrame>(),production=std::make_unique<RemasterFrame>();
 check(S9xReadRemasterFrame(argv[1],*original),"actual packet");
 check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*production),"production packet");
 check(original->schemaVersion==30 && original->scenePixels.empty(),"actual051 schema/ordinary scene pin");
 check(original->lights.size()==20 && production->lights.size()==20,"actual051 offscreen source denominator");
 for(size_t n=0;n<original->lights.size();n++) {
  auto &a=original->lights[n],&b=production->lights[n];
  check(a.x==b.x && a.y==b.y && a.z==b.z && a.radius==b.radius && a.intensity==b.intensity && a.red==b.red && a.green==b.green && a.blue==b.blue,"actual producer source height/color differs");
 }
 RemasterLightingFieldCache fields;fields.update(*original,false);fields.update(*production,false);
 RemasterLightingFieldCache full;full.update(*production,false,true);
 check(fields.height==full.height && fields.emission==full.emission && fields.participation==full.participation && fields.surface==full.surface,"actual cached fields differ");
 size_t visible=0,receivers=0;uint32_t sphereSamples=0;
 for(size_t n=0;n<fields.participation.size()/2;n++) {
  visible+=fields.participation[n*2]>127 && fields.emission[n*4+3] && (fields.emission[n*4] || fields.emission[n*4+1] || fields.emission[n*4+2]);
  receivers+=fields.participation[n*2]>127 && fields.height[n*2+1]>0;
 }
 for(auto &l:production->lights)sphereSamples+=S9xRemasterSphereSamples(l.x,l.y,l.radius,production->width,production->height);
 check(!visible && receivers>40000 && sphereSamples>0,"actual defect zero visible emitters with valid receivers/spheres");
 std::cout<<"PASS actual051: visible_emitters="<<visible<<" physical_sources="<<production->lights.size()<<" adaptive_sphere_samples="<<sphereSamples<<" known_receivers="<<receivers<<" exact captured/production source heights and incremental/full fields\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
