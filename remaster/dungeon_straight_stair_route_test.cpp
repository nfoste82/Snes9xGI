// Real reference frame-loop snapshots, no synthesized construction phases/maps.
// Project actual PPU BG words (not compositing/visibility) through EndFrame.
#include "remaster.h"
#include "dungeon_transition_attributes.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;uint32_t S9xRemasterCurrentOwner=0;
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(int argc,char **argv){try {
 need(argc==4,"usage: runtime.bin own-capture.profile.toml templates.bin");
 std::ifstream in(argv[1],std::ios::binary);char magic[8];in.read(magic,8);need(!memcmp(magic,"ALTPRT1\0",8),"route format");
 RemasterProfile p;std::vector<RemasterProfileDiagnostic> errors;need(S9xRemasterLoadProfile(argv[2],p,errors),"profile");
 std::ifstream tf(argv[3],std::ios::binary);std::vector<uint8_t> templates{std::istreambuf_iterator<char>(tf),{}};need(templates.size()==12884,"templates");
 S9xRemasterSetProfile(p);S9xRemasterSetLiveFramesEnabled(true);auto &s=S9xRemasterState();
 for(int sub:{6,7,9,10,14,15,17,20,21}) {
  need(!S9xRemasterDungeonRoomScroll(sub),"unsupported handler accepted as scroll");
  RemasterDungeonFloorContext rejected;rejected.module=7;rejected.submodule=sub;rejected.roomIndex=0x61;rejected.previousRoomIndex=0x51;
  need(S9xRemasterDungeonRoomForScreenPosition(rejected,0,0)==0x61,"unsupported handler borrowed predecessor");
 }
 auto data=std::make_unique<std::array<uint8_t,196620>>();auto complete=std::make_unique<std::array<uint8_t,8192>>();
 std::array<uint16_t,256*224> screen={};auto frame=std::make_unique<RemasterFrame>();
 uint16_t completedRoom=0xffff;int frames=0,stairs=0,scrolls=0,sourcePixels=0,loaded=0,north=0,south=0,effectiveChecks=0;
  struct Settled { RemasterDungeonHeightMap bg[2]; std::array<uint8_t,16384> words; };
  std::map<uint16_t,std::unique_ptr<Settled>> settled;
  int incomingChecks[2]={},incomingRaised[2]={},planeChecks[2][2]={};
  for(int pass=0;pass<2;pass++) {
  in.clear();in.seekg(8);completedRoom=0xffff;bool awaitingSettled=false;
  while(in.read(reinterpret_cast<char*>(data->data()),data->size())) {
  const auto *r=data->data(),*g=r+0x20000;auto word=[&](int at){return r[at]|r[at+1]<<8;};
  RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.romIdentity=0x66871d66be19ad2cULL;
  c.module=r[0x10];c.submodule=r[0x11];c.roomIndex=word(0xa0);c.previousRoomIndex=word(0xa2);c.constructionPhase=r[0xb0];
   c.attributeConstructionState=r[0x200];c.collisionMode=r[0x46c];c.linkPlane=r[0xee]&1;c.linkX=word(0x22);c.linkY=word(0x20);
   c.straightStairPlane=r[0x48a];
   if(std::getenv("ALTTP_TEST_NO_STAIR_DESTINATION_PLANE"))c.straightStairPlane=255;
  c.groundedEvidence=false; // inter-room stairs never establish level-door anchors
  c.backgroundScrollX=word(196608);c.backgroundScrollY=word(196610);c.graphicsBase=word(196616);c.bg1GraphicsBase=word(196612);
  c.constructedMaps=r+0x2000;c.graphics=g;c.collisionAttributes=r+0x12000;c.dungeonTemplateData=templates.data()+8;
  c.scrollDirection=r[0x418];c.destinationOriginX=word(0x62c);c.destinationOriginY=word(0x62e);
  const bool stair=c.submodule==18 || c.submodule==19;
  c.hasDestinationOrigin=S9xRemasterDungeonRoomScroll(c.submodule) && c.constructionPhase>=S9xRemasterDungeonRoomLoadedPhase(c.submodule);
  if(!stair || c.constructionPhase<3 || completedRoom!=c.roomIndex)completedRoom=0xffff;
  if(stair && c.constructionPhase>=3 && c.attributeConstructionState!=5) {
   if(completedRoom==0xffff){need(S9xRemasterCompleteDungeonAttributes(r,*complete),"actual stair collision completion rejected");completedRoom=c.roomIndex;}
   c.collisionAttributes=complete->data();c.hasCompletedTransitionAttributes=true;
  }
  S9xRemasterDungeonStairsFromAttributes(c);
  S9xRemasterBeginFrame(screen.size(),256,256,224);
  for(int bg=0;bg<2;bg++) {
   const int base=bg?c.graphicsBase:c.bg1GraphicsBase;
   // Footer: scroll x/y, BG1 char/map, BG2 char/map.
   const int mapBase=word(bg?196618:196614);std::map<int,uint32_t> instances;
   for(int y=0;y<224;y++)for(int x=0;x<256;x++) {
    int xx=(c.backgroundScrollX+x)&511,yy=(c.backgroundScrollY+y)&511;
    int at=(mapBase+(yy/256)*0x800+(xx/256)*0x400+(yy/8%32)*32+xx/8%32)&32767;
    int w=g[at*2]|g[at*2+1]<<8;auto found=instances.find(w);uint32_t n;
    if(found==instances.end()) {uint8_t indices[64]={};
    for(int py=0;py<8;py++)for(int px=0;px<8;px++)for(int b=0;b<4;b++)indices[py*8+px]|=((g[uint16_t(base+(w&1023)*32+b/2*16+py*2+b%2)]>>(7-px))&1)<<b;
    RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=bg;i.tileNumber=w&1023;i.palette=(w>>10)&7;i.hFlip=w&0x4000;i.vFlip=w&0x8000;i.tileId={S9xRemasterHashTile(4,indices),1,4};
    s.tileInstances.push_back(i);n=s.tileInstances.size();instances[w]=n;
    }else n=found->second;
    const auto &i=s.tileInstances[n-1];auto pos=y*256+x;
    (bg?s.mainOwners:s.subOwners)[pos]=0x02000000|(bg<<16);(bg?s.mainInstanceIds:s.subInstanceIds)[pos]=n;
    (bg?s.mainTilePixels:s.subTilePixels)[pos]=(i.vFlip?7-yy%8:yy%8)*8+(i.hFlip?7-xx%8:xx%8);
   }
  }
   S9xRemasterEndFrame(screen.data(),256,256,224,frame.get(),&c);
   if(pass==0) {
    if(stair)awaitingSettled=true;
    if(awaitingSettled && !c.submodule && !settled.count(c.roomIndex)) {
     auto oracle=std::make_unique<Settled>();oracle->bg[0]=s.runtimeDungeonBg1HeightMap;oracle->bg[1]=s.runtimeDungeonHeightMap;
     std::copy_n(c.constructedMaps,16384,oracle->words.begin());settled[c.roomIndex]=std::move(oracle);
     awaitingSettled=false;
    }
    continue;
   }
   frames++;
   if(stair && c.constructionPhase==10) {
    need(settled.count(c.roomIndex),"missing settled destination oracle");const auto &oracle=*settled.at(c.roomIndex);
    auto expected=*frame;
    for(auto &i:expected.tileInstances){i.heightOffset=0;i.hasPlacementHeight=false;}
    auto dest=c;dest.submodule=0;dest.previousRoomIndex=0xffff;dest.constructedMaps=oracle.words.data();
    for(int bg=0;bg<2;bg++)S9xRemasterApplyDungeonHeightMap(expected,dest,oracle.bg[bg],nullptr,nullptr,bg);
    for(int y=0;y<224;y++)for(int x=0;x<256;x++) {
     if(uint16_t(c.backgroundScrollY+y-c.destinationOriginY)>=512)continue;
     const int cell=(((c.backgroundScrollY+y)&511)/8)*64+((c.backgroundScrollX+x)&511)/8;
     for(int bg=0;bg<2;bg++) {
      const auto &px=(bg?frame->mainPixels:frame->subPixels)[y*256+x];
      const auto &ex=(bg?expected.mainPixels:expected.subPixels)[y*256+x];
      const auto &a=frame->tileInstances[px.instanceId-1],&e=expected.tileInstances[ex.instanceId-1];const auto &m=oracle.bg[bg];
      const int at=(bg?0:8192)+cell*2,w=oracle.words[at]|oracle.words[at+1]<<8;
      if(m.producerHashes[cell]!=a.tileId.hash || m.producerFlips[cell]!=(a.hFlip|(a.vFlip<<1)) ||
       a.tileNumber!=(w&1023) || a.palette!=((w>>10)&7))continue;
      if(a.heightOffset!=e.heightOffset || a.hasPlacementHeight!=e.hasPlacementHeight ||
       (a.hasPlacementHeight && a.placementHeight[px.tilePixel]!=e.placementHeight[ex.tilePixel])) {
       std::cerr<<"incoming frame="<<frames-1<<" room="<<c.roomIndex<<" bg="<<bg<<" cell="<<cell<<" plane="<<int(c.linkPlane)<<" base="<<a.heightOffset<<" expected="<<e.heightOffset<<'\n';
       need(false,"incoming effective destination height differs from settled solve");
      }
      incomingChecks[bg]++;
      incomingRaised[bg]+=e.heightOffset!=0;
     }
    }
    for(int bg=0;bg<2;bg++)for(int plane=0;plane<2;plane++) {
     const auto &actual=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;const auto &m=oracle.bg[bg];
     for(int cell=0;cell<4096;cell++)if(m.producerHashes[cell]==actual.producerHashes[cell] && m.planeOffsets[plane][cell]!=255) {
      if(actual.planeOffsets[plane][cell]!=m.planeOffsets[plane][cell]) {
       std::cerr<<"plane mismatch frame="<<frames-1<<" bg="<<bg<<" plane="<<plane<<" cell="<<cell<<" actual="<<actual.planeOffsets[plane][cell]<<" expected="<<m.planeOffsets[plane][cell]<<'\n';
       need(false,"incoming gameplay-plane base differs from settled solve");
      }
      planeChecks[bg][plane]++;
     }
    }
   }
  if(stair) {
   stairs++;north+=c.submodule==18;south+=c.submodule==19;
   if(c.constructionPhase<3)need(s.outgoingGeometry.valid,"actual stair lost outgoing geometry before load");
   if(c.constructionPhase>=3) {need(s.runtimeDungeonHeightValid && s.runtimeDungeonHeightMap.roomIndex==c.roomIndex,"actual stair destination unpublished");loaded++;}
   if(c.constructionPhase==10)scrolls++;
   need(!s.runtimeDungeonHeightMap.entrySupportConsumed,"stairs invented level-door anchor");
   for(int y=0;y<224;y++)for(int x=0;x<256;x++) {
    auto render=c;if(s.outgoingGeometry.valid)render.previousRoomIndex=s.outgoingGeometry.room;
    const bool source=c.constructionPhase<3 || (c.constructionPhase==10 && uint16_t(c.backgroundScrollY+y-c.destinationOriginY)>=512);
    if(source){need(s.outgoingGeometry.valid,"actual stair lost visible outgoing geometry");need(S9xRemasterDungeonRoomForScreenPosition(render,x,y)==render.previousRoomIndex,"actual stair source ownership");sourcePixels++;
     for(auto pixels:{&frame->mainPixels,&frame->subPixels}) {
      const auto &px=(*pixels)[y*256+x];if(!px.instanceId)continue;const auto &i=frame->tileInstances[px.instanceId-1];
      const auto &m=i.sourceIndex?s.previousRuntimeDungeonHeightMap:s.previousRuntimeDungeonBg1HeightMap;
      const int cell=((c.backgroundScrollY+y)&511)/8*64+((c.backgroundScrollX+x)&511)/8;
      const int at=(i.sourceIndex?0:8192)+cell*2;const int w=s.outgoingFinalMaps[at]|s.outgoingFinalMaps[at+1]<<8;
      if(m.producerZeroFloor[cell] && m.offsets[cell]!=255 && m.offsets[cell]!=0 &&
       m.producerHashes[cell]==i.tileId.hash && m.producerFlips[cell]==(i.hFlip|(i.vFlip<<1)) &&
       i.tileNumber==(w&1023) && i.palette==((w>>10)&7)) {
       need(i.heightOffset==m.offsets[cell],"actual visible outgoing floor lost effective base");effectiveChecks++;
      }
     }
    }
   }
  }
 }
  }
  need(in.eof() && frames==801 && north && south && scrolls==120 && effectiveChecks>0 && incomingChecks[0] && incomingChecks[1],"complete roundtrip denominator");
  std::cout<<"incoming effective checks BG1/BG2="<<incomingChecks[0]<<'/'<<incomingChecks[1]<<" nonzero="<<incomingRaised[0]<<'/'<<incomingRaised[1]<<" plane checks="<<planeChecks[0][0]<<','<<planeChecks[0][1]<<'/'<<planeChecks[1][0]<<','<<planeChecks[1][1]<<'\n';
 std::cout<<"PASS actual reference roundtrip frames="<<frames<<" stair_frames="<<stairs<<" scroll_frames="<<scrolls<<" destination_frames="<<loaded<<" source_coordinate_checks="<<sourcePixels<<" effective_nonzero_floor_checks="<<effectiveChecks<<"\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
