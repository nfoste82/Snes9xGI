// Source presence is not sample magnitude, nor the conjunction of channels.
#include "../runtime/lighting_field_cache.h"
#include <cstdlib>
#include <iostream>
static void require(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
static RemasterFrame fixture(int channels,int flips,bool hud){
 RemasterFrame f;f.width=256;f.height=1;
 f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 RemasterFramePixel empty;empty.owner=uint32_t(RemasterSourceType::Backdrop)<<24;
 f.mainPixels.assign(256,empty);f.subPixels=f.scenePixels=f.mainPixels;
 f.originalRgb555.assign(256,7);f.sceneRgb555.assign(256,9);
 RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=0;
 i.tileId={123,1,4};i.hFlip=flips&1;i.vFlip=flips&2;i.hasPaletteArtwork=true;i.paletteRgb555.fill(1234);i.heightOffset=-8;
 f.tileInstances.push_back(i);
 RemasterFrameAssetMetadata m;m.tileId=i.tileId;m.tileRole=RemasterTileRole::Wall;
 m.hasHeight=channels&1;m.height.fill(0);m.hasNormals=channels&2;m.normalXyz.fill(128);m.normalXyz[13*3]=255;
 m.hasOcclusion=channels&4;m.occlusion.fill(0);m.hasEmission=channels&8;m.emissionRgba.fill(0);
 m.hasMaterialSelectors=channels&16;m.materialSelectors.fill("stone");m.hasDoorframeMask=channels&32;m.doorframeMask=0;
 f.assetMetadata.push_back(m);
 f.subPixels[100]={uint32_t(RemasterSourceType::Background)<<24,1,13};
 if(hud)f.mainPixels[100]={uint32_t(RemasterSourceType::Background)<<24|2<<16,0,0};
 return f;
}
int main(){
 for(int channels=1;channels<64;channels++)for(int flips=0;flips<4;flips++)for(bool hud:{false,true}){
  auto f=fixture(channels,flips,hud);auto main=f.mainPixels;auto original=f.originalRgb555;
  require(S9xRemasterRestoreSceneSubscreenWalls(f)==1,"independent channel admission including authored zero/HUD");
  require(f.mainPixels[100].owner==main[100].owner && f.originalRgb555==original,"stock screen unchanged");
  require(f.scenePixels[100].instanceId==1 && f.scenePixels[100].tilePixel==13 && f.sceneRgb555[100]==1234,"exact donor identity");
  auto g=S9xRemasterResolveFrameGeometry(f.assetMetadata[0],f,0);
  require(g.hasHeight==bool(channels&1) && g.hasNormals==bool(channels&2) && g.hasOcclusion==bool(channels&4),"absence not fabricated");
  require(g.hasDoorframeMask==bool(channels&32) && g.doorframeMask==0 && g.hasEmission==bool(channels&8) && g.hasMaterialSelectors==bool(channels&16),"all modeled channel flags preserved");
  RemasterLightingFieldCache cpu;cpu.update(f,true);
  require(cpu.height[201] && cpu.height[200]==S9xRemasterSignedHeightField(-8,0),"support base/authored zero transported");
  require(cpu.authoredNormals[100]==bool(channels&2),"no fabricated normal");
  if(channels&2)require((cpu.surface[401]>0)==!(flips&1),"canonical normal follows H transform");
  require(cpu.occlusion[200]==((channels&4)?0:255),"authored zero occlusion vs fallback");
  if(hud)require(!cpu.participation[200] && !cpu.participation[201] && cpu.samples[100].known,"HUD no receiver, world occluder retained");
  for(int bad=0;bad<6;bad++){
   auto n=fixture(channels,flips,hud);
   if(bad==0)n.scenePixels[100]={uint32_t(RemasterSourceType::Background)<<24|1<<16,0,0}; // opaque BG2, even no metadata
   if(bad==1)n.mainPixels[100]={uint32_t(RemasterSourceType::Background)<<24|1<<16,0,0};
   if(bad==2)n.tileInstances[0].geometryStatus=RemasterOccurrenceGeometryStatus::Conflict;
   if(bad==3)n.subPixels[100].instanceId=0; // transparent/absent donor
   if(bad==4)n.subPixels[100].tilePixel=64;
   if(bad==5)n.tileInstances[0].sourceIndex=1;
   require(!S9xRemasterRestoreSceneSubscreenWalls(n),"opaque competing owner/invalid donor rejected");
  }
 }
 // Selected occurrence/context/flip sources need no unrelated canonical arrays.
 for(int source=0;source<3;source++)for(int flips=0;flips<4;flips++){
  auto f=fixture(0,flips,false);auto &m=f.assetMetadata[0];auto &i=f.tileInstances[0];
  if(source==0){RemasterTileContext c;c.hasHeight=true;c.height.fill(-3);f.contextGeometry[1]=c;}
  if(source==1){i.hasPlacementHeight=true;i.placementHeight.fill(-3);}
  if(source==2){m.geometryByFlip=true;auto &g=m.flipGeometry[S9xRemasterGeometryFlipIndex(i.hFlip,i.vFlip)];g.hasHeight=true;g.height.fill(-3);}
  require(S9xRemasterRestoreSceneSubscreenWalls(f)==1,"selected source without canonical height/normal");
  RemasterLightingFieldCache cpu;cpu.update(f,true);
  require(cpu.height[200]==S9xRemasterSignedHeightField(-8,-3) && !cpu.authoredNormals[100],"selected signed source survives CPU");
 }
 // User-approved floor/stair receiver behavior is outside this wall repair.
 auto f=fixture(1,0,false);f.assetMetadata[0].tileRole=RemasterTileRole::Floor;
 require(!S9xRemasterRestoreSceneSubscreenWalls(f),"protected height-only stair landing unchanged");
 std::cout<<"PASS channel presence/HV/zero/ownership/HUD/CPU transport\n";
}
