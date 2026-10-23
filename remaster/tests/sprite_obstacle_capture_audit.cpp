// Read-only exact packet/support/local-layer audit, without synthetic metadata.
#include "../runtime/sprite_obstacle_replay.h"
#include "../runtime/lighting_field_cache.h"
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
int main(int argc,char **argv) {
 if(argc!=2 && argc!=3)return 2;
 auto f=std::make_unique<RemasterFrame>();RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
 const std::string path=argv[1];
 if(!S9xReadRemasterFrame(argc==3?argv[2]:path,*f) || !S9xRemasterLoadProfile(path+".profile.toml",profile,errors))return 1;
 auto audit=[&](const char *stage) {
  std::set<RemasterTileContentId> ids;for(const auto &g:profile.obstacleGroups)if(g.spriteAssembly)for(const auto &m:g.members)ids.insert(m.tileId);
  std::map<RemasterTileContentId,RemasterFrameAssetMetadata> metadata;for(const auto &m:f->assetMetadata)metadata[m.tileId]=m;
  std::cout<<stage<<'\n';
  for(const auto *buffer:{&f->mainPixels,&f->subPixels,&f->scenePixels}) {
   size_t ink=0,resolved=0,height=0,normals=0;std::set<std::pair<int,int>> tiles;std::set<int> bases;
   for(size_t p=0;p<buffer->size();p++) {const auto &px=(*buffer)[p];if(!px.instanceId)continue;const auto &a=f->tileInstances.at(px.instanceId-1);
    if(a.source!=RemasterSourceType::Object || !ids.count(a.tileId))continue;
    auto g=S9xRemasterResolveFrameGeometry(metadata[a.tileId],*f,px.instanceId-1);
    ink++;resolved+=a.geometryStatus==RemasterOccurrenceGeometryStatus::Resolved;height+=g.hasHeight;normals+=g.hasNormals;bases.insert(a.heightOffset);
    tiles.emplace(int(p%f->width)-(a.hFlip?7-px.tilePixel%8:px.tilePixel%8),int(p/f->width)-(a.vFlip?7-px.tilePixel/8:px.tilePixel/8));
   }
   std::cout<<(buffer==&f->mainPixels?"main":buffer==&f->subPixels?"sub":"scene")<<" ink="<<ink<<" tiles="<<tiles.size()<<" resolved="<<resolved<<" height="<<height<<" normals="<<normals<<" bases=";for(int b:bases)std::cout<<b<<',';std::cout<<'\n';
  }
 };
 audit("captured");if(argc==3)return 0;
 if(!S9xRemasterRestoreSpriteObstacleSupport(path,*f,profile))return 1;audit("reapplied");
 RemasterSpriteObstacleDiagnostics d;S9xRemasterApplySpriteObstacles(*f,profile,nullptr,nullptr,&d);
 for(const auto &a:d.assemblies)std::cout<<a.group<<" xy="<<a.x<<','<<a.y<<" actor="<<a.producerActor<<" members="<<a.members<<" status="<<int(a.status)<<" base="<<a.base<<'\n';
}
