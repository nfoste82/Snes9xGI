// Format and actual production Begin/EndFrame recording integration.
#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <chrono>

uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=0;
using namespace RemasterRecording;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';std::abort();} } while(0)

static RemasterProfile Profile() {
	RemasterProfile p;p.gameTitle="Recording regression";p.romSha256=std::string(64,'a');
	p.schemaVersion=30;
	return p;
}
static RemasterFrame Frame() {
	RemasterFrame f;f.width=f.height=8;f.profileRomSha256=std::string(64,'a');
	f.originalRgb555.resize(64,42);f.mainPixels.resize(64);f.subPixels.resize(64);
	RemasterTileContentId tile{123,1,4};RemasterFrameAsset a;a.tileId=tile;a.indices[0]=7;f.assets.push_back(a);
	RemasterFrameAssetMetadata m;m.tileId=tile;m.hasHeight=true;m.height.fill(-7);f.assetMetadata.push_back(m);
	RemasterFrameTileInstance i;i.tileId=tile;i.source=RemasterSourceType::Background;i.heightOffset=-13;i.hFlip=true;
	i.geometryStatus=RemasterOccurrenceGeometryStatus::Conflict;f.tileInstances.push_back(i);
	for(auto &px:f.mainPixels){px.instanceId=1;px.tilePixel=0;px.owner=0x01000000;}
	f.mainRoomDiagnostics.resize(64);f.mainRoomDiagnostics[0].visiblePlane=1;
	auto &d=f.mainRoomDiagnostics[0].planes[1];d.room=17;d.plane=1;d.floorBase=-34;d.floorId=9;d.wallSource=RemasterRoomDiagnostic::WallSource::RomAssembly;
	f.area.id=17;return f;
}
static Bytes ReadAll(const std::string &path) {std::ifstream f(path,std::ios::binary);return Bytes(std::istreambuf_iterator<char>(f),{});}
static void Put(const std::string &path,const Bytes &b){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char *>(b.data()),b.size());}

int main() {
	const std::string root="build/frame-recording-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	CHECK(std::filesystem::create_directory(root));
	const auto profile=std::make_shared<const RemasterProfile>(Profile());
	const auto frame=Frame();
	std::string error;
	for(size_t n:{size_t(0),size_t(1),size_t(127),size_t(128),size_t(129),Chunk}) {
		Bytes data(n),unpacked;uint32_t state=12345;
		for(auto &v:data){state=state*1664525+1013904223;v=uint8_t(state>>24);}
		CHECK(Unpack(Pack(data),data.size(),unpacked)&&unpacked==data);
		data.assign(n,42);CHECK(Unpack(Pack(data),data.size(),unpacked)&&unpacked==data);
	}
	{
		Writer w;CHECK(w.Open(root+"/empty.s9xrmr"));CHECK(w.Finish());ReaderFile r;CHECK(r.Open(root+"/empty.s9xrmr",error));CHECK(r.complete&&r.entries.empty());
	}
	Packet packet;packet.frame=frame;packet.profile=profile;packet.evidence={1,2,3,4};packet.session=44;packet.epoch=99;packet.emulatedFrame=100;
	{
		Writer w;CHECK(w.Open(root+"/one.s9xrmr"));CHECK(w.Append(packet));CHECK(w.Finish());
		ReaderFile r;CHECK(r.Open(root+"/one.s9xrmr",error));CHECK(r.complete&&r.entries.size()==1);
		RemasterFrame loaded;Bytes evidence;std::string text;CHECK(r.Read(0,loaded,evidence,text,error));
		CHECK(evidence==packet.evidence);CHECK(loaded.originalRgb555==frame.originalRgb555);CHECK(loaded.tileInstances[0].heightOffset==-13&&loaded.tileInstances[0].hFlip);
		CHECK(loaded.tileInstances[0].geometryStatus==RemasterOccurrenceGeometryStatus::Conflict);
		CHECK(loaded.mainRoomDiagnostics[0].planes[1].floorBase==-34);CHECK(loaded.mainRoomDiagnostics[0].planes[1].wallSource==RemasterRoomDiagnostic::WallSource::RomAssembly);
		CHECK(r.entries[0].session==44&&r.entries[0].epoch==99&&r.entries[0].emulatedFrame==100);
		RemasterProfile edited=Profile();edited.assets[frame.assets[0].tileId].tileId=frame.assets[0].tileId;edited.assets[frame.assets[0].tileId].hasHeight=true;edited.assets[frame.assets[0].tileId].height.fill(80);
		S9xRemasterApplyProfileToFrame(edited,loaded);
		CHECK(frame.assetMetadata[0].height[0]==-7);
		CHECK(r.Read(0,loaded,evidence,text,error));CHECK(loaded.assetMetadata[0].height[0]==-7);
		Bytes ordinary;CHECK(S9xSerializeRemasterFrame(loaded,ordinary));RemasterFrame compatible;CHECK(S9xDeserializeRemasterFrame(ordinary,compatible));CHECK(compatible.tileInstances[0].heightOffset==-13);
	}
	{
		Writer w;CHECK(w.Open(root+"/multi.s9xrmr"));
		for(unsigned i=0;i<20;i++){packet.sequence=i;packet.emulatedFrame=100+i;packet.frame.area.id=uint16_t(i%2?18:17);CHECK(w.Append(packet));}
		CHECK(w.Finish());ReaderFile r;CHECK(r.Open(root+"/multi.s9xrmr",error));CHECK(r.entries.size()==20);
		RemasterFrame f;Bytes e;std::string p;CHECK(r.Read(19,f,e,p,error));CHECK(f.area.id==18);
		CHECK(std::filesystem::file_size(root+"/multi.s9xrmr")<std::filesystem::file_size(root+"/one.s9xrmr")*4);
	}
	{
		Writer w;CHECK(w.Open(root+"/revisions.s9xrmr"));packet.sequence=0;packet.frame=frame;CHECK(w.Append(packet));
		auto changed=Profile();changed.areaSceneSettings["room_011"]={0.75f,3};changed.areaSceneSettings["room_012"]={0.2f,7};
		changed.areaLights["room_012"].brightness=2;
		packet.profile=std::make_shared<const RemasterProfile>(changed);packet.revisionOnly=true;CHECK(w.Append(packet));
		packet.revisionOnly=false;packet.sequence=1;packet.frame.area.id=18;CHECK(w.Append(packet));CHECK(w.Finish());
		ReaderFile r;CHECK(r.Open(root+"/revisions.s9xrmr",error));CHECK(r.entries.size()==2&&r.revisionEvents==1);CHECK(r.entries[0].profile!=r.entries[1].profile);
		RemasterFrame f;Bytes e;std::string text;CHECK(r.Read(1,f,e,text,error));RemasterProfile p;std::vector<RemasterProfileDiagnostic> diagnostics;
		CHECK(S9xRemasterParseProfile(text,p,diagnostics));CHECK(p.areaSceneSettings.at("room_011").representativeEmitters==3);CHECK(p.areaSceneSettings.at("room_012").originalSceneContribution==0.2f);CHECK(p.areaLights.at("room_012").brightness==2);
	}
	// Recovery prefix and strict corrupt/oversized headers.
	{
		auto bytes=ReadAll(root+"/one.s9xrmr");bytes.resize(bytes.size()-3);Put(root+"/truncated.s9xrmr",bytes);
		ReaderFile r;CHECK(r.Open(root+"/truncated.s9xrmr",error));CHECK(!r.complete&&r.entries.size()==1&&!r.warning.empty());
		bytes=ReadAll(root+"/one.s9xrmr");bytes[9]=bytes[10]=bytes[11]=bytes[12]=255;Put(root+"/huge.s9xrmr",bytes);
		ReaderFile huge;CHECK(!huge.Open(root+"/huge.s9xrmr",error));
		bytes=ReadAll(root+"/one.s9xrmr");bytes[0]='X';Put(root+"/magic.s9xrmr",bytes);ReaderFile magic;CHECK(!magic.Open(root+"/magic.s9xrmr",error));
		bytes=ReadAll(root+"/one.s9xrmr");bytes[30]^=1;Put(root+"/corrupt.s9xrmr",bytes);ReaderFile corrupt;CHECK(!corrupt.Open(root+"/corrupt.s9xrmr",error));
		Bytes expanded;CHECK(!Unpack({255,1},1,expanded));CHECK(!Unpack({127},128,expanded));CHECK(!Unpack({},Chunk+1,expanded));
	}
	// Actual production integration: no live frame request, nonzero transition,
	// null sprite support; mutate RAM-like input/profile/session immediately after.
	{
		auto &rec=Global();CHECK(rec.Start(root+"/production.s9xrmr"));
		S9xRemasterSetProfile(Profile());S9xRemasterSetLiveFramesEnabled(false);
		auto context=std::make_unique<RemasterDungeonFloorContext>();
		context->verifiedAlttpRom=true;context->indoors=true;context->roomIndex=18;context->previousRoomIndex=17;context->module=7;context->submodule=2;context->backgroundScrollX=123;context->linkPlane=1;
		std::vector<uint8_t> maps(16384,42),attributes(8192,17),graphics(65536,9);
		context->constructedMaps=maps.data();context->collisionAttributes=attributes.data();context->graphics=graphics.data();
		uint16_t screen[64]{};RemasterAreaContext area;area.id=18;std::array<int,4> sx{123,124,125,126},sy{};
		auto produce=[&](bool gameplay,uint32_t number){rec.SetGameplay(gameplay);S9xRemasterBeginFrame(64,8,8,8);S9xRemasterEndFrame(screen,8,8,8,nullptr,context.get(),nullptr,&area,number,nullptr,&sx,&sy);};
		produce(false,1);CHECK(!S9xRemasterCompletedFrame()); // paused preview not recording
		produce(true,2);CHECK(S9xRemasterCompletedFrame());CHECK(!S9xRemasterCompletedFrame()->spriteObstacleSupport);
		maps.assign(maps.size(),77);context->roomIndex=19;area.id=19;S9xRemasterState().dungeonSession=555;
		produce(false,3);produce(true,4);rec.SetGameplay(false);rec.Stop();CHECK(rec.Status().find("ERROR")==std::string::npos);
		ReaderFile r;CHECK(r.Open(root+"/production.s9xrmr",error));CHECK(r.complete&&r.entries.size()==2);CHECK(r.entries[0].emulatedFrame==2&&r.entries[1].emulatedFrame==4);CHECK(r.entries[1].session==555);
		RemasterFrame f;Bytes e;std::string text;CHECK(r.Read(0,f,e,text,error));CHECK(f.area.id==18);CHECK(e.size()>90000);CHECK(std::string(e.begin(),e.begin()+4)=="RTE1");
		Reader exact(e);exact.offset=4;uint8_t present,publishable;uint64_t key,session;uint16_t room;
		CHECK(exact.ReadU8(present)&&present&&exact.ReadU8(publishable)&&exact.ReadU64(key)&&exact.ReadU64(session)&&exact.ReadU16(room));
		for(int axis=0;axis<2;axis++){uint8_t has;CHECK(exact.ReadU8(has)&&has);for(int bg=0;bg<4;bg++){uint32_t value;CHECK(exact.ReadU32(value));if(!axis)CHECK(value==uint32_t(123+bg));}}
		uint64_t rom;CHECK(exact.ReadU64(rom));uint16_t contextWords[14];for(auto &word:contextWords)CHECK(exact.ReadU16(word));CHECK(contextWords[0]==18&&contextWords[1]==17&&contextWords[2]==123);
		exact.offset+=15+6+34;uint8_t stairs;CHECK(exact.ReadU8(stairs));exact.offset+=size_t(stairs)*4;uint32_t mapSize;CHECK(exact.ReadU32(mapSize)&&mapSize==16384);CHECK(e[exact.offset]==42); // not later mutated77
		CHECK(r.Read(1,f,e,text,error));CHECK(f.area.id==19);
	}
	// Paused saves/area-open events retain revisions but append zero fake frames.
	{
		Recorder rec;CHECK(rec.Start(root+"/paused-revision.s9xrmr"));rec.SetGameplay(false);rec.Revision(Profile(),{18,true});rec.Stop();
		ReaderFile r;CHECK(r.Open(root+"/paused-revision.s9xrmr",error));CHECK(r.complete&&r.entries.empty()&&r.revisionEvents==1);
	}
	{
		Recorder rec;CHECK(rec.Start(root+"/bounded.s9xrmr"));rec.SetGameplay(true);
		Packet huge;huge.profile=profile;huge.frame=frame;huge.evidence.resize(MaxBlob+1);
		CHECK(!rec.Submit(std::move(huge)));rec.Stop();CHECK(rec.Status().find("ERROR")!=std::string::npos);
		CHECK(!rec.Start(root+"/bounded.s9xrmr")); // no overwrite
	}
	std::cout<<"PASS frame recording: "<<checks<<" checks; artifacts "<<root<<'\n';
}
