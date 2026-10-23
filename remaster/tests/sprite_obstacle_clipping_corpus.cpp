// Audit all available complete own-input gameplay captures, including unsupported denominators.
#include "../runtime/sprite_obstacle_replay.h"
#include <filesystem>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 std::cout<<"capture,supported,before_resolved_obj_pixels,after_resolved_obj_pixels,after_resolved_mask_fnv\n";
 for(const auto &entry:std::filesystem::directory_iterator(argv[1]))if(entry.path().extension()==".s9xrmf") {
  const auto path=entry.path().string();auto f=std::make_unique<RemasterFrame>();RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
  if(!S9xReadRemasterFrame(path,*f) || !S9xRemasterLoadProfile(path+".profile.toml",profile,errors)) {std::cout<<entry.path().filename()<<",0,0,0,0\n";continue;}
  auto count=[&]{size_t n=0;for(const auto &p:f->mainPixels)if(p.instanceId) {const auto &i=f->tileInstances[p.instanceId-1];n+=i.source==RemasterSourceType::Object && i.geometryStatus==RemasterOccurrenceGeometryStatus::Resolved;}return n;};
  const auto before=count();const bool supported=S9xRemasterRestoreSpriteObstacleSupport(path,*f,profile);
  uint64_t mask=UINT64_C(14695981039346656037);
  for(const auto &p:f->mainPixels) {
   bool resolved=false;if(p.instanceId){const auto &i=f->tileInstances[p.instanceId-1];resolved=i.source==RemasterSourceType::Object && i.geometryStatus==RemasterOccurrenceGeometryStatus::Resolved;}
   mask=(mask^uint8_t(resolved))*UINT64_C(1099511628211);
  }
  std::cout<<entry.path().filename()<<','<<supported<<','<<before<<','<<count()<<','<<mask<<'\n';
 }
}
