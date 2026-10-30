#ifndef S9X_REMASTER_CONTEXT_ROOM_H
#define S9X_REMASTER_CONTEXT_ROOM_H
#include "frame.h"
#include "tile_hash.h"
#include <memory>

// Not serialized and never reconstructed from sidecars. Only the completed-frame
// producer constructs this value, on an explicit inspector request.
struct RemasterContextRoomCapture {
 RemasterFrame packet;
 std::array<uint8_t,16384> maps{};
 std::array<uint8_t,65536> graphics{};
 std::array<uint16_t,2> bases{};
 uint64_t session=0, generation=0;
 uint16_t room=0xffff;
 bool settled=false, supported=false;
};
struct RemasterContextRoomCell {
 RemasterTileContentId tile;
 RemasterContextLocation at;
 bool hFlip=false,vFlip=false;
};
struct RemasterContextRoom {
 std::shared_ptr<const RemasterContextRoomCapture> capture;
 std::vector<RemasterContextRoomCell> cells;
 std::string reason="Unavailable: no same-frame live room capture (imported packets/sidecars unsupported)";
 static bool sameFrame(const RemasterFrame &a,const RemasterFrame &b) {
  if(a.area.id!=b.area.id || a.width!=b.width || a.height!=b.height || a.profileRomSha256!=b.profileRomSha256 || a.tileInstances.size()!=b.tileInstances.size() || a.contextLocations.size()!=b.contextLocations.size())return false;
  for(const auto &e:a.contextLocations){const auto it=b.contextLocations.find(e.first);if(it==b.contextLocations.end() || it->second<e.second || e.second<it->second)return false;}
  for(size_t i=0;i<a.tileInstances.size();++i){const auto &x=a.tileInstances[i],&y=b.tileInstances[i];if(!(x.tileId==y.tileId) || x.source!=y.source || x.sourceIndex!=y.sourceIndex || x.hFlip!=y.hFlip || x.vFlip!=y.vFlip)return false;}
  for(unsigned plane=0;plane<3;++plane){const auto &x=plane==0?a.mainPixels:plane==1?a.subPixels:a.scenePixels;const auto &y=plane==0?b.mainPixels:plane==1?b.subPixels:b.scenePixels;
   if(x.size()!=y.size())return false;for(size_t i=0;i<x.size();++i)if(x[i].instanceId!=y[i].instanceId || x[i].tilePixel!=y[i].tilePixel)return false;}
  return true;
 }
 bool admit(std::shared_ptr<const RemasterContextRoomCapture> input) {
  capture.reset(); cells.clear();
  if (!input) return false;
  const auto &c=*input; const auto &f=c.packet;
  if (!c.settled || !c.supported || c.room>=320 || f.area.id!=c.room || !f.area.valid() ||
   f.profileRomSha256!="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb") {
   reason="Unavailable: requires verified settled ALTTP indoor module 7/0, complete 4bpp maps and graphics"; return false;
  }
  std::map<uint16_t,RemasterTileContentId> cache;
  for (unsigned bg=0;bg<2;++bg) for(unsigned i=0;i<4096;++i) {
   const unsigned off=(bg?0:8192)+i*2; const unsigned word=c.maps[off]|unsigned(c.maps[off+1])<<8;
   const uint16_t address=uint16_t(c.bases[bg]+(word&1023)*32);
   auto found=cache.find(address);
   if(found==cache.end()) {
    uint8_t pixels[64];
    for(unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) {
     unsigned v=0; for(unsigned b=0;b<4;++b) v|=((c.graphics[uint16_t(address+b/2*16+y*2+b%2)]>>(7-x))&1)<<b;
     pixels[y*8+x]=uint8_t(v);
    }
    found=cache.emplace(address,RemasterTileContentId{S9xRemasterHashTile(4,pixels),1,4}).first;
   }
   RemasterContextLocation at; at.area=c.room;at.background=bg;at.x=i%64;at.y=i/64;
   cells.push_back({found->second,at,bool(word&0x4000),bool(word&0x8000)});
  }
  // Check every published observation, including non-winning scene/sub owners.
  for(const auto *plane:{&f.mainPixels,&f.subPixels,&f.scenePixels}) for(const auto &pixel:*plane) {
   if(!pixel.instanceId)continue;
   if(pixel.instanceId>f.tileInstances.size()) {reason="Unavailable: invalid pixel owner";cells.clear();return false;}
   const auto &in=f.tileInstances[pixel.instanceId-1];
   if(in.source==RemasterSourceType::Background && in.sourceIndex<2 && !f.contextLocations.count(pixel.instanceId)) {
    reason="Unavailable: background owner has no unambiguous room location";cells.clear();return false;
   }
  }
  for(const auto &entry:f.contextLocations) {
   const auto &at=entry.second;
   if(!entry.first || entry.first>f.tileInstances.size() || at.area!=c.room || at.background>1 || at.x>=64 || at.y>=64) {
    cells.clear();reason="Unavailable: invalid published room owner";return false;
   }
   const auto &in=f.tileInstances[entry.first-1];const auto &cell=cells[at.background*4096+at.y*64+at.x];
   if(in.source!=RemasterSourceType::Background || in.sourceIndex!=at.background || !(in.tileId==cell.tile) || in.hFlip!=cell.hFlip || in.vFlip!=cell.vFlip) {
    cells.clear();reason="Unavailable: complete map disagrees with published identity/transform";return false;
   }
  }
  capture=std::move(input);reason.clear();return true;
 }
};
#endif
