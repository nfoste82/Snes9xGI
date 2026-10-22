// Production EndFrame -> producer-owned paused handoff -> editor refresh.
#include "sprite_obstacle_replay.h"
#include "lighting_field_cache.h"
#include "obstacle_group_editor.h"
#include <cassert>
#include <iostream>
#include <unistd.h>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=0;

int main()
{
	auto maps=std::make_unique<std::array<uint8_t,16384>>();
	auto graphics=std::make_unique<std::array<uint8_t,65536>>();
	auto attrs=std::make_unique<std::array<uint8_t,8192>>();
	auto templates=std::make_unique<std::array<uint8_t,12876>>();
	uint8_t indices[64]={}; const RemasterTileContentId floor{S9xRemasterHashTile(4,indices),1,4}, obj{123,1,4};
	RemasterProfile profile; profile.gameTitle="paused regression";profile.upperFloorHeight=50;
	profile.romSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
	profile.assets[floor].tileId=floor; profile.assets[floor].hasHeight=true;
	profile.assetGroups["dungeon_floor"].tileIds={floor};
	auto &asset=profile.assets[obj]; asset.tileId=obj; asset.hasHeight=asset.hasNormals=true;
	for(int p=0;p<64;p++){asset.height[p]=int16_t(p/8+17);asset.normalXyz[p*3]=128;asset.normalXyz[p*3+1]=128;asset.normalXyz[p*3+2]=255;}
	RemasterObstacleGroup group; group.name="two_cells"; group.spriteAssembly=true;
	group.members={{obj,0,0,1,false,false,true},{obj,1,0,-1,false,false,true}};
	profile.obstacleGroups={group};
	RemasterDungeonFloorContext context; context.verifiedAlttpRom=context.indoors=context.groundedEvidence=true;
	context.roomIndex=0x51;context.module=7;context.linkX=context.linkY=80;context.constructedMaps=maps->data();
	std::fill(attrs->begin()+4096,attrs->end(),1); // Only one observed gameplay plane.
	context.collisionAttributes=attrs->data();context.graphics=graphics->data();context.dungeonTemplateData=templates->data();
	context.hasTerrainActors=context.hasObstacleActors=true;
	RemasterAreaContext area;area.id=0x51;
	S9xRemasterSetProfile(profile);S9xRemasterSetLiveFramesEnabled(true);
	auto &state=S9xRemasterState(); uint16_t screen[1024]={};
	auto endFrame=[&] {
		S9xRemasterBeginFrame(1024,32,32,32);
		RemasterFrameTileInstance instance; instance.tileId=obj; instance.source=RemasterSourceType::Object;
		instance.sourceIndex=20; auto second=instance;second.sourceIndex=21;state.tileInstances={instance,second};
		RemasterFrameTileInstance bg;bg.source=RemasterSourceType::Background;bg.sourceIndex=1;bg.tileId=floor;
		state.tileInstances.push_back(bg);state.subInstanceIds[0]=3;state.subTilePixels[0]=0;
		for(int y=8;y<16;y++)for(int x=8;x<24;x++){int p=y*32+x;state.mainOwners[p]=0x01000000;state.mainInstanceIds[p]=1+(x-8)/8;state.mainTilePixels[p]=(y-8)*8+x%8;}
		S9xRemasterEndFrame(screen,32,32,32,nullptr,&context,nullptr,&area);
	};
	endFrame();const auto *live=S9xRemasterCompletedFrame();
	assert(live && live->spriteObstacleSupport);
	for(size_t i=0;i<2;i++)assert(live->tileInstances[i].geometryStatus==RemasterOccurrenceGeometryStatus::Resolved);
	RemasterLightingFieldCache before;before.update(*live,false);
	assert(before.height[(8*32+8)*2+1]);
	// Same copy used by the native dispatch handoff. No mutable producer pointers.
	auto paused=std::make_shared<RemasterFrame>(*live);
	assert(paused->spriteObstacleSupport==live->spriteObstacleSupport);
	std::vector<uint8_t> bytes;assert(S9xSerializeRemasterFrame(*live,bytes));
	RemasterFrame reopened;assert(S9xDeserializeRemasterFrame(bytes,reopened));assert(!reopened.spriteObstacleSupport);
	S9xRemasterApplyProfileToFrame(profile,reopened);S9xRemasterApplySpriteObstacles(reopened,profile,nullptr,nullptr);
	assert(S9xRemasterInstanceGeometryUnresolved(reopened.tileInstances[0])); // Original failure reproduced.
	assert(state.completedDungeonEvidence);
	const auto &e=*state.completedDungeonEvidence;
	assert(e.context.constructedMaps!=context.constructedMaps && !e.context.retainedOutgoingMaps);
	char directory[]="/private/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/paused-obstacle-XXXXXX";
	assert(mkdtemp(directory));const std::string path=std::string(directory)+"/frame.s9xrmf";
	assert(S9xWriteRemasterFrame(*paused,path));
	S9xRemasterWriteDungeonCaptureEvidence(path,e.context,*state.completedEvidenceProfile,nullptr,e.key,e.epoch,e.publishable);
	assert(S9xRemasterRestoreSpriteObstacleSupport(path,reopened,profile));
	S9xRemasterApplyProfileToFrame(profile,reopened);S9xRemasterApplySpriteObstacles(reopened,profile,nullptr,nullptr);
	RemasterLightingFieldCache fromDisk;fromDisk.update(reopened,false);assert(before.height==fromDisk.height);
	for(const char *suffix:{"",".profile.toml",".dungeon.json",".dungeon-input.bin",".dungeon-templates.bin",".terrain-input.bin",".sprite-input.bin"})std::remove((path+suffix).c_str());
	rmdir(directory);
	context.roomIndex=0x61;attrs->fill(1); // New live state cannot contaminate paused support.
	S9xRemasterApplyProfileToFrame(profile,*paused);S9xRemasterApplySpriteObstacles(*paused,profile,nullptr,nullptr);
	RemasterLightingFieldCache after;after.update(*paused,false);
	assert(before.height==after.height && before.surface==after.surface && before.authoredNormals==after.authoredNormals);
	for(int iteration=0;iteration<3;iteration++) {
		auto edited=profile;std::string error;
		assert(S9xRemasterObstacleEditorSetMemberZ(edited.obstacleGroups[0],{0},-3,"two_cells",error));
		S9xRemasterApplyProfileToFrame(edited,*paused);S9xRemasterApplySpriteObstacles(*paused,edited,nullptr,nullptr);
		assert(paused->tileInstances[0].heightOffset==-24 && paused->tileInstances[1].heightOffset==-8);
		S9xRemasterApplyProfileToFrame(profile,*paused);S9xRemasterApplySpriteObstacles(*paused,profile,nullptr,nullptr);
		after.update(*paused,false);assert(before.height==after.height);
	}
	std::cout<<"PASS production EndFrame -> paused packet -> Sync: 128 effective-height/normal pixels unchanged; stripped-support failure reproduced; exact bulk Z refresh/undo and newer-state isolation\n";
}
