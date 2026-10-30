#ifndef S9X_REMASTER_CONTEXT_EDITOR_H
#define S9X_REMASTER_CONTEXT_EDITOR_H

#include "frame.h"
#include "context_room.h"

// Inspector-only state. Instance IDs are frame-local, never durable targets.
struct RemasterContextEditor
{
	std::string name;
	std::set<uint32_t> instances;
	struct Target {
		RemasterTileContentId tile;
		RemasterContextLocation at;
		bool hFlip, vFlip;
	};
	std::map<uint32_t, Target> targets;
	size_t unavailable = 0;
	bool bulk = false;
	std::map<RemasterTileContentId, uint8_t> typeBackgrounds;
	std::set<uint32_t> visibleSnapshot;
	bool roomScope = false;
	std::set<std::string> include, exclude; // empty string is Default/unbound
	std::vector<Target> roomTargets;
	struct Counts { size_t total=0,included=0,excluded=0,eligible=0,replacements=0,newBindings=0,clears=0,blocked=0; } counts;
	std::string roomStatus;
	std::shared_ptr<const RemasterContextRoomCapture> roomGeneration;
	std::string roomProfileToken;
	std::set<std::string> preparedInclude,preparedExclude;
	std::string preparedName;
	std::map<RemasterTileContentId,uint8_t> preparedTypes;
	static std::string profileToken(const RemasterProfile &p) {
		// Bindings may be invalid in an editing model; preserve them in the token
		// without making the serializer silently discard their conflict evidence.
		auto copy=p;copy.contextBindings.clear();std::string token;std::vector<RemasterProfileDiagnostic>d;
		S9xRemasterSerializeProfile(copy,token,d);
		for(const auto &b:p.contextBindings) token+="\n"+std::to_string(b.first.area)+":"+std::to_string(b.first.background)+":"+std::to_string(b.first.x)+":"+std::to_string(b.first.y)+":"+RemasterProfileSerialization::TileId(b.second.tile)+":"+b.second.name;
		return token;
	}
	std::string effective(const RemasterProfile &p, const Target &t) const {
		const auto c=p.tileContexts.find(t.tile);
		if(c!=p.tileContexts.end() && c->second.size()==1) return c->second.begin()->first;
		const auto b=p.contextBindings.find(t.at);
		if(c!=p.tileContexts.end() && b!=p.contextBindings.end() && b->second.tile==t.tile && c->second.count(b->second.name)) return b->second.name;
		return "";
	}
	bool conflict(const RemasterProfile &p,const Target &t) const {
		const auto b=p.contextBindings.find(t.at); if(b==p.contextBindings.end()) return false;
		const auto c=p.tileContexts.find(t.tile);
		return !(b->second.tile==t.tile) || c==p.tileContexts.end() || !c->second.count(b->second.name);
	}
	bool changes(const RemasterProfile &p,const Target &t) const {
		const auto b=p.contextBindings.find(t.at), end=p.contextBindings.end();
		const auto c=p.tileContexts.find(t.tile);
		if(name.empty() || c==p.tileContexts.end() || c->second.size()<2) return b!=end;
		return b==end || b->second.name!=name;
	}
	void prepareRoom(const RemasterProfile &p,const RemasterFrame &f,const RemasterContextRoom &room) {
		roomTargets.clear();counts={};roomGeneration.reset();roomStatus=room.reason;
		if(!roomScope) return;
		if(!room.capture || !RemasterContextRoom::sameFrame(f,room.capture->packet) || f.profileRomSha256!=p.romSha256) {
			if(roomStatus.empty())roomStatus="Unavailable: frame/profile ROM or room changed";return;
		}
		roomGeneration=room.capture;roomStatus.clear();
		roomProfileToken=profileToken(p);
		preparedInclude=include;preparedExclude=exclude;preparedName=name;preparedTypes=typeBackgrounds;
		for(const auto &cell:room.cells) {
			const auto bg=typeBackgrounds.find(cell.tile);
			if(bg==typeBackgrounds.end() || bg->second!=cell.at.background) continue;
			++counts.total;Target t{cell.tile,cell.at,cell.hFlip,cell.vFlip};
			// Unsafe stored ownership is never masked away by a filter.
			const bool storedConflict=conflict(p,t);
			if(storedConflict) ++counts.blocked;
			const auto cls=effective(p,t);
			if(!include.empty() && !include.count(cls))continue;
			++counts.included;
			if(exclude.count(cls)){++counts.excluded;continue;}
			roomTargets.push_back(t);++counts.eligible;
			const auto c=p.tileContexts.find(t.tile);
			if(changes(p,t)) {
				if(name.empty() || c==p.tileContexts.end() || c->second.size()<2)++counts.clears;
				else if(p.contextBindings.count(t.at))++counts.replacements;
				else ++counts.newBindings;
			}
			if(!storedConflict && !name.empty() && (c==p.tileContexts.end() || !c->second.count(name)))++counts.blocked;
		}
	}
	static std::set<uint32_t> visible(const RemasterFrame &frame) {
		std::set<uint32_t> ids;
		if (frame.mainPixels.size()!=size_t(frame.width)*frame.height || frame.subPixels.size()!=frame.mainPixels.size()) return ids;
		for (uint32_t y=0;y<frame.height;++y) for (uint32_t x=0;x<frame.width;++x) {
			const auto *in=S9xRemasterFrameVisibleInstanceAt(frame,x,y);
			if (in) ids.insert(uint32_t(in-frame.tileInstances.data()+1));
		}
		return ids;
	}
	size_t bindingCount() const {
		if(roomScope) return roomTargets.size();
		std::set<RemasterContextLocation> keys;
		for (const auto &t : targets) keys.insert(t.second.at);
		return keys.size();
	}
	size_t replacementCount(const RemasterProfile &profile) const {
		if(roomScope) return counts.replacements+counts.clears;
		std::set<RemasterContextLocation> keys;
		for (const auto &t : targets) {
			const auto b=profile.contextBindings.find(t.second.at);
			if (b!=profile.contextBindings.end() && b->second.name!=name) keys.insert(t.second.at);
		}
		return keys.size();
	}
	void pickType(const RemasterProfile &, const RemasterFrame &frame, uint32_t id, bool extend,
		std::vector<RemasterTileContentId> &tiles) {
		if (!extend || !bulk || !valid(frame)) { clear(); tiles.clear(); }
		if (!id || id>frame.tileInstances.size()) return;
		const auto &picked=frame.tileInstances[id-1]; const auto tile=picked.tileId;
		const auto found=std::find(tiles.begin(),tiles.end(),tile);
		if (found==tiles.end()) { tiles.push_back(tile); typeBackgrounds[tile]=picked.sourceIndex; }
		else { tiles.erase(found); typeBackgrounds.erase(tile); }
		instances.clear(); targets.clear(); unavailable=0; bulk=true;
		visibleSnapshot=visible(frame);
		for (auto candidate : visibleSnapshot) {
			const auto &in=frame.tileInstances[candidate-1];
			const auto bg=typeBackgrounds.find(in.tileId);
			if (bg==typeBackgrounds.end() || in.source!=RemasterSourceType::Background || in.sourceIndex!=bg->second || in.sourceIndex>1) continue;
			if (remember(frame,candidate)) instances.insert(candidate); else ++unavailable;
		}
	}

	void clear() { instances.clear(); targets.clear(); typeBackgrounds.clear(); visibleSnapshot.clear(); unavailable=0; bulk=false; roomTargets.clear();roomGeneration.reset();counts={}; }
	void choose(const std::string &choice) { name = choice; }
	bool remember(const RemasterFrame &frame, uint32_t id) {
		if (!id || id > frame.tileInstances.size() || !frame.contextLocations.count(id)) return false;
		const auto &in = frame.tileInstances[id-1];
		if (in.source != RemasterSourceType::Background || in.sourceIndex > 1) return false;
		const auto &at = frame.contextLocations.at(id);
		if (at.area != frame.area.id || at.background != in.sourceIndex) return false;
		targets[id] = {in.tileId, at, in.hFlip, in.vFlip};
		return true;
	}
	bool valid(const RemasterFrame &frame) const {
		if (instances.empty() || instances.size() != targets.size()) return false;
		if (bulk && visibleSnapshot!=visible(frame)) return false;
		for (auto id : instances) {
			if (!id || id > frame.tileInstances.size() || !frame.contextLocations.count(id) || !targets.count(id)) return false;
			const auto &t = targets.at(id); const auto &in = frame.tileInstances[id-1];
			const auto &at = frame.contextLocations.at(id);
			if (at < t.at || t.at < at || at.area != frame.area.id || !(in.tileId == t.tile) ||
				in.source != RemasterSourceType::Background || in.sourceIndex != at.background ||
				in.hFlip != t.hFlip || in.vFlip != t.vFlip) return false;
		}
		return true;
	}
	void pick(const RemasterProfile &profile, const RemasterFrame &frame, uint32_t id, bool extend) {
		const auto previous = name;
		if (!extend || !valid(frame)) clear();
		if (extend && instances.erase(id)) targets.erase(id);
		else if (remember(frame, id)) instances.insert(id);
		if (extend) { name = previous; return; } // target selection never changes an explicit multi-selection choice
		name.clear();
		if (id && id <= frame.tileInstances.size() && frame.contextLocations.count(id)) {
			const auto c = profile.tileContexts.find(frame.tileInstances[id-1].tileId);
			if (c != profile.tileContexts.end() && c->second.size() == 1) {
				name = c->second.begin()->first; return;
			}
			const auto b = profile.contextBindings.find(frame.contextLocations.at(id));
			if (b != profile.contextBindings.end() && b->second.tile == frame.tileInstances[id-1].tileId) name = b->second.name;
		}
	}
	void showAssigned(const RemasterProfile &profile, const RemasterFrame &frame,
		const std::vector<RemasterTileContentId> &tiles) {
		clear();
		for (auto id : S9xRemasterContextSelection(profile, frame, tiles, name))
			if (remember(frame, id)) instances.insert(id);
	}
	void cycle(const RemasterProfile &profile, const RemasterFrame &frame,
		const RemasterTileContentId &tile, const std::vector<RemasterTileContentId> &tiles, int direction, bool locations) {
		std::vector<std::string> names = {""};
		const auto found = profile.tileContexts.find(tile);
		if (found != profile.tileContexts.end()) for (const auto &c : found->second) names.push_back(c.first);
		const auto current = std::find(names.begin(), names.end(), name);
		const int index = current == names.end() ? 0 : int(current - names.begin());
		name = names[(index + direction + int(names.size())) % names.size()];
		(void)frame; (void)tiles; (void)locations; // Choosing a preview is not selecting assigned targets.
	}
	std::string active(const RemasterProfile &profile, const RemasterFrame &frame, uint32_t id) const {
		if (!id || id > frame.tileInstances.size()) return "No picked occurrence";
		const auto &tile = frame.tileInstances[id-1].tileId;
		const auto c = profile.tileContexts.find(tile);
		std::string selected, kind = "Default / unbound";
		if (c != profile.tileContexts.end() && c->second.size() == 1) {
			selected = c->second.begin()->first; kind = "Automatic sole context: " + selected;
		} else if (c != profile.tileContexts.end() && frame.contextLocations.count(id)) {
			const auto b = profile.contextBindings.find(frame.contextLocations.at(id));
			if (b != profile.contextBindings.end() && b->second.tile == tile && c->second.count(b->second.name)) {
				selected = b->second.name; kind = "Explicitly assigned: " + selected;
			}
		}
		if (!selected.empty()) {
			const auto &g = c->second.at(selected);
			kind += g.hasHeight ? "; height authored" : "; height absent";
			kind += g.hasNormals ? "; normals authored" : "; normals absent (renderer fallback, not inherited)";
		}
		return kind;
	}
	bool canAssign(const RemasterProfile &profile, const RemasterFrame &frame) const {
		if(roomScope) {
			if(!roomGeneration || !roomStatus.empty() || counts.blocked || roomTargets.empty() || frame.area.id!=roomGeneration->room || profile.romSha256!=roomGeneration->packet.profileRomSha256)return false;
			if(!RemasterContextRoom::sameFrame(frame,roomGeneration->packet))return false;
			if(include!=preparedInclude || exclude!=preparedExclude || name!=preparedName || typeBackgrounds!=preparedTypes || profileToken(profile)!=roomProfileToken)return false;
			for(const auto &t:roomTargets) {const auto c=profile.tileContexts.find(t.tile);if(conflict(profile,t) || (!name.empty() && (c==profile.tileContexts.end() || !c->second.count(name))))return false;}
			return true;
		}
		if (!valid(frame)) return false;
		std::map<RemasterContextLocation, const Target *> owners;
		for (auto id : instances) {
			const auto &target = targets.at(id);
			const auto c = profile.tileContexts.find(target.tile);
			if (!name.empty() && (c == profile.tileContexts.end() || !c->second.count(name))) return false;
			// A durable key has only one owner. Visual clones may share it only
			// with the same canonical identity and placement transform; otherwise
			// instance-ID order would silently decide which assignment survives.
			const auto owner = owners.emplace(target.at, &target);
			if (!owner.second) {
				const auto &previous = *owner.first->second;
				if (!(previous.tile == target.tile) || previous.hFlip != target.hFlip || previous.vFlip != target.vFlip) return false;
			}
			// An unselected/stored owner must not be overwritten or erased by
			// None (or by the automatic sole-context clear path), either.
			const auto binding = profile.contextBindings.find(target.at);
			if (binding != profile.contextBindings.end() && conflict(profile,target)) return false;
		}
		if (bulk) for (const auto &entry : frame.contextLocations) {
			const auto owner=owners.find(entry.second);
			if (owner==owners.end()) continue;
			if (!entry.first || entry.first>frame.tileInstances.size()) return false;
			const auto &in=frame.tileInstances[entry.first-1]; const auto &t=*owner->second;
			if (!(in.tileId==t.tile) || in.source!=RemasterSourceType::Background || in.sourceIndex!=t.at.background ||
				in.hFlip!=t.hFlip || in.vFlip!=t.vFlip) return false;
		}
		return true;
	}
	bool assign(RemasterProfile &profile, const RemasterFrame &frame) const {
		if (!canAssign(profile, frame)) return false; // atomic validation, including mixed tile identities
		if(roomScope) {
			for(const auto &t:roomTargets) {const auto c=profile.tileContexts.find(t.tile);
				if(name.empty() || c==profile.tileContexts.end() || c->second.size()<2) profile.contextBindings.erase(t.at);
				else profile.contextBindings[t.at]={t.tile,name};}
			return true;
		}
		std::set<RemasterContextLocation> written;
		for (auto id : instances) {
			const auto &tile = frame.tileInstances[id-1].tileId;
			const auto c = profile.tileContexts.find(tile);
			const auto &at = frame.contextLocations.at(id);
			if (!written.insert(at).second) continue; // exact main/sub clones are one durable edit
			if (name.empty() || c == profile.tileContexts.end() || c->second.size() < 2) profile.contextBindings.erase(at);
			else profile.contextBindings[at] = {tile, name};
		}
		return true;
	}
};

#endif
