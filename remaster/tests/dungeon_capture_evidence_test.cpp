// Running capture evidence must belong to retained FIRST packet, not final inputs.
#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main(){
 auto maps=std::make_unique<std::array<uint8_t,16384>>();maps->fill(11);
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(12);
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();graphics->fill(13);
 auto templates=std::make_unique<std::array<uint8_t,12876>>();templates->fill(14);
 auto map=std::make_unique<RemasterDungeonHeightMap>();map->roomIndex=0x60;map->offsets[123]=26;
 RemasterProfile profile;profile.upperFloorHeight=50;
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=0x60;c.linkY=3219;c.backgroundScrollY=3112;
 c.constructedMaps=maps->data();c.collisionAttributes=attr->data();c.graphics=graphics->data();c.dungeonTemplateData=templates->data();
 auto pinned=S9xRemasterPinDungeonCaptureEvidence(&c,profile,map.get(),17,19,true);
 maps->fill(21);attr->fill(22);graphics->fill(23);templates->fill(24);map->offsets[123]=0;
 c.roomIndex=0x61;c.linkY=3449;c.backgroundScrollY=3342;profile.upperFloorHeight=60;
 assert(pinned->context.roomIndex==0x60 && pinned->context.linkY==3219 && pinned->context.backgroundScrollY==3112);
 assert(pinned->context.constructedMaps[16383]==11 && pinned->context.collisionAttributes[8191]==12);
 assert(pinned->context.graphics[65535]==13 && pinned->context.dungeonTemplateData[12875]==14);
 assert(pinned->map->offsets[123]==26 && pinned->profile.upperFloorHeight==50 && pinned->key==17 && pinned->epoch==19 && pinned->publishable);
 c.indoors=false;assert(!S9xRemasterPinDungeonCaptureEvidence(&c,profile,map.get(),0,0,false));
 std::cout<<"PASS immutable first-packet dungeon evidence across room/camera/profile/map mutations\n";
}
