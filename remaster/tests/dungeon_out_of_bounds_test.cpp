// Exercise the production wall graph and selected CPU field, not just an enum.
#define REMASTER_WALL_BAND_DIAGNOSTICS 1
#define main visibleFaceLegacyMain
#include "dungeon_visible_face_runtime_test.cpp"
#undef main
#include "../runtime/remaster.h"
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;

static constexpr auto oobRole=static_cast<RemasterTileRole>(11);
static constexpr auto oobSurface=static_cast<C::Surface>(7);
struct PlateauFixture : Fixture {
 int plateau,far; std::vector<std::pair<int,uint32_t>> added;
 PlateauFixture(int axis,int flip,int bg,int plane):Fixture(axis,flip,bg,plane) {
  const int step=axis?64:1;
  plateau=first-down*step;far=plateau-down*step;
  const auto oldWidth=frame.width,oldHeight=frame.height;
  const auto old=frame.mainPixels;
  if(axis)frame.height+=16;else frame.width+=16;
  int shift=down>0?16:0;
  if(axis)c.backgroundScrollY-=shift;else c.backgroundScrollX-=shift;
  frame.mainPixels.assign(frame.width*frame.height,{});
  for(unsigned y=0;y<oldHeight;y++)for(unsigned x=0;x<oldWidth;x++)
   frame.mainPixels[(y+(axis?shift:0))*frame.width+x+(axis?0:shift)]=old[y*oldWidth+x];
  for(int cell:{plateau,far}) {
   auto &t=tiles[bg][cell];t.surface=oobSurface;t.tileHash=20000+cell;
   RemasterFrameAssetMetadata m;m.tileId={t.tileHash,1,4};m.tileRole=oobRole;
   frame.assetMetadata.push_back(m);
   RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=bg;i.tileId=m.tileId;
   frame.tileInstances.push_back(i);added.push_back({cell,uint32_t(frame.tileInstances.size())});
   for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
    const int sx=cell%64*8-c.backgroundScrollX+x,sy=cell/64*8-c.backgroundScrollY+y;
    frame.mainPixels[sy*frame.width+sx]={uint32_t(RemasterSourceType::Background)<<24|uint32_t(bg)<<16,uint32_t(frame.tileInstances.size()),uint8_t(y*8+x)};
   }
  }
  frame.scenePixels=frame.mainPixels;frame.subPixels.assign(frame.mainPixels.size(),{});
  frame.originalRgb555.resize(frame.mainPixels.size());
  frame.sceneRgb555.resize(frame.mainPixels.size());
 }
 size_t at(int cell)const{return (cell/64*8-c.backgroundScrollY)*frame.width+cell%64*8-c.backgroundScrollX;}
 void local(int value,bool nonflat=false) {
  for(auto [cell,id]:added) {
   auto &t=tiles[bg][cell];t.hasHeight=true;t.height.fill(value);if(nonflat)t.height[27]++;
   for(auto &m:frame.assetMetadata)if(m.tileId==frame.tileInstances[id-1].tileId){m.hasHeight=true;m.height=t.height;}
  }
 }
};

static void blocker(int number) {
 if(number==1) {
  auto f=std::make_unique<PlateauFixture>(1,0,1,0);
  auto &map=f->heights[1];
  for(int cell:{f->plateau,f->far})map.offsets[cell]=50;
  S9xRemasterApplyDungeonHeightMap(f->frame,f->c,map);
  need(f->frame.tileInstances[f->frame.mainPixels[f->at(f->far)].instanceId-1].heightOffset==0,"B1 role-only OOB must reject physical map datum");
  f->c.roomIndex=0x55;f->c.backgroundScrollX=100;f->c.backgroundScrollY=330;
  S9xRemasterApplyDungeonFloorHeight(f->frame,f->c,50);
  for(auto [cell,id]:f->added)need(f->frame.tileInstances[id-1].heightOffset==0,"B1 legacy OOB must reject upper-floor datum");
  // Exact legacy counterexample uses a one-pixel occurrence, not a broad
  // fixture rectangle whose connected copies might hide a raised original.
  RemasterFrame legacy;legacy.width=legacy.height=1;legacy.mainPixels={{uint32_t(RemasterSourceType::Background)<<24|1u<<16,1,0}};legacy.subPixels.resize(1);
  RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=1;i.tileId={123,1,4};legacy.tileInstances={i};
  RemasterFrameAssetMetadata m;m.tileId=i.tileId;m.tileRole=oobRole;legacy.assetMetadata={m};
  S9xRemasterApplyDungeonFloorHeight(legacy,f->c,50);
  need(legacy.tileInstances[0].heightOffset==0,"B1 exact room055(100,330) legacy projection excludes role-only OOB");
 }
 if(number==2) {
  for(int authored=0;authored<2;authored++)for(int lower=0;lower<2;lower++) {
   auto f=std::make_unique<PlateauFixture>(1,0,0,0);f->attrs.fill(1);
   if(authored)f->local(0);
   auto donor=f->frame.tileInstances.front();donor.sourceIndex=1;donor.heightOffset=30;
   f->frame.tileInstances.push_back(donor);
   if(lower)f->frame.subPixels[f->at(f->far)]={uint32_t(RemasterSourceType::Background)<<24|1u<<16,uint32_t(f->frame.tileInstances.size()),0};
   f->run();RemasterLightingFieldCache cpu;cpu.update(f->frame,true);
   need(cpu.height[f->at(f->far)*2+1]==0 && !cpu.samples[f->at(f->far)].known,"B2 unanchored OOB cannot render borrowed/provisional numeric height");
   auto px=f->frame.scenePixels[f->at(f->far)];auto m=RemasterFrameChannelResolver(f->frame).effective[px.instanceId-1];
   need(m.hasHeight==bool(authored),"B2 unknown absolute placement must preserve authored channel presence including zero");
   need(!RemasterFrameHeightResolver(f->frame).selectHeight(f->frame,f->at(f->far),true).donors[0].instanceId,"B2 every shared height consumer reports unknown absolute OOB, not numeric authored local zero");
  }
 }
 if(number==3) {
  auto f=std::make_unique<PlateauFixture>(1,0,0,0);
  for(auto [cell,id]:f->added)f->frame.tileInstances[id-1].hasPaletteArtwork=true;
  for(size_t at=0;at<f->frame.mainPixels.size();at++) {
   auto p=f->frame.mainPixels[at];if(p.instanceId && f->frame.tileInstances[p.instanceId-1].tileId.hash>=20000) {
    f->frame.subPixels[at]=p;
    f->frame.mainPixels[at]=f->frame.scenePixels[at]={uint32_t(RemasterSourceType::Backdrop)<<24,0,0xff};
   }
  }
  need(S9xRemasterRestoreSceneSubscreenWalls(f->frame)==128,"B3 role-only BG1 subscreen admission");
  f->run();RemasterLightingFieldCache cpu;cpu.update(f->frame,true);
  const auto at=f->at(f->far);auto mesh=RemasterSurfaceMesh::build(f->frame.width,f->frame.height,cpu.samples,1.0f/255);
  need(cpu.samples[at].sheet && !cpu.samples[at].solidWall && mesh[at].thickness==0,"B3 solved OOB must be planar occlusion sheet, not finite shell");
  for(float c:mesh[at].corners)need(std::abs(c-77.0f/255*f->frame.lightingCoordinateScale)<1e-6,"B3 sheet corners remain crest-flat");
 }
 if(number==4) {
  auto storage=std::make_unique<std::array<std::array<C::Placement,4096>,2>>();
  const C::Placement *tiles[]={(*storage)[0].data(),(*storage)[1].data()};
  for(int cell:{650,651})(*storage)[0][cell].surface=oobSurface;
  auto values=S9xRemasterVisibleFace::outOfBoundsPlateaus(tiles,[](int bg,int cell,int,int &h,int &o,int &p,int,int){
   if((!bg && cell==649) || (bg && cell==651)){h=bg?31:23;o=10;p=0;return RemasterVisualClaimStatus::Solved;}
   return RemasterVisualClaimStatus::NotEvaluated;
    },[](int,int,int,int,int){return true;});
   need(values[650].status==RemasterVisualClaimStatus::Solved && values[650].base==23,"B4 inspect peer-BG crest but choose minimum accepted absolute height");
 }
 std::cout<<"PASS blocker B"<<number<<"\n";
}
static void endFrameUnknown() {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);
 auto rom=std::make_unique<std::array<uint8_t,12876>>();rom->fill(255);
 for(int y=0;y<8;y++)(*gfx)[y*2]=255;
 uint8_t ink[64];std::fill_n(ink,64,1);RemasterTileContentId id{S9xRemasterHashTile(4,ink),1,4};
 C c;c.verifiedAlttpRom=c.indoors=true;c.module=7;c.roomIndex=7;
 c.constructedMaps=maps->data();c.graphics=gfx->data();c.collisionAttributes=attrs->data();c.dungeonTemplateData=rom->data();
 RemasterProfile p;p.schemaVersion=34;p.romSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";p.upperFloorHeight=50;
 p.assets[id].tileId=id;p.assets[id].tileRole=oobRole;
 S9xRemasterSetLiveFramesEnabled(true);S9xRemasterSetProfile(p);S9xRemasterInvalidateDungeonContinuity();
 auto &s=S9xRemasterState();std::array<uint16_t,64> colors={};
 for(int round=0;round<3;round++) {
  S9xRemasterBeginFrame(64,8,8,8);
  RemasterFrameTileInstance i;i.tileId=id;i.source=RemasterSourceType::Background;i.hasPaletteArtwork=true;
  s.tileInstances={i};auto &t=s.observedTiles[id.hash];t.hash=id.hash;t.bitDepth=4;std::copy_n(ink,64,t.indices);
  for(int k=0;k<64;k++) {
   s.mainOwners[k]=uint32_t(RemasterSourceType::Backdrop)<<24;s.mainInstanceIds[k]=0;s.mainTilePixels[k]=0xff;
   s.subOwners[k]=uint32_t(RemasterSourceType::Background)<<24;s.subInstanceIds[k]=1;s.subTilePixels[k]=k;
  }
  s.scenePixels.assign(64,{uint32_t(RemasterSourceType::Backdrop)<<24,0,0xff});s.sceneRgb.assign(64,0);
  auto out=std::make_unique<RemasterFrame>();S9xRemasterEndFrame(colors.data(),8,8,8,out.get(),&c);
  need(out->scenePixels.size()==64 && out->scenePixels[0].instanceId,"EndFrame admits actual observed role-only BG1");
  RemasterLightingFieldCache fields;fields.update(*out,true);need(!fields.height[1] && !fields.samples[0].known,"EndFrame unanchored role-only has no numeric fake floor");
  std::vector<uint8_t> bytes;RemasterFrame replay;
  need(S9xSerializeRemasterFrame(*out,bytes) && S9xDeserializeRemasterFrame(bytes,replay),"EndFrame OOB capture replay");
  fields.update(replay,true);need(!fields.height[1],"replayed unknown remains unknown");
 }
 std::cout<<"PASS EndFrame role-only BG1 admission/unknown/reuse/capture 3 rounds\n";
}
static void productionContacts() {
 for(int mode=0;mode<5;mode++) {
  auto f=std::make_unique<PlateauFixture>(1,0,0,0);
  const int wall=f->far+1,floor=wall+1,bg=1,plane=mode==3?1:0;
  auto &t=f->tiles[bg][wall];t.surface=C::Surface::Wall;t.hasHeight=true;t.tileHash=90001;t.wallTemplateOffset=0x2c8;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)t.height[y*8+x]=7-x;
  const int word=501,address=wall*2;f->maps[address]=uint8_t(word);f->maps[address+1]=word>>8;
  RemasterFrameAssetMetadata m;m.tileId={t.tileHash,1,4};m.hasHeight=true;m.height=t.height;f->frame.assetMetadata.push_back(m);
  RemasterFrameTileInstance inst;inst.source=RemasterSourceType::Background;inst.sourceIndex=bg;inst.tileId=m.tileId;inst.tileNumber=word;inst.heightOffset=90;
  f->frame.tileInstances.push_back(inst);uint32_t id=f->frame.tileInstances.size();
  if(mode!=4)for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
   size_t at=f->at(wall)+y*f->frame.width+x;
   f->frame.mainPixels[at]=f->frame.scenePixels[at]={uint32_t(RemasterSourceType::Background)<<24|uint32_t(bg)<<16,id,uint8_t(y*8+x)};
  }
  auto &map=f->heights[bg];map.offsets[wall]=90;map.producerHashes[wall]=t.tileHash;
  auto &s=f->tiles[bg][floor];s.hasHeight=true;
  int base=mode==0?24:70;map.planeOffsets[plane][floor]=base;map.occupiedFloorOwners[plane][floor]=mode==2?200:100;
  f->attrs[plane*4096+floor]=0;
  auto &d=const_cast<RemasterRoomDiagnostics&>(*map.roomDiagnostics).cells[plane*4096+floor];
  d.planeObserved=true;d.floorId=map.occupiedFloorOwners[plane][floor];d.floorBase=base;
  // Explicitly identical occupied footprint in the peer namespace is needed
  // to certify the same owner, not just the same integer owner ID.
  if(mode==1 || mode==4) {
   f->heights[0].planeOffsets[plane][floor]=base;f->heights[0].occupiedFloorOwners[plane][floor]=100;
   for(int cell:{f->foot,f->foot+1}) {
    f->heights[0].planeOffsets[plane][cell]=30;f->heights[0].occupiedFloorOwners[plane][cell]=100;
   }
  }
  f->run();auto px=f->frame.scenePixels[f->at(f->far)];auto &i=f->frame.tileInstances[px.instanceId-1];
   need(i.visualClaim.status==(mode!=3?RemasterVisualClaimStatus::Solved:RemasterVisualClaimStatus::Conflict),"production minimum accepts valid unequal crests/different floor owners, not different planes; hidden peer wall is not contact");
  RemasterLightingFieldCache cpu;cpu.update(f->frame,true);
   need(bool(cpu.height[f->at(f->far)*2+1])==(mode!=3),"production plane conflict never publishes fake OOB height");
   if(mode!=3)need(std::abs(cpu.height[f->at(f->far)*2]-(mode==0?31.f:77.f)/255)<1e-6,"minimum of independently solved wall crests, not minimum wall equations");
 }
 { // Source-switch invalidation must not retain a solved semantic plateau.
  auto f=std::make_unique<PlateauFixture>(1,0,0,0);f->run();RemasterLightingFieldCache cpu;cpu.update(f->frame,true);
  need(cpu.height[f->at(f->far)*2+1],"cache solved baseline");
  auto unknown=std::make_unique<PlateauFixture>(1,0,0,0);unknown->attrs.fill(1);unknown->run();cpu.update(unknown->frame,true);
  need(!cpu.height[unknown->at(unknown->far)*2+1],"cache switch to unanchored drops semantic height");
  cpu.update(f->frame,true);need(cpu.height[f->at(f->far)*2+1],"cache switchback restores independently solved height");
 }
 std::cout<<"PASS production contacts: unequal crest / owner / plane, certified equal owner, hidden wall 5 cases\n";
}
#ifndef REMASTER_OOB_NO_MAIN
int main(int argc,char **argv) {
 if(argc==2){blocker(std::atoi(argv[1]));return 0;}
 for(int b=1;b<=4;b++)blocker(b);
 endFrameUnknown();
 productionContacts();
 static_assert(unsigned(RemasterTileRole::Doorway)==10 && unsigned(RemasterTileRole::OutOfBounds)==11,"append role IDs only");
 for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++)for(int plane=0;plane<2;plane++) {
  auto f=std::make_unique<PlateauFixture>(axis,flip,bg,plane);
  const auto physical=f->heights[bg].planeOffsets;
  f->run();RemasterLightingFieldCache cpu;cpu.update(f->frame,true);
  // Fixture wall solves base70; its transformed high edge is local7.
  for(int cell:{f->plateau,f->far}) {
   const auto at=f->at(cell);const auto px=f->frame.scenePixels[at];
   const auto &i=f->frame.tileInstances[px.instanceId-1];
   need(i.visualClaim.status==RemasterVisualClaimStatus::Solved && i.visualClaim.published,
    "FAIL explicit out-of-bounds plateau must inherit proved terminal wall crest");
   need(i.heightOffset==77,"crest is authored last pixel 70+7, no +1 or +8");
   need(std::abs(cpu.height[at*2]-77.0f/255)<1e-6 && cpu.height[at*2+1]==1,"actual selected CPU plateau height source");
   auto m=RemasterFrameChannelResolver(f->frame).effective[px.instanceId-1];
    need(m.hasHeight && m.height[0]==0 && m.hasNormals && m.normalXyz[2]==255,"role-only explicit semantic flat plane and up normal");
    need(cpu.semanticNormals[at],"geometric up normal is explicitly not authored inventory");
  }
  need(physical==f->heights[bg].planeOffsets,"plateau never mutates physical floor planes");
  need(S9xRemasterChannelProvenance(f->frame,f->at(f->far)).find("explicit-role semantic plane")!=std::string::npos,"semantic source is not reported as authored");
  // A real cross-BG continuation has two visible, identity-checked edges.
  auto swapped=std::make_unique<PlateauFixture>(axis,flip,bg,plane);
  swapped->tiles[1-bg][swapped->far]=swapped->tiles[bg][swapped->far];swapped->tiles[bg][swapped->far]={};
  auto &cross=swapped->frame.tileInstances[swapped->added[1].second-1];cross.sourceIndex=1-bg;
  for(auto *r:{&swapped->frame.mainPixels,&swapped->frame.scenePixels})for(auto &p:*r)if(p.instanceId==swapped->added[1].second)p.owner=uint32_t(RemasterSourceType::Background)<<24|uint32_t(1-bg)<<16;
  swapped->run();RemasterLightingFieldCache crossCpu;crossCpu.update(swapped->frame,true);
  need(std::abs(crossCpu.height[swapped->at(swapped->far)*2]-77.f/255)<1e-6,"proved visible cross-BG sheet continuation");
  if(axis==1 && flip==0 && bg==0 && plane==0)if(const char *path=std::getenv("REMASTER_OOB_CAPTURE"))
   need(S9xWriteRemasterFrame(f->frame,path),"write isolated semantic plateau GPU fixture");
 }
 for(int value:{0,5,-3}) {
  auto f=std::make_unique<PlateauFixture>(1,0,0,0);f->local(value);f->run();
  auto px=f->frame.scenePixels[f->at(f->far)];auto &i=f->frame.tileInstances[px.instanceId-1];
  need(i.heightOffset==77-value,"authored constant local origin preserved with contact-exact placement base");
 }
 { // Independent exact policy oracle through selected production CPU, not +8.
  auto f=std::make_unique<PlateauFixture>(1,0,0,0);
  for(int cell:{f->foot,f->foot+1}) {
   auto &m=f->heights[1];m.offsets[cell]=m.planeOffsets[0][cell]=-24;
   const_cast<RemasterRoomDiagnostics&>(*m.roomDiagnostics).cells[cell].floorBase=-24;
  }
  for(auto &i:f->frame.tileInstances)if(i.heightOffset==30)i.heightOffset=-24;
  f->run();RemasterLightingFieldCache cpu;cpu.update(f->frame,true);
  need(std::abs(cpu.height[f->at(f->far)*2]-23.f/255)<1e-6,"selected CPU exact16+7=23, no additional cap increment");
  if(const char *path=std::getenv("REMASTER_OOB_CAPTURE_23"))need(S9xWriteRemasterFrame(f->frame,path),"write crest23 GPU fixture");
 }
 for(int mode=0;mode<7;mode++) {
  auto f=std::make_unique<PlateauFixture>(1,0,0,0);
  if(mode==0)f->c.collisionMode=1;
  if(mode==1)f->c.submodule=2;
  if(mode==2)f->heights[0].roomIndex++;
  if(mode==3)f->c.collisionAttributes=nullptr;
  if(mode==4)f->c.dungeonTemplateData=nullptr;
  if(mode==5)f->c.verifiedAlttpRom=false;
  if(mode==6)f->c.indoors=false;
  f->run();RemasterLightingFieldCache cpu;cpu.update(f->frame,true);
  need(cpu.height[f->at(f->far)*2+1]==0,"ineligible visual solve cannot guess role-only absolute geometry");
 }
 for(int mode=0;mode<3;mode++) {
  auto f=std::make_unique<PlateauFixture>(1,0,0,0);
  auto &m=f->frame.assetMetadata.back();
  m.hasNormals=m.hasOcclusion=true;m.normalXyz.fill(mode==0?0:255);m.occlusion.fill(mode==1?0:137);
  const auto normals=m.normalXyz;const auto mask=m.occlusion;
  if(mode==2)f->local(0,true);
  f->run();auto p=f->frame.scenePixels[f->at(f->far)];auto effective=RemasterFrameChannelResolver(f->frame).effective[p.instanceId-1];
  need(effective.hasNormals && effective.normalXyz==normals && effective.hasOcclusion && effective.occlusion==mask,"authored normal and mask arrays including zeros remain byte-exact");
 }
 for(int mode=0;mode<5;mode++) {
  auto f=std::make_unique<PlateauFixture>(1,0,0,0);const auto id=f->added[1].second;
  auto &m=f->frame.assetMetadata.back();
  if(mode==0) {m.geometryByFlip=true;auto &g=m.flipGeometry[0];g.hasHeight=true;g.height.fill(19);f->tiles[0][f->far].stairSourceAmbiguous=true;}
  if(mode==1) {RemasterTileContext g;g.hasHeight=true;g.height.fill(19);f->frame.contextGeometry[id]=g;f->tiles[0][f->far].stairSourceAmbiguous=true;}
  if(mode==2)f->tiles[0][f->far].stairSourceAmbiguous=true; // decoded transparency
  if(mode==3)for(int x=0;x<8;x++) {auto at=f->at(f->far)+7*f->frame.width+x;f->frame.mainPixels[at]=f->frame.scenePixels[at]={};}
  if(mode==4)f->maps[(8192)+f->far*2]^=1; // overwritten actual producer
  f->run();RemasterLightingFieldCache cpu;cpu.update(f->frame,true);
  const auto px=f->frame.scenePixels[f->at(f->far)];
  need(!cpu.height[f->at(f->far)*2+1],"unsupported/partial/overwritten source cannot fabricate absolute OOB geometry");
  if(mode<3)need(f->frame.tileInstances[px.instanceId-1].visualClaim.status==RemasterVisualClaimStatus::Unsupported,"unsupported source explicitly annotated, not silently solved");
  if(mode<2)need(RemasterFrameChannelResolver(f->frame).effective[px.instanceId-1].height[0]==19,"unsupported alternate/context authored channel is retained");
 }
 for(int mode=0;mode<4;mode++) {
  auto f=std::make_unique<PlateauFixture>(1,0,0,0);
  if(mode==0)f->attrs.fill(1);
  if(mode==1)f->local(0,true);
  if(mode==2)f->tiles[0][f->plateau].surface=C::Surface::Unknown;
  if(mode==3)f->tiles[0][f->plateau].stairSourceAmbiguous=true;
  f->run();const auto &i=f->frame.tileInstances[f->frame.scenePixels[f->at(f->far)].instanceId-1];
  need(!i.visualClaim.published,"unanchored/nonflat/unknown/ambiguous region cannot fabricate plateau datum");
 }
  { // Policy change: unequal accepted crests/floor identities are nonfatal; planes are not.
  auto storage=std::make_unique<std::array<std::array<C::Placement,4096>,2>>();
  const C::Placement *tiles[]={(*storage)[0].data(),(*storage)[1].data()};
  for(int i:{650,651,652})(*storage)[0][i].surface=oobSurface;
  for(int mode=0;mode<4;mode++) {
    auto values=S9xRemasterVisibleFace::outOfBoundsPlateaus(tiles,[&](int bg,int cell,int,int &h,int &o,int &p,int,int){
    if(bg || (cell!=649 && cell!=653))return RemasterVisualClaimStatus::NotEvaluated;
    h=23+(mode==1 && cell==653?8:0);o=10+(mode==2 && cell==653);p=mode==3 && cell==653;
     return RemasterVisualClaimStatus::Solved;
    },[](int,int,int,int,int){return true;});
    for(int cell:{650,651,652})need(values[cell].status==(mode==3?RemasterVisualClaimStatus::Conflict:RemasterVisualClaimStatus::Solved),"minimum policy changes crest/owner agreement only, never plane validity");
   if(!mode)need(values[652].base==23,"base16 local7 crest23 no additional increment");
  }
 }
 { // Real classifier and publication cache, role-only metadata, opaque/transparent.
  auto maps=std::make_unique<std::array<uint8_t,16384>>();
  auto gfx=std::make_unique<std::array<uint8_t,65536>>();
  auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);
  for(int y=0;y<8;y++)(*gfx)[y*2]=255;
  uint8_t pixels[64];std::fill_n(pixels,64,1);
  RemasterTileContentId id{S9xRemasterHashTile(4,pixels),1,4};
  RemasterProfile profile;profile.upperFloorHeight=50;profile.assets[id].tileRole=oobRole;
  profile.assetGroups["wall_faces"].tileIds.push_back(id); // stale group must lose
  C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.constructedMaps=maps->data();c.graphics=gfx->data();c.collisionAttributes=attrs->data();
  auto placed=std::make_unique<std::array<C::Placement,4096>>();
  S9xRemasterDungeonPlacements(c,profile,*placed);
  need((*placed)[0].surface==oobSurface && !(*placed)[0].hasHeight && !(*placed)[0].floorPlaneMask && !(*placed)[0].stairSourceAmbiguous,"compiled role-only metadata is terminal semantic sheet, never Floor or Wall");
  (*gfx)[0]=254;pixels[7]=0;RemasterTileContentId transparent{S9xRemasterHashTile(4,pixels),1,4};
  profile.assets[transparent].tileRole=oobRole;S9xRemasterDungeonPlacements(c,profile,*placed);
  need((*placed)[0].stairSourceAmbiguous,"transparent sheet cannot bridge connected plateau");
 }
 RemasterProfile p;p.schemaVersion=34;p.gameTitle="OOB fixture";p.romSha256=std::string(64,'1');
 RemasterTileContentId id{0x12345678,1,4};auto &a=p.assets[id];a.tileId=id;a.tileRole=oobRole;
 std::string text;std::vector<RemasterProfileDiagnostic> d;RemasterProfile decoded;
 need(S9xRemasterSerializeProfile(p,text,d) && text.find("tile_role = \"out-of-bounds\"")!=std::string::npos,"named metadata writer");
 need(S9xRemasterParseProfile(text,decoded,d) && decoded.assets.at(id).tileRole==oobRole,"metadata role-only parser roundtrip");
 auto malformed=text;auto roleAt=malformed.find("out-of-bounds");malformed.replace(roleAt,13,"invalid-role-12");
 need(!S9xRemasterParseProfile(malformed,decoded,d),"malformed new role rejected, not silently truncated");
 RemasterFrame frame,captured;frame.width=frame.height=1;frame.originalRgb555.resize(1);frame.mainPixels.resize(1);frame.subPixels.resize(1);
 RemasterFrameAssetMetadata m;m.tileId=id;m.tileRole=oobRole;frame.assetMetadata.push_back(m);
 std::vector<uint8_t> bytes;
 need(S9xSerializeRemasterFrame(frame,bytes) && S9xDeserializeRemasterFrame(bytes,captured),"capture role roundtrip");
 need(captured.assetMetadata[0].tileRole==oobRole,"capture retains new role ID");
  std::cout<<"PASS out-of-bounds production crest plateau, transforms, planes, authored origins and metadata/capture\n";
}
#endif
