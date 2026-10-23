#include "../runtime/obstacle_group_editor.h"
#include "../runtime/profile_merge.h"
#include <cassert>
#include <iostream>

static std::string serialized(const RemasterProfile &profile)
{
	std::string text; std::vector<RemasterProfileDiagnostic> errors;
	assert(S9xRemasterSerializeProfile(profile,text,errors)); return text;
}

static void canonicalEastContacts()
{
	RemasterProfile authored; std::vector<RemasterProfileDiagnostic> diagnostics;
	assert(S9xRemasterLoadProfile("remaster/profiles/ALttP/alttp-profile.toml",authored,diagnostics));
	const auto original=serialized(authored);
	auto found=std::find_if(authored.obstacleGroups.begin(),authored.obstacleGroups.end(),[](const auto &g){return g.name=="East_facing_curtains";});
	assert(found!=authored.obstacleGroups.end() && found->members.size()==8 && !found->spriteAssembly);
	assert(found->members[3].x==3 && found->members[3].y==0 && found->members[7].x==3 && found->members[7].y==1);
	assert(found->members[3].tileId==found->members[7].tileId && !found->members[3].vflip && found->members[7].vflip);
	for (int flip=0;flip<4;flip++) {
		auto group=*found;
		// Reproduce the reported starting flags in memory only, even after the
		// user later saves different flags. Never write the canonical file.
		for (auto &m:group.members) m.supportContact=false;
		group.members[7].supportContact=true;
		RemasterProfile beforeProfile;beforeProfile.schemaVersion=22;beforeProfile.gameTitle="East contact fixture";
		beforeProfile.romSha256=std::string(64,'a');beforeProfile.obstacleGroups={group};
		const auto beforeText=serialized(beforeProfile);
		RemasterFrame frame; frame.width=128;frame.height=64;
		frame.mainPixels.resize(128*64);frame.subPixels.resize(128*64);
		std::vector<RemasterObstacleEditorCell> cells;
		auto layout=S9xRemasterObstacleEditorDisplayLayout(group,flip);
		for (size_t i=0;i<layout.members.size();i++) {
			const auto &m=layout.members[i]; RemasterFrameTileInstance inst;
			inst.tileId=m.tileId;inst.source=RemasterSourceType::Background;inst.sourceIndex=1;inst.hFlip=m.hflip;inst.vFlip=m.vflip;
			frame.tileInstances.push_back(inst);
			int x=16+8*m.x,y=16+8*m.y;auto &p=frame.mainPixels[y*128+x];
			p.instanceId=i+1;p.tilePixel=(m.vflip?7:0)*8+(m.hflip?7:0);
			RemasterObstacleEditorCell c;assert(S9xRemasterObstacleEditorCellAt(frame,x,y,c));cells.push_back(c);
		}
		std::vector<size_t> members;std::string error;bool intent=false;
		assert(S9xRemasterObstacleEditorFindGroup(frame,{group},{cells[3]})==0);
		assert(S9xRemasterObstacleEditorResolveSelection(frame,group,{cells[3]},members,error));
		assert((members==std::vector<size_t>{3}));
		assert(S9xRemasterObstacleEditorContactIntent(group,members,intent,error) && intent);
		assert(S9xRemasterObstacleEditorSetContacts(group,members,intent,error));
		for(size_t i=0;i<group.members.size();i++) assert(group.members[i].supportContact==(i==3 || i==7));
		auto afterProfile=beforeProfile;afterProfile.obstacleGroups[0]=group;
		const auto afterText=serialized(afterProfile);RemasterProfile restored;
		assert(S9xRemasterParseProfile(afterText,restored,diagnostics));
		assert(restored.obstacleGroups[0].members[3].supportContact && restored.obstacleGroups[0].members[7].supportContact);
		assert(S9xRemasterParseProfile(beforeText,restored,diagnostics) && serialized(restored)==beforeText);
		assert(S9xRemasterParseProfile(afterText,restored,diagnostics) && serialized(restored)==afterText);
		assert(S9xRemasterObstacleSupportDirection(group,flip)==((flip&1)?2:3));
		assert(S9xRemasterObstacleEditorResolveSelection(frame,group,{cells[3],cells[7]},members,error));
		assert((members==std::vector<size_t>{3,7}));
		assert(S9xRemasterObstacleEditorContactIntent(group,members,intent,error) && !intent);
		assert(!S9xRemasterObstacleEditorSetContacts(group,members,intent,error));
		assert(error.find("Enable another member first")!=std::string::npos);
		assert(group.members[3].supportContact && group.members[7].supportContact);
		assert(S9xRemasterObstacleEditorSetContacts(group,{3},false,error));
		assert(S9xRemasterObstacleEditorContactIntent(group,members,intent,error) && intent); // mixed -> on
		assert(!S9xRemasterObstacleEditorContactIntent(group,{3,99},intent,error));
		assert(!S9xRemasterObstacleEditorContactIntent(group,{},intent,error));
		auto competing=group;competing.name="shared_competing";
		assert(S9xRemasterObstacleEditorFindGroup(frame,{group,competing},{cells[3]})==-2);
		// Explicit saved-member edits do not resolve by shared identity.
		assert(S9xRemasterObstacleEditorContactIntent(group,{3},intent,error) && intent);
	}
	assert(serialized(authored)==original);
}

static void contactEditing()
{
	const RemasterTileContentId repeat{123,1,4}, unique{456,1,4}, other{789,1,4};
	for (bool sprite : {false,true}) for (int flip=0;flip<4;flip++) {
		RemasterObstacleGroup group; group.name="contacts"; group.spriteAssembly=sprite;
		group.members={{repeat,-1,0,-3,true,false,false},{repeat,0,0,7,false,true,true},
			{unique,2,1,19,false,false,false},{repeat,0,2,4,false,true,true}};
		RemasterFrame frame; frame.width=frame.height=96;
		frame.mainPixels.resize(96*96); frame.subPixels.resize(96*96);
		std::vector<RemasterObstacleEditorCell> cells;
		for (size_t i=0;i<group.members.size();i++) {
			const auto &m=group.members[i];
			RemasterFrameTileInstance instance; instance.tileId=m.tileId;
			instance.source=sprite?RemasterSourceType::Object:RemasterSourceType::Background;
			instance.sourceIndex=sprite?uint8_t(120+i):1;
			instance.hFlip=m.hflip!=bool(flip&1); instance.vFlip=m.vflip!=bool(flip&2);
			frame.tileInstances.push_back(instance);
			const int x=40+((flip&1)?-m.x:m.x)*8,y=40+((flip&2)?-m.y:m.y)*8;
			auto &pixel=frame.mainPixels[y*96+x]; pixel.instanceId=uint32_t(i+1);
			pixel.tilePixel=(instance.vFlip?7:0)*8+(instance.hFlip?7:0);
			RemasterObstacleEditorCell cell;
			assert(S9xRemasterObstacleEditorCellAt(frame,x,y,cell)); cells.push_back(cell);
		}
		std::vector<size_t> selected; std::string error;
		// Different individual H/V flips and repeated identities, with sparse
		// coordinates and every whole-object mirror/translation.
		assert(S9xRemasterObstacleEditorResolveSelection(frame,group,{cells[0],cells[1]},selected,error));
		assert((selected==std::vector<size_t>{0,1}));
		RemasterProfile profile; profile.schemaVersion=30;
		profile.gameTitle="dirty fixture"; profile.romSha256=std::string(64,'a');
		profile.obstacleGroups={group};
		profile.assets[other].tileId=other;
		profile.assets[other].hasHeight=true; profile.assets[other].height[17]=-32; // Unrelated dirty layer.
		const auto before=serialized(profile);
		assert(S9xRemasterObstacleEditorSetContacts(profile.obstacleGroups[0],selected,true,error));
		const auto after=serialized(profile);
		assert(after!=before);
		for (size_t i=0;i<group.members.size();i++) {
			const auto &m=profile.obstacleGroups[0].members[i], &old=group.members[i];
			assert(m.z==old.z && m.x==old.x && m.y==old.y && m.hflip==old.hflip && m.vflip==old.vflip);
			assert(m.supportContact==(i<2?true:old.supportContact));
		}
		RemasterProfile roundtrip; std::vector<RemasterProfileDiagnostic> diagnostics;
		assert(S9xRemasterParseProfile(after,roundtrip,diagnostics));
		assert(serialized(roundtrip)==after);
		RemasterProfile baseline, disk;
		assert(S9xRemasterParseProfile(before,baseline,diagnostics)); disk=baseline;
		disk.assets[other].height[18]=42;
		RemasterProfileMergeResult merged;
		assert(S9xRemasterMergeProfiles(baseline,profile,disk,{},false,merged,diagnostics));
		assert(merged.conflicts.empty() && merged.saved.obstacleGroups[0].members[0].supportContact);
		assert(merged.saved.assets.at(other).height[17]==-32 && merged.saved.assets.at(other).height[18]==42);
		assert(roundtrip.assets.at(other).height[17]==-32);
		// The native Undo path restores this serialized pre-edit snapshot;
		// restoration/redo retain the unrelated dirty layer as well.
		assert(S9xRemasterParseProfile(before,roundtrip,diagnostics));
		assert(serialized(roundtrip)==before);
		assert(S9xRemasterParseProfile(after,roundtrip,diagnostics));
		assert(serialized(roundtrip)==after);
		assert(S9xRemasterObstacleEditorSetContacts(profile.obstacleGroups[0],selected,false,error));
		assert(!profile.obstacleGroups[0].members[0].supportContact && !profile.obstacleGroups[0].members[1].supportContact);
		const auto remaining=serialized(profile);
		assert(!S9xRemasterObstacleEditorSetContacts(profile.obstacleGroups[0],{3},false,error));
		assert(!error.empty() && serialized(profile)==remaining);
		assert(!S9xRemasterObstacleEditorSetContacts(profile.obstacleGroups[0],{0,99},true,error));
		assert(serialized(profile)==remaining);
		assert(S9xRemasterObstacleEditorSetMemberZ(profile.obstacleGroups[0],{0,2},-4095,"bulk_z",error));
		for (size_t i=0;i<group.members.size();i++) {
			const auto &m=profile.obstacleGroups[0].members[i];
			assert(m.z==((i==0 || i==2)?-4095:group.members[i].z));
			assert(m.supportContact==(i==3));
			assert(m.x==group.members[i].x && m.y==group.members[i].y && m.hflip==group.members[i].hflip && m.vflip==group.members[i].vflip);
		}
		const auto bulk=serialized(profile);
		assert(!S9xRemasterObstacleEditorSetMemberZ(profile.obstacleGroups[0],{0,99},4,"no",error));
		assert(!S9xRemasterObstacleEditorSetMemberZ(profile.obstacleGroups[0],{0},4096,"no",error));
		assert(!S9xRemasterObstacleEditorSetMemberZ(profile.obstacleGroups[0],{},4,"no",error));
		assert(serialized(profile)==bulk);
		assert(S9xRemasterParseProfile(bulk,roundtrip,diagnostics) && serialized(roundtrip)==bulk);
		assert(S9xRemasterParseProfile(remaining,roundtrip,diagnostics) && serialized(roundtrip)==remaining);
		// Other group/source selected: reject, never silently select a subset.
		auto foreign=cells[2]; foreign.tileId=other;
		assert(!S9xRemasterObstacleEditorResolveSelection(frame,group,{cells[0],foreign},selected,error));
		foreign=cells[2]; foreign.background++;
		assert(!S9xRemasterObstacleEditorResolveSelection(frame,group,{cells[0],foreign},selected,error));
		foreign=cells[2]; foreign.spriteAssembly=!sprite;
		assert(!S9xRemasterObstacleEditorResolveSelection(frame,group,{cells[0],foreign},selected,error));
		// Occluded remainder: uniquely identifiable sparse selection still works.
		frame.mainPixels.assign(96*96,{});
		const auto &c=cells[2]; auto &p=frame.mainPixels[c.y*96+c.x]; p.instanceId=3;
		p.tilePixel=(c.vflip?7:0)*8+(c.hflip?7:0);
		assert(S9xRemasterObstacleEditorResolveSelection(frame,group,{c},selected,error));
		assert((selected==std::vector<size_t>{2}));
		// With only repeated artwork visible, there are two possible authored
		// members with the same individual flips: fail instead of hash fan-out.
		frame.mainPixels.assign(96*96,{});
		const auto &r=cells[1]; auto &q=frame.mainPixels[r.y*96+r.x]; q.instanceId=2;
		q.tilePixel=(r.vflip?7:0)*8+(r.hflip?7:0);
		assert(!S9xRemasterObstacleEditorResolveSelection(frame,group,{r},selected,error));
		assert(error.find("ambiguous")!=std::string::npos);
	}
}

static void occurrenceTransfer()
{
	for (bool sprite : {false,true}) {
		RemasterFrame frame; frame.width=64; frame.height=16;
		frame.mainPixels.resize(1024); frame.subPixels.resize(1024);
		std::vector<RemasterObstacleEditorCell> cells, picked;
		for (int n=0;n<6;n++) {
			RemasterFrameTileInstance instance; instance.tileId={123,1,4};
			instance.source=sprite?RemasterSourceType::Object:RemasterSourceType::Background;
			instance.sourceIndex=sprite?n:1; instance.hFlip=n&1; instance.vFlip=n&2;
			frame.tileInstances.push_back(instance);
			auto &pixel=frame.mainPixels[n*8]; pixel.instanceId=n+1;
			pixel.tilePixel=(instance.vFlip?7:0)*8+(instance.hFlip?7:0);
			RemasterObstacleEditorCell cell;
			assert(S9xRemasterObstacleEditorCellAt(frame,n*8,0,cell)); cells.push_back(cell);
		}
		// Two adjacent objects share every identity. Select just the first three
		// clicks, as in an identity layer; repeated hashes must survive transfer.
		for (int n=0;n<3;n++) S9xRemasterObstacleEditorPick(picked,&cells[n],n!=0);
		std::set<uint32_t> ids;
		assert(S9xRemasterObstacleEditorSelectionInstances(frame,picked,ids));
		assert((ids==std::set<uint32_t>{1,2,3}) && picked.size()==3);
		RemasterObstacleGroup group; std::string error;
		assert(S9xRemasterObstacleEditorBuild(picked,{},"exact",group,error));
		assert(group.members.size()==3); // No adjacent same-identity flood fill.
		S9xRemasterObstacleEditorPick(picked,&cells[1],true);
		assert(S9xRemasterObstacleEditorSelectionInstances(frame,picked,ids));
		assert((ids==std::set<uint32_t>{1,3}));
		assert(S9xRemasterObstacleEditorSelectionInstances(frame,{},ids) && ids.empty());
		// Another packet's coordinates are never promoted when evidence is stale.
		frame.mainPixels.assign(1024,{});
		assert(!S9xRemasterObstacleEditorSelectionInstances(frame,picked,ids) && ids.empty());
	}
}

static void orientedLayout()
{
	// The user's horizontal pillar: canonical west head left, support right.
	// V symmetry does not reverse that support direction; H mirroring does.
	RemasterObstacleGroup group;group.name="wall_pillar";
	for(int y=0;y<2;y++)for(int x=0;x<4;x++)
		group.members.push_back({{uint64_t(100+x),1,4},x,y,0,false,bool(y),x==3});
	for(int flip=0;flip<4;flip++) {
		assert(S9xRemasterObstacleSupportDirection(group,flip)==((flip&1)?2:3));
		auto layout=S9xRemasterObstacleEditorDisplayLayout(group,flip);
		for(size_t i=0;i<group.members.size();i++) {
			assert(layout.members[i].x==((flip&1)?3-group.members[i].x:group.members[i].x));
			assert(layout.members[i].supportContact==group.members[i].supportContact);
		}
		RemasterFrame frame;frame.width=80;frame.height=40;frame.mainPixels.resize(3200);frame.subPixels.resize(3200);
		std::vector<RemasterObstacleEditorCell> cells;
		for(size_t i=0;i<layout.members.size();i++) {
			const auto &m=layout.members[i];RemasterFrameTileInstance inst;inst.tileId=m.tileId;
			inst.source=RemasterSourceType::Background;inst.sourceIndex=1;inst.hFlip=m.hflip;inst.vFlip=m.vflip;
			frame.tileInstances.push_back(inst);const int x=16+m.x*8,y=8+m.y*8;
			auto &px=frame.mainPixels[y*80+x];px.instanceId=i+1;px.tilePixel=(m.vflip?7:0)*8+(m.hflip?7:0);
			RemasterObstacleEditorCell cell;assert(S9xRemasterObstacleEditorCellAt(frame,x,y,cell));cells.push_back(cell);
		}
		std::vector<size_t> indexes;std::string error;uint8_t observedFlip=255;
		assert(S9xRemasterObstacleEditorResolveSelection(frame,group,cells,indexes,error,&observedFlip));
		assert((observedFlip&1)==(flip&1)); // symmetric V placement is intentionally equivalent
		auto displayed=S9xRemasterObstacleEditorDisplayLayout(group,observedFlip);
		for(const auto &m:displayed.members)if(m.supportContact)assert(m.x==((flip&1)?0:3));
	}
}

static void sharedIdentityFocus()
{
	const RemasterTileContentId a{101,1,4},b{102,1,4},c{103,1,4};
	for(bool sprite:{false,true})for(int flip=0;flip<4;flip++) {
		RemasterObstacleGroup east{"east",{{a,0,0,0,false,false,false},{b,1,0,0,false,false,true}}};
		east.spriteAssembly=sprite;auto west=east;west.name="west";west.members[1].tileId=c;
		std::vector<RemasterObstacleGroup> groups{east};
		RemasterFrame frame;frame.width=96;frame.height=64;frame.mainPixels.resize(96*64);frame.subPixels.resize(96*64);
		std::vector<std::vector<RemasterObstacleEditorCell>> cells(2);
		for(int n=0;n<2;n++)for(const auto &m:(n?west:east).members) {
			RemasterFrameTileInstance i;i.tileId=m.tileId;i.source=sprite?RemasterSourceType::Object:RemasterSourceType::Background;
			i.sourceIndex=sprite?uint8_t(frame.tileInstances.size()):1;i.hFlip=m.hflip!=bool(flip&1);i.vFlip=m.vflip!=bool(flip&2);
			frame.tileInstances.push_back(i);
			const int x=16+n*40+((flip&1)?-m.x:m.x)*8,y=24+((flip&2)?-m.y:m.y)*8;
			auto &p=frame.mainPixels[y*96+x];p.instanceId=frame.tileInstances.size();p.tilePixel=(i.vFlip?7:0)*8+(i.hFlip?7:0);
			RemasterObstacleEditorCell cell;assert(S9xRemasterObstacleEditorCellAt(frame,x,y,cell));cells[n].push_back(cell);
		}
		assert(S9xRemasterObstacleEditorFindGroup(frame,groups,{cells[0][0]})==0); // Existing member opens existing group.
		assert(S9xRemasterObstacleEditorFindGroup(frame,groups,{cells[1][0]})==-1); // Same hash, different footprint: create.
		assert(S9xRemasterObstacleEditorFindGroup(frame,groups,cells[1])==-1);
		RemasterObstacleGroup created;std::string error;
		assert(S9xRemasterObstacleEditorBuild(cells[1],groups,"west",created,error));
		groups.push_back(west);
		assert(S9xRemasterObstacleEditorFindGroup(frame,groups,{cells[1][0]})==1);
		assert(S9xRemasterObstacleEditorFindGroup(frame,groups,{cells[0][0],cells[1][0]})==-1);
		auto duplicate=east;duplicate.name="competing";duplicate.members[0].z=7;groups.push_back(duplicate);
		assert(S9xRemasterObstacleEditorFindGroup(frame,groups,{cells[0][0]})==-2);
		frame.mainPixels.assign(96*64,{});
		assert(S9xRemasterObstacleEditorFindGroup(frame,groups,{cells[0][0]})==-2); // Stale clicks never select an owner or create.
		RemasterProfile baseline;baseline.gameTitle="shared";baseline.romSha256=std::string(64,'a');baseline.obstacleGroups={east};
		auto session=baseline,disk=baseline;session.obstacleGroups.push_back(west);disk.obstacleGroups[0].members[0].z=3;
		RemasterProfileMergeResult merged;std::vector<RemasterProfileDiagnostic> diagnostics;
		assert(S9xRemasterMergeProfiles(baseline,session,disk,{},false,merged,diagnostics));
		assert(merged.conflicts.empty() && merged.saved.obstacleGroups.size()==2 && merged.saved.obstacleGroups[0].members[0].z==3);
		RemasterProfile roundtrip;assert(S9xRemasterParseProfile(serialized(merged.saved),roundtrip,diagnostics));
		assert(roundtrip.obstacleGroups.size()==2 && roundtrip.obstacleGroups[0].members[0].tileId==roundtrip.obstacleGroups[1].members[0].tileId);
	}
}

int main(int argc, char **argv)
{
	canonicalEastContacts();
	contactEditing();
	occurrenceTransfer();
	orientedLayout();
	sharedIdentityFocus();
	RemasterTileContentId repeated{123,1,4}, foot{456,1,4};
	std::vector<RemasterObstacleEditorCell> cells{
		{repeated,40,16,1,false,false},{repeated,40,24,1,false,false},
		{foot,40,32,1,true,false}};
	RemasterObstacleGroup group; std::string error;
	assert(S9xRemasterObstacleEditorBuild(cells,{},"column",group,error));
	assert(group.members.size()==3 && group.members[0].y==0 && group.members[2].y==2);
	assert(!group.members[0].supportContact && !group.members[1].supportContact && group.members[2].supportContact);
	assert(group.members[0].z==0 && group.members[2].hflip);
	assert(S9xRemasterObstacleEditorBuild(cells,{group},"second",group,error)); // Ownership is contextual, not hash-exclusive.
	cells[1].background=0;
	assert(!S9xRemasterObstacleEditorBuild(cells,{},"column",group,error));
	cells[1].background=1; cells[1].x++;
	assert(!S9xRemasterObstacleEditorBuild(cells,{},"column",group,error));
	// Rasterizer canonical pixels recover the same occurrence origin under flips.
	RemasterFrame frame; frame.width=frame.height=16;
	frame.mainPixels.resize(256); frame.subPixels.resize(256);
	RemasterFrameTileInstance instance; instance.tileId=repeated;
	instance.source=RemasterSourceType::Background; instance.sourceIndex=1;
	assert(S9xRemasterObstacleEditorUsesFootprintSelection(&instance));
	assert(!S9xRemasterObstacleEditorUsesFootprintSelection(nullptr));
	for (int flip=0;flip<4;flip++)
	{
		instance.hFlip=flip&1; instance.vFlip=flip&2; frame.tileInstances={instance};
		frame.mainPixels[6*16+5].instanceId=1;
		frame.mainPixels[6*16+5].tilePixel=(flip&2?1:6)*8+(flip&1?2:5);
		RemasterObstacleEditorCell cell;
		assert(S9xRemasterObstacleEditorCellAt(frame,5,6,cell));
		assert(cell.x==0 && cell.y==0 && cell.hflip==instance.hFlip && cell.vflip==instance.vFlip);
	}
	// Movable OBJ parts from different OAM slots form a single footprint.
	instance.source=RemasterSourceType::Object;
	frame.tileInstances={instance};
	RemasterObstacleEditorCell cell;
	assert(S9xRemasterFrameVisibleInstanceAt(frame,5,6));
	assert(S9xRemasterObstacleEditorUsesFootprintSelection(&instance));
	assert(S9xRemasterObstacleEditorCellAt(frame,5,6,cell));
	assert(cell.spriteAssembly && cell.background==0);
	// OAM slot indexes are transient draw ownership, not separate backgrounds.
	instance.sourceIndex=121;frame.tileInstances={instance};
	RemasterObstacleEditorCell otherSlot;
	assert(S9xRemasterObstacleEditorCellAt(frame,5,6,otherSlot));
	assert(otherSlot.background==cell.background && otherSlot.spriteAssembly);
	std::vector<RemasterObstacleEditorCell> spriteCells{cell};
	cell.x+=8; spriteCells.push_back(cell);
	assert(S9xRemasterObstacleEditorBuild(spriteCells,{},"moving",group,error));
	assert(group.spriteAssembly && group.members.size()==2);
	spriteCells.back().spriteAssembly=false;
	assert(!S9xRemasterObstacleEditorBuild(spriteCells,{},"mixed",group,error));
	RemasterProfile profile; profile.gameTitle="Fixture"; profile.romSha256=std::string(64,'a'); profile.obstacleGroups={group};
	RemasterProfile decoded; std::vector<RemasterProfileDiagnostic> diagnostics;
	std::string text;
	if(!S9xRemasterSerializeProfile(profile,text,diagnostics)) {
		for(const auto &d:diagnostics)std::cerr<<d.message<<'\n';
		assert(false);
	}
	assert(S9xRemasterParseProfile(text,decoded,diagnostics));
	assert(decoded.schemaVersion>=30 && decoded.obstacleGroups[0].spriteAssembly);
	// Default contact generation follows a sparse south boundary, not the
	// bounding rectangle or number of unique artwork identities.
	spriteCells={{repeated,0,0,0,false,false,true},{repeated,0,8,0,false,false,true},
		{foot,16,0,0,true,false,true}};
	assert(S9xRemasterObstacleEditorBuild(spriteCells,{},"sparse",group,error));
	assert(!group.members[0].supportContact && group.members[1].supportContact && group.members[2].supportContact);
	auto legacy=text;legacy.replace(legacy.find("schema_version = 30"),19,"schema_version = 29");
	assert(!S9xRemasterParseProfile(legacy,decoded,diagnostics));
	auto malformed=text;malformed.replace(malformed.find("sprite_assembly = true"),22,"sprite_assembly = 1");
	assert(!S9xRemasterParseProfile(malformed,decoded,diagnostics));
	profile.obstacleGroups[0].spriteAssembly=false;
	assert(S9xRemasterSerializeProfile(profile,text,diagnostics));
	assert(text.find("sprite_assembly")==std::string::npos);
	assert(S9xRemasterParseProfile(text,decoded,diagnostics) && !decoded.obstacleGroups[0].spriteAssembly);
	instance.source=RemasterSourceType::Background; instance.tileId.bitDepth=2;
	frame.tileInstances={instance};
	assert(!S9xRemasterObstacleEditorUsesFootprintSelection(&instance));
	assert(!S9xRemasterObstacleEditorCellAt(frame,5,6,cell));
	// Completion is an explicit proposal: all mirrors and arbitrary signed Z,
	// preservation of existing fields, no ambiguous occurrence placement.
	for(int flip=0;flip<4;flip++) {
		RemasterFrame f;f.width=f.height=64;f.mainPixels.resize(4096);f.subPixels.resize(4096);
		RemasterObstacleGroup sparse;sparse.name="motif";sparse.spriteAssembly=true;
		sparse.members={{repeated,0,0,-2,false,false,true},{foot,1,0,3,true,false,true}};
		for(int n=0;n<3;n++) {
			RemasterFrameTileInstance a;a.source=RemasterSourceType::Object;a.tileId=n==1?foot:repeated;
			a.hFlip=(n==1)!=bool(flip&1);a.vFlip=flip&2;f.tileInstances.push_back(a);
			const int x=24+((flip&1)?-n:n)*8,y=24;
			auto &px=f.mainPixels[y*64+x];px.instanceId=n+1;px.tilePixel=(a.vFlip?7:0)*8+(a.hFlip?7:0);
		}
		RemasterObstacleGroup completed;
		assert(S9xRemasterObstacleEditorCompleteSpriteFootprint(f,sparse,completed));
		assert(completed.members.size()==3 && sparse.members.size()==2);
		assert(completed.members[0].z==-2 && completed.members[1].z==3 && completed.members[2].z==-2);
		assert(completed.members[2].x==2 && !completed.members[2].hflip);
		// Selecting an omitted repeat must offer a contact-only completion
		// transaction, not reject the selection or reset unrelated contacts.
		RemasterObstacleEditorCell omitted;
		assert(S9xRemasterObstacleEditorCellAt(f,24+((flip&1)?-16:16),24,omitted));
		RemasterObstacleEditorCell anchorCell;
		assert(S9xRemasterObstacleEditorCellAt(f,24,24,anchorCell));
		RemasterObstacleGroup contactProposal; std::vector<size_t> contactMembers;
		bool needsCompletion=false;
		assert(S9xRemasterObstacleEditorPrepareContactSelection(f,sparse,{anchorCell,omitted},contactProposal,contactMembers,needsCompletion,error));
		assert(needsCompletion && (contactMembers==std::vector<size_t>{0,2}));
		assert(contactProposal.members[0].supportContact && contactProposal.members[1].supportContact && !contactProposal.members[2].supportContact);
		assert(S9xRemasterObstacleEditorSetContacts(contactProposal,contactMembers,true,error));
		assert(contactProposal.members[2].supportContact && contactProposal.members[2].z==-2 && sparse.members.size()==2);
		assert(!S9xRemasterObstacleEditorCompleteSpriteFootprint(f,completed,completed));
	}
	if (argc >= 2)
	{
		RemasterFrame captured;
		assert(S9xReadRemasterFrame(argv[1],captured));
		assert(captured.width==256 && captured.height==224);
		unsigned objectPixels=0;
		std::map<std::pair<int,int>,RemasterObstacleEditorCell> occurrences;
		for (uint32_t y=35;y<=66;y++) for (uint32_t x=135;x<=182;x++)
		{
			const auto *picked=S9xRemasterFrameVisibleInstanceAt(captured,x,y);
			assert(picked);
			if (picked->source != RemasterSourceType::Object) continue;
			assert(picked->sourceIndex>=116 && picked->sourceIndex<=121);
			assert(S9xRemasterObstacleEditorUsesFootprintSelection(picked));
			assert(S9xRemasterObstacleEditorCellAt(captured,x,y,cell));
			assert(cell.spriteAssembly);
			occurrences[{cell.x,cell.y}]=cell;
			objectPixels++;
		}
		assert(objectPixels==1518);
		assert(occurrences.size()==24);
		spriteCells.clear(); for(const auto &entry:occurrences)spriteCells.push_back(entry.second);
		std::vector<RemasterObstacleEditorCell> preselection;
		std::set<RemasterTileContentId> preselectedTypes;
		for (const auto &c : spriteCells) {
			S9xRemasterObstacleEditorPick(preselection,&c,!preselection.empty());
			preselectedTypes.insert(c.tileId);
		}
		std::set<uint32_t> transferred;
		assert(preselection.size()==24 && preselectedTypes.size()==16);
		assert(S9xRemasterObstacleEditorSelectionInstances(captured,preselection,transferred));
		assert(!transferred.empty());
		assert(S9xRemasterObstacleEditorBuild(preselection,{},"transferred",group,error));
		assert(group.members.size()==24);
		assert(S9xRemasterObstacleEditorBuild(spriteCells,{},"ThroneRoomDoorBlocker",group,error));
		assert(group.spriteAssembly && group.members.size()==24);
		assert(std::count_if(group.members.begin(),group.members.end(),[](const auto &m){return m.supportContact;})==6);
		std::vector<size_t> selected; std::string selectionError;
		assert(S9xRemasterObstacleEditorResolveSelection(captured,group,spriteCells,selected,selectionError));
		assert(selected.size()==24);
		// Actual capture repeated occurrences remain independently selectable.
		for (size_t i=0;i<spriteCells.size();i++) {
			assert(S9xRemasterObstacleEditorResolveSelection(captured,group,{spriteCells[i]},selected,selectionError));
			assert(selected.size()==1);
			assert(group.members[selected[0]].x==(spriteCells[i].x-135)/8);
			assert(group.members[selected[0]].y==(spriteCells[i].y-35)/8);
		}
		if (argc>=3) {
			RemasterProfile authored; assert(S9xRemasterLoadProfile(argv[2],authored,diagnostics));
			size_t checked=0;
			for (const auto &authoredGroup : authored.obstacleGroups) if (authoredGroup.spriteAssembly) {
				bool contains=true;
				for (const auto &c : spriteCells)
					if (std::none_of(authoredGroup.members.begin(),authoredGroup.members.end(),[&](const auto &m){return m.tileId==c.tileId;})) contains=false;
				if (!contains) continue;
				std::vector<RemasterObstacleEditorCell> subset;
				// A saved sparse footprint need not own every observed repeat.
				// Pick a unique artwork identity, then require exact occurrence
				// resolution, or an explicit ambiguity/nonmatch diagnostic.
				for (const auto &c : spriteCells) if (std::count_if(authoredGroup.members.begin(),authoredGroup.members.end(),[&](const auto &m){return m.tileId==c.tileId;})==1)
				{ subset.push_back(c); break; }
				assert(!subset.empty());
				const auto before=serialized(authored);
				assert(S9xRemasterObstacleEditorResolveSelection(captured,authoredGroup,subset,selected,selectionError));
				assert(selected.size()==1 && authoredGroup.members[selected[0]].tileId==subset[0].tileId);
				const bool full=S9xRemasterObstacleEditorResolveSelection(captured,authoredGroup,spriteCells,selected,selectionError);
				assert(full ? selected.size()==24 : !selectionError.empty());
				for (int flip=1;flip<4;flip++) {
					auto mirrored=captured;
					mirrored.mainPixels.assign(captured.mainPixels.size(),{});
					mirrored.subPixels.assign(captured.subPixels.size(),{});
					for (auto &instance : mirrored.tileInstances) {
						instance.hFlip=instance.hFlip!=bool(flip&1);
						instance.vFlip=instance.vFlip!=bool(flip&2);
					}
					for (uint32_t y=0;y<captured.height;y++) for (uint32_t x=0;x<captured.width;x++) {
						const size_t src=size_t(y)*captured.width+x;
						const size_t dst=size_t((flip&2)?captured.height-1-y:y)*captured.width+((flip&1)?captured.width-1-x:x);
						mirrored.mainPixels[dst]=captured.mainPixels[src];
						mirrored.subPixels[dst]=captured.subPixels[src];
					}
					auto transformed=subset;
					for (auto &c : transformed) {
						if (flip&1) c.x=int(captured.width)-8-c.x;
						if (flip&2) c.y=int(captured.height)-8-c.y;
						c.hflip=c.hflip!=bool(flip&1); c.vflip=c.vflip!=bool(flip&2);
					}
					assert(S9xRemasterObstacleEditorResolveSelection(mirrored,authoredGroup,transformed,selected,selectionError));
					assert(selected.size()==1 && authoredGroup.members[selected[0]].tileId==subset[0].tileId);
				}
				assert(serialized(authored)==before); checked++;
			}
			assert(checked>0);
			std::cout<<"PASS read-only current profile/capture exact occurrence selection ("<<checked<<" sprite group)\n";
		}
		std::cout<<"PASS room051 capture: 1518 pixels, 24 occurrences, six contacts form one sprite assembly\n";
	}
	std::cout<<"PASS occurrence footprint authoring; bulk contacts, signed Z isolation, all mirrors, sparse/ambiguous selections, atomic rejection, undo snapshots, save merge and roundtrip\n";
}
