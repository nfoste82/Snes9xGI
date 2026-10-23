// Repeated exact H probes of one bilateral ROM footprint are one owner.
#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto words=std::make_unique<std::array<uint8_t,16384>>();auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();auto rom=std::make_unique<std::array<uint8_t,12876>>();rom->fill(255);
 RemasterProfile p;uint8_t zero[64]={};RemasterTileContentId floor={S9xRemasterHashTile(4,zero),1,4};p.assets[floor].hasHeight=true;p.assetGroups["dungeon_floor"].tileIds={floor};
 for(int n=1;n<=4;n++) {
  uint8_t pixels[64];std::fill_n(pixels,64,n);
  for(int y=0;y<8;y++)for(int b=0;b<4;b++)(*gfx)[n*32+b/2*16+y*2+b%2]=(n&(1<<b))?255:0;
  auto &a=p.assets[{S9xRemasterHashTile(4,pixels),1,4}];a.hasHeight=true;
  // Per-asset ABSOLUTE relief is deliberately non-affine and not8-per-row:
  // a terminal column must retain it, never turn it into border ramps.
  for(int k=0;k<64;k++)a.height[k]=n==1?17+k%3:n==2?15+k%5:n==3?k%16:k%7;
  for(int x=0;x<2;x++){const int off=0x84a+(x*4+n-1)*2;const int w=n|(x?0x4000:0);(*rom)[off]=w;(*rom)[off+1]=w>>8;}
 }
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=80;c.linkY=80;c.constructedMaps=words->data();c.graphics=gfx->data();c.collisionAttributes=attrs->data();c.dungeonTemplateData=rom->data();
 c.stairCount=1;c.stairs[0]={20,20,0,0,true,false};
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();auto map=std::make_unique<RemasterDungeonHeightMap>();
 auto setup=[&](int flip) {
  words->fill(0);attrs->fill(1);
  for(int y=8;y<20;y++)for(int x=8;x<30;x++)(*attrs)[y*64+x]=0;
  for(int y=24;y<38;y++)for(int x=8;x<30;x++)(*attrs)[y*64+x]=0;
  for(int oy:{13,28})for(int y=0;y<4;y++)for(int x=0;x<2;x++) {
   int sx=flip&1?1-x:x,sy=flip&2?3-y:y,w=(sy+1)|(sx?0x4000:0);w^=(flip&1?0x4000:0)|(flip&2?0x8000:0);
   int i=(oy+y)*64+10+x;(*words)[2*i]=w;(*words)[2*i+1]=w>>8;(*attrs)[i]=y==(flip&2?3:0)?0:1;
  }
  S9xRemasterDungeonPlacements(c,p,*tiles);c.placements=tiles->data();
 };
 for(int flip=0;flip<4;flip++) {
  setup(flip);need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"solve");
  for(int oy:{13,28})for(int y=0;y<4;y++)for(int x=0;x<2;x++) {
   int i=(oy+y)*64+10+x;const auto &t=(*tiles)[i];
   need(t.obstacleAssembly==oy*64+11 && t.obstacleAssemblyHeight==4 && t.surface==S::Obstacle,"symmetric footprint rejected");
   need(map->offsets[i]==(oy==13?50:0) && !map->fallbackDirections[i],"column parts received distance ramps/incorrect base");
   need(t.height==p.assets.at({t.tileHash,1,4}).height,"authored column shape replaced");
  }
  // Both normal and reverse footprint keep TOP body support separate from
  // visible shell base. Current collision need not block every cap pixel.
  need(map->requiredPlaneCells[1]==0 && !map->structuralConflicts,"plane activation/conflict");
  // Real priority-only overlay keeps footprint; tile overwrite rejects all.
  (*words)[(13*64+10)*2+1]^=0x20;S9xRemasterDungeonPlacements(c,p,*tiles);need((*tiles)[13*64+10].obstacleAssembly,"priority overwrite erased geometry");
  (*words)[(13*64+10)*2]=9;S9xRemasterDungeonPlacements(c,p,*tiles);for(int y=13;y<17;y++)for(int x=10;x<12;x++)need(!(*tiles)[y*64+x].obstacleAssembly,"partial overwrite grouped");
 }
 setup(0);auto top=(*tiles)[13*64+10];need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"application solve");
 {RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=1;i.tileId={top.tileHash,1,4};f.tileInstances={i};for(int k=0;k<64;k++)f.mainPixels[k]={0,1,uint8_t(k)};c.backgroundScrollX=80;c.backgroundScrollY=104;S9xRemasterApplyDungeonHeightMap(f,c,*map);need(f.tileInstances[0].heightOffset==50 && !f.tileInstances[0].hasPlacementHeight,"runtime relief replaced or doublecounted");}
 // Differing template dispatches are NOT H symmetry of one template.
 {const auto &t=(*tiles)[13*64+10];auto changed=t;changed.obstacleAssemblyFlip^=2;need(!t.sameGeometry(changed),"cache forgot foot transform");}
 // Negative approved entry bases remain signed; do not clamp or require a
 // positive floor rise merely because this is obstacle geometry.
 c.previousRoomIndex=8;c.entrySupport={17*64+10,-8,8,123,456,S9xRemasterDungeonEntryStateKey(c,50)};
 need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->entrySupportConsumed && map->offsets[13*64+10]==-8,"signed support base clipped");c.entrySupport={};
 // Missing outward width2foot cannot be rescued by walkable head/side art.
 for(int y=17;y<20;y++)for(int x=8;x<30;x++)(*attrs)[y*64+x]=1;
 need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[13*64+10]==255,"head/side adjacency invented foot support");
 std::copy_n(rom->data()+0x84a,16,rom->data()+0x85a);S9xRemasterDungeonPlacements(c,p,*tiles);need(!(*tiles)[13*64+10].obstacleAssembly,"different-template ambiguous ownership accepted");
 std::cout<<"PASS bilateral ROM2x4columns, allflips, independent50/0support, originalnon8relief, runtimecomposition, priority/overwrite/templateambiguity\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
