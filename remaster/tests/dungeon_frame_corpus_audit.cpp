// Failure-class inventory over complete own-input captures, no viewport denominator.
#include "../runtime/remaster.h"
#include <filesystem>
#include <iostream>
static std::vector<uint8_t> load(const std::string &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 std::cout<<"capture,bg,door_cells,owned_frame_cells,ambiguous_frame_cells,authored_wall_unknown,diagonal_cells,diagonal_authored_equal,diagonal_solved,required0,required1,missing0,missing1,conflicts,unknown_walls,stair_records,complete_stair_proofs,attached_stairs,stair_cells,doorway_support_markers,doorway_plane_attachments,unknown_doorway_cells,conflicting_doorway_cells\n";
 for(const auto &file:std::filesystem::directory_iterator(argv[1])){
  const auto name=file.path().string();const std::string suffix=".s9xrmf.dungeon-input.bin";
  if(name.size()<suffix.size() || name.substr(name.size()-suffix.size())!=suffix)continue;
  const auto path=name.substr(0,name.size()-18); // strip .dungeon-input.bin
  auto input=load(name),tpl=load(path+".dungeon-templates.bin"),meta=load(path+".dungeon.json");
  RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
  const std::string text(meta.begin(),meta.end());
  if(input.size()!=90120 || tpl.size()!=12884 || std::memcmp(input.data(),"ALTPDI1\0",8) ||
   std::memcmp(tpl.data(),"ALTPDT1\0",8) || text.find("66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb")==std::string::npos ||
   !S9xRemasterLoadProfile(path+".profile.toml",profile,errors)){
   std::cerr<<"unsupported "<<path<<'\n';continue;
  }
  auto number=[&](const char *key,int index=0){auto pos=text.find(std::string("\"")+key+"\":");if(pos==std::string::npos)return 0;pos=text.find(':',pos)+1;if(text[pos]=='[')pos++;for(int i=0;i<index;i++)pos=text.find(',',pos)+1;return std::atoi(text.c_str()+pos);};
  RemasterDungeonFloorContext c;c.indoors=c.verifiedAlttpRom=true;c.roomIndex=number("room");
  c.linkX=number("link_xy");c.linkY=number("link_xy",1);c.linkPlane=number("plane");
  c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");
  c.graphics=input.data()+24584;c.collisionAttributes=input.data()+16392;c.dungeonTemplateData=tpl.data()+8;
  c.constructedMaps=input.data()+8;S9xRemasterDungeonStairsFromAttributes(c);
  auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
  auto lower=std::make_unique<RemasterDungeonHeightMap>();
  for(int bg=0;bg<2;bg++){
   std::rotate(input.begin()+8,input.begin()+8200,input.begin()+16392);std::swap(c.graphicsBase,c.bg1GraphicsBase);
   c.placementBackgroundIndex=bg;
   if(bg){c.supportRelations=&lower->authoredSupportRelations;c.wallSupportRelations=&lower->authoredWallSupportRelations;c.doorwaySupportRelations=&lower->doorwaySupportRelations;}
   S9xRemasterDungeonPlacements(c,profile,*tiles);c.placements=tiles->data();
    auto map=std::make_unique<RemasterDungeonHeightMap>();S9xRemasterBuildDungeonHeightMap(c,profile.upperFloorHeight,*map);
    if(const char *out=std::getenv("ALTTP_TRANSITION_FLOOR_AUDIT")) {
     std::ofstream f(out,std::ios::app);
     for(int p=0;p<2;p++)for(int cell=0;cell<4096;cell++)if(c.collisionAttributes[p*4096+cell]==0x22) {
      const auto &d=map->roomDiagnostics->cells[p*4096+cell];
      f<<file.path().filename().string()<<','<<bg<<','<<p<<','<<cell<<','<<d.semanticFloorSupport<<','<<d.floorId<<','<<d.floorBase<<','<<map->planeOffsets[p][cell]<<','<<d.heightConflict<<','<<d.planeObserved<<'\n';
     }
    }
   if(const char *out=std::getenv("ALTTP_FRAME_ATTACHMENTS_AUDIT")){
    std::ofstream f(out,std::ios::app);
    for(const auto &a:map->doorframeAttachments)f<<file.path().filename().string()<<','<<bg<<','<<a.cell<<','<<a.plane<<','<<a.base<<','<<a.visible<<','<<a.mask<<'\n';
   }
   int doors=0,owned=0,ambiguous=0,unknown=0,diagonal=0,equal=0,solved=0,proofs=0,attached=0,stairCells=0,doorwayMarkers=0;
   for(int si=0;si<c.stairCount;si++){RemasterDungeonStairProof proof;proofs+=S9xRemasterDungeonStairProof(c,c.stairs[si],proof);}
   for(const auto &a:map->stairAttachments){attached+=!a.conflict;
    if(const char *out=std::getenv("ALTTP_FRAME_STAIRS_AUDIT")){std::ofstream f(out,std::ios::app);f<<file.path().filename().string()<<','<<bg<<','<<a.origin<<','<<a.lowPlane<<','<<a.highPlane<<','<<a.rise<<','<<a.base<<','<<a.conflict<<'\n';}}
   for(bool ownedStair:map->producerOwnedStair)stairCells+=ownedStair;
    for(int i=0;i<4096;i++){
     const auto &t=(*tiles)[i];const auto a=profile.assets.find({t.tileHash,1,4});
     if(const char *out=std::getenv("ALTTP_OBSTACLE_MEMBER_AUDIT"))if(t.authoredObstacle){
      std::ofstream f(out,std::ios::app);const RemasterDungeonHeightMap::AuthoredObstacleEvidence *e=nullptr;
      for(const auto &v:map->authoredObstacleEvidence)if(v.assembly==t.authoredObstacleAssembly)e=&v;
      f<<file.path().filename().string()<<','<<bg<<','<<i<<','<<std::hex<<t.tileHash<<std::dec<<','<<t.hflip<<','<<t.vflip<<','<<t.authoredObstacleAssembly<<','<<t.authoredObstacleContact<<','<<int(t.authoredObstacleDirection)<<','<<t.hasHeight<<','<<map->offsets[i]<<','<<map->planeOffsets[0][i]<<','<<map->planeOffsets[1][i]<<','<<t.distinctLowerUsage<<','<<int(t.floorPlaneMask)<<','<<int(c.collisionAttributes[i])<<','<<int(c.collisionAttributes[4096+i])<<','<<(e?e->plane:-1)<<','<<(e?e->component:-1)<<','<<(e?e->conflict:false)<<','<<(e?e->witnessedContacts:0)<<','<<(e?e->contacts:0);
      for(int k=0;k<64;k++)f<<','<<t.height[k];f<<'\n';
     }
    doorwayMarkers+=t.doorwaySupport;
    if(const char *out=std::getenv("ALTTP_FRAME_OWNERSHIP_AUDIT"))if(t.surface==RemasterDungeonFloorContext::Surface::Door){
     std::ofstream f(out,std::ios::app);f<<file.path().filename().string()<<','<<bg<<','<<i<<','<<t.doorTemplateOffset<<','<<t.doorAssembly<<','<<int(t.doorDirection)<<','<<t.hasDoorframeMask<<','<<t.doorframeMask<<'\n';
    }
    doors+=t.surface==RemasterDungeonFloorContext::Surface::Door;
    owned+=t.doorTemplateOffset && t.doorDirection<4;ambiguous+=t.doorDirection==0xfe;
    unknown+=a!=profile.assets.end() && a->second.tileRole==RemasterTileRole::Wall && t.surface==RemasterDungeonFloorContext::Surface::Unknown;
    if(t.diagonalHeightPreview){diagonal++;equal+=a!=profile.assets.end() && a->second.height==t.height;solved+=map->offsets[i]!=255;}
   }
   std::cout<<file.path().filename().string()<<','<<bg<<','<<doors<<','<<owned<<','<<ambiguous<<','<<unknown<<','<<diagonal<<','<<equal<<','<<solved
    <<','<<map->requiredPlaneCells[0]<<','<<map->requiredPlaneCells[1]<<','<<map->unknownPlaneCells[0]<<','<<map->unknownPlaneCells[1]<<','<<map->structuralConflicts<<','<<map->unknownWallCells
    <<','<<int(c.stairCount)<<','<<proofs<<','<<attached<<','<<stairCells
    <<','<<doorwayMarkers<<','<<map->doorframeAttachments.size()<<','<<map->unknownDoorframeCells<<','<<map->conflictingDoorframeCells<<'\n';
   if(!bg)lower=std::move(map);
  }
 }
}
