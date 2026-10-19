#ifndef S9X_REMASTER_PROFILE_MERGE_H
#define S9X_REMASTER_PROFILE_MERGE_H

#include "profile.h"
#include <set>

// Compare canonical saved data, not timestamps, struct padding or parser line
// numbers. Assets are indivisible records; settings merge independently. Rules
// remain one ordered record; obstacle members remain with their owning group.
namespace RemasterProfileMerge
{
	using Records = std::map<std::string, std::string>;
	inline Records Split(const std::string &text)
	{
		Records records;
		std::istringstream input(text);
		std::string line, header, block, key;
		auto flush = [&] { if (!key.empty()) records[key] = block; block.clear(); key.clear(); };
		while (std::getline(input, line))
		{
			if (line.empty() || line.compare(0, 17, "schema_version = ") == 0) continue;
			if (line.front() == '[')
			{
				if (line == "[[obstacle_groups.members]]") { block += line + '\n'; continue; }
				if (line == "[[rules]]" && header == line) { block += line + '\n'; continue; }
				flush(); header = line;
				if (header == "[[rules]]") key = "7:rules";
				else if (header.compare(0, 11, "[materials.") == 0) key = "3:" + header;
				else if (header.compare(0, 14, "[asset_groups.") == 0) key = "4:" + header;
				else if (header.compare(0, 13, "[area_lights.") == 0) key = "2:" + header;
				block = line + '\n';
				continue;
			}
			if (header == "[game]" || header == "[lighting_space]")
			{
				const auto equal = line.find(" = ");
				records[(header == "[game]" ? "0:" : "1:") + line.substr(0, equal)] = line + '\n';
				continue;
			}
			if (header == "[[assets]]" && key.empty()) key = "6:" + line;
			if (header == "[[tile_contexts]]") {
				if (key.empty()) key = "8:" + line;
				else if (line.compare(0, 7, "name = ") == 0) key += ":" + line;
			}
			if (header == "[[context_bindings]]") {
				if (key.empty()) key="9:";
				if (line.compare(0,7,"area = ")==0 || line.compare(0,13,"background = ")==0 ||
					line.compare(0,4,"x = ")==0 || line.compare(0,4,"y = ")==0) key+=line+":";
			}
			if (header == "[[obstacle_groups]]" && key.empty()) key = "5:" + line;
			block += line + '\n';
		}
		flush();
		return records;
	}

	inline std::string Join(const Records &records, uint32_t schema, const std::string &order)
	{
		std::string text = "schema_version = " + std::to_string(schema) + "\n";
		std::vector<Records::const_iterator> ordered;
		for (auto it = records.begin(); it != records.end(); ++it) ordered.push_back(it);
		std::stable_sort(ordered.begin(), ordered.end(), [&](auto a, auto b) {
			if (a->first.compare(0, 2, "5:") == 0 && b->first.compare(0, 2, "5:") == 0)
				return order.find(a->first.substr(2)) < order.find(b->first.substr(2));
			return a->first < b->first;
		});
		char section = 0;
		for (const auto it : ordered)
		{
			const auto &record = *it;
			const char next = record.first.front();
			if (next != section && next <= '1') text += next == '0' ? "\n[game]\n" : "\n[lighting_space]\n";
			if (next > '1') text += '\n';
			text += record.second;
			section = next;
		}
		return text;
	}

	inline std::string Label(const std::string &key)
	{
		if (key.compare(0, 2, "6:") == 0) return key.substr(2);
		if (key.compare(0, 2, "5:") == 0) return "obstacle group " + key.substr(2);
		if (key.compare(0, 2, "0:") == 0) return "game: " + key.substr(2);
		if (key.compare(0, 2, "1:") == 0) return "lighting setting: " + key.substr(2);
		return key.substr(2);
	}
}

struct RemasterProfileMergeResult
{
	RemasterProfile saved, session;
	std::set<std::string> conflicts;
};

inline bool S9xRemasterMergeProfiles(const RemasterProfile &base, const RemasterProfile &session,
	const RemasterProfile &disk, const std::set<std::string> &pendingConflicts, bool overwriteConflicts,
	RemasterProfileMergeResult &result, std::vector<RemasterProfileDiagnostic> &diagnostics, bool inspectConflictsOnly = false)
{
	using namespace RemasterProfileMerge;
	if (base.romSha256 != disk.romSha256 || session.romSha256 != disk.romSha256)
	{
		diagnostics = {{0, "Profile game identity changed on disk; reload the intended profile before saving."}};
		return false;
	}
	std::string b, s, d;
	if (!S9xRemasterSerializeProfile(base, b, diagnostics, false) ||
		!S9xRemasterSerializeProfile(session, s, diagnostics, false) ||
		!S9xRemasterSerializeProfile(disk, d, diagnostics, false)) return false;
	const auto baseline = Split(b), local = Split(s), remote = Split(d);
	std::set<std::string> keys;
	for (const auto *records : {&baseline, &local, &remote}) for (const auto &entry : *records) keys.insert(entry.first);
	Records saved, retained;
	result.conflicts.clear();
	for (const auto &key : keys)
	{
		const auto bi = baseline.find(key), si = local.find(key), di = remote.find(key);
		auto equal = [](auto a, const auto &am, auto c, const auto &cm) {
			return a == am.end() ? c == cm.end() : c != cm.end() && a->second == c->second;
		};
		const bool localChanged = !equal(si, local, bi, baseline);
		const bool diskChanged = !equal(di, remote, bi, baseline);
		const bool same = equal(si, local, di, remote);
		const bool conflict = !same && ((localChanged && diskChanged) || pendingConflicts.count(key));
		if (conflict) result.conflicts.insert(key);
		const bool useSession = conflict ? overwriteConflicts : localChanged;
		const auto &chosen = useSession ? local : remote;
		const auto at = chosen.find(key);
		if (at != chosen.end()) saved[key] = at->second;
		const auto &sessionChosen = conflict && !overwriteConflicts ? local : chosen;
		const auto pending = sessionChosen.find(key);
		if (pending != sessionChosen.end()) retained[key] = pending->second;
	}
	// Serialization may promote a profile for newly authored features. Use the
	// emitted schema, not the pre-serialization struct's version.
	const auto schema = static_cast<uint32_t>(std::max({std::stoul(b.substr(17)), std::stoul(s.substr(17)), std::stoul(d.substr(17))}));
	if (inspectConflictsOnly) return true;
	// Validate the complete merged profile, including cross-record references.
	return S9xRemasterParseProfile(Join(saved, schema, d + s), result.saved, diagnostics) &&
		S9xRemasterParseProfile(Join(retained, schema, d + s), result.session, diagnostics);
}

#endif
