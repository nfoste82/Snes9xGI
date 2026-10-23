#include "../runtime/frame.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv){try {
 if(argc!=2)throw std::runtime_error("usage: reference-engine-route.jsonl");
 std::ifstream in(argv[1]);std::string line;int frames=0,north=0,south=0,gridDifferences=0;
 while(std::getline(in,line)) {
  auto number=[&](const char *key,int index=0){auto p=line.find(std::string("\"")+key+"\":");if(p==std::string::npos)throw std::runtime_error("missing route pin");p=line.find(':',p)+1;if(line[p]=='[')p++;for(int k=0;k<index;k++)p=line.find(',',p)+1;return std::stoi(line.substr(p));};
  if(line.find("\"room\":")==std::string::npos || number("submodule")!=2 || number("phase")<2)continue;
  RemasterDungeonFloorContext c;c.module=7;c.submodule=2;c.constructionPhase=number("phase");c.roomIndex=number("room");c.previousRoomIndex=number("previous_room");
  c.scrollDirection=number("direction");c.destinationOriginX=number("destination_origin");c.destinationOriginY=number("destination_origin",1);
  c.hasDestinationOrigin=true;c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1)+1;
  for(int y=0;y<224;y++)for(int x=0;x<256;x++) {
   // Independent oracle: actual loader origin + full512 room interval in
   // 16bit arithmetic, on ONLY the engine-selected transition axis.
   const uint16_t local=c.scrollDirection<2?uint16_t(c.backgroundScrollY+y-c.destinationOriginY):uint16_t(c.backgroundScrollX+x-c.destinationOriginX);
   const auto expected=c.constructionPhase>=14 || local<512?c.roomIndex:c.previousRoomIndex;
   const auto observed=S9xRemasterDungeonRoomForScreenPosition(c,x,y);
   if(observed!=expected)throw std::runtime_error("actual engine loader-boundary ownership mismatch");
   auto grid=c;grid.hasDestinationOrigin=false;gridDifferences+=S9xRemasterDungeonRoomForScreenPosition(grid,x,y)!=expected;
  }
  frames++;north+=c.roomIndex<c.previousRoomIndex;south+=c.roomIndex>c.previousRoomIndex;
 }
 if(!north || !south)throw std::runtime_error("both actual door directions required");
 std::cout<<"PASS actual engine scroll frames="<<frames<<" north="<<north<<" south="<<south<<" old_grid_pixel_disagreements="<<gridDifferences<<'\n';
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
