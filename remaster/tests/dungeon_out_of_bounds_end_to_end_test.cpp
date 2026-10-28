// Research scene: real capture VRAM/template/profile bytes, synthetic settled
// maps/collision/raster. Never claims the modified scene is the original capture.
// No Placement, solved map, support owner or visual claim is supplied by this test.
#define REMASTER_WALL_BAND_DIAGNOSTICS 1
#include "../runtime/remaster.h"
#include "../runtime/lighting_field_cache.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void require(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static std::vector<uint8_t> bytes(const std::string &p){std::ifstream f(p,std::ios::binary);require(bool(f),"missing fixture dependency");return {std::istreambuf_iterator<char>(f),{}};}
using C=RemasterDungeonFloorContext;
struct Research {
 std::vector<uint8_t> input,templates;
 std::array<uint8_t,16384> maps={};std::array<uint8_t,8192> attrs;
 RemasterProfile original,profile;C c;
 std::array<std::array<uint16_t,4096>,2> words={};
 int axis,bg,flip,plane,first,foot,sheet,far,down,floorWord=-1,sheetWord=-1;
 std::map<int,RemasterFrameAsset> art;
 Research(const std::string &path,int ax,int layer,int fl,int pl,int variant=0):axis(ax),bg(layer),flip(fl),plane(pl) {
  input=bytes(path+".dungeon-input.bin");templates=bytes(path+".dungeon-templates.bin");
  require(input.size()==90120 && templates.size()==12884,"complete raw inputs");
  std::vector<RemasterProfileDiagnostic> d;
  require(S9xRemasterLoadProfile(path+".profile.toml",original,d),"own profile parse");profile=original;
  c.verifiedAlttpRom=c.indoors=true;c.module=7;c.roomIndex=7;c.linkPlane=plane;
  c.graphics=input.data()+24584;c.graphicsBase=c.bg1GraphicsBase=16384;
  c.dungeonTemplateData=templates.data()+8;c.constructedMaps=maps.data();c.collisionAttributes=attrs.data();attrs.fill(1);
  // Independent zero-local authored Floor, opaque and distinct from OOB art.
  for(int n=0;n<1024;n++) {
   auto a=decode(n);auto it=profile.assets.find(a.tileId);if(it==profile.assets.end())continue;
   const auto &m=it->second;bool opaque=std::all_of(a.indices,a.indices+64,[](int v){return v!=0;});
   if(opaque && m.hasHeight && std::all_of(m.height.begin(),m.height.end(),[](int v){return v==0;}) && m.tileRole==RemasterTileRole::Floor && floorWord<0)floorWord=n;
   bool flat=m.hasHeight && std::all_of(m.height.begin(),m.height.end(),[&](int v){return v==m.height[0];});
   bool candidate=variant==0?flat && m.height[0]!=0 && m.tileRole!=RemasterTileRole::Floor:variant==1?!m.hasHeight:variant==2?flat && m.height[0]==0 && m.tileRole!=RemasterTileRole::Floor:flat && m.height[0]==5;
   if(opaque && candidate && sheetWord<0)sheetWord=n;
  }
  require(floorWord>=0 && sheetWord>=0,"authored opaque floor and constant sheet candidates");
  down=(flip&(axis?2:1))?-1:1;
  const int off[]={axis?0x2e8:0x2c8,axis?0x328:0x308};
  for(int n=0;n<7;n++)for(int t=0;t<2;t++) {
   int depth=down>0?n:6-n,x=12+(axis?t:depth),y=12+(axis?depth:t),cell=y*64+x;
   int layer=n<3?bg:1-bg,word=floorWord;
   if(n<6){int canonical=(flip&(axis?1:2))?1-t:t,at=8+off[n/3]+2*(canonical*4+n%3);word=(templates[at]|templates[at+1]<<8)^((flip&1?0x4000:0)|(flip&2?0x8000:0));}
   words[layer][cell]=word;
   if(n==0 && !t)first=cell;
   if(n==6){attrs[plane*4096+cell]=0;if(!t)foot=cell;}
  }
  // Legal 4x4 occupied foot body; not merely an isolated walkable pixel/cell.
  for(int d=0;d<4;d++)for(int t=0;t<4;t++) {
   int cell=foot+down*d*(axis?64:1)+t*(axis?1:64);
   words[1-bg][cell]=floorWord;attrs[plane*4096+cell]=0;
  }
  sheet=first-down*(axis?64:1);far=sheet-down*(axis?64:1);
  for(int cell:{sheet,far})for(int t=0;t<2;t++)words[bg][cell+t*(axis?1:64)]=sheetWord;
  c.backgroundScrollX=(axis?12:10)*8;c.backgroundScrollY=(axis?10:12)*8;
  if(down<0){c.backgroundScrollX=(axis?12:12)*8;c.backgroundScrollY=12*8;}
  c.linkX=(foot%64)*8;c.linkY=(foot/64)*8;
  sync();
 }
 RemasterFrameAsset decode(int word)const {
  RemasterFrameAsset a;int address=16384+(word&1023)*32;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(address+b/2*16+y*2+b%2)]>>(7-x))&1)<<b;a.indices[y*8+x]=v;}
  a.tileId={S9xRemasterHashTile(4,a.indices),1,4};return a;
 }
 void sync(){for(int b=0;b<2;b++)for(int cell=0;cell<4096;cell++){int at=(b?0:8192)+cell*2;maps[at]=words[b][cell];maps[at+1]=words[b][cell]>>8;}}
 size_t at(int cell)const{return (cell/64*8-c.backgroundScrollY)*width()+cell%64*8-c.backgroundScrollX;}
 int width()const{return axis?32:88;}int height()const{return axis?88:32;}
 std::unique_ptr<RemasterFrame> run(bool role,bool noFoot=false,bool subscreen=false,bool partial=false) {
  profile=original;auto &a=profile.assets.at(decode(sheetWord).tileId);
  if(role)a.tileRole=RemasterTileRole::OutOfBounds;
  std::string text;std::vector<RemasterProfileDiagnostic> d;RemasterProfile parsed;
  require(S9xRemasterSerializeProfile(profile,text,d) && S9xRemasterParseProfile(text,parsed,d),"regular serialized profile activation");
  require(parsed.assets.at(a.tileId).tileRole==a.tileRole,"user-assignable role persists");
  if(const char *dir=std::getenv("REMASTER_OOB_E2E_DIR"))if(role && !noFoot && !partial)std::ofstream(std::string(dir)+"/profile-a"+std::to_string(axis)+"b"+std::to_string(bg)+"f"+std::to_string(flip)+"p"+std::to_string(plane)+"w"+std::to_string(sheetWord)+".toml")<<text;
  S9xRemasterSetLiveFramesEnabled(true);S9xRemasterSetProfile(parsed);S9xRemasterInvalidateDungeonContinuity();
  auto &s=S9xRemasterState();S9xRemasterBeginFrame(width()*height(),width(),width(),height());
  const uint32_t backdrop=uint32_t(RemasterSourceType::Backdrop)<<24;
  s.scenePixels.assign(width()*height(),{backdrop,0,0xff});s.sceneRgb.assign(width()*height(),0);
  std::fill(s.mainOwners.begin(),s.mainOwners.end(),backdrop);std::fill(s.subOwners.begin(),s.subOwners.end(),backdrop);
  // Encode actual raster observations from final words and unflipped VRAM ink.
  for(int cell=0;cell<4096;cell++)for(int b=0;b<2;b++) {
   int word=words[b][cell];if(!word)continue;int sx=cell%64*8-c.backgroundScrollX,sy=cell/64*8-c.backgroundScrollY;
   if(sx<0 || sy<0 || sx+8>width() || sy+8>height())continue;
   auto asset=decode(word);RemasterFrameTileInstance i;i.tileId=asset.tileId;i.source=RemasterSourceType::Background;i.sourceIndex=b;i.tileNumber=word&1023;i.palette=(word>>10)&7;i.hFlip=word&0x4000;i.vFlip=word&0x8000;i.hasPaletteArtwork=true;
   s.tileInstances.push_back(i);auto &ink=s.observedTiles[i.tileId.hash];ink.hash=i.tileId.hash;ink.bitDepth=4;std::copy_n(asset.indices,64,ink.indices);
   uint32_t id=s.tileInstances.size(),owner=uint32_t(RemasterSourceType::Background)<<24|uint32_t(b)<<16;
   bool sub=subscreen && b==bg && (cell==sheet || cell==far || cell==sheet+(axis?1:64) || cell==far+(axis?1:64));
   for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
    size_t p=(sy+y)*width()+sx+x;int k=(i.vFlip?7-y:y)*8+(i.hFlip?7-x:x);
    if(!asset.indices[k] || (partial && (cell==sheet || cell==sheet+(axis?1:64)) && y==0))continue;
    (sub?s.subOwners:s.mainOwners)[p]=owner;(sub?s.subInstanceIds:s.mainInstanceIds)[p]=id;(sub?s.subTilePixels:s.mainTilePixels)[p]=k;
    if(!sub)s.scenePixels[p]={owner,id,uint8_t(k)};
   }
  }
  auto saved=attrs;if(noFoot)attrs.fill(1);
  std::vector<uint16_t> rgb(width()*height());auto out=std::make_unique<RemasterFrame>();
  S9xRemasterEndFrame(rgb.data(),width(),width(),height(),out.get(),&c);attrs=saved;
  return out;
 }
};
static void malformedRoles(const RemasterFrame &frame) {
 // Test both wire roles, independently of profile text validation. Locate the
 // field by a valid one-field differential serialization, then corrupt to12.
 for(bool placement:{false,true}) {
  RemasterFrame a=frame,b=frame;std::vector<uint8_t> x,y;RemasterFrame decoded;
  if(placement){a.tileInstances[0].geometryStatus=b.tileInstances[0].geometryStatus=RemasterOccurrenceGeometryStatus::Resolved;a.tileInstances[0].placementTileRole=RemasterTileRole::Floor;b.tileInstances[0].placementTileRole=RemasterTileRole::OutOfBounds;}
  else {a.assetMetadata[0].tileRole=RemasterTileRole::Floor;b.assetMetadata[0].tileRole=RemasterTileRole::OutOfBounds;}
  require(S9xSerializeRemasterFrame(a,x) && S9xSerializeRemasterFrame(b,y) && x.size()==y.size(),"role boundary valid writes");
  int changed=0;for(size_t n=0;n<x.size();n++)if(x[n]!=y[n]){require(x[n]==1 && y[n]==11,"exact role wire differential");y[n]=12;changed++;}
  require(changed==1 && !S9xDeserializeRemasterFrame(y,decoded),"malformed role12 capture read must fail");
  if(placement)b.tileInstances[0].placementTileRole=static_cast<RemasterTileRole>(12);else b.assetMetadata[0].tileRole=static_cast<RemasterTileRole>(12);
  require(!S9xSerializeRemasterFrame(b,y),"malformed role12 capture write must fail");
 }
}
int main(){try {
 const char *env=std::getenv("REMASTER_OOB_E2E_INPUT");
 if(!env){std::cout<<"SKIP research fixture requires REMASTER_OOB_E2E_INPUT complete capture companion prefix\n";return 0;}
 for(int axis=0;axis<2;axis++)for(int bg=0;bg<2;bg++)for(int flip=0;flip<4;flip++)for(int plane=0;plane<2;plane++) {
 std::cout<<"CASE axis="<<axis<<" bg="<<bg<<" flip="<<flip<<" plane="<<plane<<std::endl;
 auto f=std::make_unique<Research>(env,axis,bg,flip,plane);
 auto baseline=f->run(false);auto &state=S9xRemasterState();
 auto planes0=state.runtimeDungeonBg1HeightMap.planeOffsets,planes1=state.runtimeDungeonHeightMap.planeOffsets;
 auto owners0=state.runtimeDungeonBg1HeightMap.occupiedFloorOwners,owners1=state.runtimeDungeonHeightMap.occupiedFloorOwners;
 auto out=f->run(true,std::getenv("REMASTER_OOB_E2E_ABLATE_FOOT")!=nullptr,bg==0);
 auto &s=S9xRemasterState();
 for(int cell:{f->first,f->foot,f->sheet,f->far}) {
  for(int bg=0;bg<2;bg++){const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;auto &t=(bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1)[cell];std::cout<<"cell="<<cell%64<<','<<cell/64<<" bg="<<bg<<" role="<<int(t.surface)<<" local="<<t.height[0]<<','<<t.height[63]<<" template="<<t.wallTemplateOffset<<" map="<<m.offsets[cell]<<" plane="<<m.planeOffsets[0][cell]<<" owner="<<m.occupiedFloorOwners[0][cell]<<" diag="<<bool(m.roomDiagnostics)<<'\n';}
  auto px=out->scenePixels.at(f->at(cell));require(px.instanceId,"selected scene occurrence");auto &i=out->tileInstances.at(px.instanceId-1);std::cout<<"scene bg="<<int(i.sourceIndex)<<" base="<<i.heightOffset<<" status="<<int(i.visualClaim.status)<<" published="<<i.visualClaim.published<<'\n';
 }
 const auto px=out->scenePixels.at(f->at(f->far));const auto &i=out->tileInstances.at(px.instanceId-1);
 require(i.visualClaim.status==RemasterVisualClaimStatus::Solved && i.visualClaim.published,"positive real classifier/physical solver/EndFrame must publish solved OOB");
 require(planes0==s.runtimeDungeonBg1HeightMap.planeOffsets && planes1==s.runtimeDungeonHeightMap.planeOffsets && owners0==s.runtimeDungeonBg1HeightMap.occupiedFloorOwners && owners1==s.runtimeDungeonHeightMap.occupiedFloorOwners,"role assignment preserves physical planes and occupied owners");
 // Independent authored oracle, walking low-to-high in each column. Each seam
 // advances exactly one sample; do not derive the expected value from claims.
 int expected=-1;
 for(int tangent=0;tangent<2;tangent++) {
  int crest=-1;
  for(int n=5;n>=0;n--) {
   int cell=f->first+f->down*n*(axis?64:1)+tangent*(axis?1:64),layer=n<3?bg:1-bg;
   int word=f->words[layer][cell];auto a=f->original.assets.at(f->decode(word).tileId);
   auto sample=[&](bool high){int x=axis?tangent:((high==(f->down>0))?0:7),y=axis?((high==(f->down>0))?0:7):tangent;return a.height[(word&0x8000?7-y:y)*8+(word&0x4000?7-x:x)];};
   int low=sample(false),high=sample(true);require(high>=low,"independent monotonic authored band");
   crest+=high-low+1;
  }
  require(expected<0 || expected==crest,"independent two column crests agree");expected=crest;
 }
 const auto &footMap=bg?s.runtimeDungeonBg1HeightMap:s.runtimeDungeonHeightMap;
 require(footMap.planeOffsets[plane][f->foot]==0 && footMap.occupiedFloorOwners[plane][f->foot]>=0,"independent collision-backed relative floor datum0");
 RemasterLightingFieldCache fields;fields.update(*out,true);
 int count=0;for(size_t p=0;p<out->scenePixels.size();p++) {
  auto px=out->scenePixels[p];if(!px.instanceId)continue;const auto &inst=out->tileInstances[px.instanceId-1];
  if(inst.tileId==f->decode(f->sheetWord).tileId) {
   require(inst.visualClaim.published && fields.samples[p].known && std::abs(fields.height[p*2]*255-expected)<1e-5,"all connected plateau pixels equal independent authored crest");
   require(fields.samples[p].sheet && !fields.samples[p].solidWall,"actual material is planar");count++;
  } else {
   auto old=baseline->scenePixels[p];require(old.instanceId,"non-OOB baseline occurrence");const auto &prior=baseline->tileInstances[old.instanceId-1];
   require(inst.tileId==prior.tileId && inst.heightOffset==prior.heightOffset && inst.placementHeight==prior.placementHeight,"non-OOB effective geometry unchanged");
  }
 }
 require(count==256,"complete four-cell connected plateau denominator");
 for(const auto &entry:f->original.assets) {
  const auto &a=entry.second,&b=f->profile.assets.at(entry.first);
  require(a.hasHeight==b.hasHeight && a.height==b.height && a.hasNormals==b.hasNormals && a.normalXyz==b.normalXyz && a.hasOcclusion==b.hasOcclusion && a.occlusion==b.occlusion && a.hasEmission==b.hasEmission && a.emissionRgba==b.emissionRgba,"all authored arrays and presence unchanged");
 }
 if(axis==1 && bg==1 && !flip && !plane) {
  malformedRoles(*out);
  if(const char *path=std::getenv("REMASTER_OOB_E2E_CAPTURE"))require(S9xWriteRemasterFrame(*out,path),"actual EndFrame GPU packet");
  if(const char *dir=std::getenv("REMASTER_OOB_E2E_DIR")) {
   std::ofstream maps(std::string(dir)+"/research-maps.bin",std::ios::binary);maps.write(reinterpret_cast<const char *>(f->maps.data()),f->maps.size());
   std::ofstream attrs(std::string(dir)+"/research-collision.bin",std::ios::binary);attrs.write(reinterpret_cast<const char *>(f->attrs.data()),f->attrs.size());
   std::ofstream context(std::string(dir)+"/research-context.txt");context<<"SYNTHETIC settled scene, NOT original capture\nroom=7 module=7 submodule=0 plane=0 collision_mode=0 scroll=96,80 graphics_base=16384,16384\nindependent_floor_datum=0 independent_wall_base=40 high_edge=7 crest=47\n";
  }
 }
 if(axis==0 && bg==1 && !flip && !plane)if(const char *path=std::getenv("REMASTER_OOB_E2E_HORIZONTAL_CAPTURE"))require(S9xWriteRemasterFrame(*out,path),"horizontal actual EndFrame GPU packet");
 std::cout<<"positive observed crest="<<fields.height[f->at(f->far)*2]*255<<" expected="<<expected<<" pixels="<<count<<" semantic_normal="<<int(fields.semanticNormals[f->at(f->far)])<<'\n';
 auto unknown=f->run(true,true);fields.update(*unknown,true);
 require(!unknown->tileInstances[unknown->scenePixels[f->at(f->far)].instanceId-1].visualClaim.published && !fields.samples[f->at(f->far)].known,"all-blocking collision ablation must deny crest and numeric height");
 std::cout<<"PASS positive pipeline; no-foot ablation unknown\n";
 }
 // Missing and authored-zero variants select existing unmodified VRAM/profile
 // assets, not manufactured placeholder arrays. Constant5 is attempted only if
 // present in these immutable authored inputs.
 for(int variant=1;variant<=3;variant++) {
  try {
   auto f=std::make_unique<Research>(env,1,0,0,0,variant);auto out=f->run(true,false,true);
   const auto source=f->original.assets.at(f->decode(f->sheetWord).tileId);
   auto px=out->scenePixels[f->at(f->far)];auto &i=out->tileInstances.at(px.instanceId-1);
   require(i.visualClaim.published,"existing absent/zero/5 authored variant solves EndFrame");
   RemasterLightingFieldCache fields;fields.update(*out,true);require(std::abs(fields.height[f->at(f->far)*2]*255-47)<1e-5,"variant independent crest47");
   auto m=S9xRemasterFrameMetadataForTile(*out,i.tileId);require(m && m->hasHeight==source.hasHeight && m->height==source.height && m->hasNormals==source.hasNormals && m->normalXyz==source.normalXyz,"variant metadata preserves original channels");
   if(variant==1)require(fields.semanticNormals[f->at(f->far)],"role-only actual EndFrame supplies separately flagged semantic normal");
   std::vector<uint8_t> wire;RemasterFrame replay;require(S9xSerializeRemasterFrame(*out,wire) && S9xDeserializeRemasterFrame(wire,replay),"actual EndFrame variant capture roundtrip");fields.update(replay,true);require(std::abs(fields.height[f->at(f->far)*2]*255-47)<1e-5,"capture replay preserves actual solved crest");
   auto partial=f->run(true,false,true,true);fields.update(*partial,true);require(!fields.samples[f->at(f->far)].known,"missing exposed contact cannot anchor through hidden fallback");
   std::cout<<"PASS existing channel variant="<<variant<<" word="<<f->sheetWord<<" local_presence="<<source.hasHeight<<" local="<<source.height[0]<<" crest47 and partial-edge unknown\n";
  }catch(const std::exception &e){if(variant==3 && std::string(e.what())=="authored opaque floor and constant sheet candidates")std::cout<<"GAP no existing opaque authored constant5 in input VRAM/profile; no arrays invented\n";else throw;}
 }
 return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
