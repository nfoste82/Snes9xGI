// Failure-class regression through the real production terminal callback.
#define REMASTER_OOB_NO_MAIN 1
#include "dungeon_out_of_bounds_test.cpp"
#include "dungeon_geometry_cache_inventory.h"
int main() {
 size_t cases=0;
 for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++)for(int plane=0;plane<2;plane++)
 for(int shift:{0,-100})for(int origin:{-1,0,5}) {
  auto f=std::make_unique<PlateauFixture>(axis,flip,bg,plane);
  if(origin>=0)f->local(origin);
  // Only interior tangent samples change, leaving every independent wall seam
  // and occupied floor anchor intact. Canonical uphill local5/7 is monotone in
  // the floorward axis, including all placement mirrors.
  auto &t=f->tiles[bg][f->first];
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)if((axis?x:y)>=2 && (axis?x:y)<=4)t.height[y*8+x]=std::min<int>(5,t.height[y*8+x]);
  for(auto &m:f->frame.assetMetadata)if(m.tileId.hash==t.tileHash)m.height=t.height;
  for(int cell:{f->foot,f->foot+(axis?1:64)}) {
   auto &map=f->heights[1-bg];map.offsets[cell]=map.planeOffsets[plane][cell]=30+shift;
   const_cast<RemasterRoomDiagnostics&>(*map.roomDiagnostics).cells[plane*4096+cell].floorBase=30+shift;
  }
  for(auto &i:f->frame.tileInstances)if(i.heightOffset==30)i.heightOffset+=shift;
  const auto a=dungeonMapInventory(f->heights[0]),b=dungeonMapInventory(f->heights[1]);
  const auto authored=f->frame.assetMetadata;
  RemasterVisibleFaceTemplateCache cache;f->run(&cache);
  bool edge=false;
  for(const auto &e:cache.diagnosticTerminals)if(e[0]==bg && e[1]==f->first) {
   need(e[7]==int(S9xRemasterVisibleFace::Status::Solved) && e[8]==70+shift,"independent numeric graph remains solved");
   need(e[19]==75+shift && e[20]==77+shift && e[21]==8,"all transformed full-band absolute crest samples retained");
   need(e[18]==int(RemasterVisualClaimStatus::Solved),"FAIL valid nonuniform uphill edge must be accepted, not poison component");edge=true;
  }
  need(edge,"actual terminal callback exercised");
  RemasterLightingFieldCache cpu;cpu.update(f->frame,true);
  for(auto [cell,id]:f->added)for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
   const auto at=f->at(cell)+y*f->frame.width+x;const auto px=f->frame.scenePixels[at];
   const auto &i=f->frame.tileInstances[px.instanceId-1];
   need(i.visualClaim.status==RemasterVisualClaimStatus::Solved && i.visualClaim.published,"every selected sheet sample solved");
   need(i.heightOffset==75+shift-(origin<0?0:origin),"minimum absolute before local origin subtraction");
   need(cpu.samples[at].known && std::abs(cpu.height[at*2]-(75+shift)/255.f)<1e-6,"every actual CPU height equals independent crest minimum");
  }
  need(a==dungeonMapInventory(f->heights[0]) && b==dungeonMapInventory(f->heights[1]),"all physical maps unchanged");
  need(authored.size()==f->frame.assetMetadata.size(),"no authored asset fabricated");
  for(size_t k=0;k<authored.size();k++)need(authored[k].height==f->frame.assetMetadata[k].height && authored[k].normalXyz==f->frame.assetMetadata[k].normalXyz && authored[k].occlusion==f->frame.assetMetadata[k].occlusion,"authored arrays unchanged");
  ++cases;
 }
 // Invalid/uncertain evidence must not become a minimum constraint. These run
 // the same production callback/selection, not a manufactured reducer result.
 for(int mode=0;mode<14;mode++) {
  auto f=std::make_unique<PlateauFixture>(1,0,0,0);
  auto &t=f->tiles[0][f->first];
  for(int x=2;x<=4;x++)t.height[x]=5;
  for(auto &m:f->frame.assetMetadata)if(m.tileId.hash==t.tileHash)m.height=t.height;
  if(mode==0)f->attrs.fill(1);
  if(mode==1)for(auto &a:f->heights[1].occupiedFloorOwners)a.fill(-1);
  if(mode==2)for(auto &a:f->heights[1].planeOffsets)a.fill(255);
  if(mode==3)for(auto &d:const_cast<RemasterRoomDiagnostics&>(*f->heights[1].roomDiagnostics).cells)d.planeObserved=false;
  if(mode==4)for(auto &d:const_cast<RemasterRoomDiagnostics&>(*f->heights[1].roomDiagnostics).cells)d.heightConflict=true;
  if(mode==5) { // True inconsistent signed numeric graph, not owner-ID conflict.
   const int cell=f->foot+1;auto &map=f->heights[1];map.offsets[cell]=map.planeOffsets[0][cell]=31;
   const_cast<RemasterRoomDiagnostics&>(*map.roomDiagnostics).cells[cell].floorBase=31;
   for(auto &i:f->frame.tileInstances)if(i.tileId.hash==f->tiles[1][cell].tileHash)i.heightOffset=31;
  }
  if(mode==6)f->local(0,true);
  if(mode==7)t.stairSourceAmbiguous=true;
  if(mode==8)f->maps[8192+f->first*2]^=1;
  if(mode==9) {auto at=f->at(f->first)+3;f->frame.mainPixels[at]=f->frame.scenePixels[at]={};}
  if(mode==10) {auto at=f->at(f->plateau)+7*f->frame.width+3;f->frame.mainPixels[at]=f->frame.scenePixels[at]={};}
  if(mode==11)for(auto &i:f->frame.tileInstances)if(i.tileId.hash==t.tileHash)i.tileNumber^=1;
  if(mode==12) {RemasterTileContext g;g.hasHeight=true;g.height.fill(19);f->frame.contextGeometry[f->added[0].second]=g;}
  if(mode==13) {
   t.surface=C::Surface::Door;t.doorAssembly=f->first-128+1;t.doorDirection=0;t.doorTemplateOffset=1;
   t.hasDoorframeMask=true;t.doorframeMask=UINT64_MAX^(UINT64_C(1)<<3);
  }
  f->run();RemasterLightingFieldCache cpu;cpu.update(f->frame,true);
  const auto px=f->frame.scenePixels[f->at(f->far)];
  need(!f->frame.tileInstances[px.instanceId-1].visualClaim.published && !cpu.samples[f->at(f->far)].known,"invalid/uncertain actual callback must not solve a sheet");
 }
 productionContacts();
 std::cout<<"PASS production nonuniform crests "<<cases<<" axis/flip/BG/plane/signed/origin cases\n";
}
