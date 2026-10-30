// Read-only packet bridge. Reuse the production decoder and existing CPU trace.
#include "../../runtime/frame.h"
#include <iostream>
int main(int argc,char **argv) {
 if(argc!=3 || std::string(argv[2])!="--inventory")return 2;
 auto f=std::make_unique<RemasterFrame>();
 if(!S9xReadRemasterFrame(argv[1],*f))return 2;
 std::cout<<"{\"schema\":"<<f->schemaVersion<<",\"width\":"<<f->width<<",\"height\":"<<f->height
  <<",\"rom_sha256\":\""<<f->profileRomSha256<<"\",\"instances\":[";
 for(size_t n=0;n<f->tileInstances.size();++n) {
  const auto &i=f->tileInstances[n];if(n)std::cout<<',';
  std::cout<<"{\"id\":"<<n+1<<",\"source\":"<<int(i.source)<<",\"bg\":"<<int(i.sourceIndex)+1
   <<",\"slot\":"<<i.tileNumber<<",\"vram_address\":"<<i.vramAddress<<",\"palette\":"<<int(i.palette)
   <<",\"priority\":"<<int(i.ppuPriority)<<",\"priority_serialized\":"<<(f->schemaVersion>=18 ? "true" : "false")
   // BG SetDraw callers omit the priority argument: the serialized default zero
   // is NOT the map's bit 13. Packet schema alone cannot attest another producer.
   <<",\"priority_semantics\":\"unattested-bg-field\""
   <<",\"hash\":\""<<std::hex<<i.tileId.hash<<std::dec<<"\",\"hflip\":"<<i.hFlip<<",\"vflip\":"<<i.vFlip<<",\"base\":"<<i.heightOffset<<'}';
 }
 std::cout<<"],\"assets\":[";
 for(size_t n=0;n<f->assets.size();++n) {
  const auto &a=f->assets[n];if(n)std::cout<<',';
  std::cout<<"{\"hash\":\""<<std::hex<<a.tileId.hash<<std::dec<<"\",\"bit_depth\":"<<int(a.tileId.bitDepth)<<",\"indices\":[";
  for(int k=0;k<64;++k){if(k)std::cout<<',';std::cout<<int(a.indices[k]);}std::cout<<"]}";
 }
 std::cout<<"]}\n";
}
