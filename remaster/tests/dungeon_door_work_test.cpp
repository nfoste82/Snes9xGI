// Deterministic failure-class regression: known-inexact open frames must never
// examine collision thresholds. No wall-clock pass threshold or authored input.
#ifndef ALTTP_TEST_DOOR_WORK
#define ALTTP_TEST_DOOR_WORK
#endif
#include "../runtime/remaster.h"
#include <iostream>
int main() {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();maps->fill(0x55);
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(0x80);
 auto templates=std::make_unique<std::array<uint8_t,12876>>();templates->fill(0xaa);
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;
 c.constructedMaps=maps->data();c.graphics=graphics->data();
 c.collisionAttributes=attrs->data();c.dungeonTemplateData=templates->data();
 RemasterProfile profile;
 S9xRemasterDungeonPlacements(c,profile,*tiles);
 const auto &w=S9xRemasterDungeonDoorWork();
 std::cout<<"origins="<<w.openOrigins<<" words="<<w.frameWords
  <<" threshold_visits="<<w.thresholdVisits<<" rejected_visits="<<w.rejectedThresholdVisits<<'\n';
 if(w.rejectedThresholdVisits || w.thresholdVisits) {
  std::cerr<<"known-inexact candidates scanned collision thresholds\n";return 1;
 }
 if(w.openOrigins) {
  std::cerr<<"empty mandatory-word buckets scanned candidate origins\n";return 1;
 }
 if(w.jambFeet) {
  std::cerr<<"ineligible jamb feet scanned templates\n";return 1;
 }
 if(w.closedOrigins) {
  std::cerr<<"empty mandatory-word buckets scanned closed origins\n";return 1;
 }
}
