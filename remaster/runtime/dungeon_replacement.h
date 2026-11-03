// Value-owned USA-ROM liftable evidence. This observes the game, never writes RAM.
#ifndef _REMASTER_DUNGEON_REPLACEMENT_H_
#define _REMASTER_DUNGEON_REPLACEMENT_H_
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

struct RemasterDungeonReplacementEvidence {
 struct Slot {
  uint16_t state=0, objectOffset=0, position=0;
  std::array<uint16_t,4> words={}; // UL, LL, UR, LR (handler order)
  bool ordinaryPotDeclaration=false;
  bool operator==(const Slot &b) const {return state==b.state && objectOffset==b.objectOffset && position==b.position && words==b.words && ordinaryPotDeclaration==b.ordinaryPotDeclaration;}
 };
 struct Secret {
  uint16_t position=0;uint8_t value=0;
  bool operator==(const Secret &b) const {return position==b.position && value==b.value;}
 };
 bool available=false;
 uint16_t room=0xffff;
 std::array<Slot,16> slots={};
 std::array<uint8_t,1024> attributes={};
 std::vector<Secret> secrets;
 bool operator==(const RemasterDungeonReplacementEvidence &b) const {
  return available==b.available && (!available || (room==b.room && slots==b.slots && attributes==b.attributes && secrets==b.secrets));
 }
 bool operator!=(const RemasterDungeonReplacementEvidence &b) const {return !(*this==b);}
};

inline RemasterDungeonReplacementEvidence S9xRemasterExtractDungeonReplacementEvidence(
 const uint8_t *ram,size_t ramSize,const uint8_t *rom,size_t romSize,bool verified,uint16_t room)
{
 RemasterDungeonReplacementEvidence e;
 if(!verified || !ram || ramSize<0x10200 || !rom || room>=320 || romSize!=1048576)return e;
 auto word=[](const uint8_t *p){return uint16_t(p[0]|p[1]<<8);};
 // Verified USA $81:DB69 is a little-endian bank81 address table.
 // Keep the entire search in bank81 and require its terminator. No inferred
 // "no secret" on truncation, duplicate destinations, or malformed pointers.
 constexpr size_t table=0xdb69;
 const size_t start=word(rom+table+room*2);
 if(start<table+640 || start>=0x10000)return e;
 bool terminated=false;
 for(size_t p=start;e.secrets.size()<256 && p+2<=0x10000;p+=3) {
  const uint16_t pos=word(rom+p);
  if(pos==0xffff){terminated=true;break;}
  if(p+3>0x10000 || (pos&0xc001) || (pos&0x1fff)/2%64==63 || (pos&0x1fff)/2/64==63)return {};
  for(const auto &s:e.secrets)if(s.position==pos)return {};
  e.secrets.push_back({pos,rom[p+2]});
 }
 if(!terminated)return {};
 e.room=room;
 const size_t ptrAt=0xf8000+room*3;
 const uint32_t ptr=rom[ptrAt]|uint32_t(rom[ptrAt+1])<<8|uint32_t(rom[ptrAt+2])<<16;
 const size_t stream=((ptr>>16)&127)*32768+(ptr&32767);
 for(size_t i=0;i<16;i++) {
  auto &s=e.slots[i];s.state=word(ram+0x500+2*i);s.objectOffset=word(ram+0x520+2*i);s.position=word(ram+0x540+2*i);
  for(size_t k=0;k<4;k++)s.words[k]=word(ram+0x560+k*32+2*i);
  if((ptr&0x8000) && s.objectOffset>=3 && stream+s.objectOffset<=romSize &&
     (ptr&0xffff)+s.objectOffset<=0x10000) {
   const auto *d=rom+stream+s.objectOffset-3;const uint16_t raw=word(d);
   // First supported declaration: exact subtype2 single-pot, no repeated runs.
   if((raw&0xfc)==0xfc && (d[2]&63)==0x2f) {
    const int x=((raw&3)<<4)|(raw>>12),y=((raw>>8)&15)*4+(d[2]>>6);
    s.ordinaryPotDeclaration=(s.position&0x1fff)==2*(y*64+x);
   }
  }
 }
 std::copy_n(ram+0xfe00,1024,e.attributes.begin());
 e.available=true;return e;
}

// Independently versioned additive sidecar and recording trailer. All integers
// are explicit little endian, never native structs or borrowed WRAM storage.
inline std::vector<uint8_t> S9xRemasterSerializeDungeonReplacementEvidence(const RemasterDungeonReplacementEvidence &e)
{
 if(!e.available || e.room>=320 || e.secrets.size()>256)return {};
 std::vector<uint8_t> b={'A','L','T','P','D','R','1',0};
 auto w=[&](uint16_t v){b.push_back(uint8_t(v));b.push_back(uint8_t(v>>8));};
 w(e.room);w(uint16_t(e.secrets.size()));
 for(const auto &s:e.slots){w(s.state);w(s.objectOffset);w(s.position);for(auto v:s.words)w(v);b.push_back(s.ordinaryPotDeclaration);}
 b.insert(b.end(),e.attributes.begin(),e.attributes.end());
 for(const auto &s:e.secrets){w(s.position);b.push_back(s.value);}
 return b;
}
inline bool S9xRemasterDeserializeDungeonReplacementEvidence(const std::vector<uint8_t> &b,RemasterDungeonReplacementEvidence &out)
{
 out={};
 if(b.size()<1276 || std::memcmp(b.data(),"ALTPDR1\0",8))return false;
 size_t p=8;auto w=[&](){uint16_t v=b[p]|b[p+1]<<8;p+=2;return v;};
 RemasterDungeonReplacementEvidence e;e.room=w();const size_t count=w();
 if(e.room>=320 || count>256 || b.size()!=1276+count*3)return false;
 for(auto &s:e.slots){s.state=w();s.objectOffset=w();s.position=w();for(auto &v:s.words)v=w();if(b[p]>1)return false;s.ordinaryPotDeclaration=b[p++]!=0;}
 std::copy_n(b.data()+p,1024,e.attributes.begin());p+=1024;
 for(size_t i=0;i<count;i++) {
  const uint16_t pos=w();const uint8_t value=b[p++];
  if((pos&0xc001) || (pos&0x1fff)/2%64==63 || (pos&0x1fff)/2/64==63)return false;
  for(const auto &s:e.secrets)if(s.position==pos)return false;
  e.secrets.push_back({pos,value});
 }
 e.available=true;out=std::move(e);return true;
}
#endif
