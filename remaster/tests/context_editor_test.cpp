// The native pick/cycle/assign callbacks use this exact editor model.
#include "../runtime/context_editor.h"
#include "../runtime/remaster.h"
#include <cstdlib>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = 0;
static void require(bool ok, const char *message) {
	if (!ok) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
static std::string text(const RemasterProfile &p) {
	std::string out; std::vector<RemasterProfileDiagnostic> d;
	require(S9xRemasterSerializeProfile(p,out,d), "serialize"); return out;
}
static RemasterProfile parse(const std::string &s) {
	RemasterProfile p; std::vector<RemasterProfileDiagnostic> d;
	require(S9xRemasterParseProfile(s,p,d), "parse"); return p;
}
static void duplicateOwners() {
	const RemasterTileContentId tile{0x1234,1,4}, other{0x5678,1,4};
	RemasterProfile base; base.gameTitle="Duplicate owner test"; base.romSha256=std::string(64,'a');
	for (const auto &t : {tile, other}) {
		base.assets[t].tileId=t;
		base.tileContexts[t]["Doorway"].hasNormals=true;
		base.tileContexts[t]["OOB_Black"].hasHeight=true; // valid zero height, missing normals
	}
	for (unsigned flip=0; flip<4; ++flip) {
		RemasterFrame f; f.area.id=8;
		for (unsigned i=0; i<3; ++i) {
			RemasterFrameTileInstance in; in.tileId=i==2?other:tile;
			in.source=RemasterSourceType::Background; in.sourceIndex=1;
			in.hFlip=flip&1; in.vFlip=flip&2; f.tileInstances.push_back(in);
			RemasterContextLocation at; at.area=8; at.background=1; at.x=i?34:33; at.y=50;
			f.contextLocations[i+1]=at;
		}
		for (const std::string choice : {std::string("OOB_Black"), std::string()}) {
			auto profile=base;
			RemasterContextEditor unbound;
			for (uint32_t id : {1u,2u,3u}) unbound.pick(profile,f,id,id!=1);
			unbound.choose(choice); const auto empty=text(profile);
			require(!unbound.canAssign(profile,f) && !unbound.assign(profile,f) && text(profile)==empty, "unbound competing owners rejected without partial assignments");
			profile.contextBindings[f.contextLocations.at(1)]={tile,"Doorway"};
			profile.contextBindings[f.contextLocations.at(2)]={tile,"Doorway"};
			RemasterContextEditor e;
			for (uint32_t id : {1u,2u,3u}) e.pick(profile,f,id,id!=1);
			e.choose(choice); const auto before=text(profile);
			require(!e.canAssign(profile,f), "competing selected hashes veto before mutation, including None");
			require(!e.assign(profile,f) && text(profile)==before, "no partial assignment or erasure before conflicting key");
			// Even selecting only the new owner cannot steal/erase a different stored owner.
			e.pick(profile,f,3,false); e.choose(choice);
			require(!e.canAssign(profile,f) && !e.assign(profile,f) && text(profile)==before, "existing other owner vetoes single pick atomically");
			// The veto is ownership-based, including the automatic sole-context erase path.
			profile.tileContexts[other].erase("Doorway");
			const auto sole=text(profile);
			require(!e.assign(profile,f) && text(profile)==sole, "sole context cannot erase another owner's binding");
		}
		// Same key + canonical tile + placement transform proves equivalent context occurrences.
		f.tileInstances[2].tileId=tile;
		auto profile=base; RemasterContextEditor e;
		for (uint32_t id : {1u,2u,3u}) e.pick(profile,f,id,id!=1);
		e.choose("OOB_Black");
		require(e.canAssign(profile,f) && e.assign(profile,f) && profile.contextBindings.size()==2, "equivalent visual clones accepted");
		const auto assigned=text(profile);
		require(e.assign(profile,f) && text(profile)==assigned, "duplicate assignment idempotent");
		f.tileInstances[2].hFlip=!f.tileInstances[2].hFlip;
		e.clear(); for (uint32_t id : {1u,2u,3u}) e.pick(profile,f,id,id!=1);
		e.choose("OOB_Black");
		require(!e.assign(profile,f) && text(profile)==assigned, "same hash with conflicting transform is not a proven duplicate");
		f.tileInstances[2].hFlip=f.tileInstances[1].hFlip;
		e.clear(); for (uint32_t id : {1u,2u,3u}) e.pick(profile,f,id,id!=1);
		e.choose(""); require(e.assign(profile,f) && profile.contextBindings.empty(), "equivalent duplicates clear idempotently");
		require(e.assign(profile,f) && profile.contextBindings.empty(), "duplicate None repeated safely");
		// Identical XY in a different BG or room is a different durable key, even for another hash.
		f.tileInstances[2].tileId=other; f.tileInstances[2].sourceIndex=0; f.contextLocations[3].background=0;
		e.clear(); e.pick(profile,f,2,false); e.pick(profile,f,3,true); e.choose("OOB_Black");
		require(e.assign(profile,f) && profile.contextBindings.size()==2, "same XY different BG different hashes valid");
		f.area.id=9; for (auto &at : f.contextLocations) at.second.area=9;
		f.tileInstances[1].tileId=other;
		e.clear(); e.pick(profile,f,2,false); e.choose("OOB_Black");
		require(e.assign(profile,f) && profile.contextBindings.size()==3, "same XY different room different hash valid");
	}
}
static void bulkTypes() {
	const RemasterTileContentId tile{0x1234,1,4}, other{0x5678,1,4};
	RemasterProfile base; base.gameTitle="Bulk test"; base.romSha256=std::string(64,'a');
	for (auto t : {tile,other}) {
		base.assets[t].tileId=t;
		base.tileContexts[t]["Doorway"].hasNormals=true;
		base.tileContexts[t]["OOB_Black"].hasHeight=true;
	}
	for (unsigned flip=0; flip<4; ++flip) {
		RemasterFrame f; f.area.id=8; f.width=8; f.height=1;
		f.originalRgb555.resize(8); f.mainPixels.resize(8); f.subPixels.resize(8);
		for (unsigned i=0;i<10;++i) {
			RemasterFrameTileInstance in; in.tileId=i==5?other:tile;
			in.source=RemasterSourceType::Background; in.sourceIndex=i==4?0:1;
			in.hFlip=flip&1; in.vFlip=flip&2; f.tileInstances.push_back(in);
			RemasterContextLocation at; at.area=i==7?9:8; at.background=in.sourceIndex;
			at.x=i==2?0:i; at.y=5; f.contextLocations[i+1]=at;
		}
		f.contextLocations.erase(7); // visible but unpublished
		// Main wins over sub; sub is pickable only where main has no owner.
		for (unsigned i=0;i<8;++i) f.mainPixels[i].instanceId=i+1;
		f.mainPixels[3].instanceId=1; // another retained occurrence is wholly hidden
		f.subPixels[0].instanceId=9; // hidden, must not bind
		f.mainPixels[1].instanceId=0; f.subPixels[1].instanceId=2;
		f.mainPixels[1].owner=uint32_t(RemasterSourceType::Backdrop)<<24;
		f.mainPixels[2].instanceId=0; f.mainPixels[2].owner=uint32_t(RemasterSourceType::Backdrop)<<24; f.subPixels[2].instanceId=3;
		for (auto &pixel:f.mainPixels) pixel.tilePixel=0;
		for (auto &pixel:f.subPixels) pixel.tilePixel=0;
		auto p=base; RemasterContextEditor e; std::vector<RemasterTileContentId> types;
		e.pickType(p,f,1,false,types); e.choose("OOB_Black");
		require(e.instances==std::set<uint32_t>({1,2,3}) && e.bindingCount()==2 && e.unavailable==2,
			"type click prepares only visible current-room published locations on clicked BG; clones dedup");
		const auto before=text(p); require(e.assign(p,f) && p.contextBindings.size()==2,"bulk exact assignment");
		const auto after=text(p); p=parse(after); S9xRemasterSetProfile(p); S9xRemasterApplyProfileToFrame(p,f);
		require(f.contextGeometry.count(1) && f.contextGeometry.at(1).height[0]==0 && !f.contextGeometry.at(1).hasNormals,
			"bulk authored zero missing normal source activation");
		require(!f.contextGeometry.count(5)&&!f.contextGeometry.count(6)&&!f.contextGeometry.count(9)&&!f.contextGeometry.count(10),"bulk excludes other BG ID hidden offscreen");
		std::vector<uint8_t> bytes; require(S9xSerializeRemasterFrame(f,bytes),"bulk serialize replay");
		RemasterFrame replay; require(S9xDeserializeRemasterFrame(bytes,replay),"bulk parse replay");
		p=parse(before); S9xRemasterSetProfile(p); S9xRemasterApplyProfileToFrame(p,replay);
		require(replay.contextGeometry.empty(),"bulk serialized undo");
		p=parse(after); S9xRemasterSetProfile(p); S9xRemasterApplyProfileToFrame(p,replay);
		require(replay.contextGeometry.count(2),"bulk serialized redo save parse replay");
		e.choose("Doorway"); require(e.replacementCount(p)==2,"distinct replacement count");
		e.pickType(p,f,6,true,types); require(types.size()==2 && e.name=="Doorway" && e.bindingCount()==3,"type multiselect preserves batch choice");
		auto missing=p; missing.tileContexts[other].erase("Doorway"); const auto missingBefore=text(missing);
		require(!e.assign(missing,f)&&text(missing)==missingBefore,"bulk missing context name rejects entire batch");
		e.pickType(p,f,6,true,types); require(types.size()==1 && e.bindingCount()==2,"type toggle removes only its batch");
		auto stale=f; stale.contextLocations[1].x++; require(!e.assign(p,stale),"bulk camera stale rejected");
		stale=f; stale.area.id=9; require(!e.assign(p,stale),"bulk room stale rejected");
		stale=f; stale.mainPixels[0].instanceId=0; stale.mainPixels[3].instanceId=0; require(!e.assign(p,stale),"bulk changed visible layer rejected");
		stale=f; stale.tileInstances[0].vFlip=!stale.tileInstances[0].vFlip; require(!e.assign(p,stale),"bulk transform stale rejected");
		// A published competing owner is not silently ignored just because its type isn't selected.
		for (bool transform : {false,true}) {
			auto conflict=f; conflict.contextLocations[6]=conflict.contextLocations[1];
			if(transform) { conflict.tileInstances[5].tileId=tile; conflict.tileInstances[5].hFlip=!conflict.tileInstances[0].hFlip; }
			e.clear(); types.clear(); e.pickType(p,conflict,1,false,types); e.choose("Doorway");
			const auto unchanged=text(p); require(!e.assign(p,conflict)&&text(p)==unchanged,"bulk competing owner transform atomic no partial");
		}
		e.clear(); types.clear(); e.pickType(p,f,5,false,types); e.choose("Doorway");
		require(e.instances==std::set<uint32_t>{5} && e.assign(p,f) && p.contextBindings.size()==3,"BG0 type click isolated from BG1");
		e.choose(""); require(e.assign(p,f)&&p.contextBindings.size()==2,"bulk None clears only batch");
		p.tileContexts[tile].erase("Doorway"); e.choose("OOB_Black");
		require(e.assign(p,f)&&p.contextBindings.size()==2,"bulk sole context preserves automatic clear semantics");
		e.clear(); require(!e.assign(p,f),"new retained frame invalidates bulk");
	}
}
int main() {
	bulkTypes();
	duplicateOwners();
	const RemasterTileContentId tile{0x40f4527fb3da5661,1,4}, other{0x1234,1,4};
	RemasterProfile p; p.gameTitle="Context editor test"; p.romSha256=std::string(64,'a');
	auto &asset=p.assets[tile]; asset.tileId=tile; asset.hasHeight=asset.hasNormals=true;
	asset.height.fill(-16); asset.normalXyz.fill(128);
	p.tileContexts[tile]["Doorway"].hasHeight=true;
	p.tileContexts[tile]["OOB_Black"].hasHeight=true; // valid authored zeros, absent normals
	for (unsigned flip=0;flip<4;++flip) {
		RemasterFrame f; f.width=8; f.height=8; f.area.id=8;
		f.originalRgb555.resize(64); f.mainPixels.resize(64); f.subPixels.resize(64);
		for(unsigned i=0;i<4;++i) {
			RemasterFrameTileInstance in; in.tileId=i==3?other:tile;
			in.source=RemasterSourceType::Background; in.sourceIndex=i==2?0:1;
			in.hFlip=flip&1; in.vFlip=flip&2; f.tileInstances.push_back(in);
			RemasterContextLocation at; at.area=8; at.background=in.sourceIndex; at.x=33+(i==1); at.y=50;
			f.contextLocations[i+1]=at;
		}
		for(unsigned i=0;i<64;++i) { f.mainPixels[i].instanceId=1+i/16; f.mainPixels[i].tilePixel=i; }
		auto profile=p; RemasterContextEditor e;
		e.pick(profile,f,1,false);
		e.cycle(profile,f,tile,{tile},1,true); // Doorway
		e.cycle(profile,f,tile,{tile},1,true); // OOB_Black, no assignments yet
		require(e.name=="OOB_Black" && e.instances==std::set<uint32_t>{1}, "choose unused existing context preserves explicit unbound target");
		const auto before=text(profile);
		require(e.assign(profile,f), "first assignment");
		require(profile.contextBindings.size()==1 && profile.contextBindings.at(f.contextLocations.at(1)).name=="OOB_Black", "exact location binding");
		const auto after=text(profile); profile=parse(after); // save/parse/reopen, no real user file
		S9xRemasterSetProfile(profile); S9xRemasterApplyProfileToFrame(profile,f);
		require(f.contextGeometry.count(1) && f.contextGeometry.at(1).height[0]==0 && !f.contextGeometry.at(1).hasNormals, "zero height selected without normal inheritance");
		require(!f.contextGeometry.count(2) && !f.contextGeometry.count(3), "no other location or BG implicitly bound");
		std::vector<uint8_t> bytes; require(S9xSerializeRemasterFrame(f,bytes), "capture serialize");
		RemasterFrame replay; require(S9xDeserializeRemasterFrame(bytes,replay), "capture replay");
		e.clear(); e.pick(profile,replay,1,false); require(e.name=="OOB_Black", "repick assigned context");
		e.pick(profile,replay,2,false); // second unbound occurrence, existing context now used
		e.choose("OOB_Black"); // exact dropdown model path, independent of cycling
		e.pick(profile,replay,1,true); // Command-click must retain chosen name and target
		require(e.name=="OOB_Black" && e.instances.size()==2, "multi-select preserves choice");
		require(e.assign(profile,replay) && profile.contextBindings.size()==2,"add unbound target to used context");
		e.pick(profile,replay,3,true); require(e.name=="OOB_Black", "unbound command-click retains choice");
		require(e.assign(profile,replay) && profile.contextBindings.size()==3,"same tile different BG explicit target");
		e.pick(profile,replay,4,true); const auto atomic=text(profile);
		require(!e.assign(profile,replay) && text(profile)==atomic,"mixed tiles missing name rejected atomically");
		e.pick(profile,replay,4,true); // toggle back off
		e.name.clear(); require(e.assign(profile,replay) && profile.contextBindings.empty(),"None clears selected bindings");
		profile=parse(before); S9xRemasterSetProfile(profile); S9xRemasterApplyProfileToFrame(profile,replay);
		require(replay.contextGeometry.empty(),"undo binding and publication cache refresh");
		profile=parse(after); S9xRemasterSetProfile(profile); S9xRemasterApplyProfileToFrame(profile,replay);
		require(replay.contextGeometry.count(1) && !replay.contextGeometry.at(1).hasNormals,"redo/reopen refresh");
		e.pick(profile,replay,1,false); auto stale=replay; stale.contextLocations.at(1).x++;
		require(!e.assign(profile,stale),"camera/id reuse stale location rejected");
		stale=replay; stale.area.id=9; require(!e.assign(profile,stale),"room transition rejected");
		stale=replay; stale.tileInstances[0].tileId=other; require(!e.assign(profile,stale),"frame identity replacement rejected");
		stale=replay; stale.tileInstances[0].vFlip=!stale.tileInstances[0].vFlip; require(!e.assign(profile,stale),"stale transform rejected");
		stale=replay; stale.contextLocations.erase(1); require(!e.assign(profile,stale),"missing location rejected");
		e.name="OOB_Black"; e.showAssigned(profile,replay,{tile}); require(e.instances==std::set<uint32_t>{1},"show assigned separate operation");
		e.name.clear(); e.showAssigned(profile,replay,{tile}); require(e.instances.count(2)&&e.instances.count(3),"show default/unbound separate operation");
		profile.tileContexts[tile].erase("Doorway"); e.pick(profile,replay,2,false);
		require(e.name=="OOB_Black", "sole automatic context shown on pick");
		e.name.clear(); require(e.assign(profile,replay),"None accepts sole context clear");
		S9xRemasterApplyProfileToFrame(profile,replay);
		require(replay.contextGeometry.count(2),"None clear does not disable automatic sole context");
		require(e.active(profile,replay,2).find("Automatic sole context: OOB_Black")!=std::string::npos &&
			e.active(profile,replay,2).find("normals absent")!=std::string::npos,"sole-auto channel status");
		profile=p; e.pick(profile,replay,1,false);
		require(e.active(profile,replay,1)=="Default / unbound","default status independent of edit choice");
		e.choose("OOB_Black"); require(e.active(profile,replay,1)=="Default / unbound","preview not assignment");
		require(e.assign(profile,replay) && e.active(profile,replay,1).find("Explicitly assigned: OOB_Black")!=std::string::npos,"exact assigned status");
		// A newly retained frame clears all invocation-local targets before IDs can be reused.
		e.clear(); require(!e.assign(profile,replay),"new frame/reopen cannot reuse old targets");
		stale=replay; stale.area.id=9; stale.contextLocations.at(1).area=9;
		e.pick(profile,stale,1,false); e.choose("Doorway"); require(e.assign(profile,stale),"same coordinates other room explicit binding");
		require(profile.contextBindings.at(replay.contextLocations.at(1)).name=="OOB_Black" &&
			profile.contextBindings.at(stale.contextLocations.at(1)).name=="Doorway","room bindings independent");
	}
	std::cout << "PASS production context editor flow: competing-owner atomic veto, equivalent clones, first binding, multiselect, save/parse/replay/repick, undo/redo, stale targets, H/V, sole-auto and None\n";
}
