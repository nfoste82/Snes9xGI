// Deterministic synthetic WRAM, not a live room capture. Independent 512px map
// coordinates and camera bounds follow the verified USA loader/RoomBounds code.
#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr; uint32_t S9xRemasterCurrentOwner=0;
static unsigned checks=0;
static void need(bool b,const char *s){++checks;if(!b)throw std::runtime_error(s);}
int main(){try {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();
 auto heights=std::make_unique<RemasterDungeonHeightMap>();
 (*graphics)[32]=128;uint8_t pixels[64]={};pixels[0]=1;
 RemasterTileContentId id{S9xRemasterHashTile(4,pixels),1,4};
 RemasterProfile p;p.upperFloorHeight=50;p.romSha256=std::string(64,'a');auto &a=p.assets[id];a.tileId=id;
 a.hasEmission=a.hasHeight=true;a.height.fill(0);a.emissionRgba[0]=255;a.emissionRgba[3]=25;
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=0x55;
 c.constructedMaps=maps->data();c.graphics=graphics->data();heights->roomIndex=c.roomIndex;
 c.backgroundScrollX=256;c.backgroundScrollY=273;
 auto put=[&](int x,int y){int cell=y*64+x;(*maps)[2*cell]=(*maps)[8192+2*cell]=1;heights->offsets[cell]=0;};
 // Six northern, three southern independent one-tile fixtures.
 for(auto xy:{std::pair<int,int>{27,9},{39,9},{27,21},{39,21},{12,27},{18,27},{12,37},{18,37},{48,37}})put(xy.first,xy.second);
 auto &e=c.layoutEvidence;e.available=true;e.loadedLayout=16;e.quadrant={{2,0,18,0,2}};
 e.bounds={{2816,2560,2832,2832,2560,2560,2560,2816}};e.scroll={{2816,2832}};
 RemasterFrame f;f.width=256;f.height=224;
 auto run=[&](){f.hiddenEmitters.clear();return S9xRemasterAppendOffscreenRoomLights(f,c,p,heights.get(),heights.get(),S9xRemasterHashTile);};
 const int correction=std::getenv("ALTTP_COMPARTMENT_CORRECTION")?std::atoi(std::getenv("ALTTP_COMPARTMENT_CORRECTION")):0;
 if(correction==1) {
  e.scroll[1]=2560;need(run().placements==9,"contradictory northern WRAM scroll must fail open");return 0;
 }
 if(correction==2) {
  c.submodule=5;need(run().placements==9,"entire shutter animation retains whole source union");return 0;
 }
 need(run().placements==3,"settled half-height must exclude six northern fixtures (baseline has nine)");
 for(int phase=0;phase<8;phase++){c.submodule=1;c.constructionPhase=phase;need(run().placements==9,"all eight intra-room phases retain both compartments");}
 c.submodule=5;c.constructionPhase=0;need(run().placements==9,"shutter animation keeps complete source union");
 c.submodule=0;e.available=false;need(run().placements==9,"legacy absent evidence fails open");
 e.available=true;const auto settled=e;
 e.scroll[1]=2560;need(run().placements==9,"northern WRAM scroll contradicts southern bounds: fail open");
 e=settled;e.scroll[1]=2833;need(run().placements==9,"raw copy2 one past bound is invalid, not renderer bias");
 e=settled;c.backgroundScrollY=17;need(run().placements==9,"rendered northern camera contradicts southern raw camera: fail open");
 f.mainPixels.resize(256*224);f.subPixels.resize(256*224);f.originalRgb555.resize(256*224);
 f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 // Northern tile (39,9) is at screen(56,55), under actual HUD ink.
 f.mainPixels[55*256+56].owner=0x02020000;
 need(run().hiddenPixels>0,"contradictory camera retains northern HUD source");
 bool hudFound=false;for(const auto &h:f.hiddenEmitters)hudFound|=h.x==56.5f && h.y==55.5f;
 need(hudFound,"specific northern HUD source must not be culled");
 c.backgroundScrollY=273;
 for(int axis=0;axis<2;axis++) {
  e=settled;e.scroll[axis]++;need(run().placements==9,"WRAM/PPU asynchronous or shaken camera fails open");
 }
 e=settled;c.constructionPhase=1;need(run().placements==0,"nonzero settled-module phase remains lifecycle-ineligible");c.constructionPhase=0;
 for(int field=0;field<8;field++){e=settled;e.bounds[field]++;need(run().placements==9,"contradictory camera bounds fail open");}
 e=settled;e.quadrant[2]^=1;need(run().placements==9,"composite layout mismatch fails open");
 e=settled;e.quadrant[1]=2;need(run().placements==9,"derived size mismatch fails open");
 e=settled;e.overrides=0x0100;need(run().placements==9,"unknown override fails open");
 e=settled;e.overrides=0x0200;e.quadrant[1]=2;need(run().placements==9,"verified full-height override enlarges membership");
 e=settled;e.blastwall[1]=1;e.quadrant[1]=2;need(run().placements==9,"blast wall enlarges membership");
 // Exhaustive independent layout table expectations at all four quadrants.
 const unsigned layoutFlags[8][4]={{15,15,15,15},{11,11,7,7},{15,11,15,7},{11,15,7,15},{14,13,14,13},{15,15,14,13},{14,13,15,15},{10,9,6,5}};
 for(int layout=0;layout<8;layout++)for(int qy=0;qy<2;qy++)for(int qx=0;qx<2;qx++) {
  e={};e.available=true;e.loadedLayout=layout*4;
  const unsigned flags=layoutFlags[layout][qy*2+qx];
  const bool fullX=!(flags&(qx?2:1)),fullY=!(flags&(qy?8:4));
  e.quadrant={{uint8_t(fullX?2:0),uint8_t(fullY?2:0),uint8_t(layout*4+qy*2+qx),uint8_t(qx),uint8_t(qy*2)}};
  e.bounds={{uint16_t(2560+qy*256),2560,uint16_t(2576+qy*256),2832,uint16_t(2560+qx*256),2560,uint16_t(2560+qx*256),2816}};
  e.scroll={{uint16_t(2560+qx*256),uint16_t(2560+qy*256)}};
  auto r=S9xRemasterDungeonCompartment(e);need(r.valid,"normal full/half/quarter layout validates");
  for(int y:{64,384})for(int x:{64,384})need(r.containsTile(x,y)==((fullX || (x/256==qx))&&(fullY || (y/256==qy))),"independent quadrant membership");
 }
 e=settled;put(40,31);put(40,32);need(run().placements==5,"both halves of boundary fixture retained");
 put(40,30);put(40,29);need(run().placements==7,"connected fixture extends beyond split margin and remains whole");
 (*maps)[2*(29*64+40)]=(*maps)[8192+2*(29*64+40)]=0;
 (*maps)[2*(30*64+40)]=(*maps)[8192+2*(30*64+40)]=0;
 // Unknown bases and producer overwrites remain unknown, never rescued.
 heights->offsets[37*64+48]=255;need(run().unknownBases==1,"missing artwork base not invented");
 heights->offsets[37*64+48]=0;heights->producerHashes[37*64+48]=id.hash^1;
 need(run().unknownBases==1,"overwritten producer rejected");heights->producerHashes[37*64+48]=0;
 // Invocation-local membership: interrupt, reverse and change sessions without
 // any historical outgoing-source cache or retained animation identity.
 for(int repeat=0;repeat<3;repeat++) {
  c.submodule=1;for(int phase:{0,7,3,1}){c.constructionPhase=phase;need(run().placements==11,"interrupted/reversed intra-room union");}
  c.submodule=0;c.constructionPhase=0;e=settled;need(run().placements==5,"settle after reversal recomputes membership");
  e.available=false;need(run().placements==11,"reload without evidence does not retain old membership");e=settled;
 }
 // Actual EndFrame consumer with current deterministic WRAM/VRAM and solved
 // authored-zero floors. No review map or direct append helper in this gate.
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(0);
 c.collisionAttributes=attrs->data();c.linkX=2560+12*8;c.linkY=2560+37*8;c.bg1GraphicsBase=0;c.groundedEvidence=true;
 a.tileRole=RemasterTileRole::Floor;p.assetGroups["dungeon_floor"].tileIds={id};
 uint8_t blank[64]={};RemasterTileContentId blankId{S9xRemasterHashTile(4,blank),1,4};
 p.assets[blankId].tileId=blankId;p.assets[blankId].hasHeight=true;p.assets[blankId].tileRole=RemasterTileRole::Floor;
 p.assetGroups["dungeon_floor"].tileIds.push_back(blankId);
 S9xRemasterSetProfile(p);S9xRemasterSetLiveFramesEnabled(true);
 c.constructionPhase=0;
 std::vector<uint16_t> screen(256*224);RemasterFrame output;
 auto end=[&](){S9xRemasterBeginFrame(256*224,256,256,224);S9xRemasterEndFrame(screen.data(),256,256,224,&output,&c);return output.hiddenEmitters.size();};
 if(correction==4) {
  // Explicit review-only bases do not establish current producer provenance.
  // Their presence must not disable independently solved current sources.
  auto review=std::make_unique<RemasterDungeonHeightMap>();*review=*heights;
  S9xRemasterSetDungeonHeightMaps({*review});
  need(end()==4,"compiled review map without sprite assemblies must not suppress current EndFrame sources");return 0;
 }
 // Only southern x48 is viewport-visible, so four other retained sources are hidden.
 auto initial=end();std::cout<<"EndFrame settled hidden="<<initial<<" valid="<<S9xRemasterState().runtimeDungeonHeightValid<<" base="<<int(S9xRemasterState().runtimeDungeonHeightMap.offsets[37*64+12])<<" pub="<<S9xRemasterDungeonHeightInputsPublishable(c,0xffff)<<" width="<<output.width<<" active="<<S9xRemasterState().liveFramesActive<<"\n";
 need(initial==4,"production EndFrame settled actual hidden source count");
 if(correction==3) {
  maps->fill(0);(*maps)[8192+2*(37*64+12)]=1;
  need(end()==1,"prime actual BG1-only source with current solved base");
  c.collisionAttributes=nullptr;
  need(end()==0,"missing current collision must not admit cached BG1 source");return 0;
 }
 for(int phase=0;phase<8;phase++){c.submodule=1;c.constructionPhase=phase;need(end()==10,"production EndFrame all intra-room phases retain both sides with bases");}
 c.submodule=5;
 for(int phase:{0,1,7,15,255}){c.constructionPhase=phase;need(end()==10,"production EndFrame entire shutter animation retains both sides");}
 c.submodule=0;c.constructionPhase=0;need(end()==4,"only actual settled return restricts production sources");
 c.submodule=0;e.available=false;need(end()==10,"production EndFrame legacy evidence retains output");
 // Prime and withdraw exact current input ownership for both planes and all
 // newly admitted handlers. Identity/flip remain identical across withdrawal.
 auto savedMaps=*maps;
 for(int bg:{0,1})for(int handler:{0,1,5}) {
  maps->fill(0);(*maps)[(bg?0:8192)+2*(37*64+12)]=1;
  c.submodule=handler;c.constructionPhase=0;e=settled;
  need(end()==1,"prime actual source with current solved base on each BG");
  c.collisionAttributes=nullptr;need(end()==0,"absent current collision rejects both cached BG bases");
  c.collisionAttributes=attrs->data();need(end()==1,"current collision restoration resolves sources again");
  c.collisionMode=1;need(end()==0,"unpublishable collision mode rejects both cached BG bases");
  c.collisionMode=0;need(end()==1,"restoring publishability restores only current sources");
  S9xRemasterState().runtimeDungeonHeightValid=false;c.collisionAttributes=nullptr;
  need(end()==0,"invalid solve cannot supply cached same-hash BG1 datum");c.collisionAttributes=attrs->data();
 }
 *maps=savedMaps;c.submodule=0;c.constructionPhase=0;
 // Review visualization is independent of current emission ownership. Cover
 // valid-looking, unknown and mismatched producer review data, with/without
 // sprite assemblies, profile publication/reload, map changes and both BGs.
 for(bool sprite:{false,true})for(int reviewKind:{0,1,2}) {
  auto review=std::make_unique<RemasterDungeonHeightMap>();*review=*heights;
  if(reviewKind==0)review->offsets.fill(99);
  if(reviewKind==1)review->offsets.fill(255);
  if(reviewKind==2)review->producerHashes.fill(id.hash^1);
  p.obstacleGroups.clear();
  if(sprite){RemasterObstacleGroup group;group.spriteAssembly=true;p.obstacleGroups.push_back(group);}
  S9xRemasterSetProfile(p);S9xRemasterSetDungeonHeightMaps({*review});
  for(int bg:{0,1}) {
   maps->fill(0);(*maps)[(bg?0:8192)+2*(37*64+12)]=1;e=settled;
   need(end()==1,"review map cannot suppress current independently solved source on either BG");
   need(output.hiddenEmitters[0].z==0,"review bases/unknowns cannot replace current owned emission datum");
   S9xRemasterSetProfile(p);need(end()==1,"profile publication with review rebuilds current source solve");
   c.collisionAttributes=nullptr;need(end()==0,"review map cannot rescue missing current collision");
   c.collisionAttributes=attrs->data();need(end()==1,"review current inputs recover after interruption");
   (*maps)[(bg?0:8192)+2*(37*64+12)]=0;need(end()==0,"review map cannot resurrect extinguished current artwork");
  }
 }
 p.obstacleGroups.clear();S9xRemasterSetProfile(p);S9xRemasterSetDungeonHeightMaps({});
 *maps=savedMaps;need(end()==4,"removing review restores ordinary current-source path");
 e=settled;
 // Same source inventory, current palette/animation/profile, both BG bases and
 // both canonical transforms. Changes must never resurrect an old source.
 for(int layer:{0,1})for(int flip:{0,0x4000,0x8000,0xc000}) {
  maps->fill(0);const int cell=37*64+12,at=(layer?0:8192)+2*cell;
  (*maps)[at]=1;(*maps)[at+1]=uint8_t(flip>>8);
  c.bg1GraphicsBase=0;heights->offsets[cell]=0;
  auto d=run();need(d.hiddenPixels==1,"both BGs and transformed placement retained exactly once");
  need(f.hiddenEmitters[0].x==96+(flip&0x4000?7:0)-256+0.5f && f.hiddenEmitters[0].y==296+(flip&0x8000?7:0)-273+0.5f,"source coordinates use room-local map, not camera bounds");
 }
 std::array<uint16_t,256> palette={};palette[1]=31;c.emitterPalette=palette.data();a.emissionColorMask=0;
 need(run().hiddenPixels==1 && f.hiddenEmitters[0].emission[0]==255,"current source palette consumed");
 palette[1]=0;need(run().hiddenPixels==0,"current black palette does not retain old source");palette[1]=31;
 (*graphics)[32]=0;need(run().hiddenPixels==0,"current animation removes previous graphics identity");(*graphics)[32]=128;
 a.emissionRgba[3]=0;need(run().hiddenPixels==0,"authored zero emission remains zero");a.emissionRgba[3]=25;
 // Viewport/HUD admission has the same ownership policy: do not double visible
 // pixels, recover covered pixels only, and leave all artwork/owners untouched.
 c.backgroundScrollX=0;c.backgroundScrollY=256;f.mainPixels.resize(256*224);f.subPixels.resize(256*224);f.originalRgb555.resize(256*224);
 f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 need(run().visiblePixels==1 && f.hiddenEmitters.empty(),"viewport source remains exclusively visible");
 const int hud=47*256+103;f.mainPixels[hud].owner=0x02020000;
 need(run().hiddenPixels==1 && f.hiddenEmitters[0].x==103.5f && f.hiddenEmitters[0].y==47.5f,"HUD source recovered once with both flips");
 need(f.mainPixels[hud].owner==0x02020000,"source filter does not alter HUD artwork or ownership");
 c.submodule=2;for(int phase=0;phase<16;phase++){c.constructionPhase=phase;need(run().hiddenPixels==0,"inter-room loader behavior unchanged; no stale reconstruction");}
 std::cout<<"PASS "<<checks<<" compartment checks\n";
 }catch(const std::exception &ex){std::cerr<<"FAIL "<<checks<<": "<<ex.what()<<'\n';return 1;}}
