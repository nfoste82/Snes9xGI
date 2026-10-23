#include "../../runtime/dungeon_transition_attributes.h"
extern "C" int RemasterCheckTransitionAttributes(const unsigned char *ram,unsigned char *result) {
  std::array<uint8_t,8192> out;
  if(!S9xRemasterCompleteDungeonAttributes(ram,out))return 0;
  for(int i=0;i<8192;i++)result[i]=out[i];
  return 1;
}
