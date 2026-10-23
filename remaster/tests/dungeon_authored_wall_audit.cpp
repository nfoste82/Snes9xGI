// Production field audit: canonical metadata is not solved-placement coverage.
#include "../runtime/lighting_field_cache.h"
#include <iostream>
int main(int argc,char **argv) {
 if(argc<2)return 2;
 bool regression=false,mainOnly=false,receiverRegression=false;
 for(int a=2;a<argc;a++) {
  if(std::string(argv[a])=="--require-authored-wall-fields")regression=true;
  else if(std::string(argv[a])=="--require-visible-wall-receivers")receiverRegression=true;
  else if(std::string(argv[a])=="--main-plane")mainOnly=true;
  else return 2;
 }
 auto f=std::make_unique<RemasterFrame>();
 if(!S9xReadRemasterFrame(argv[1],*f)){std::cerr<<"Rejected packet: "<<argv[1]<<'\n';return 2;}
  const bool scene=!mainOnly && f->scenePixels.size()==f->mainPixels.size() && f->sceneRgb555.size()==f->originalRgb555.size();
 const auto &pixels=scene?f->scenePixels:f->mainPixels;
 RemasterLightingFieldCache fields;fields.update(*f,scene,true);
 std::map<std::string,size_t> counts;size_t walls=0,failures=0;
 size_t replacedWalls=0,replacedMissingHeight=0,replacedMissingNormals=0,replacedMissingOcclusion=0;
 size_t missingHeight=0,missingWithInstance=0,missingHud=0;
 for(size_t k=0;k<pixels.size();k++)if(!fields.height[k*2+1]) {
  missingHeight++;missingWithInstance+=pixels[k].instanceId!=0;missingHud+=S9xRemasterHeldLampHudPixel(*f,k);
  std::ostringstream key;key<<"missing_height owner="<<int(pixels[k].owner>>24)<<":"<<((pixels[k].owner>>16)&255)
   <<" instance="<<(pixels[k].instanceId!=0)<<" hud="<<S9xRemasterHeldLampHudPixel(*f,k);
  counts[key.str()]++;
 }
 if(scene)for(size_t k=0;k<f->mainPixels.size();k++) {
  const auto &main=f->mainPixels[k];
  const auto &p=!main.instanceId && uint8_t(main.owner>>24)==uint8_t(RemasterSourceType::Backdrop)?f->subPixels[k]:main;
  if(!p.instanceId || p.tilePixel>=64 || S9xRemasterHeldLampHudPixel(*f,k))continue;
  const auto &i=f->tileInstances.at(p.instanceId-1);
  const auto *m=S9xRemasterFrameMetadataForTile(*f,i.tileId);
   if(i.source!=RemasterSourceType::Background || i.sourceIndex>1 || !m)continue;
   const auto effective=S9xRemasterResolveFrameGeometry(*m,*f,p.instanceId-1);
   if(!effective.hasHeight || !effective.hasNormals ||
    (effective.tileRole!=RemasterTileRole::Wall && effective.tileRole!=RemasterTileRole::WallCorner && i.assetGroup!="wall_faces"))continue;
  const auto &s=f->scenePixels[k];
  if(s.instanceId==p.instanceId && s.tilePixel==p.tilePixel)continue;
  replacedWalls++;replacedMissingHeight+=!fields.height[k*2+1];
  replacedMissingNormals+=!fields.authoredNormals[k];replacedMissingOcclusion+=!fields.occlusion[k*2+1];
  std::ostringstream key;key<<"main_wall_replaced bg="<<int(i.sourceIndex)<<" hash="<<std::hex<<i.tileId.hash<<std::dec
   <<" scene_owner="<<int(s.owner>>24)<<":"<<((s.owner>>16)&255)<<" scene_instance="<<(s.instanceId!=0)
   <<" field="<<(fields.height[k*2+1]!=0)<<int(fields.authoredNormals[k])<<(fields.occlusion[k*2+1]!=0);
  counts[key.str()]++;
 }
 for(size_t k=0;k<pixels.size();k++) {
  const auto &main=pixels[k];
  const auto &p=!scene && !main.instanceId && uint8_t(main.owner>>24)==uint8_t(RemasterSourceType::Backdrop)?f->subPixels[k]:main;
  if(!p.instanceId || p.tilePixel>=64)continue;
  const auto &i=f->tileInstances.at(p.instanceId-1);if(i.source!=RemasterSourceType::Background || i.sourceIndex>1)continue;
  const auto *m=S9xRemasterFrameMetadataForTile(*f,i.tileId);if(!m)continue;
  const auto r=S9xRemasterResolveFrameGeometry(*m,*f,p.instanceId-1);
   // Inventory effective sources (context/placement/flip), not hash-only keys.
   const bool wall=r.tileRole==RemasterTileRole::Wall || r.tileRole==RemasterTileRole::WallCorner || i.assetGroup=="wall_faces";
   if(wall && r.hasHeight && r.hasNormals && !S9xRemasterHeldLampHudPixel(*f,k)) {
   walls++;
   if(!r.hasHeight || !r.hasNormals || !fields.height[k*2+1] || !fields.authoredNormals[k] || !fields.occlusion[k*2+1])failures++;
  }
  std::ostringstream key;key<<"bg="<<int(i.sourceIndex)<<" hash="<<std::hex<<i.tileId.hash<<std::dec
   <<" status="<<int(i.geometryStatus)<<" authored="<<m->hasHeight<<m->hasNormals<<m->hasOcclusion
   <<" effective="<<r.hasHeight<<r.hasNormals<<r.hasOcclusion<<" field="<<(fields.height[k*2+1]!=0)
   <<int(fields.authoredNormals[k])<<(fields.occlusion[k*2+1]!=0)<<" base="<<i.heightOffset
   <<" placement="<<i.hasPlacementHeight<<" context="<<f->contextGeometry.count(p.instanceId)<<" group="<<i.assetGroup;
  counts[key.str()]++;
 }
 for(const auto &v:counts)std::cout<<v.first<<" pixels="<<v.second<<'\n';
 std::cout<<"authored_wall_pixels="<<walls<<" suppressed_wall_pixels="<<failures<<" scene="<<scene<<'\n';
 std::cout<<"replaced_main_wall_pixels="<<replacedWalls<<" replacement_missing_height="<<replacedMissingHeight
  <<" replacement_missing_normals="<<replacedMissingNormals<<" replacement_missing_occlusion="<<replacedMissingOcclusion<<'\n';
 std::cout<<"missing_height_pixels="<<missingHeight<<" missing_with_instance="<<missingWithInstance<<" missing_hud="<<missingHud<<'\n';
 return (regression && (!walls || failures)) || (receiverRegression &&
  (replacedMissingHeight || replacedMissingNormals || replacedMissingOcclusion)) ? 1 : 0;
}
