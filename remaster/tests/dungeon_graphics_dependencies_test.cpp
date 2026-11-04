// Exact closure includes current words and prospective replacement words, not
// screen/OBJ address ranges. Unknown producer inputs keep full VRAM coverage.
#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
int main() {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto saved=std::make_unique<std::array<uint8_t,65536>>();
 RemasterDungeonFloorContext c;c.constructedMaps=maps->data();c.graphics=gfx->data();
 c.replacementEvidence.available=true;
 c.replacementEvidence.slots[0].words={0x4001,0x8002,0xc003,4};
 for(int base:{0,0xfff0,0x4000})for(int other:{0x2000,0xffff,0xffef}) {
  c.graphicsBase=base;c.bg1GraphicsBase=other;
  std::array<bool,65536> expected={},actual={};
  auto mark=[&](int b,int tile){for(int n=0;n<32;n++)expected[uint16_t(b+tile*32+n)]=true;};
  const int peer=other==0xffff?base:other;
  mark(base,0);mark(peer,0);
  for(int tile=1;tile<=4;tile++){mark(base,tile);mark(peer,tile);}
  S9xRemasterDungeonGraphicsDependencies(c,[&](uint16_t at){actual[at]=true;});
  assert(actual==expected);
  // Inventory equality above covers all 65536 addresses independently. Check
  // the exact-byte consumer on boundaries and each prospective tile as well.
  for(int n:{0,31,32,63,127,128,159,50000,65535,int(uint16_t(base+32)),int(uint16_t(peer+128))}) {
   (*gfx)[n]=1;assert(S9xRemasterDungeonGraphicsMatch(c,saved->data())==!expected[n]);(*gfx)[n]=0;
  }
 }
 RemasterDungeonWriteTrace writes;c.producerWrites=&writes;
 (*gfx)[50000]=1;assert(!S9xRemasterDungeonGraphicsMatch(c,saved->data()));
 c.producerWrites=nullptr;c.constructedMaps=nullptr;
 assert(!S9xRemasterDungeonGraphicsMatch(c,saved->data()));
 std::cout<<"PASS exact dependency inventory both bases/wraps/HV/four prospective words/full fallback\n";
}
