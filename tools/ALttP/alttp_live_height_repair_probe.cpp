// Recompute both complete BG solves and apply them to a matching gameplay packet.
// Outputs are review artifacts, not new gameplay evidence or room approval.
#include "../../remaster/remaster.h"
#include <iostream>
#include <stdexcept>
static void require(bool v,const char *s){if(!v)throw std::runtime_error(s);}
static std::vector<uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);require(bool(f),"missing input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
 require(argc==3,"usage: capture output-prefix");const std::string path=argv[1],out=argv[2];
 auto frame=std::make_unique<RemasterFrame>();require(S9xReadRemasterFrame(path,*frame),"packet parse");
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
 require(S9xRemasterLoadProfile(path+".profile.toml",profile,errors),"profile parse");
 auto input=read(path+".dungeon-input.bin"),templates=read(path+".dungeon-templates.bin"),meta=read(path+".dungeon.json");
 require(input.size()==90120 && !std::memcmp(input.data(),"ALTPDI1\0",8),"complete input");
 require(templates.size()==12884 && !std::memcmp(templates.data(),"ALTPDT1\0",8),"template input");
 const std::string text(meta.begin(),meta.end());
 require(text.find("66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb")!=std::string::npos,"ROM pin");
 auto number=[&](const char *key,int index=0){auto pos=text.find(std::string("\"")+key+"\":");require(pos!=std::string::npos,"metadata key");pos=text.find(':',pos)+1;if(text[pos]=='[')pos++;for(int i=0;i<index;i++)pos=text.find(',',pos)+1;return std::stoi(text.substr(pos));};
 std::string serialized;require(S9xRemasterSerializeProfile(profile,serialized,errors),"serialization");
 uint64_t digest=UINT64_C(14695981039346656037);for(unsigned char b:serialized)digest=(digest^b)*UINT64_C(1099511628211);
 std::ostringstream hex;hex<<std::hex<<digest;require(text.find("\"profile_serialized_fnv1a64\":\""+hex.str()+"\"")!=std::string::npos,"profile pin");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=number("room");c.previousRoomIndex=number("previous_room");
 c.submodule=number("submodule");c.collisionMode=number("collision_mode");require(!c.submodule && !c.collisionMode,"unsupported/unsettled state");
 c.linkX=number("link_xy");c.linkY=number("link_xy",1);c.linkPlane=number("plane");
 c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);
 c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");
 c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=templates.data()+8;
 size_t matched=0;
 for(auto *pixels:{&frame->mainPixels,&frame->subPixels})for(size_t at=0;at<pixels->size();at++) {
  const auto &px=(*pixels)[at];if(!px.instanceId)continue;const auto &i=frame->tileInstances.at(px.instanceId-1);
  if(i.source!=RemasterSourceType::Background || i.sourceIndex>1)continue;
  const int x=(c.backgroundScrollX+at%frame->width)&511,y=(c.backgroundScrollY+at/frame->width)&511;
  const int address=(y/8*64+x/8)*2+(i.sourceIndex?0:8192),word=c.constructedMaps[address]|c.constructedMaps[address+1]<<8;
  uint8_t decoded[64];const int base=i.sourceIndex?c.graphicsBase:c.bg1GraphicsBase;
  for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++) {
   int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(base+(word&1023)*32+(b/2)*16+yy*2+b%2)]>>(7-xx))&1)<<b;
   decoded[yy*8+xx]=v;
  }
  require(i.tileId.hash==S9xRemasterHashTile(4,decoded) && i.hFlip==bool(word&0x4000) && i.vFlip==bool(word&0x8000) &&
          px.tilePixel==((word&0x8000?7-y%8:y%8)*8+(word&0x4000?7-x%8:x%8)),"stale packet/maps/camera");matched++;
 }
 S9xRemasterDungeonStairsFromAttributes(c);
 auto image=[&](const std::string &name) {
  std::ofstream ppm(out+name+".ppm",std::ios::binary);ppm<<"P6\n"<<frame->width<<' '<<frame->height<<"\n255\n";
  for(size_t at=0;at<frame->mainPixels.size();at++) {
   const auto *px=&frame->mainPixels[at];
   if(!px->instanceId && (px->owner>>24)==static_cast<uint8_t>(RemasterSourceType::Backdrop))px=&frame->subPixels[at];
   int h=0;if(px->instanceId) {
    const auto &i=frame->tileInstances.at(px->instanceId-1);h=i.heightOffset;
    if(px->tilePixel<64) {
     if(i.hasPlacementHeight)h+=i.placementHeight[px->tilePixel];
     else for(const auto &m:frame->assetMetadata)if(m.tileId==i.tileId && m.hasHeight){h+=m.height[px->tilePixel];break;}
    }
   }
   const char shade=char(std::max(0,std::min(255,h*255/100)));for(int k=0;k<3;k++)ppm.put(shade);
  }
 };
 image("-captured-composed-height");
 std::array<std::unique_ptr<RemasterDungeonHeightMap>,2> maps;
 std::ofstream report(out+".json");report<<"{\"room\":"<<c.roomIndex<<",\"profile_pin\":\""<<hex.str()<<"\",\"layers\":[";
 // Reset old packet placement results; metadata/ownership/flips remain captured.
 for(auto &i:frame->tileInstances)if(i.source==RemasterSourceType::Background && i.sourceIndex<2){i.heightOffset=0;i.hasPlacementHeight=false;}
  for(int pass=0;pass<2;pass++) {const int bg=pass;
  auto words=std::make_unique<std::array<uint8_t,16384>>();auto lc=c;lc.placementBackgroundIndex=bg;
  if(!bg){std::copy_n(c.constructedMaps+8192,8192,words->begin());std::copy_n(c.constructedMaps,8192,words->begin()+8192);lc.constructedMaps=words->data();std::swap(lc.graphicsBase,lc.bg1GraphicsBase);}
  auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();S9xRemasterDungeonPlacements(lc,profile,*tiles);lc.placements=tiles->data();
   if(bg)lc.supportRelations=&maps[0]->authoredSupportRelations;
   maps[bg]=std::make_unique<RemasterDungeonHeightMap>();auto &m=*maps[bg];require(S9xRemasterBuildDungeonHeightMap(lc,profile.upperFloorHeight,m),"solve");
  if(std::getenv("ALTTP_RUNTIME024_REGRESSION")) {
   require(c.roomIndex==0x60 && c.linkPlane==1 && hex.str()=="650c170f8c4a746e","024 regression pin");
    if(bg)require(m.planeOffsets[0][18*64+47]==24 && m.planeOffsets[1][18*64+47]==0,"authored bridge rise / independent underpass");
   else {
    const int middle=34*64+42,decorated=35*64+42;
    require((*tiles)[middle].tileHash==UINT64_C(0x83ff6d16868aaa72) && m.offsets[middle]==8,"reused middle base");
    require(m.offsets[decorated]==0 && (*tiles)[decorated].height[0]==15,"decorated local source");
    for(int k=0;k<64;k++)require((*tiles)[middle].height[k]+8==(*tiles)[decorated].height[k],"effective variant mismatch");
    require(m.geometryConflicts.empty() && m.solvedWallCells==243 && m.unknownWallCells==0,"BG1 equality repair");
   }
  }
   if(bg)report<<',';report<<"{\"bg\":"<<bg<<",\"shared_faces\":"<<m.sharedStructuralFaces<<",\"stair_fallbacks\":"<<m.configuredStairFallbacks<<",\"authored_relations\":"<<m.authoredSupportRelations.size()<<",\"solved_walls\":"<<m.solvedWallCells<<",\"unknown_walls\":"<<m.unknownWallCells<<",\"conflicts\":"<<m.structuralConflicts<<",\"equality_conflicts\":"<<m.geometryConflicts.size()<<",\"required_planes\":["<<m.requiredPlaneCells[0]<<','<<m.requiredPlaneCells[1]<<"],\"missing_planes\":["<<m.unknownPlaneCells[0]<<','<<m.unknownPlaneCells[1]<<"],\"cells\":[";
  for(int i=0;i<4096;i++) {
   if(i)report<<',';const auto &t=(*tiles)[i];
   report<<"{\"xy\":["<<i%64<<','<<i/64<<"],\"hash\":\""<<std::hex<<t.tileHash<<std::dec<<"\",\"role\":"<<int(t.surface)<<",\"has_local\":"<<t.hasHeight<<",\"local_span\":["<<int(*std::min_element(t.height.begin(),t.height.end()))<<','<<int(*std::max_element(t.height.begin(),t.height.end()))<<"],\"base\":"<<m.offsets[i]<<",\"structural\":["<<m.structuralPlaneOffsets[0][i]<<','<<m.structuralPlaneOffsets[1][i]<<"],\"support\":["<<m.planeOffsets[0][i]<<','<<m.planeOffsets[1][i]<<"],\"fallback\":"<<int(m.fallbackDirections[i])<<",\"flips\":["<<t.hflip<<','<<t.vflip<<"],\"distinct\":"<<t.distinctLowerUsage<<'}';
  }
    report<<"],\"relations\":[";for(size_t k=0;k<m.authoredSupportRelations.size();k++){if(k)report<<',';const auto &r=m.authoredSupportRelations[k];report<<'['<<r.a<<','<<r.b<<','<<r.rise<<']';}report<<"],\"local_samples\":[";
    for(int i=0;i<4096;i++) { if(i)report<<',';const auto &t=(*tiles)[i];report<<"{\"template\":"<<t.wallTemplateOffset<<",\"attr\":["<<int(c.collisionAttributes[i])<<','<<int(c.collisionAttributes[4096+i])<<"],\"height\":[";for(int k=0;k<64;k++){if(k)report<<',';report<<int(t.height[k]);}report<<"]}"; }
    report<<"]}";
  S9xRemasterApplyDungeonHeightMap(*frame,c,m,nullptr,nullptr,bg);
 }
  report<<"],\"runtime_instances\":[";
  bool first=true;for(const auto &i:frame->tileInstances)if(i.source==RemasterSourceType::Object || i.assetGroup=="stair_treads" || i.assetGroup=="stair_rails") {
   if(!first)report<<',';first=false;int lo=0,hi=0;
   if(i.hasPlacementHeight){lo=*std::min_element(i.placementHeight.begin(),i.placementHeight.end());hi=*std::max_element(i.placementHeight.begin(),i.placementHeight.end());}
   else for(const auto &m:frame->assetMetadata)if(m.tileId==i.tileId && m.hasHeight){lo=*std::min_element(m.height.begin(),m.height.end());hi=*std::max_element(m.height.begin(),m.height.end());break;}
   report<<"{\"source\":"<<int(i.source)<<",\"index\":"<<int(i.sourceIndex)<<",\"group\":\""<<i.assetGroup<<"\",\"base\":"<<i.heightOffset<<",\"effective_span\":["<<lo+i.heightOffset<<','<<hi+i.heightOffset<<"]}";
  }
  report<<"]}\n";
 if(std::getenv("ALTTP_RUNTIME024_REGRESSION")) {
  const auto &middle=frame->subPixels[135*frame->width+84];const auto &decorated=frame->subPixels[143*frame->width+84];
  const auto &a=frame->tileInstances.at(middle.instanceId-1),&b=frame->tileInstances.at(decorated.instanceId-1);
  require(a.sourceIndex==0 && a.tileId.hash==UINT64_C(0x83ff6d16868aaa72) && a.heightOffset==8 && !a.hasPlacementHeight,"packet middle runtime offset");
  require(b.sourceIndex==0 && b.tileId.hash==UINT64_C(0x9836d0b8491c70b9) && b.heightOffset==0 && !b.hasPlacementHeight,"packet decorated runtime offset");
 }
  image("-recomputed-composed-height");
  if(std::getenv("ALTTP_RUNTIME028_REGRESSION")) {
   require(c.roomIndex==0x60 && hex.str()=="650c170f8c4a746e","028 pin");
   for(int bg=0;bg<2;bg++) {
    require(maps[bg]->planeOffsets[0][47*64+47]==24,"authored landing rise");
    require(maps[bg]->configuredStairFallbacks==0,"fixed rise remained");
   }
   require(maps[0]->offsets[42*64+54]==0 && maps[0]->planeOffsets[1][42*64+55]==24,"eastern wall uphill boundary");
   require(maps[1]->planeOffsets[0][18*64+47]==24 && maps[1]->planeOffsets[1][18*64+47]==0,"independent overlap");
  }
 // Upgrade legacy packets to the current representation before serialization.
 frame->schemaVersion=REMASTER_FRAME_SCHEMA_VERSION;
 require(S9xWriteRemasterFrame(*frame,out+".s9xrmf"),"write review packet");
 std::cout<<"room="<<c.roomIndex<<" matching_BG_pixels="<<matched<<" pinned-profile="<<hex.str()<<" BG2="<<maps[1]->solvedWallCells<<'/'<<maps[1]->unknownWallCells<<'/'<<maps[1]->structuralConflicts<<" BG1="<<maps[0]->solvedWallCells<<'/'<<maps[0]->unknownWallCells<<'/'<<maps[0]->structuralConflicts<<'\n';
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
