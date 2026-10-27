#include "../runtime/dungeon_visible_face_runtime.h"
#include "../runtime/lighting_field_cache.h"
#include <cstdlib>
#include <iostream>
#include <memory>
static void need(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
using C=RemasterDungeonFloorContext;
struct Fixture {
 std::array<uint8_t,16384> maps={};std::array<uint8_t,8192> attrs={};std::array<uint8_t,12876> rom={};
 std::array<std::array<C::Placement,4096>,2> tiles={};
 std::array<RemasterDungeonHeightMap,2> heights;
 RemasterFrame frame; C c;
 int axis,flip,bg,plane,down,foot,first;
 Fixture(int axis_,int flip_,int bg_,int plane_):axis(axis_),flip(flip_),bg(bg_),plane(plane_) {
  attrs.fill(1);c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.constructedMaps=maps.data();c.collisionAttributes=attrs.data();c.dungeonTemplateData=rom.data();
  frame.width=axis?16:56;frame.height=axis?56:16;frame.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
  frame.mainPixels.resize(frame.width*frame.height);frame.scenePixels.resize(frame.mainPixels.size());frame.subPixels.resize(frame.mainPixels.size());
  c.backgroundScrollX=80;c.backgroundScrollY=80;
  down=(flip&(axis?2:1))?-1:1;
  for(auto &m:heights){m.roomIndex=7;m.hasRoomGeometry=m.hasPlaneOffsets=true;m.offsets.fill(255);for(auto &a:m.planeOffsets)a.fill(255);for(auto &a:m.occupiedFloorOwners)a.fill(-1);m.roomDiagnostics=std::make_shared<RemasterRoomDiagnostics>();}
  const int off[2]={axis?0x2e8:0x2c8,axis?0x328:0x308};
  for(int family=0;family<2;family++) {
   for(int t=0;t<2;t++)for(int d=0;d<4;d++){int word=100+family*40+t*4+d;rom[off[family]+2*(t*4+d)]=word;rom[off[family]+2*(t*4+d)+1]=word>>8;}
   const int wordFlip=axis?0x8000:0x4000;
   for(int t=0;t<2;t++)for(int d=0;d<4;d++){int word=(rom[off[family]+2*(t*4+3-d)]|rom[off[family]+2*(t*4+3-d)+1]<<8)^wordFlip;rom[off[family]+16+2*(t*4+d)]=word;rom[off[family]+17+2*(t*4+d)]=word>>8;}
  }
  // Logical bands are ordered high to low; H/V and layer swap change actual
  // final words, raster indexing, support cell and selected source.
  for(int n=0;n<7;n++)for(int t=0;t<2;t++) {
   int depth=down>0?n:6-n,x=10+(axis?t:depth),y=10+(axis?depth:t),cell=y*64+x;
   int layer=n<3?bg:1-bg,word=0;
   if(n<6) {
    int family=n<3?0:1,local=n%3,tcanonical=(flip&(axis?1:2))?1-t:t;
    word=(rom[off[family]+2*(tcanonical*4+local)]|rom[off[family]+2*(tcanonical*4+local)+1]<<8)^((flip&1?0x4000:0)|(flip&2?0x8000:0));
   } else word=300+t;
   int at=(layer?0:8192)+cell*2;maps[at]=word;maps[at+1]=word>>8;
   auto &tile=tiles[layer][cell];tile.tileHash=10000+(word&1023);tile.hasHeight=true;tile.hflip=word&0x4000;tile.vflip=word&0x8000;tile.surface=n<6?C::Surface::Wall:C::Surface::Unknown;
   if(n<6)for(int py=0;py<8;py++)for(int px=0;px<8;px++)tile.height[py*8+px]=7-(axis?py:px);
   RemasterFrameAssetMetadata m;m.tileId={tile.tileHash,1,4};m.hasHeight=true;m.height=tile.height;frame.assetMetadata.push_back(m);
   RemasterFrameTileInstance inst;inst.tileId=m.tileId;inst.source=RemasterSourceType::Background;inst.sourceIndex=layer;inst.tileNumber=word&1023;inst.palette=(word>>10)&7;inst.hFlip=tile.hflip;inst.vFlip=tile.vflip;
   inst.heightOffset=n<6?90:30;frame.tileInstances.push_back(inst);
   heights[layer].offsets[cell]=inst.heightOffset;heights[layer].producerHashes[cell]=tile.tileHash;heights[layer].producerFlips[cell]=tile.hflip|(tile.vflip<<1);
   if(n==6){attrs[plane*4096+cell]=0;heights[layer].planeOffsets[plane][cell]=30;heights[layer].occupiedFloorOwners[plane][cell]=100+plane;
    auto &diag=const_cast<RemasterRoomDiagnostics&>(*heights[layer].roomDiagnostics).cells[plane*4096+cell];diag.planeObserved=true;diag.floorId=100+plane;diag.floorBase=30;if(t==0)foot=cell;}
   if(n==0 && t==0)first=cell;
   for(int py=0;py<8;py++)for(int px=0;px<8;px++) {
    int sx=(x-10)*8+px,sy=(y-10)*8+py,k=(tile.vflip?7-py:py)*8+(tile.hflip?7-px:px),screen=sy*frame.width+sx;
    RemasterFramePixel p{(uint32_t(RemasterSourceType::Background)<<24)|(uint32_t(layer)<<16),uint32_t(frame.tileInstances.size()),uint8_t(k)};
    frame.mainPixels[screen]=frame.scenePixels[screen]=p;
   }
  }
 }
  void run(RemasterVisibleFaceTemplateCache *cache=nullptr){S9xRemasterApplyVisibleDungeonFaces(frame,c,tiles[0].data(),tiles[1].data(),heights[0],heights[1],cache);}
 int baseAt(int cell){int x=(cell%64-10)*8,y=(cell/64-10)*8;return frame.tileInstances[frame.scenePixels[y*frame.width+x].instanceId-1].heightOffset;}
};
int main(){
  { // Full-room recognition is independent of the selected camera. Cache a
    // second offscreen section too, not a camera-visible ownership winner.
   RemasterVisibleFaceTemplateCache cache;
   auto a=std::make_unique<Fixture>(0,0,0,0);a->run(&cache);
   auto b=std::make_unique<Fixture>(0,0,0,0);b->c.backgroundScrollX+=8;b->run(&cache);
#ifdef REMASTER_VISIBLE_ROOM_CANDIDATES
   need(cache.modelBuilds==1 && cache.modelHits==1,"camera motion reuses complete-room candidate recognition");
   b=std::make_unique<Fixture>(0,0,0,0);b->tiles[1][4095].height.fill(2);b->run(&cache);
   need(cache.modelBuilds==2,"offscreen local source edit invalidates room model");
   b=std::make_unique<Fixture>(0,0,0,0);b->run(&cache);
   need(cache.modelBuilds==3,"switchback rebuilds changed local source");
#else
   need(false,"FAIL complete-room candidate recognition reruns on every camera");
#endif
  }
 for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++)for(int plane=0;plane<2;plane++) {
  auto f=std::make_unique<Fixture>(axis,flip,bg,plane);const auto before=f->heights[bg].planeOffsets;f->run();
  need(f->baseAt(f->first)==70,"high band receives occupied-floor visual base");
  int second=f->first+(axis?f->down*64:f->down);
  need(f->baseAt(second)==62,"exposed two-BG face anchored to exact occupied floor");
  need(f->baseAt(f->foot)==30,"visible floor unchanged");need(f->heights[bg].planeOffsets==before,"physical planes unchanged");
  RemasterLightingFieldCache cpu;cpu.update(f->frame,true);int x=(second%64-10)*8,y=(second/64-10)*8;need(cpu.height[(y*f->frame.width+x)*2]>0,"selected CPU geometry receives solved visual occurrence");
  auto ambiguous=std::make_unique<Fixture>(axis,flip,bg,plane);int floorLayer=1-bg;
  for(int cell:{ambiguous->foot,ambiguous->foot+(axis?1:64)}) {
   ambiguous->attrs[(1-plane)*4096+cell]=0;ambiguous->heights[floorLayer].occupiedFloorOwners[1-plane][cell]=555;
   ambiguous->heights[floorLayer].planeOffsets[1-plane][cell]=30;
    auto &d=const_cast<RemasterRoomDiagnostics&>(*ambiguous->heights[floorLayer].roomDiagnostics).cells[(1-plane)*4096+cell];d.planeObserved=true;d.floorId=555;d.floorBase=30;
  }
  ambiguous->run();need(ambiguous->baseAt(second)==90,"distinct plane/owner cannot select even equal bases");
  auto blocked=std::make_unique<Fixture>(axis,flip,bg,plane);for(int cell:{blocked->foot,blocked->foot+(axis?1:64)})blocked->attrs[plane*4096+cell]=1;blocked->run();need(blocked->baseAt(second)==90,"collision blocker denies floor owner");
  auto overwritten=std::make_unique<Fixture>(axis,flip,bg,plane);int at=(bg?0:8192)+2*second;overwritten->maps[at]^=1;overwritten->run();need(overwritten->baseAt(second)==90,"changed ROM footprint denies visual publication");
  auto unexposed=std::make_unique<Fixture>(axis,flip,bg,plane);
  for(int t=0;t<2;t++)for(int k=0;k<8;k++) {
   int cell=unexposed->foot+(axis?t:t*64),x=(cell%64-10)*8,y=(cell/64-10)*8;
   int at=(y+(axis?(unexposed->down>0?0:7):k))*unexposed->frame.width+x+(axis?k:(unexposed->down>0?0:7));
   unexposed->frame.mainPixels[at]={};unexposed->frame.scenePixels[at]={};
  }
  unexposed->run();need(unexposed->baseAt(second)==90,"unexposed floor edge cannot anchor wall");
  auto sameArt=std::make_unique<Fixture>(axis,flip,bg,plane);
  for(int cell:{sameArt->foot,sameArt->foot+(axis?1:64)}) {
   sameArt->heights[floorLayer].planeOffsets[plane][cell]=55;sameArt->heights[floorLayer].offsets[cell]=55;
   const_cast<RemasterRoomDiagnostics&>(*sameArt->heights[floorLayer].roomDiagnostics).cells[plane*4096+cell].floorBase=55;
   for(size_t k=0;k<sameArt->frame.scenePixels.size();k++){const auto &p=sameArt->frame.scenePixels[k];if(p.instanceId && sameArt->frame.tileInstances[p.instanceId-1].sourceIndex==floorLayer) {
    int xx=(sameArt->c.backgroundScrollX+k%sameArt->frame.width)&511,yy=(sameArt->c.backgroundScrollY+k/sameArt->frame.width)&511;
    if(yy/8*64+xx/8==cell)sameArt->frame.tileInstances[p.instanceId-1].heightOffset=55;
   }}
  }
  sameArt->run();need(sameArt->baseAt(second)==87 && sameArt->baseAt(sameArt->foot)==55,"same artwork follows its own occupied floor, not max or nearby datum");
  auto hidden=std::make_unique<Fixture>(axis,flip,bg,plane);
  for(int cell:{hidden->foot,hidden->foot+(axis?1:64)})hidden->heights[floorLayer].planeOffsets[plane][cell]=55;
  hidden->run();need(hidden->baseAt(second)==90,"hidden occupied plane cannot claim a differently based selected floor");
 }
  // A selected flat relief is not necessarily the support datum. A raised
  // authored sheet touching a wall may be another object/overpass; until an
  // independent contact identifies it, only zero-local physical support is
  // admitted. Zero is data, not an absent source.
  auto raised=std::make_unique<Fixture>(0,0,0,0);
  for(int cell:{raised->foot,raised->foot+64}) {
   auto &t=raised->tiles[1][cell];t.height.fill(4);
   for(auto &m:raised->frame.assetMetadata)if(m.tileId.hash==t.tileHash)m.height=t.height;
  }
  raised->run();need(raised->baseAt(raised->first)==90,"nonzero local floor sheet is not a proven support datum");
  auto fallback=std::make_unique<Fixture>(0,0,0,0);
  RemasterFrameTileInstance cover;cover.source=RemasterSourceType::Background;cover.sourceIndex=0;cover.tileId={999999,1,4};
  fallback->frame.tileInstances.push_back(cover);
  for(int cell:{fallback->foot,fallback->foot+64}) {
   int x=(cell%64-10)*8,y=(cell/64-10)*8;
   for(int k=0;k<8;k++)fallback->frame.mainPixels[(y+k)*fallback->frame.width+x]={uint32_t(RemasterSourceType::Background)<<24,uint32_t(fallback->frame.tileInstances.size()),uint8_t(k*8)};
  }
  fallback->run();need(fallback->baseAt(fallback->first)==90,"opposite-BG height fallback is not exposed floor contact");
  auto clipped=std::make_unique<Fixture>(0,0,0,0);
  // Keep only the downhill edge of the first band; chain/floor stay observed.
  for(size_t at=0;at<clipped->frame.mainPixels.size();at++) {
   auto px=clipped->frame.mainPixels[at];
   if(px.instanceId && clipped->frame.tileInstances[px.instanceId-1].heightOffset==90 && at%clipped->frame.width<8 && at%clipped->frame.width%8!=7)
    clipped->frame.mainPixels[at]=clipped->frame.scenePixels[at]={};
  }
  clipped->run();
  for(const auto &px:clipped->frame.scenePixels)if(px.instanceId)
   need(clipped->frame.tileInstances[px.instanceId-1].heightOffset!=70,"single exposed edge does not prove a face direction");
  // Warm the only persistent cache (exact ROM/map sections), then independently
  // mutate every frame-local dependency. Compare against an uncached consumer;
  // restore the raster between calls, as actual BeginFrame does.
  for(int mutation=0;mutation<12;mutation++) {
   RemasterVisibleFaceTemplateCache cache;
   auto warm=std::make_unique<Fixture>(0,0,0,0);warm->run(&cache);
   auto a=std::make_unique<Fixture>(0,0,0,0),b=std::make_unique<Fixture>(0,0,0,0);
   for(auto *f:{a.get(),b.get()}) {
    int cell=f->foot;
    if(mutation==0)f->maps[8192+f->first*2]^=1;
    if(mutation==1)f->rom[0x2c8]^=1;
    if(mutation==2)for(auto &m:f->frame.assetMetadata)m.hasHeight=false;
    if(mutation==3)f->tiles[0][f->first].tileHash++;
    if(mutation==4)f->tiles[0][f->first].hflip=true;
    if(mutation==5)for(auto &px:f->frame.mainPixels)px={};
    if(mutation==6)f->attrs.fill(1);
    if(mutation==7)for(auto &p:f->heights[1].occupiedFloorOwners)p.fill(-1);
    if(mutation==8)for(auto &p:f->heights[1].planeOffsets)p.fill(255);
    if(mutation==9)for(auto &inst:f->frame.tileInstances)inst.hasPlacementHeight=true;
    if(mutation==10)for(auto &d:const_cast<RemasterRoomDiagnostics&>(*f->heights[1].roomDiagnostics).cells)d.heightConflict=true;
    if(mutation==11) {f->c.backgroundScrollX++;f->tiles[1][cell].height.fill(0);}
    RemasterFrameChannelResolver resolver(f->frame);
    for(size_t k=0;k<f->frame.mainPixels.size();k++) {
     const auto full=resolver.select(f->frame,k),height=resolver.selectHeight(f->frame,k);
     need(full.visible.instanceId==height.visible.instanceId && full.donors[0].instanceId==height.donors[0].instanceId && full.donors[0].tilePixel==height.donors[0].tilePixel,"height-only selector preserves production channel policy");
    }
   }
   a->run(&cache);b->run();
   need(a->frame.scenePixels.size()==b->frame.scenePixels.size(),"same exposure size");
   for(size_t k=0;k<a->frame.scenePixels.size();k++) {
    auto x=a->frame.scenePixels[k],y=b->frame.scenePixels[k];
    need(x.instanceId==y.instanceId && x.tilePixel==y.tilePixel,"all dependency mutations cached/uncached identity parity");
   }
   need(a->baseAt(a->first)==b->baseAt(b->first),"all dependency mutations cached/uncached base parity");
  }
  std::cout<<"PASS real two-BG exposed owner adapter, both axes/HV/BGs/planes and negative cases\n";
}
