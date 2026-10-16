// Read-only exact aligned viewport audit. Usage FRAME [OWREPLAY1.txt ORIGIN_X ORIGIN_Y BG]
#include "../../remaster/frame.h"
#include <iostream>
#include <map>
#include <tuple>
// Candidate stream can be supplied as '-' for stdin, avoiding another artifact.
int main(int argc,char **argv) {
 if(argc!=6&&argc!=7&&argc!=2)return 2;
 RemasterFrame f;if(!S9xReadRemasterFrame(argv[1],f))return 2;
 if(argc==2) {
  using K=std::tuple<int,int,uint64_t>;std::map<K,size_t> inventory;
  for(const auto &p:f.mainPixels)if(p.instanceId) {
   const auto &i=f.tileInstances[p.instanceId-1];
   if(i.source==RemasterSourceType::Background)inventory[{i.sourceIndex,i.tileNumber,i.tileId.hash}]++;
  }
  for(auto e:inventory)std::cout<<"bg "<<std::get<0>(e.first)<<" slot "<<std::get<1>(e.first)
   <<" hash "<<std::hex<<std::get<2>(e.first)<<std::dec<<" pixels "<<e.second<<"\n";
  return 0;
 }
 std::ifstream file; if(std::string(argv[2])!="-")file.open(argv[2]);
 std::istream &in=std::string(argv[2])=="-"?std::cin:file;std::string magic,rom;size_t count;
 if(!(in>>magic>>rom>>count)||magic!="OWREPLAY1"||rom!=f.profileRomSha256)return 2;
 using Sig=std::tuple<uint64_t,int,int,bool,bool>;
 std::map<std::pair<int,int>,Sig> cells;
 std::map<std::pair<int,int>,int> status;
 for(size_t k=0;k<count;++k) {
  int x,y,w,s,r,b;std::string hash;if(!(in>>x>>y>>w>>hash>>s>>r>>b))return 2;
  cells[{x,y}]={std::stoull(hash,nullptr,16),w&1023,(w>>10)&7,bool(w&0x4000),bool(w&0x8000)};
  status[{x,y}]=s;
  if(s==1)for(int j=0;j<256;++j){int v;if(!(in>>v))return 2;}
 }
 int ox=std::stoi(argv[3]),oy=std::stoi(argv[4]),bg=std::stoi(argv[5]);
 bool exportCells=argc==7&&std::string(argv[6])=="--export-cells";
 std::map<std::pair<int,int>,Sig> observed;
 using Diff=std::tuple<int,int,uint64_t,int,int,int>;
 std::map<Diff,size_t> differences;
 size_t matched=0,solved=0;
 for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x) {
  const auto &p=f.mainPixels[y*f.width+x];if(!p.instanceId||p.tilePixel>=64)continue;
  const auto &i=f.tileInstances[p.instanceId-1];if(i.source!=RemasterSourceType::Background||i.sourceIndex!=bg)continue;
  int dx=p.tilePixel%8,dy=p.tilePixel/8;if(i.hFlip)dx=7-dx;if(i.vFlip)dy=7-dy;
  int wx=ox+int(x)-dx,wy=oy+int(y)-dy;auto it=cells.find({wx/8,wy/8});
   Sig actual{i.tileId.hash,i.tileNumber,i.palette,i.hFlip,i.vFlip};
   if(exportCells&&wx%8==0&&wy%8==0&&it!=cells.end()) {
    auto cell=std::make_pair(wx/8,wy/8);auto found=observed.find(cell);
    if(found!=observed.end()&&found->second!=actual)return 3;
    observed[cell]=actual;
   }
  if(wx%8==0&&wy%8==0&&it!=cells.end()&&it->second==actual){++matched;if(status[{wx/8,wy/8}]==1)++solved;continue;}
  int expected=it==cells.end()?-1:std::get<1>(it->second);
  differences[{i.tileNumber,i.palette,i.tileId.hash,expected,wx/8,wy/8}]++;
 }
 if(exportCells) {
  std::cout<<"{\"rom_sha256\":\""<<rom<<"\",\"cells\":[";bool first=true;
  for(const auto &e:observed){if(!first)std::cout<<',';first=false;
   const auto &s=e.second;int word=std::get<1>(s)|(std::get<2>(s)<<10)|(std::get<3>(s)?0x4000:0)|(std::get<4>(s)?0x8000:0);
   std::cout<<"{\"x\":"<<e.first.first<<",\"y\":"<<e.first.second<<",\"word\":"<<word<<",\"hash\":\""<<std::hex<<std::get<0>(s)<<std::dec<<"\"}";
  }std::cout<<"]}\n";return 0;
 }
 std::cout<<"matched "<<matched<<" solved "<<solved<<"\n";
 for(auto d:differences)std::cout<<"actual_slot "<<std::get<0>(d.first)<<" palette "<<std::get<1>(d.first)
  <<" hash "<<std::hex<<std::get<2>(d.first)<<std::dec<<" expected_slot "<<std::get<3>(d.first)
  <<" cell "<<std::get<4>(d.first)<<','<<std::get<5>(d.first)<<" pixels "<<d.second<<"\n";
}
