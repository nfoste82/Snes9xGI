// Full ordered packet and CPU consumer parity, raw input on every invocation.
#define REMASTER_OOB_NO_MAIN
#include "dungeon_out_of_bounds_test.cpp"
static size_t allocations=0,bytesAllocated=0;
void *operator new(size_t n){++allocations;bytesAllocated+=n;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept{std::free(p);}
void operator delete(void *p,size_t) noexcept{std::free(p);}
int main(){
 for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++) {
  auto f=std::make_unique<PlateauFixture>(axis,flip,bg,0);
  auto cache=std::make_unique<RemasterVisibleFaceTemplateCache>();
  // Avoid duplicate synthetic metadata identities, retaining every distinct asset.
  std::set<RemasterTileContentId> ids;
  auto &meta=f->frame.assetMetadata;meta.erase(std::remove_if(meta.begin(),meta.end(),[&](const auto &m){return !ids.insert(m.tileId).second;}),meta.end());
  const auto raw=f->frame;
  auto compare=[&](const char *label){
   auto cached=std::make_unique<RemasterFrame>(f->frame),forced=std::make_unique<RemasterFrame>(f->frame);
   auto apply=[&](RemasterFrame &out,RemasterVisibleFaceTemplateCache *c){S9xRemasterApplyVisibleDungeonFaces(out,f->c,f->tiles[0].data(),f->tiles[1].data(),f->heights[0],f->heights[1],c);};
   const auto n=allocations,b=bytesAllocated;apply(*cached,cache.get());
   const auto used=allocations-n,bytes=bytesAllocated-b;apply(*forced,nullptr);
   std::vector<uint8_t> a,z;need(S9xSerializeRemasterFrame(*cached,a) && S9xSerializeRemasterFrame(*forced,z) && a==z,"cached/forced ordered packet parity");
   RemasterLightingFieldCache ca,cz;ca.update(*cached,true);cz.update(*forced,true);
   need(ca.height==cz.height && ca.authoredNormals==cz.authoredNormals && ca.surface==cz.surface,"cached/forced CPU channels parity");
   if(axis==0 && flip==0 && bg==0)std::cout<<label<<" hits="<<cache->preparationHits<<" misses="<<cache->preparationMisses<<" graphs="<<cache->graphBuilds<<" oobs="<<cache->oobBuilds<<" allocations="<<used<<" bytes="<<bytes<<'\n';
  };
  compare("cold");
  for(int handler:{0,8,16})for(int n=0;n<60;n++) {
   f->c.submodule=handler;f->c.constructionPhase=n&1;f->c.linkPlane=n&1;
   compare(n==59?"warm60":"warm");
  }
  need(cache->graphBuilds==1 && cache->oobBuilds==1 && cache->preparationHits==180,"actor plane alone cannot rebuild unchanged two-plane visual model");
  f->c.submodule=0;
  // Each mutation is followed by a forced fresh solve, then restored and checked.
  auto mutate=[&](const char *label,auto change,auto restore){auto before=cache->graphBuilds;change();compare(label);restore();compare("restore");need(cache->graphBuilds>before,"real graph dependency must invalidate");};
  mutate("collision",[&]{f->attrs.fill(1);},[&]{f->attrs.fill(0);});
  // Preserve exact initial collision values for subsequent isolated cases.
  auto t=f->tiles[bg][f->first];
  mutate("height",[&]{f->tiles[bg][f->first].height[0]++;},[&]{f->tiles[bg][f->first]=t;});
  mutate("role",[&]{f->tiles[bg][f->first].surface=C::Surface::Unknown;},[&]{f->tiles[bg][f->first]=t;});
  mutate("graphics_identity",[&]{f->tiles[bg][f->first].tileHash++;},[&]{f->tiles[bg][f->first]=t;});
  mutate("flips",[&]{f->tiles[bg][f->first].hflip=!t.hflip;},[&]{f->tiles[bg][f->first]=t;});
  mutate("door_mask",[&]{f->tiles[bg][f->first].doorframeMask^=1;},[&]{f->tiles[bg][f->first]=t;});
  mutate("assembly",[&]{f->tiles[bg][f->first].doorAssembly++;},[&]{f->tiles[bg][f->first]=t;});
  mutate("ambiguous",[&]{f->tiles[bg][f->first].stairSourceAmbiguous=true;},[&]{f->tiles[bg][f->first]=t;});
  const auto word=f->maps[0];mutate("map_word",[&]{f->maps[0]^=1;},[&]{f->maps[0]=word;});
  auto owner=f->heights[bg].occupiedFloorOwners[0][f->foot];mutate("owner",[&]{f->heights[bg].occupiedFloorOwners[0][f->foot]++;},[&]{f->heights[bg].occupiedFloorOwners[0][f->foot]=owner;});
  auto room=f->c.roomIndex;f->c.roomIndex++;compare("wrong_room_rejected");f->c.roomIndex=room;
  for(int mode=0;mode<8;mode++) {
   f->frame=raw;
   for(auto [cell,id]:f->added) {
    auto &i=f->frame.tileInstances[id-1];
    if(mode==0)i.heightOffset++;
    if(mode==1)i.hasPlacementHeight=true;
    if(mode==2){auto &g=f->frame.contextGeometry[id];g.hasHeight=true;g.height.fill(0);}
    if(mode==3)i.tileId.hash++;
    if(mode==4)i.geometryStatus=RemasterOccurrenceGeometryStatus::Unresolved;
   }
   if(mode==5)for(auto &p:f->frame.scenePixels)p={};
   if(mode==6)for(auto &m:f->frame.assetMetadata){m.hasNormals=true;m.normalXyz.fill(42);}
   if(mode==7)f->local(0); // valid authored zero, not absent
   compare("source_mutation");
  }
 }
 std::cout<<"PASS 16 transforms/BGs, 2880 static frames, exact forced packet/CPU parity and mutation matrix; bounded_cache_bytes="<<sizeof(RemasterVisibleFaceSolutionCache)<<'\n';
}
