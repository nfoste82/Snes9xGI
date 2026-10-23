#include "../runtime/remaster.h"
#include "../runtime/hidden_emitter_samples.h"
#include <stdexcept>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;uint32_t S9xRemasterCurrentOwner=0;
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();auto gfx=std::make_unique<std::array<uint8_t,65536>>();(*gfx)[32]=128;
 uint8_t pixels[64]={};pixels[0]=1;RemasterTileContentId id={S9xRemasterHashTile(4,pixels),1,4};
 RemasterProfile p;auto &a=p.assets[id];a.tileId=id;a.hasEmission=a.hasHeight=a.hasNormals=true;a.emissionDepth=2;
 for(int k=0;k<64;k++){a.height[k]=7-k/8;a.emissionRgba[k*4]=255;a.emissionRgba[k*4+3]=25;a.normalXyz[k*3]=128;a.normalXyz[k*3+1]=255;a.normalXyz[k*3+2]=128;}
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.constructedMaps=maps->data();c.graphics=gfx->data();
 int cell=8*64+20;(*maps)[cell*2]=(*maps)[8192+cell*2]=1;(*maps)[cell*2+1]=(*maps)[8193+cell*2]=0xc0;
 auto m=std::make_unique<RemasterDungeonHeightMap>();m->roomIndex=7;m->offsets[cell]=40;
 RemasterDungeonHeightMap::LocalHeightPreview v;v.cell=cell;for(int k=0;k<64;k++)v.height[k]=7-k%8;m->localHeightPreviews.push_back(v);
 auto before=a;float referencePower=-1;
 for(int scroll=56;scroll<=80;scroll++) {
  c.backgroundScrollY=scroll;RemasterFrame f;f.width=256;f.height=64;f.mainPixels.resize(256*64);f.subPixels.resize(256*64);f.originalRgb555.resize(256*64);
  auto d=S9xRemasterAppendOffscreenRoomLights(f,c,p,m.get(),m.get(),S9xRemasterHashTile);
  need(f.lights.empty() && d.visiblePixels+f.hiddenEmitters.size()==64,"edge lost/doubled source area");
  for(auto &e:f.hiddenEmitters){need(e.normal[1]<-0.99f && e.depth==2 && e.emission[3]==25,"depth/normal/power changed");e.y+=scroll;}
  // Add visible oracle members at actual fixed-world positions.
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)if(64+y>=scroll && 64+y<scroll+64) {
   int k=(7-y)*8+7-x;RemasterFrameHiddenEmitter e;e.x=160+x+0.5f;e.y=64+y+0.5f;e.z=(40+v.height[k])*16.0f/255;e.depth=2;e.normal={{0,-1,0}};e.emission={{255,0,0,25}};f.hiddenEmitters.push_back(e);
  }
  need(f.hiddenEmitters.size()==64,"partial visibility duplication");
  auto samples=S9xRemasterHiddenDirectSamples(f);float power=0;for(auto &s:samples)power+=s.radiance[0];
  if(referencePower<0)referencePower=power;need(power==referencePower && power==6400,"source power differs across edge");
  f.representativeEmitters=3;auto clustered=S9xRemasterHiddenDirectSamples(f,false,true);float clusteredPower=0;for(auto &s:clustered)clusteredPower+=s.radiance[0];need(clusteredPower==power,"clustering power loss");
  for(auto &e:f.hiddenEmitters){e.x-=200;e.y-=100;}std::vector<uint8_t> bytes;RemasterFrame replay;
  need(S9xSerializeRemasterFrame(f,bytes) && S9xDeserializeRemasterFrame(bytes,replay) && replay.hiddenEmitters.size()==64,"signed HES2 capture roundtrip");
  auto signedSamples=S9xRemasterHiddenDirectSamples(replay,false,true);need(!signedSamples.empty(),"signed clustering invalid");
  f.hiddenEmitters[0].depth=0;f.hiddenEmitters[0].omnidirectional=false;
  need(S9xRemasterHiddenDirectSamples(f).size()==253,"planar signed source incorrectly sphere-promoted");
  f.hiddenEmitters[0].x=-512;need(!S9xSerializeRemasterFrame(f,bytes),"unbounded room source accepted");
 }
 need(a.height==before.height && a.emissionRgba==before.emissionRgba && a.normalXyz==before.normalXyz,"authored metadata mutated");
 c.backgroundScrollY=80;m->offsets[cell]=255;RemasterFrame f;f.width=256;f.height=64;
 need(S9xRemasterAppendOffscreenRoomLights(f,c,p,m.get(),m.get(),S9xRemasterHashTile).unknownBases==1 && f.hiddenEmitters.empty(),"unknown base invented");
 m->offsets[cell]=40;c.backgroundScrollY=56;
 f.mainPixels.resize(256*64);f.subPixels.resize(256*64);f.originalRgb555.resize(256*64);f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 for(auto &px:f.mainPixels)px.owner=0x02020000;
 need(S9xRemasterAppendOffscreenRoomLights(f,c,p,m.get(),m.get(),S9xRemasterHashTile).hiddenPixels==64,"HUD recovery changed");
 f.hiddenEmitters.clear();for(auto &px:f.mainPixels)px.owner=0x02010000;
 need(S9xRemasterAppendOffscreenRoomLights(f,c,p,m.get(),m.get(),S9xRemasterHashTile).visiblePixels==64 && f.hiddenEmitters.empty(),"world-covered source double recovered");
 std::cout<<"PASS exact partial/offscreen area, power, signed depth/flip sources, deterministic clusters, HES2 and unresolved rejection\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
