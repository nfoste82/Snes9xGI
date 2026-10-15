// Read-only obstacle diagnostics; in-memory role experiments never rewrite profiles.
#include "../../remaster/remaster.h"
#include <iostream>
#include <stdexcept>
#include "../../sha256.cpp"
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static std::vector<uint8_t> bytes(const std::string &path){std::ifstream f(path,std::ios::binary);require(bool(f),"missing input");return {std::istreambuf_iterator<char>(f),{}};}
static bool target(uint64_t hash){return hash==UINT64_C(0xfd86d3ed217c0d03)||hash==UINT64_C(0x73caad274b5bb270);}
static std::string sha(std::vector<uint8_t> v){unsigned char h[32];sha256sum(v.data(),static_cast<unsigned>(v.size()),h);std::ostringstream s;for(auto b:h)s<<std::hex<<std::setw(2)<<std::setfill('0')<<int(b);return s.str();}
int main(int argc,char **argv){try{
 require(argc>=2,"usage: packet [profile] [exact011-final-sidecars|candidate011-final-sidecars] [diagnostic-original-heights-profile]");
 auto frame=std::make_unique<RemasterFrame>();require(S9xReadRemasterFrame(argv[1],*frame),"invalid packet");
 std::cout<<"packet="<<argv[1]<<'\n';
 for(const auto &a:frame->assetMetadata)if(target(a.tileId.hash)){
  std::cout<<"packet hash="<<std::hex<<a.tileId.hash<<std::dec<<" role="<<int(a.tileRole)<<" local="<<int(*std::min_element(a.height.begin(),a.height.end()))<<".."<<int(*std::max_element(a.height.begin(),a.height.end()))<<'\n';
  std::map<std::tuple<int,int,bool,int,int>,int> usages;
  for(const auto &i:frame->tileInstances)if(i.tileId==a.tileId){const auto &h=i.hasPlacementHeight?i.placementHeight:a.height;
   usages[{i.sourceIndex,i.heightOffset,i.hasPlacementHeight,*std::min_element(h.begin(),h.end()),*std::max_element(h.begin(),h.end())}]++;}
  for(const auto &u:usages)std::cout<<" instances="<<u.second<<" BG="<<std::get<0>(u.first)+1<<" base="<<std::get<1>(u.first)<<" override="<<std::get<2>(u.first)<<" effective="<<std::get<1>(u.first)+std::get<3>(u.first)<<".."<<std::get<1>(u.first)+std::get<4>(u.first)<<'\n';
 }
 if(argc<3)return 0;
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;require(S9xRemasterLoadProfile(argv[2],profile,errors),"invalid profile");
 for(const auto &a:profile.assets)if(target(a.first.hash)){
  std::cout<<"profile hash="<<std::hex<<a.first.hash<<std::dec<<" role="<<int(a.second.tileRole)<<" local="<<int(*std::min_element(a.second.height.begin(),a.second.height.end()))<<".."<<int(*std::max_element(a.second.height.begin(),a.second.height.end()))<<" groups=";
  for(const auto &g:profile.assetGroups)if(std::find(g.second.tileIds.begin(),g.second.tileIds.end(),a.first)!=g.second.tileIds.end())std::cout<<g.first<<',';std::cout<<'\n';
 }
 if(argc<4)return 0;
 const bool candidate=std::string(argv[3])=="candidate011-final-sidecars";
 require(candidate || std::string(argv[3])=="exact011-final-sidecars","only the pinned011 context is supported");
 require(sha(bytes(argv[1]))=="4439c9d8f2bee56c921e9c5e4e60b36985b2b4ef66c2e76bb6f83f686031e5ee","not pinned011 packet");
 if(!candidate)require(sha(bytes(argv[2]))=="b9c36c6a2c5964665ce792391f90e9cb6ed1f6fe3b66a5eec55b6dd2dd4b9997","not pinned011 profile");
 std::cout<<"profile_sha256="<<sha(bytes(argv[2]))<<'\n';
 if(argc==5){
  require(candidate,"height comparison requires candidate mode");
  RemasterProfile original;require(S9xRemasterLoadProfile(argv[4],original,errors),"invalid original heights profile");
  int restored=0;for(auto &a:profile.assets){auto b=original.assets.find(a.first);if(b!=original.assets.end() && a.second.height!=b->second.height){a.second.height=b->second.height;a.second.hasHeight=b->second.hasHeight;restored++;}}
  if(std::getenv("DIAGNOSE_SOURCE_ROLES")){
   profile.assetGroups=original.assetGroups;
   for(auto &a:profile.assets){auto b=original.assets.find(a.first);a.second.tileRole=b==original.assets.end()?RemasterTileRole::Unknown:b->second.tileRole;}
   for(auto &a:profile.assets)if(target(a.first.hash))a.second.tileRole=RemasterTileRole::Obstacle;
   std::cout<<"DIAGNOSTIC source groups/roles, target obstacles retained\n";
  }
  if(const char *role=std::getenv("DIAGNOSE_REMOVE_ROLE")){
   std::set<RemasterTileContentId> removed;
   for(auto &a:profile.assets)if(S9xRemasterTileRoleName(a.second.tileRole)==std::string(role)){
    auto b=original.assets.find(a.first);a.second.tileRole=b==original.assets.end()?RemasterTileRole::Unknown:b->second.tileRole;removed.insert(a.first);
   }
   for(auto &g:profile.assetGroups){auto &ids=g.second.tileIds;ids.erase(std::remove_if(ids.begin(),ids.end(),[&](const auto &id){return removed.count(id);}),ids.end());auto og=original.assetGroups.find(g.first);if(og!=original.assetGroups.end())for(const auto &id:og->second.tileIds)if(removed.count(id))ids.push_back(id);}
   std::cout<<"DIAGNOSTIC removed new role "<<role<<" ids="<<removed.size()<<'\n';
  }
  std::cout<<"DIAGNOSTIC ONLY in-memory original height arrays restored="<<restored<<"; candidate roles and groups unchanged\n";
 }
 auto input=bytes(std::string(argv[1])+".dungeon-input.bin"),templates=bytes(std::string(argv[1])+".dungeon-templates.bin");
 require(input.size()==90120 && !std::memcmp(input.data(),"ALTPDI1\0",8),"invalid complete input");
 require(sha(input)=="906c8089fd9863af5aa1ad81b7fa1be164cd0521c165c1102fc41d6b4a62a76b","not pinned011 final maps");
 require(templates.size()==12884 && !std::memcmp(templates.data(),"ALTPDT1\0",8),"invalid templates");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=0x60;c.previousRoomIndex=0x61;
 c.linkX=312;c.linkY=3449;c.linkPlane=0;c.backgroundScrollX=256;c.backgroundScrollY=3342;c.graphicsBase=c.bg1GraphicsBase=16384;
 c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=templates.data()+8;S9xRemasterDungeonStairsFromAttributes(c);
 for(bool authoredObstacle:{false,true}){
  if(candidate && authoredObstacle)continue; // actual candidate, no role mutations
  auto p=profile;if(authoredObstacle)for(auto &a:p.assets)if(target(a.first.hash))a.second.tileRole=RemasterTileRole::Obstacle;
  for(int layer=1;layer>=0;layer--){
   auto maps=std::make_unique<std::array<uint8_t,16384>>();std::copy_n(input.data()+8,16384,maps->begin());
   if(!layer)std::rotate(maps->begin(),maps->begin()+8192,maps->end());c.constructedMaps=maps->data();c.placementBackgroundIndex=layer;
   auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();S9xRemasterDungeonPlacements(c,p,*tiles);c.placements=tiles->data();
    auto map=std::make_unique<RemasterDungeonHeightMap>();require(S9xRemasterBuildDungeonHeightMap(c,p.upperFloorHeight,*map),"solve failed");
   std::cout<<"BG="<<layer+1<<" walls="<<map->solvedWallCells<<'/'<<map->unknownWallCells<<" conflicts="<<map->structuralConflicts<<" required="<<map->requiredPlaneCells[0]<<'/'<<map->requiredPlaneCells[1]<<" missing="<<map->unknownPlaneCells[0]<<'/'<<map->unknownPlaneCells[1]<<'\n';
   if(candidate && std::getenv("ASSERT_ACTIVATION") && layer){
    require(map->structuralConflicts==0 && !map->unknownPlaneCells[0] && !map->unknownPlaneCells[1],"activation support conflict");
    require(map->offsets[40*64+41]==42 && map->offsets[45*64+41]==42,"activation west chain");
    require(map->offsets[42*64+57]==26 && map->offsets[43*64+57]==66,"activation corner chain");
    require(map->planeOffsets[0][44*64+58]==74 && map->planeOffsets[1][44*64+58]==0,"activation cap planes");
    require(map->planeOffsets[0][18*64+47]==50 && map->planeOffsets[1][18*64+47]==0,"activation bridge planes");
   }
   if(candidate)for(int cell:{40*64+41,45*64+41,42*64+57,43*64+57,44*64+58,45*64+58,44*64+57,18*64+47,29*64+61,30*64+61,30*64+60,31*64+60,31*64+59}){
    const auto &t=(*tiles)[cell];std::cout<<"reference BG="<<layer+1<<" xy="<<cell%64<<','<<cell/64<<" hash="<<std::hex<<t.tileHash<<std::dec<<" role="<<int(t.surface)<<" base="<<map->offsets[cell]<<" planes="<<map->planeOffsets[0][cell]<<'/'<<map->planeOffsets[1][cell]<<" structural="<<map->structuralPlaneOffsets[0][cell]<<'/'<<map->structuralPlaneOffsets[1][cell]<<" local="<<int(*std::min_element(t.height.begin(),t.height.end()))<<".."<<int(*std::max_element(t.height.begin(),t.height.end()))<<'\n';
   }
   int targets=0;
   if(candidate && layer)for(int y=27;y<=33;y++)for(int x=57;x<=62;x++){
    int i=y*64+x;const auto &t=(*tiles)[i];std::cout<<"contact xy="<<x<<','<<y<<" hash="<<std::hex<<t.tileHash<<std::dec<<" role="<<int(t.surface)<<" attr="<<int(c.collisionAttributes[i])<<'/'<<int(c.collisionAttributes[4096+i])<<" floorMask="<<int(t.structuralFloorPlaneMask)<<" template="<<t.wallTemplateOffset<<" local="<<int(*std::min_element(t.height.begin(),t.height.end()))<<".."<<int(*std::max_element(t.height.begin(),t.height.end()))<<'\n';
   }
   if(candidate && layer)for(int i:{15*64+59,25*64+56,9*64+46,59*64+44,15*64+34}){
    const auto &t=(*tiles)[i];std::cout<<"audit xy="<<i%64<<','<<i/64<<" hash="<<std::hex<<t.tileHash<<std::dec<<" role="<<int(t.surface)<<" attr="<<int(c.collisionAttributes[i])<<'/'<<int(c.collisionAttributes[4096+i])<<" base="<<map->offsets[i]<<" planes="<<map->planeOffsets[0][i]<<'/'<<map->planeOffsets[1][i]<<'\n';
   }
   for(int i=0;i<4096;i++)if(target((*tiles)[i].tileHash)){
    targets++;
    if(!candidate){
     require(map->planeOffsets[1][i]==0,"lower support changed");
     require(map->planeOffsets[0][i]==(authoredObstacle?50:255),"obstacle upper support regression");
     require(map->offsets[i]==(authoredObstacle?50:0) && !map->fallbackDirections[i],"obstacle relief replaced or wrong visible base");
    }else {
     require((*tiles)[i].surface==RemasterDungeonFloorContext::Surface::Obstacle && std::all_of((*tiles)[i].height.begin(),(*tiles)[i].height.end(),[](int16_t h){return h==0;}),"candidate obstacle metadata not preserved");
     if(std::getenv("ASSERT_ACTIVATION"))require(map->planeOffsets[0][i]==50 && map->planeOffsets[1][i]==0 && map->offsets[i]==50 && !map->fallbackDirections[i],"actual activation obstacle base/local regression");
    }
    const auto &t=(*tiles)[i];std::cout<<"experiment="<<authoredObstacle<<" BG="<<layer+1<<" xy="<<i%64<<','<<i/64<<" hash="<<std::hex<<t.tileHash<<std::dec<<" role="<<int(t.surface)<<" attr="<<int(c.collisionAttributes[i])<<'/'<<int(c.collisionAttributes[4096+i])<<" local="<<int(*std::min_element(t.height.begin(),t.height.end()))<<".."<<int(*std::max_element(t.height.begin(),t.height.end()))<<" planes="<<map->planeOffsets[0][i]<<'/'<<map->planeOffsets[1][i]<<" visible="<<map->offsets[i]<<" fallback="<<int(map->fallbackDirections[i])<<'\n';
    if(authoredObstacle || candidate)for(int delta:{-64,64,-1,1}){int j=i+delta;const auto &n=(*tiles)[j];std::cout<<" neighbor="<<j%64<<','<<j/64<<" role="<<int(n.surface)<<" attr="<<int(c.collisionAttributes[j])<<'/'<<int(c.collisionAttributes[4096+j])<<" bases="<<map->planeOffsets[0][j]<<'/'<<map->planeOffsets[1][j]<<'\n';}
   }
   require(targets==(layer?8:0),"complete target placement denominator changed");
  }
 }
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
