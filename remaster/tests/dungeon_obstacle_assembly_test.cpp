#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto words=std::make_unique<std::array<uint8_t,16384>>();auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();auto rom=std::make_unique<std::array<uint8_t,12876>>();rom->fill(255);
 RemasterProfile p;p.upperFloorHeight=50;
 uint8_t zero[64]={};RemasterTileContentId floor={S9xRemasterHashTile(4,zero),1,4};p.assets[floor].hasHeight=true;p.assetGroups["dungeon_floor"].tileIds={floor};
 // Distinct complete indexed template parts, own canonical relief arrays.
 for(int n=1;n<=6;n++) {
  uint8_t pixels[64];std::fill_n(pixels,64,n);
  for(int y=0;y<8;y++)for(int b=0;b<4;b++)(*gfx)[n*32+b/2*16+y*2+b%2]=(n&(1<<b))?255:0;
  RemasterTileContentId id={S9xRemasterHashTile(4,pixels),1,4};auto &a=p.assets[id];a.hasHeight=true;a.tileRole=RemasterTileRole::Wall;
  for(int k=0;k<64;k++)a.height[k]=23-(n-1)%3*8-k/8;
  const int off=0xe26+(n-1)*2;(*rom)[off]=n;(*rom)[off+1]=0;
 }
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.constructedMaps=words->data();c.graphics=gfx->data();c.dungeonTemplateData=rom->data();c.collisionAttributes=attrs->data();
 c.linkX=80;c.linkY=80;c.stairCount=1;c.stairs[0]={20,20,0,0,true,false};
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();auto map=std::make_unique<RemasterDungeonHeightMap>();
 auto build=[&](int flip,bool overwrite) {
  words->fill(0);attrs->fill(1);
  for(int y=8;y<20;y++)for(int x=8;x<30;x++)(*attrs)[y*64+x]=0;
  for(int y=24;y<36;y++)for(int x=8;x<30;x++)(*attrs)[y*64+x]=0;
  for(int originY:{14,28})for(int yy=0;yy<3;yy++)for(int xx=0;xx<2;xx++) {
   const int n=1+(flip&1?1-xx:xx)*3+(flip&2?2-yy:yy),cell=(originY+yy)*64+10+xx;
   const int word=n|((flip&1)?0x4000:0)|((flip&2)?0x8000:0);(*words)[cell*2]=word;(*words)[cell*2+1]=word>>8;(*attrs)[cell]=1;
  }
  if(overwrite)(*words)[(14*64+10)*2]=7;
  S9xRemasterDungeonPlacements(c,p,*tiles);c.placements=tiles->data();
 };
 for(int flip=0;flip<4;flip++) {
  build(flip,false);need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"solve");
  need(!map->structuralConflicts,"assembly must not form relief constraints");
  for(int originY:{14,28})for(int yy=0;yy<3;yy++)for(int xx=0;xx<2;xx++) {
   const int cell=(originY+yy)*64+10+xx;const auto &t=(*tiles)[cell];
   need(t.obstacleAssembly && t.surface==S::Obstacle,"ROM footprint failed obstacle classification");
   need(map->offsets[cell]==(originY==14?50:0),"parts did not share exact independent support base");
   need(t.height==p.assets.at({t.tileHash,1,4}).height && !map->fallbackDirections[cell],"authored arrays changed");
  }
  build(flip,true);for(int yy=0;yy<3;yy++)for(int xx=0;xx<2;xx++)need(!(*tiles)[(14+yy)*64+10+xx].obstacleAssembly,"partial overwrite retained assembly");
 }
 // Effective runtime application retains TOP relief and adds shared50 once.
 build(0,false);S9xRemasterBuildDungeonHeightMap(c,50,*map);
 {RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
  const auto &top=(*tiles)[14*64+10];RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=1;i.tileId={top.tileHash,1,4};
  f.tileInstances={i};for(int k=0;k<64;k++){f.mainPixels[k].instanceId=1;f.mainPixels[k].tilePixel=k;}
  c.backgroundScrollX=80;c.backgroundScrollY=112;S9xRemasterApplyDungeonHeightMap(f,c,*map);
  need(f.tileInstances[f.mainPixels[0].instanceId-1].heightOffset==50 && !f.tileInstances[f.mainPixels[0].instanceId-1].hasPlacementHeight,"runtime top not composed50+authored23");
 }
 {auto base=(*tiles)[14*64+10],changed=base;changed.obstacleAssembly++;need(!base.sameGeometry(changed),"cache lost assembly identity");changed=base;changed.obstacleTemplate++;need(!base.sameGeometry(changed),"cache lost template");changed=base;changed.obstacleAssemblyFlip^=1;need(!base.sameGeometry(changed),"cache lost footprint transform");}
 // Missing complete foot support stays unknown, not nearby top floor.
 build(0,false);for(int y=17;y<20;y++)for(int x=8;x<30;x++)(*attrs)[y*64+x]=1;
 need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[14*64+10]==255,"head proximity rescued missing foot");
 // Independent planes touching the same complete foot cannot identify its
 // owner even when both normalize0. Never merge support to satisfy artwork.
 build(0,false);c.stairs[0].changesPlane=true;
 for(int y=8;y<20;y++)for(int x=8;x<30;x++)(*attrs)[4096+y*64+x]=0;
 for(int yy=0;yy<3;yy++)for(int xx=0;xx<2;xx++)(*attrs)[4096+(14+yy)*64+10+xx]=1;
 need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[14*64+10]==255 && map->structuralConflicts>0,"ambiguous two-plane foot accepted");
 // Exact duplicated ROM templates are ambiguous ownership, not equivalent
 // permission to select the first dispatch family.
 build(0,false);std::copy_n(rom->data()+0xe26,12,rom->data()+0xed2);
 S9xRemasterDungeonPlacements(c,p,*tiles);
 need(!(*tiles)[14*64+10].obstacleAssembly,"ambiguous template family accepted");
 // Foot on an observed50support and opposite-plane0support is a real height
 // conflict, not a reason to choose the larger or current actor's base.
 std::fill_n(rom->data()+0xed2,12,255);build(0,false);c.stairs[0].changesPlane=false;
 for(int y=8;y<20;y++)for(int x=8;x<30;x++)(*attrs)[4096+y*64+x]=0;
 for(int yy=0;yy<3;yy++)for(int xx=0;xx<2;xx++)(*attrs)[4096+(14+yy)*64+10+xx]=1;
 c.stairCount=2;c.stairs[1]={50,50,0,0,true,true};
 need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[14*64+10]==255 && map->structuralConflicts>0,"conflicting50/0support selected");
 std::cout<<"PASS ROM2x3 assemblies/allflips, wall-mislabel correction, two copies support50/0, local arrays, terminal graph, overwrite/missingfoot/planeconflict/ambiguoustemplate\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
