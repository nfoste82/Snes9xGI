#ifndef REMASTER_DUNGEON_PRODUCER_WRITES_H
#define REMASTER_DUNGEON_PRODUCER_WRITES_H
#define REMASTER_LIVE_PRODUCER_CONTRACT 1
#define REMASTER_DUNGEON_RESEARCH_CONTAINMENT 1
#include <array>
#include <cstdint>
#include <memory>

// Incomplete producer/contact model: session-only opt-in, never profile data.
inline bool &S9xRemasterDungeonResearchEnabled(){static bool enabled=false;return enabled;}

// Read-only observer of verified USA CPU stores, not a final-artwork matcher.
// PCs are POST-operand PCs, as passed by S9xSetByte/Word. ROM instruction bytes
// are checked as well as register/source/width/destination. Any other write,
// including an equal-value or partial-byte write, revokes the last writer.
struct RemasterDungeonProducerCell {
 uint64_t owner=0;
 uint16_t source=0,origin=0,word=0,dispatch=0;
 uint8_t part=0,operation=0;
 bool operator==(const RemasterDungeonProducerCell &b) const {
  return owner==b.owner && source==b.source && origin==b.origin && word==b.word && dispatch==b.dispatch && part==b.part && operation==b.operation;
 }
};
struct RemasterDungeonWriteTrace {
 bool enabled=false;
 uint16_t room=0xffff;
 uint64_t revision=0,sequence=0;
 std::array<RemasterDungeonProducerCell,8192> cells={}; // BG1 then BG2
 uint64_t currentOwner=0;
 uint16_t currentOrigin=0,currentSource=0;
 uint8_t currentOperation=0;
 void clear(){cells={};currentOwner=0;room=0xffff;++revision;}
 void enable(bool value){if(enabled!=value){clear();enabled=value;}}
 void write(const uint8_t *ram,const uint8_t *rom,uint32_t address,uint16_t value,
  int bytes,uint32_t pc,uint16_t xreg,uint16_t yreg) {
  if(!enabled || !ram || !rom)return;
  if(!ram[0x1b] || ram[0x10]!=7){if(room!=0xffff)clear();return;}
  const uint16_t observedRoom=ram[0xa0]|ram[0xa1]<<8;
  if(room!=observedRoom){clear();room=observedRoom;}
  // The caller passes a RAM-relative address after resolving actual mapping.
  if(address<0x2000 || address>=0x6000)return;
  ++revision;
  const int bg=address>=0x4000?0:1,cell=(address&0x1fff)/2;
  cells[bg*4096+cell]={};
  if(bytes!=2 || (address&1)) {
   if(bytes==2 && cell<4095)cells[bg*4096+cell+1]={};
   return;
  }
  int row=-1,operation=0;
  static const uint32_t single[]={0x0197f7,0x0197fc,0x019801,0x019806};
  static const uint32_t both[][2]={{0x019822,0x019826},{0x01982d,0x019831},{0x019838,0x01983c},{0x019843,0x019847}};
  for(int y=0;y<4;y++) {
   if(pc==single[y] && rom[(pc&0xffff)-2]==0x97){row=y;operation=1;}
   if(pc==both[y][bg] && rom[(pc&0xffff)-4]==0x9f && rom[(pc&0xffff)-1]==0x7e){row=y;operation=2;}
  }
  const int remaining=ram[0xe]|ram[0xf]<<8,column=4-remaining;
  if(row<0 || column<0 || column>3)return;
  const int source=int(operation==1?xreg:yreg)-column*8;
  const int origin=cell-column-row*64;
  if(source<0 || source+32>12876 || origin<0 || origin%64>60 || origin/64>60)return;
  const int at=0x1b52+source+2*(column*4+row);
  if((rom[at]|rom[at+1]<<8)!=value)return;
  // Every invocation starts at column0,row0 (BG1 first for the both-BG
  // handler). Repeated invocations with identical words have distinct owners.
  if(column==0 && row==0 && (operation==1 || bg==0)) {
   currentOwner=++sequence;currentOrigin=origin;currentSource=source;currentOperation=operation;
  }
  if(!currentOwner || currentOrigin!=origin || currentSource!=source || currentOperation!=operation)return;
  cells[bg*4096+cell]={currentOwner,uint16_t(source),uint16_t(origin),value,uint16_t(ram[0]|ram[1]<<8),uint8_t(column*4+row),uint8_t(operation)};
 }
};
inline std::unique_ptr<RemasterDungeonWriteTrace> &S9xRemasterDungeonWritesStorage(){static std::unique_ptr<RemasterDungeonWriteTrace> t;return t;}
inline RemasterDungeonWriteTrace &S9xRemasterDungeonWrites(){auto &t=S9xRemasterDungeonWritesStorage();if(!t)t=std::make_unique<RemasterDungeonWriteTrace>();return *t;}
#endif
