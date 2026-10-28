// Minimum is applied only to accepted absolute crests, never wall equations.
#define REMASTER_OOB_NO_MAIN 1
#include "dungeon_out_of_bounds_test.cpp"
#include "dungeon_geometry_cache_inventory.h"
int main() {
 if(!std::getenv("REMASTER_OOB_OWNER_ONLY"))for(int signedShift:{0,-40})for(int local:{0,5,-3})for(int order=0;order<6;order++) {
  auto storage=std::make_unique<std::array<std::array<C::Placement,4096>,2>>();
  const C::Placement *tiles[]={(*storage)[0].data(),(*storage)[1].data()};
  for(int i:{650,651,652}){auto &t=(*storage)[0][i];t.surface=oobSurface;t.hasHeight=true;t.height.fill(local+i-650);}
  std::array<int,3> heights={21+signedShift,23+signedShift,22+signedShift};
  for(int k=0;k<order;k++)std::next_permutation(heights.begin(),heights.end());
  auto result=S9xRemasterVisibleFace::outOfBoundsPlateaus(tiles,[&](int bg,int cell,int,int &h,int &o,int &p,int,int){
   if(bg || cell<586 || cell>588)return RemasterVisualClaimStatus::NotEvaluated;
   h=heights[cell-586];o=100+cell;p=0;return RemasterVisualClaimStatus::Solved;
  },[](int a,int,int b,int,int){return a==b;});
  for(int i:{650,651,652})need(result[i].status==RemasterVisualClaimStatus::Solved && result[i].base==21+signedShift-local-(i-650),"minimum absolute crest before per-tile local subtraction, signed and permutation independent");
 }
 // One signed wall graph, two independently observed floor contacts. Floor
 // identity alone must not veto an OOB crest; numeric and plane conflicts must.
 for(int mode=0;mode<3;mode++) {
  auto f=std::make_unique<PlateauFixture>(1,0,0,0);
  int cell=f->foot+1;auto &map=f->heights[1];
  map.occupiedFloorOwners[0][cell]=933;
  auto &d=const_cast<RemasterRoomDiagnostics&>(*map.roomDiagnostics).cells[cell];d.floorId=933;
  if(mode==1) {
   map.offsets[cell]=map.planeOffsets[0][cell]=31;d.floorBase=31;
   for(auto &inst:f->frame.tileInstances)if(inst.tileId.hash==f->tiles[1][cell].tileHash)inst.heightOffset=31;
  }
  if(mode==2) {
   f->attrs[4096+cell]=0;map.occupiedFloorOwners[1][cell]=933;map.planeOffsets[1][cell]=30;
   auto &peer=const_cast<RemasterRoomDiagnostics&>(*map.roomDiagnostics).cells[4096+cell];peer=d;
  }
  const auto inventory=dungeonMapInventory(map); // all owners/bases remain inputs
  f->run();auto at=f->at(f->far);const auto &inst=f->frame.tileInstances[f->frame.scenePixels[at].instanceId-1];
  need(inst.visualClaim.status==(mode==0?RemasterVisualClaimStatus::Solved:RemasterVisualClaimStatus::Conflict),"floor-ID-only veto removed for OOB, numeric wall equations/planes retained");
  need(dungeonMapInventory(map)==inventory,"minimum never mutates physical wall/floor/owner/walkability maps");
  if(!mode)need(inst.heightOffset==77,"consistent multi-owner signed graph preserves independent crest");
 }
 productionContacts();
 std::cout<<"PASS minimum signed/permutation/origin and production plane/owner contacts\n";
}
