// Actual runtime consumer, not a synthetic ownership-only solver witness.
#define REMASTER_WALL_BAND_DIAGNOSTICS 1
#define main legacyVisibleFaceMain
#include "dungeon_visible_face_runtime_test.cpp"
#undef main

int main() {
 { // A visual owner conflict must not erase independently available channels.
  auto f=std::make_unique<Fixture>(1,0,0,0);
  for(int cell:{f->foot,f->foot+1}) {
   f->attrs[4096+cell]=0;f->heights[1].occupiedFloorOwners[1][cell]=555;
   f->heights[1].planeOffsets[1][cell]=30;
   auto &d=const_cast<RemasterRoomDiagnostics&>(*f->heights[1].roomDiagnostics).cells[4096+cell];
   d.planeObserved=true;d.floorId=555;d.floorBase=30;
  }
  for(auto &m:f->frame.assetMetadata){m.hasNormals=m.hasOcclusion=m.hasEmission=true;}
  f->frame.originalRgb555.resize(f->frame.mainPixels.size());
  RemasterLightingFieldCache before;before.update(f->frame,true);
  f->run();RemasterLightingFieldCache after;after.update(f->frame,true);
  need(before.height==after.height && before.surface==after.surface && before.occlusion==after.occlusion && before.emission==after.emission,
   "visual conflict preserves numeric fallback and valid authored zero channels");
  const int at=(f->first/64-10)*8*f->frame.width+(f->first%64-10)*8;
  need(S9xRemasterChannelProvenance(f->frame,at).find("visual Conflict")!=std::string::npos,
   "actual selected consumer must expose visual Conflict separately from known physical height");
  const auto &p=after.channelSources[at].donors[0];
  need(S9xRemasterPixelVisualClaim(f->frame,p).component>=0 && !S9xRemasterPixelVisualClaim(f->frame,p).published,
   "conflict fallback cannot masquerade as graph-published geometry");
  // A shared source occurrence outside this placement must not inherit its claim.
  need(f->frame.tileInstances[0].visualClaim.status==RemasterVisualClaimStatus::NotEvaluated,
   "original shared occurrence is immutable");
 }
 for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++) {
  auto f=std::make_unique<Fixture>(axis,flip,bg,0);f->attrs.fill(1);f->run();
  const size_t at=(f->first/64-10)*8*f->frame.width+(f->first%64-10)*8;
  RemasterLightingFieldCache cpu;cpu.update(f->frame,true);
  need(cpu.visualClaims[at][0].status==RemasterVisualClaimStatus::Unanchored && cpu.height[at*2+1]==1,
   "true unanchored graph retains available physical fallback, axes flips BGs");
  auto unsupported=std::make_unique<Fixture>(axis,flip,bg,0);
  unsupported->maps[(bg?0:8192)+2*unsupported->first]^=1;
  // Producer mismatch remains NotEvaluated, not an invented unsupported owner.
  unsupported->run();cpu.update(unsupported->frame,true);
  need(cpu.visualClaims[at][0].status==RemasterVisualClaimStatus::NotEvaluated,
   "overwritten producer cannot borrow a claim");
 }
 // Exact failure class observed in 060 east / 061 west: the outer HIGH
 // corner band has a ceiling plateau, next to an unclipped door lintel.
 // Their face samples agree; their ceiling pixels are not that tangent face.
 for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++) {
  auto f=std::make_unique<Fixture>(axis,flip,bg,0);
  const int tangent=axis?1:64,along=axis?64:1;
  const int corner=f->first,door=corner+tangent;
  // Replace one complete straight section with a classified corner branch
  // and the neighboring complete frame branch. Local origins are retained.
  for(int n=0;n<3;n++)for(int lane=0;lane<2;lane++) {
   int cell=corner+n*f->down*along+lane*tangent;
   auto &t=f->tiles[bg][cell];
   t.wallTemplateOffset=0xb66;
   if(lane) {
    t.surface=C::Surface::Door;t.doorAssembly=std::min(corner,corner+2*f->down*along)+tangent+1;
    t.doorTemplateOffset=0x2f42;t.doorDirection=axis?(f->down>0?0:1):(f->down>0?2:3);
    t.hasDoorframeMask=true;t.doorframeMask=UINT64_MAX;
   } else if(n==0)for(auto &h:t.height)h=std::min<int>(5,h);
   for(auto &m:f->frame.assetMetadata)if(m.tileId.hash==t.tileHash)m.height=t.height;
   if(lane)f->heights[bg].doorframeAttachments.push_back({cell,0,90,UINT64_MAX,t.doorTemplateOffset,true,74});
  }
  RemasterVisibleFaceTemplateCache cache;f->run(&cache);
  need(cache.result[bg][corner].status==1 && cache.result[bg][door].status==1,
   "clipped high-corner ceiling is not a contradictory door/wall face equation");
  need(cache.result[bg][corner].base==70 && cache.result[bg][door].base==70,
   "exact corner/door face follows occupied floor, preserves both local origins");
  need(f->baseAt(corner)==70 && f->baseAt(door)==70,"corner and frame publish actual selected scene bases");
  // A different/missing downhill writer must not license cap exclusion.
  auto &next=f->tiles[bg][corner+f->down*along];auto saved=next;
  next.wallTemplateOffset=0xc66;
  need(S9xRemasterVisibleFace::highCornerFaceMask(f->tiles[bg].data(),corner,axis?(f->down>0?1:0):(f->down>0?3:2))==UINT64_MAX,
   "different corner writer cannot establish clipped face");
  next=saved;next.hasHeight=false;
  need(S9xRemasterVisibleFace::highCornerFaceMask(f->tiles[bg].data(),corner,axis?(f->down>0?1:0):(f->down>0?3:2))==UINT64_MAX,
   "missing continuation does not manufacture cap region");
  next=saved;
 }
 { // Exact 050 seven-contact equation family, independent of owner policy.
  using namespace S9xRemasterVisibleFace;
  std::vector<Region> r(8);for(auto &t:r){t.established=true;for(int y=0;y<8;y++)for(int x=0;x<8;x++)t.height[y*8+x]=7-y;}
  std::vector<Contact> e={{0,1,0,-1,1,false},{1,2,0,-1,1,false},{2,3,1,0,0,false},
   {3,4,0,-1,1,false},{4,5,0,-1,1,false},{5,6,0,-1,1,false},{6,7,1,0,0,false}};
  auto bad=solve(r,e,{{0,0,7,0},{7,0,7,24}});
  for(auto s:bad.status)need(s==Status::Conflict,"050 16-unit competing datum remains conflict, not max or disconnection");
  auto good=solve(r,e,{{0,0,7,0},{7,0,7,40}});
  need(good.status[7]==Status::Solved && good.base[7]==40,"independent anchors with consistent datum remain solved");
  auto unknown=solve(r,e,{});need(unknown.status[0]==Status::Unanchored,"no floor anchor does not fabricate datum");
 }
 for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++)for(int plane=0;plane<2;plane++) {
  auto f=std::make_unique<Fixture>(axis,flip,bg,plane);
  // Retain just the first band on screen. All intermediate bands and the
  // independently occupied floor remain in the complete maps, not the camera.
  for(size_t at=0;at<f->frame.mainPixels.size();at++) {
   int cell=((f->c.backgroundScrollY+at/f->frame.width)&511)/8*64+
    ((f->c.backgroundScrollX+at%f->frame.width)&511)/8;
   if(cell!=f->first)f->frame.mainPixels[at]=f->frame.scenePixels[at]={};
  }
  const auto before=f->heights[bg].planeOffsets;
  f->run();need(f->baseAt(f->first)==70,"complete-room offscreen chain and floor anchor must solve selected high band");
  need(before==f->heights[bg].planeOffsets,"wall graph cannot alter physical planes");
  auto doorway=std::make_unique<Fixture>(axis,flip,bg,plane);
  for(int cell:{doorway->foot,doorway->foot+(axis?1:64)})doorway->tiles[1-bg][cell].doorAssembly=123;
  doorway->run();need(doorway->baseAt(doorway->first)==90,"doorway interior floor is never a wall seed");
  auto sideways=std::make_unique<Fixture>(axis,flip,bg,plane);
  // Only one tangent lane has an occupied foot. The other lane carries the
  // same band but a different intrinsic origin, requiring a signed seam.
  int otherFoot=sideways->foot+(axis?1:64);
  sideways->heights[1-bg].occupiedFloorOwners[plane][otherFoot]=-1;
  for(int n=0;n<6;n++) {
   int depth=sideways->down>0?n:6-n,x=10+(axis?1:depth),y=10+(axis?depth:1),cell=y*64+x;
   int layer=n<3?bg:1-bg;auto &t=sideways->tiles[layer][cell];
   for(auto &h:t.height)h+=13;
   // Give each displayed occurrence its own effective local layer; artwork
   // identity can repeat, so asset-wide edits would be a false witness.
   for(auto &inst:sideways->frame.tileInstances)if(inst.sourceIndex==layer && inst.tileId.hash==t.tileHash) {
    inst.hasPlacementHeight=true;inst.placementHeight=t.height;
   }
  }
  // Source remains intrinsic for the first lane; validate complete solve of
  // the second lane independently of publication's contextual-layer guard.
  RemasterVisibleFaceTemplateCache cache;sideways->run(&cache);
  int otherFirst=sideways->first+(axis?1:64);
  need(cache.result[bg][otherFirst].status==1 && cache.result[bg][otherFirst].base==57,"tangent seam subtracts signed local origin without floor below");
 }
 // Assembly masks, closed leaves, phases and two explicit corner facings.
 for(int direction=0;direction<4;direction++)for(int flip=0;flip<4;flip++) {
  C::Placement t;t.hasHeight=true;t.surface=C::Surface::Door;t.doorAssembly=10*64+10+1;
  t.doorTemplateOffset=0x86a;t.doorDirection=direction;t.hflip=flip&1;t.vflip=flip&2;
  t.hasDoorframeMask=true;t.doorframeMask=UINT64_C(0x8181818181818181);
  t.height.fill(0);int cell=10*64+10;
  auto open=S9xRemasterVisibleFace::wallBand(t,cell,-1,0,false);
  need(open.frame && open.mask==t.doorframeMask,"open frame retains authored mask, never opening floor");
  t.surface=C::Surface::ClosedDoor;t.closedDoorAssembly=true;
  auto closed=S9xRemasterVisibleFace::wallBand(t,cell,-1,0,false);
  need(closed.frame && closed.mask==UINT64_MAX,"complete closed leaf is a barrier, not open mask");
  t.hasHeight=false;need(!S9xRemasterVisibleFace::wallBand(t,cell,-1,0,false).downhill,"missing height cannot become authored zero");
 }
 for(bool inner:{false,true})for(int flip=0;flip<4;flip++) {
  C::Placement t;t.surface=C::Surface::Wall;t.hasHeight=true;t.wallTemplateOffset=0xbe6;
  t.terminalCorner=true;t.hflip=flip&1;t.vflip=flip&2;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)t.height[y*8+x]=inner?std::min(7-x,7-y):std::max(7-x,7-y);
  auto b=S9xRemasterVisibleFace::wallBand(t,0,-1,0,false);
  need(b.corner && (b.downhill&3) && (b.downhill&12),"inside/outside folded corner has both transformed facings");
 }
 { // Door jambs bridge an open void using assembly phase, not a floor
   // under the doorway. Two sections have a common datum but unequal origins.
  using namespace S9xRemasterVisibleFace;
  std::vector<Region> r(4);for(auto &t:r)t.established=true;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
   r[0].height[y*8+x]=7-y;r[1].height[y*8+x]=20-y;
   r[2].height[y*8+x]=36-y;r[3].height[y*8+x]=7-y;
  }
  auto solved=solve(r,{{0,1,1,0,0,false},{2,3,1,0,0,false}},{{0,0,7,0}},{{1,2,-16}});
  need(solved.status[3]==Status::Solved && solved.base[1]==-13 && solved.base[2]==-29 && solved.base[3]==0,"signed frame datum crosses unsupported doorway and preserves local origins");
  auto bad=solve(r,{{0,1,1,0,0,false},{2,3,1,0,0,false}},{{0,0,7,0},{3,0,7,24}},{{1,2,-16}});
  need(bad.status[0]==Status::Conflict && bad.status[3]==Status::Conflict,"conflicting frame anchors invalidate whole claim");
  // Nonunit/plateau cap contact is explicitly assembly-proven. Ordinary
  // unproved unit edges continue to reject the same samples.
  r.resize(2);for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
   r[0].height[y*8+x]=std::min(5,7-y);r[1].height[y*8+x]=std::min(5,7-y);
  }
  auto cap=solve(r,{{0,1,0,1,-1,false}},{{1,0,7,0}});
  need(cap.status[0]==Status::Solved && cap.base[0]==6,"authored plateau cap is not normalized to an eight-unit ramp");
  need(solve(r,{{0,1,0,1,-1}},{{1,0,7,0}}).status[0]==Status::Conflict,"plateau requires independent assembly admission");
  for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
   r[0].height[y*8+x]=std::min(5,std::max(x,7-y));r[1].height[y*8+x]=7-y;
  }
  auto mask=cornerFaceSamples(r[0],r[1],-1,0,255);
  need(mask==248,"clipped corner excludes only ceiling plateau from tangent vote");
  auto folded=solve(r,{{0,1,-1,0,0,false,mask}},{{1,0,7,24}});
  need(folded.status[0]==Status::Solved && folded.base[0]==24,"clipped folded cap follows same floor datum without normalizing artwork");
 }
 for(int flip=0;flip<4;flip++) {
  auto f=std::make_unique<Fixture>(0,flip,0,0);
  int cell=f->first;auto &t=f->tiles[0][cell];t.hasHeight=false;
  f->run();need(f->baseAt(cell)==90,"missing source blocks complete section without fabricated zero");
 }
 std::cout<<"PASS complete-room wall graph camera independence and doorway-floor exclusion\n";
}
