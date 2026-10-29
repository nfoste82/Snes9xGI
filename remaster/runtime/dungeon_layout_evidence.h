#ifndef _REMASTER_DUNGEON_LAYOUT_EVIDENCE_H_
#define _REMASTER_DUNGEON_LAYOUT_EVIDENCE_H_
#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>

// Raw USA WRAM observations: not geometry or a
// binary capture extension. Old adapters/recordings explicitly lack evidence.
// Offsets and word order verified against zelda3 variables.h: RoomBounds is
// {a0,b0,a1,b1}, Y at $600 and X at $608. $610/$612 are up/down target/end;
// $614/$616 are left/right target/end, NOT additional room bounds.
struct RemasterDungeonLayoutEvidence {
 bool available=false;
 std::array<uint8_t,5> quadrant={}; // $a6..aa: fullsize X/Y, composite, Link X/Y
 uint16_t loadedLayout=0; // $40e dung_layout_and_starting_quadrant
 std::array<uint8_t,2> blastwall={}; // $452/$453 X/Y flags
 uint16_t overrides=0; // $fc dung_unk2; retain raw, do not infer meaning
 std::array<uint16_t,8> bounds={}; // $600..60f Y then X: a0,b0,a1,b1
 std::array<uint16_t,2> scroll={}; // $e2 BG2HOFS_copy2, $e8 BG2VOFS_copy2
 std::array<uint16_t,4> targets={}; // $610..617
 uint8_t transitionCounter=0; // $126
};
// Membership in the constructed 512x512 source map, NOT camera scroll space.
// USA Dungeon_AdjustForRoomLayout / RoomBounds_AddA/SubA change A by 256
// within a supertile; AddB/SubB change the supertile origin B by 512.
struct RemasterDungeonCompartment {
 bool valid=false;
 int left=0,top=0,right=512,bottom=512;
 bool containsTile(int x,int y) const {
  // Keep a complete tile margin on both sides of a split. A fixture touching
  // the split must not be partially extinguished. No authored grouping edits.
  return !valid || (x+8>=left && x<=right && y+8>=top && y<=bottom);
 }
};
inline RemasterDungeonCompartment S9xRemasterDungeonCompartment(const RemasterDungeonLayoutEvidence &e)
{
 RemasterDungeonCompartment r;
 static constexpr uint8_t flags[32]={15,15,15,15,11,11,7,7,15,11,15,7,11,15,7,15,14,13,14,13,15,15,14,13,14,13,15,15,10,9,6,5};
 const auto &q=e.quadrant;
 if(!e.available || e.loadedLayout>=32 || (e.loadedLayout&3) || q[3]>1 || (q[4]!=0 && q[4]!=2) ||
    q[2]!=(e.loadedLayout|q[3]|q[4]))return r;
 for(int axis=0;axis<2;axis++) {
  const int quadrant=axis?q[4]/2:q[3];
  int full=(e.blastwall[axis] || !(flags[q[2]] & (axis?(quadrant?8:4):(quadrant?2:1))))?2:0;
  const int overrideValue=(e.overrides>>(axis*8))&255;
  if(overrideValue)full=overrideValue;
  if((full!=0 && full!=2) || q[axis]!=full)return r;
  const int at=axis?0:4;const auto *b=e.bounds.data()+at;
  // B0/B1 span the entire camera range; A0/A1 select the quadrant's
  // stationary camera position. Y includes the verified 16px lower bound.
  const int tail=axis?16:0;
  if((b[1]&511)!=0 || uint16_t(b[1]+256+tail)!=b[3] ||
     uint16_t(b[1]+quadrant*256)!=b[0] || uint16_t(b[0]+tail)!=b[2])return r;
  // Dungeon_HandleCamera selects A0/A1 for half-size, B0/B1 for
  // full-size and stops exactly at those endpoints (dungeon.c:8118/8144).
  // copy2 is the RAW scroll: the renderer's +1 scanline bias is not part of
  // these bounds. No tolerance for unobserved camera/transition states.
  const uint16_t first=b[full?1:0],last=b[full?3:2];
  if(uint16_t(e.scroll[axis]-first)>uint16_t(last-first))return r;
  const int start=full?0:quadrant*256,end=full?512:start+256;
  if(axis){r.top=start;r.bottom=end;}else{r.left=start;r.right=end;}
 }
 r.valid=true;return r;
}
inline RemasterDungeonCompartment S9xRemasterDungeonCompartment(
 const RemasterDungeonLayoutEvidence &e,uint16_t renderedX,uint16_t renderedY)
{
 // Module07 writes copy = copy2 + shake offsets, then NMI uploads copy
 // (dungeon.c:6549-50, nmi.c:84-87). gfx.cpp uses PPU HOffset and VOffset+1.
 // Only the unshifted, synchronized case proves membership. Shake, raster
 // effects or a next-frame WRAM camera differing from displayed PPU fail open.
 // Compare modulo the complete 512px map; PPU high bits are not world origins.
 if((renderedX&511)!=(e.scroll[0]&511) ||
    (renderedY&511)!=((e.scroll[1]+1)&511))return {};
 return S9xRemasterDungeonCompartment(e);
}
inline RemasterDungeonLayoutEvidence S9xRemasterExtractDungeonLayoutEvidence(
 const uint8_t *ram,size_t size,bool verifiedUsaRom,bool indoors)
{
 RemasterDungeonLayoutEvidence e;
 if(!ram || size<0x618 || !verifiedUsaRom || !indoors)return e;
 auto word=[&](size_t at){return uint16_t(ram[at]|uint16_t(ram[at+1])<<8);};
 e.available=true;
 for(size_t k=0;k<5;k++)e.quadrant[k]=ram[0xa6+k];
 e.loadedLayout=word(0x40e);e.blastwall={{ram[0x452],ram[0x453]}};e.overrides=word(0xfc);
 for(size_t k=0;k<8;k++)e.bounds[k]=word(0x600+2*k);
 e.scroll={{word(0xe2),word(0xe8)}};
 for(size_t k=0;k<4;k++)e.targets[k]=word(0x610+2*k);
 e.transitionCounter=ram[0x126];
 return e;
}
inline void S9xRemasterWriteDungeonLayoutEvidenceJson(std::ostream &out,const RemasterDungeonLayoutEvidence &e)
{
 out<<"{\"available\":"<<(e.available?"true":"false");
 if(e.available) {
  auto array=[&](const char *name,const auto &values,size_t first,size_t count) {
   out<<",\""<<name<<"\":[";
   for(size_t k=0;k<count;k++){if(k)out<<',';out<<unsigned(values[first+k]);}out<<']';
  };
  array("quadrant_a6_aa",e.quadrant,0,5);
  out<<",\"loaded_layout_word\":"<<e.loadedLayout;
  array("blastwall_x_y",e.blastwall,0,2);
  out<<",\"override_word_fc\":"<<e.overrides;
  array("bounds_y_a0_b0_a1_b1",e.bounds,0,4);array("bounds_x_a0_b0_a1_b1",e.bounds,4,4);
  array("scroll_copy2_x_y",e.scroll,0,2);array("targets_ud_ud_end_lr_lr_end",e.targets,0,4);
  out<<",\"transition_counter\":"<<unsigned(e.transitionCounter);
 }
 out<<'}';
}
#endif
