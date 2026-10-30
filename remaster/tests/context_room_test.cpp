#include "../runtime/context_editor.h"
#include "../runtime/remaster.h"
#include <iostream>
#include <chrono>
#include <cstdlib>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=0;
static void check(bool b,const char *s){if(!b){std::cerr<<"FAIL "<<s<<'\n';std::exit(1);}}
static std::string text(const RemasterProfile &p){std::string s;std::vector<RemasterProfileDiagnostic>d;if(!S9xRemasterSerializeProfile(p,s,d)){for(auto &e:d)std::cerr<<e.message<<'\n';check(false,"serialize");}return s;}
int main() {
 auto c=std::make_shared<RemasterContextRoomCapture>();c->room=8;c->settled=c->supported=true;c->session=4;c->generation=1;
 c->packet.area.id=8;c->packet.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 c->bases={65520,8192};
 // Distinct BG bases, 16-bit wrap, canonical index1 pattern with two planes.
 for(unsigned y=0;y<8;++y){c->graphics[uint16_t(65520+32+y*2)]=0x80;c->graphics[uint16_t(65520+32+y*2+1)]=0x01;}
 for(unsigned i=0;i<4096;++i){unsigned w=1|((i%4)<<14);c->maps[8192+i*2]=w&255;c->maps[8193+i*2]=w>>8;}
 uint8_t pixels[64]={};for(unsigned y=0;y<8;++y){pixels[y*8]=1;pixels[y*8+7]=2;}
 const RemasterTileContentId tile{S9xRemasterHashTile(4,pixels),1,4};
 RemasterFrameTileInstance in;in.source=RemasterSourceType::Background;in.sourceIndex=0;in.tileId=tile;
 c->packet.tileInstances.push_back(in);RemasterContextLocation at;at.area=8;at.background=0;at.x=0;at.y=0;c->packet.contextLocations[1]=at;
 RemasterContextRoom room;check(room.admit(c),"complete 8192-cell input admitted");check(room.cells.size()==8192,"complete denominator");
 for(unsigned i=0;i<4096;++i)check(room.cells[i].tile==tile && room.cells[i].hFlip==bool(i&1) && room.cells[i].vFlip==bool(i&2),"independent decode/wrap/all flips");
 check(!(room.cells[4096].tile==tile),"BG base independent");
 RemasterProfile p;p.gameTitle="Room context test";p.romSha256=c->packet.profileRomSha256;p.assets[tile].tileId=tile;
 p.tileContexts[tile]["Special"].hasHeight=true;p.tileContexts[tile]["bulkOOB"].hasNormals=true;
 p.contextBindings[at]={tile,"Special"};
 auto other=at;other.background=1;p.contextBindings[other]={tile,"Special"};auto otherRoom=at;otherRoom.area=9;p.contextBindings[otherRoom]={tile,"Special"};
 RemasterContextEditor e;e.roomScope=true;e.bulk=true;e.typeBackgrounds[tile]=0;e.name="bulkOOB";e.exclude.insert("Special");
 e.prepareRoom(p,c->packet,room);check(e.instances.empty() && e.counts.total==4096 && e.counts.included==4096 && e.counts.excluded==1 && e.counts.eligible==4095,"genuinely offscreen-only target batch");
 auto staleFrame=c->packet;staleFrame.tileInstances[0].hFlip=true;check(!e.canAssign(p,staleFrame),"same-room changed frame veto");
 e.exclude.clear();check(!e.canAssign(p,c->packet),"modal filter mutation veto");e.exclude={"Special"};
 p.tileContexts[tile]["bulkOOB"].hasHeight=true;check(!e.canAssign(p,c->packet),"modal profile mutation veto");p.tileContexts[tile]["bulkOOB"].hasHeight=false;
 const auto before=text(p);check(e.assign(p,c->packet),"offscreen atomic assignment");check(p.contextBindings.size()==4098 && p.contextBindings.at(at).name=="Special" && p.contextBindings.at(other).name=="Special" && p.contextBindings.at(otherRoom).name=="Special","special/BG/room preserved");
 auto off=at;off.x=63;off.y=63;check(p.contextBindings.at(off).name=="bulkOOB","offscreen saved binding");
 RemasterProfile parsed;std::vector<RemasterProfileDiagnostic>d;check(S9xRemasterParseProfile(text(p),parsed,d),"roundtrip");
 auto f=c->packet;f.contextLocations[1]=off;S9xRemasterApplyContextBindings(parsed,f);check(f.contextGeometry.count(1) && f.contextGeometry.at(1).hasNormals,"saved offscreen activation");
 const auto after=text(p);check(S9xRemasterParseProfile(before,p,d),"undo");check(p.contextBindings.size()==3 && p.contextBindings.at(at).name=="Special","undo bindings");check(S9xRemasterParseProfile(after,p,d),"redo");check(p.contextBindings.size()==4098 && p.contextBindings.at(off).name=="bulkOOB","redo bindings");
 e.prepareRoom(p,c->packet,room);check(e.counts.replacements==0,"actual no-op count");
 e.name="Absent";check(!e.canAssign(p,c->packet),"destination change needs fresh counts");e.prepareRoom(p,c->packet,room);check(e.counts.blocked==4095 && !e.assign(p,c->packet),"missing destination blocks complete edit");e.name="bulkOOB";
 e.include={"Special","bulkOOB"};e.exclude={"Special","bulkOOB"};e.prepareRoom(p,c->packet,room);check(e.counts.included==4096 && e.counts.excluded==4096 && !e.canAssign(p,c->packet),"union exclusion wins");
 e.include={""};e.exclude.clear();e.prepareRoom(p,c->packet,room);check(e.counts.eligible==0 && e.roomStatus.empty(),"available zero distinct from unavailable");
 check(S9xRemasterParseProfile(before,p,d),"restore base");e.include={""};e.prepareRoom(p,c->packet,room);check(e.counts.eligible==4095,"default effective class");
 p.tileContexts[tile].erase("Special");p.contextBindings.erase(at);e.include={"bulkOOB"};e.prepareRoom(p,c->packet,room);check(e.counts.eligible==4096,"sole-auto named not default");
 p.contextBindings[off]={tile,"bulkOOB"};e.name="";e.prepareRoom(p,c->packet,room);check(e.counts.clears==1 && e.assign(p,c->packet) && !p.contextBindings.count(off),"None clears sole explicit only");
 p.contextBindings[off]={tile,"Missing"};e.exclude={"bulkOOB"};e.prepareRoom(p,c->packet,room);check(e.counts.blocked==1 && !e.assign(p,c->packet) && p.contextBindings.at(off).name=="Missing","invalid stored name blocked even excluded");
 p.contextBindings[off].tile={42,1,4};e.prepareRoom(p,c->packet,room);check(e.counts.blocked==1 && !e.assign(p,c->packet),"wrong owner blocked");
 RemasterContextRoom rejected;check(!rejected.admit(nullptr),"unpaired same-room sidecars never input");
 auto mutated=std::make_shared<RemasterContextRoomCapture>(*c);mutated->packet.tileInstances[0].hFlip=true;check(!rejected.admit(mutated),"published flip mismatch");
 mutated=std::make_shared<RemasterContextRoomCapture>(*c);mutated->packet.contextLocations[2]=at;check(!rejected.admit(mutated),"invalid duplicate owner");
 mutated=std::make_shared<RemasterContextRoomCapture>(*c);mutated->packet.contextLocations[1].x=64;check(!rejected.admit(mutated),"invalid coordinates");
 mutated=std::make_shared<RemasterContextRoomCapture>(*c);mutated->supported=false;check(!rejected.admit(mutated),"unsupported format");
 mutated=std::make_shared<RemasterContextRoomCapture>(*c);mutated->settled=false;check(!rejected.admit(mutated),"transition");
 mutated=std::make_shared<RemasterContextRoomCapture>(*c);mutated->packet.area.id=9;check(!rejected.admit(mutated),"wrong room");
 mutated=std::make_shared<RemasterContextRoomCapture>(*c);mutated->packet.profileRomSha256=std::string(64,'0');check(!rejected.admit(mutated),"wrong ROM");
 check(S9xRemasterParseProfile(before,p,d),"multi-type base");
 const auto second=room.cells[4096].tile;p.assets[second].tileId=second;
 p.tileContexts[second]["Special"].hasHeight=true;p.tileContexts[second]["bulkOOB"].hasNormals=true;
 p.contextBindings.erase(other);e.typeBackgrounds[second]=1;e.include.clear();e.exclude={"Special"};e.name="bulkOOB";
 e.prepareRoom(p,c->packet,room);check(e.counts.total==8192 && e.counts.eligible==8191 && e.assign(p,c->packet),"multiple selected IDs retain distinct clicked BGs");
 p.tileContexts[second].erase("bulkOOB");e.prepareRoom(p,c->packet,room);check(e.counts.blocked>0 && !e.assign(p,c->packet),"one missing destination atomically vetoes all selected types");
 const auto start=std::chrono::steady_clock::now();for(int i=0;i<100;++i){check(room.admit(c),"repeat decode");}
 std::cout<<"PASS complete room/filter/serialization activation. decode_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/100<<" cells=8192 matched_BG1=4096 special=1 offscreen_eligible=4095\n";
 // Actual production EndFrame request -> immutable packet/maps -> same UI model.
 RemasterProfile liveProfile;liveProfile.gameTitle="Live capture test";liveProfile.romSha256=c->packet.profileRomSha256;
 S9xRemasterSetProfile(liveProfile);S9xRemasterSetLiveFramesEnabled(true);
 RemasterDungeonFloorContext dc;dc.verifiedAlttpRom=dc.indoors=true;dc.roomIndex=8;dc.module=7;
 dc.constructedMaps=c->maps.data();dc.graphics=c->graphics.data();dc.bg1GraphicsBase=c->bases[0];dc.graphicsBase=c->bases[1];
 auto &state=S9xRemasterState();RemasterAreaContext area;area.id=8;uint16_t screen[64]={};
 auto produce=[&](bool request){state.requestedContextRoom.store(request);S9xRemasterBeginFrame(64,8,8,8);state.contextRoomFormatSupported=true;S9xRemasterEndFrame(screen,8,8,8,nullptr,&dc,nullptr,&area);};
 produce(false);check(!state.completedContextRoom,"normal frame no room snapshot allocation");
 produce(true);check(bool(state.completedContextRoom),"actual producer publishes requested packet and maps");
 auto handoff=state.completedContextRoom;RemasterContextRoom consumer;check(consumer.admit(handoff),"producer snapshot admitted by UI consumer");
 const auto saved=handoff->maps[8192];c->maps[8192]^=1;check(handoff->maps[8192]==saved,"immutable async handoff not mutable RAM pointer");
 produce(false);check(!state.completedContextRoom && handoff->maps[8192]==saved,"advance invalidates producer snapshot, retained value immutable");
}
