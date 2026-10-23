#ifndef REMASTER_OBSTACLE_GROUP_EDITOR_H
#define REMASTER_OBSTACLE_GROUP_EDITOR_H

#include "frame.h"

// Occurrence selection is deliberately separate from identity selection: one
// repeated middle tile can occupy several distinct members of a footprint.
struct RemasterObstacleEditorCell
{
	RemasterTileContentId tileId;
	int x = 0, y = 0;
	uint8_t background = 0;
	bool hflip = false, vflip = false;
	bool spriteAssembly = false;
};

// Both static background footprints and movable OBJ assemblies retain each
// occurrence, even when a tile identity repeats within the object.
inline bool S9xRemasterObstacleEditorUsesFootprintSelection(const RemasterFrameTileInstance *instance)
{
	return instance && (instance->source == RemasterSourceType::Background ||
		instance->source == RemasterSourceType::Object) && instance->tileId.bitDepth == 4;
}

inline bool S9xRemasterObstacleEditorCellAt(const RemasterFrame &frame,
	uint32_t x, uint32_t y, RemasterObstacleEditorCell &cell)
{
	bool sub = false;
	const auto *instance = S9xRemasterFrameVisibleInstanceAt(frame, x, y, &sub);
	if (!S9xRemasterObstacleEditorUsesFootprintSelection(instance))
		return false;
	const auto &pixels = sub ? frame.subPixels : frame.mainPixels;
	const auto pixel = pixels[size_t(y) * frame.width + x].tilePixel;
	if (pixel >= 64) return false;
	const int dx = instance->hFlip ? 7 - pixel % 8 : pixel % 8;
	const int dy = instance->vFlip ? 7 - pixel / 8 : pixel / 8;
	const bool sprite = instance->source == RemasterSourceType::Object;
	cell = {instance->tileId, int(x) - dx, int(y) - dy, uint8_t(sprite ? 0 : instance->sourceIndex),
		instance->hFlip, instance->vFlip, sprite};
	return true;
}

inline bool S9xRemasterObstacleEditorSameCell(const RemasterObstacleEditorCell &a,
	const RemasterObstacleEditorCell &b)
{
	return std::tie(a.x,a.y,a.background,a.spriteAssembly,a.tileId,a.hflip,a.vflip) ==
		std::tie(b.x,b.y,b.background,b.spriteAssembly,b.tileId,b.hflip,b.vflip);
}

inline void S9xRemasterObstacleEditorPick(std::vector<RemasterObstacleEditorCell> &selected,
	const RemasterObstacleEditorCell *cell, bool extend)
{
	if (!extend) selected.clear();
	if (!cell) return;
	auto found=std::find_if(selected.begin(),selected.end(),[&](const auto &c) {
		return S9xRemasterObstacleEditorSameCell(c,*cell); });
	if (found==selected.end()) selected.push_back(*cell);
	else if (extend) selected.erase(found);
}

// Transfer only observed clicks, never identity fan-out or adjacent completion.
// A cell may have multiple scanline instance records, all at the same location.
inline bool S9xRemasterObstacleEditorSelectionInstances(const RemasterFrame &frame,
	const std::vector<RemasterObstacleEditorCell> &selected, std::set<uint32_t> &instances)
{
	instances.clear();
	std::set<size_t> seen;
	for (uint32_t y=0;y<frame.height;y++) for (uint32_t x=0;x<frame.width;x++) {
		RemasterObstacleEditorCell cell;
		if (!S9xRemasterObstacleEditorCellAt(frame,x,y,cell)) continue;
		for (size_t i=0;i<selected.size();i++) if (S9xRemasterObstacleEditorSameCell(cell,selected[i])) {
			const auto *instance=S9xRemasterFrameVisibleInstanceAt(frame,x,y);
			instances.insert(uint32_t(instance-frame.tileInstances.data()+1)); seen.insert(i); break;
		}
	}
	if (seen.size()!=selected.size()) { instances.clear(); return false; }
	return true;
}

inline bool S9xRemasterObstacleEditorBuild(const std::vector<RemasterObstacleEditorCell> &cells,
	const std::vector<RemasterObstacleGroup> &existing, const std::string &name,
	RemasterObstacleGroup &result, std::string &error)
{
	if (cells.empty()) { error = "Select the individual tiles of one obstacle in the scene."; return false; }
	int left = cells.front().x, top = cells.front().y;
	for (const auto &cell : cells) { left = std::min(left, cell.x); top = std::min(top, cell.y); }
	RemasterObstacleGroup candidate; candidate.name = name;
	candidate.spriteAssembly = cells.front().spriteAssembly;
	std::set<std::pair<int,int>> positions;
	for (const auto &cell : cells)
	{
		if (cell.spriteAssembly != candidate.spriteAssembly || cell.background != cells.front().background || (cell.x-left)%8 || (cell.y-top)%8)
		{ error = "Select aligned 8×8 tiles from one background layer or one sprite assembly; do not mix sources."; return false; }
		const int x = (cell.x-left)/8, y = (cell.y-top)/8;
		if (!positions.emplace(x,y).second) { error = "The selection contains duplicate positions."; return false; }
		candidate.members.push_back({cell.tileId,x,y,0,cell.hflip,cell.vflip,false});
	}
	std::sort(candidate.members.begin(),candidate.members.end(),[](const auto &a,const auto &b)
		{ return std::tie(a.y,a.x)<std::tie(b.y,b.x); });
	// Editable initial contacts on the selected footprint's south boundary.
	// Z starts at zero: no vertical geometry is guessed from the artwork.
	for (auto &member : candidate.members)
		member.supportContact = !positions.count({member.x,member.y+1});
	auto groups = existing; groups.push_back(candidate);
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterValidateObstacleGroups(groups,diagnostics))
	{ error = diagnostics.empty() ? "Invalid obstacle group." : diagnostics.front().message; return false; }
	result = std::move(candidate); return true;
}

// Focus only a complete observed occurrence containing EVERY selected cell.
// Shared hashes and a last-clicked identity never choose a group. -1 means a
// new footprint; -2 means competing complete footprints (no editable owner).
inline int S9xRemasterObstacleEditorFindGroup(const RemasterFrame &frame,
	const std::vector<RemasterObstacleGroup> &groups,
	const std::vector<RemasterObstacleEditorCell> &selected)
{
	if(selected.empty())return -1;
	using Key=std::tuple<int,int,RemasterTileContentId,bool,bool>;
	std::set<Key> observed;
	const auto &seed=selected.front();
	for(const auto &c:selected)if(c.background!=seed.background || c.spriteAssembly!=seed.spriteAssembly)return -1;
	for(uint32_t y=0;y<frame.height;y++)for(uint32_t x=0;x<frame.width;x++) {
		RemasterObstacleEditorCell c;
		if(S9xRemasterObstacleEditorCellAt(frame,x,y,c) && c.background==seed.background && c.spriteAssembly==seed.spriteAssembly)
			observed.emplace(c.x,c.y,c.tileId,c.hflip,c.vflip);
	}
	for(const auto &c:selected)if(!observed.count({c.x,c.y,c.tileId,c.hflip,c.vflip}))return -2; // stale, not new
	int owner=-1;
	for(size_t gi=0;gi<groups.size();gi++) {
		const auto &g=groups[gi];if(g.spriteAssembly!=seed.spriteAssembly)continue;
		bool matches=false;
		for(const auto &anchor:g.members)if(anchor.tileId==seed.tileId)
		for(int flip=0;flip<4;flip++) {
			const bool hf=flip&1,vf=flip&2;
			if(seed.hflip!=(anchor.hflip!=hf) || seed.vflip!=(anchor.vflip!=vf))continue;
			const int ox=seed.x-(hf?-anchor.x:anchor.x)*8,oy=seed.y-(vf?-anchor.y:anchor.y)*8;
			std::set<Key> footprint;bool complete=true;
			for(const auto &m:g.members) {
				Key k{ox+(hf?-m.x:m.x)*8,oy+(vf?-m.y:m.y)*8,m.tileId,m.hflip!=hf,m.vflip!=vf};
				if(!observed.count(k)){complete=false;break;}footprint.insert(k);
			}
			if(!complete)continue;
			for(const auto &c:selected)if(!footprint.count({c.x,c.y,c.tileId,c.hflip,c.vflip})){complete=false;break;}
			matches|=complete;
		}
		if(matches){if(owner>=0)return -2;owner=int(gi);}
	}
	return owner;
}

// Resolve scene occurrences, not hashes. Missing/occluded members are allowed,
// but repeated artwork must have a unique mapping under the best captured
// translated/mirrored footprint evidence. No runtime support/height is inferred.
inline bool S9xRemasterObstacleEditorResolveSelection(const RemasterFrame &frame,
	const RemasterObstacleGroup &group, const std::vector<RemasterObstacleEditorCell> &selected,
	std::vector<size_t> &members, std::string &error, uint8_t *placementFlip=nullptr)
{
	members.clear();
	if (selected.empty()) { error = "Select obstacle occurrences in the scene or members in the footprint."; return false; }
	using Key = std::tuple<int,int,RemasterTileContentId,bool,bool>;
	std::set<Key> observed;
	for (const auto &cell : selected)
		if (cell.spriteAssembly != group.spriteAssembly || cell.background != selected.front().background)
		{ error = "Select occurrences from one obstacle and source layer."; return false; }
	for (uint32_t y=0;y<frame.height;y++) for (uint32_t x=0;x<frame.width;x++) {
		RemasterObstacleEditorCell cell;
		if (S9xRemasterObstacleEditorCellAt(frame,x,y,cell) && cell.spriteAssembly==group.spriteAssembly &&
			cell.background==selected.front().background)
			observed.emplace(cell.x,cell.y,cell.tileId,cell.hflip,cell.vflip);
	}
	for (const auto &cell : selected)
		if (!observed.count({cell.x,cell.y,cell.tileId,cell.hflip,cell.vflip}))
		{ error="Selected occurrences are no longer visible in this capture. Reselect them or use the footprint buttons."; return false; }
	std::set<std::tuple<int,int,int>> attempted;
	std::set<std::vector<size_t>> mappings;
	uint8_t bestFlip=255;
	size_t best = 0;
	const auto &seed = selected.front();
	for (const auto &anchor : group.members) if (anchor.tileId==seed.tileId)
	for (int flip=0;flip<4;flip++) {
		const bool hf=flip&1, vf=flip&2;
		if (seed.hflip!=(anchor.hflip!=hf) || seed.vflip!=(anchor.vflip!=vf)) continue;
		const int ox=seed.x-(hf?-anchor.x:anchor.x)*8, oy=seed.y-(vf?-anchor.y:anchor.y)*8;
		if (!attempted.emplace(ox,oy,flip).second) continue;
		std::vector<size_t> mapping;
		for (const auto &cell : selected) {
			for (size_t i=0;i<group.members.size();i++) {
				const auto &m=group.members[i];
				if (cell.x==ox+(hf?-m.x:m.x)*8 && cell.y==oy+(vf?-m.y:m.y)*8 &&
					cell.tileId==m.tileId && cell.hflip==(m.hflip!=hf) && cell.vflip==(m.vflip!=vf))
				{ mapping.push_back(i); break; }
			}
		}
		if (mapping.size()!=selected.size()) continue;
		size_t score=0;
		for (const auto &m : group.members)
			score += observed.count({ox+(hf?-m.x:m.x)*8,oy+(vf?-m.y:m.y)*8,m.tileId,m.hflip!=hf,m.vflip!=vf});
		std::sort(mapping.begin(),mapping.end()); mapping.erase(std::unique(mapping.begin(),mapping.end()),mapping.end());
		if (score>best) { best=score; mappings.clear(); bestFlip=255; }
		if (score==best) {mappings.insert(mapping);bestFlip=std::min(bestFlip,uint8_t(flip));}
	}
	if (mappings.size()!=1) {
		error = mappings.empty() ? "Selected occurrences do not match one authored obstacle footprint." :
			"Repeated occurrences are ambiguous. Select more of the obstacle or use its footprint buttons.";
		return false;
	}
	members=*mappings.begin(); if(placementFlip)*placementFlip=bestFlip; return true;
}

// View-only occurrence transform. Keep member indexes stable so displayed
// mirrored buttons still edit the canonical member, never its same-hash peer.
inline RemasterObstacleGroup S9xRemasterObstacleEditorDisplayLayout(const RemasterObstacleGroup &group, uint8_t flip)
{
	auto result=group;int left=63,top=63;
	for(auto &m:result.members) {
		if(flip&1){m.x=-m.x;m.hflip=!m.hflip;}
		if(flip&2){m.y=-m.y;m.vflip=!m.vflip;}
		left=std::min(left,m.x);top=std::min(top,m.y);
	}
	for(auto &m:result.members){m.x-=left;m.y-=top;}
	return result;
}

// Transactional contact-only edit. In particular, the profile's at-least-one
// contact invariant remains enforced; failed edits leave every field untouched.
// Derive click intent from the MODEL, never NSButton's post-click state:
// AppKit's three-state switch cycles Off -> Mixed -> On -> Off.
inline bool S9xRemasterObstacleEditorContactIntent(const RemasterObstacleGroup &group,
	const std::vector<size_t> &members, bool &contact, std::string &error)
{
	if (members.empty()) { error="Select at least one obstacle member in the footprint or member dropdown."; return false; }
	contact=false;
	for (auto i : members) {
		if (i>=group.members.size()) { error="The obstacle member selection is stale. Reselect members in the footprint or member dropdown."; return false; }
		contact |= !group.members[i].supportContact;
	}
	return true;
}

inline bool S9xRemasterObstacleEditorSetContacts(RemasterObstacleGroup &group,
	const std::vector<size_t> &members, bool contact, std::string &error)
{
	if (members.empty()) { error="Select at least one obstacle member."; return false; }
	auto candidate=group;
	for (auto i : members) {
		if (i>=candidate.members.size()) { error="The obstacle member selection is stale."; return false; }
		candidate.members[i].supportContact=contact;
	}
	if (std::none_of(candidate.members.begin(),candidate.members.end(),[](const auto &m){return m.supportContact;})) {
		error="At least one support contact must remain. Enable another member first, then remove this contact, or select the intended contacts together.";
		return false;
	}
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterValidateObstacleGroups({candidate},diagnostics)) {
		error=diagnostics.empty()?"Invalid obstacle contacts.":diagnostics.front().message; return false;
	}
	group=std::move(candidate); return true;
}

// Atomic bulk Z edit; contact flags, flips, coordinates and unselected Z stay
// byte-for-byte equivalent. The group name participates in the same undo step.
inline bool S9xRemasterObstacleEditorSetMemberZ(RemasterObstacleGroup &group,
	const std::vector<size_t> &members, int z, const std::string &name, std::string &error)
{
	if (members.empty()) { error="Select at least one obstacle member."; return false; }
	if (z < -4095 || z > 4095) { error="Member Z must be between -4095 and 4095."; return false; }
	auto candidate=group; candidate.name=name;
	for (auto i : members) {
		if (i>=candidate.members.size()) { error="The obstacle member selection is stale."; return false; }
		candidate.members[i].z=z;
	}
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterValidateObstacleGroups({candidate},diagnostics)) {
		error=diagnostics.empty()?"Invalid obstacle group.":diagnostics.front().message; return false;
	}
	group=std::move(candidate); return true;
}

// REVIEW-ONLY proposal, never runtime ownership/floor evidence. Recover repeated
// occurrences around a uniquely matched authored motif. Existing coordinates,
// flips and Z survive; contacts are explicitly regenerated on confirmation.
// Ambiguous motifs and identity-dependent Z disagreeing across repeats reject.
inline bool S9xRemasterObstacleEditorCompleteSpriteFootprint(const RemasterFrame &frame,
	const RemasterObstacleGroup &group, RemasterObstacleGroup &result, bool preserveContacts = false)
{
	if (!group.spriteAssembly || group.members.empty()) return false;
	using Key = std::tuple<int,int,RemasterTileContentId,bool,bool>;
	std::set<Key> observed;
	std::set<RemasterTileContentId> identities;
	for (const auto &m : group.members) identities.insert(m.tileId);
	for (uint32_t y=0;y<frame.height;y++) for (uint32_t x=0;x<frame.width;x++) {
		RemasterObstacleEditorCell cell;
		if (S9xRemasterObstacleEditorCellAt(frame,x,y,cell) && cell.spriteAssembly && identities.count(cell.tileId))
			observed.emplace(cell.x,cell.y,cell.tileId,cell.hflip,cell.vflip);
	}
	std::set<std::tuple<int,int,int>> attempted;
	std::vector<RemasterObstacleGroup> proposals;
	for (const auto &tile : observed) for (const auto &seed : group.members) if (std::get<2>(tile)==seed.tileId)
	for (int flip=0;flip<4;flip++) {
		const bool hf=flip&1,vf=flip&2;
		if (std::get<3>(tile)!=(seed.hflip!=hf) || std::get<4>(tile)!=(seed.vflip!=vf)) continue;
		const int ox=std::get<0>(tile)-(hf?-seed.x:seed.x)*8,oy=std::get<1>(tile)-(vf?-seed.y:seed.y)*8;
		if (!attempted.emplace(ox,oy,flip).second) continue;
		std::set<std::pair<int,int>> positions;
		bool exact=true;
		for (const auto &m : group.members) {
			if (!observed.count({ox+(hf?-m.x:m.x)*8,oy+(vf?-m.y:m.y)*8,m.tileId,m.hflip!=hf,m.vflip!=vf})) exact=false;
			positions.emplace(m.x,m.y);
		}
		if (!exact) continue;
		auto candidate=group;
		bool changed=true;
		while (changed) {
			changed=false;
			for (const auto &t : observed) {
				int dx=std::get<0>(t)-ox,dy=std::get<1>(t)-oy;
				if (dx%8 || dy%8) continue;
				int x=(hf?-dx:dx)/8,y=(vf?-dy:dy)/8;
				if (positions.count({x,y}) || x < -63 || x > 63 || y < -63 || y > 63) continue;
				if (!positions.count({x-1,y}) && !positions.count({x+1,y}) &&
					!positions.count({x,y-1}) && !positions.count({x,y+1})) continue;
				int z=0;bool found=false,ambiguous=false;
				for (const auto &m : group.members) if (m.tileId==std::get<2>(t)) {
					if (found && z!=m.z) ambiguous=true;
					found=true;z=m.z;
				}
				if (!found || ambiguous) { exact=false;continue; }
				candidate.members.push_back({std::get<2>(t),x,y,z,std::get<3>(t)!=hf,std::get<4>(t)!=vf,false});
				positions.emplace(x,y);changed=true;
			}
		}
		if (!exact || candidate.members.size()==group.members.size()) continue;
		if (!preserveContacts)
			for (auto &m : candidate.members) m.supportContact=!positions.count({m.x,m.y+1});
		std::vector<RemasterProfileDiagnostic> errors;
		if (!S9xRemasterValidateObstacleGroups({candidate},errors)) continue;
		proposals.push_back(std::move(candidate));
	}
	if (proposals.size()!=1) return false;
	result=std::move(proposals.front());return true;
}

// Prepare only; the caller must explicitly confirm additions before committing.
// Existing contacts are preserved, new occurrences start noncontact. Completion
// is not a floor inference and cannot rescue cross-source/group/ambiguous picks.
inline bool S9xRemasterObstacleEditorPrepareContactSelection(const RemasterFrame &frame,
	const RemasterObstacleGroup &group, const std::vector<RemasterObstacleEditorCell> &selected,
	RemasterObstacleGroup &result, std::vector<size_t> &members, bool &needsCompletion, std::string &error)
{
	needsCompletion=false;
	if (S9xRemasterObstacleEditorResolveSelection(frame,group,selected,members,error))
	{ result=group; return true; }
	RemasterObstacleGroup completed;
	if (!S9xRemasterObstacleEditorCompleteSpriteFootprint(frame,group,completed,true) ||
		!S9xRemasterObstacleEditorResolveSelection(frame,completed,selected,members,error)) return false;
	result=std::move(completed); needsCompletion=true; error.clear(); return true;
}

#endif
