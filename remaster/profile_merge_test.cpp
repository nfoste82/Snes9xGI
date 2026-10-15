#include "profile_merge.h"
#include <iostream>
#include <stdexcept>

static void check(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}

int main(int argc, char **argv)
{
	try
	{
		RemasterProfile base;
		base.gameTitle = "Three-way merge";
		base.romSha256 = std::string(64, 'a');
		const RemasterTileContentId a{1, 1, 4}, b{2, 1, 4}, added{3, 1, 4};
		for (auto id : {a, b})
		{
			auto &asset = base.assets[id]; asset.tileId = id;
			asset.hasHeight = true; asset.height.fill(0);
			asset.hasOcclusion = true; asset.occlusion.fill(0);
		}
		std::vector<RemasterProfileDiagnostic> errors;
		RemasterProfileMergeResult result;
		auto merge = [&](const RemasterProfile &baseline, const RemasterProfile &local,
			const RemasterProfile &disk, const std::set<std::string> &pending = {}, bool overwrite = false) {
			check(S9xRemasterMergeProfiles(baseline, local, disk, pending, overwrite, result, errors),
				 errors.empty() ? "merge failed" : errors.front().message.c_str());
		};
		auto session = base, disk = base;
		session.assets[a].height.fill(7); disk.assets[b].height.fill(16);
		merge(base, session, disk);
		check(result.conflicts.empty() && result.saved.assets[a].height[0] == 7 && result.saved.assets[b].height[0] == 16,
			"disjoint tiles merge");
		check(result.session.assets[b].height[0] == 16 && result.saved.assets[a].occlusion[0] == 0,
			"disk edits adopted without coverage loss");
		disk.assets[a].height.fill(32);
		merge(base, session, disk);
		check(result.conflicts.size() == 1 && result.saved.assets[a].height[0] == 32 && result.session.assets[a].height[0] == 7,
			"keep disk preserves pending session tile");
		auto saved = result.saved, retained = result.session;
		auto pending = result.conflicts;
		// Crucial: disk is now the new baseline, yet a second Save must prompt.
		merge(saved, retained, saved, pending);
		check(result.conflicts == pending && result.saved.assets[a].height[0] == 32 && result.session.assets[a].height[0] == 7,
			"repeat keep disk does not overwrite silently");
		merge(saved, retained, saved, pending, true);
		check(result.saved.assets[a].height[0] == 7 && result.session.assets[b].height[0] == 16,
			"later overwrite saves retained edit only");
		merge(base, disk, disk);
		check(result.conflicts.empty(), "identical simultaneous edits are not conflicts");
		session = base; disk = base;
		session.assets[a].height[0] = -256; disk.assets[a].occlusion[0] = 255;
		merge(base, session, disk);
		check(result.conflicts.size() == 1 && result.saved.assets[a].height[0] == 0 &&
			result.session.assets[a].occlusion[0] == 0, "same tile different layers remain a whole-record conflict");
		session = base; disk = base;
		session.assets.erase(a); disk.assets[a].height.fill(3);
		merge(base, session, disk);
		check(result.conflicts.size() == 1 && result.saved.assets.count(a) && !result.session.assets.count(a), "delete/edit conflict retained");
		saved = result.saved; retained = result.session; pending = result.conflicts;
		merge(saved, retained, saved, pending, true);
		check(!result.saved.assets.count(a), "pending deletion can be saved later");
		session = base; disk = base;
		session.assets[added] = base.assets[a]; session.assets[added].tileId = added;
		disk.assets.erase(b);
		merge(base, session, disk);
		check(result.conflicts.empty() && result.saved.assets.count(added) && !result.saved.assets.count(b), "addition and unrelated deletion");
		session = base; disk = base;
		session.originalSceneContribution = 0.2f; disk.lightingCoordinateScale = 19;
		session.areaLights["room_060"].brightness = 0.3f; disk.areaLights["room_061"].brightness = 0.4f;
		merge(base, session, disk);
		check(result.conflicts.empty() && result.saved.originalSceneContribution == 0.2f && result.saved.lightingCoordinateScale == 19 &&
			result.saved.areaLights.size() == 2, "independent settings and area lights merge");
		disk.originalSceneContribution = 0.4f;
		merge(base, session, disk);
		check(result.conflicts.size() == 1 && result.session.originalSceneContribution == 0.2f &&
			result.saved.originalSceneContribution == 0.4f, "setting conflict retains session choice");
		disk.romSha256 = std::string(64, 'b');
		check(!S9xRemasterMergeProfiles(base, session, disk, {}, true, result, errors), "different game cannot merge");
		// Map records and ordered rules retain cross-record references. Removing a
		// referenced material concurrently must reject the complete merged result.
		RemasterProfile structured = base;
		structured.materials["stone"] = RemasterMaterial{};
		RemasterAssetGroup group; group.tileIds = {a}; group.material = "stone";
		structured.assetGroups["walls"] = group;
		RemasterProfileRule rule; rule.hasAssetGroup = true; rule.assetGroup = "walls";
		rule.material = "stone"; structured.rules.push_back(rule);
		RemasterObstacleGroup obstacle; obstacle.name = "fixture";
		RemasterObstacleMember member; member.tileId = b; member.supportContact = true;
		obstacle.members.push_back(member); structured.obstacleGroups.push_back(obstacle);
		session = structured; disk = structured;
		session.materials["stone"].roughness = 0.25f;
		disk.assetGroups["walls"].tileIds.push_back(b);
		disk.obstacleGroups[0].members[0].z = -8;
		merge(structured, session, disk);
		check(result.conflicts.empty() && result.saved.materials.at("stone").roughness == 0.25f &&
			result.saved.assetGroups.at("walls").tileIds.size() == 2 && result.saved.obstacleGroups[0].members[0].z == -8 &&
			result.saved.rules.size() == 1, "materials, groups, obstacle members and rules preserved");
		session = structured; disk = structured;
		session.rules[0].material = "missing";
		check(!S9xRemasterMergeProfiles(structured, session, disk, {}, false, result, errors), "invalid merged references rejected");
		// Full-schema identity inventory: catches omitted, reordered or truncated
		// obstacle members, rules, materials, variants and future serialized fields.
		if (argc > 1)
		{
			RemasterProfile corpus;
			check(S9xRemasterLoadProfile(argv[1], corpus, errors), "load complete profile");
			merge(corpus, corpus, corpus);
			std::string expected, actual;
			check(S9xRemasterSerializeProfile(corpus, expected, errors) &&
				S9xRemasterSerializeProfile(result.saved, actual, errors) && expected == actual,
				"complete profile no-op merge preserves every serialized field");
		}
		std::cout << "Profile merge: disjoint edits, conflict retry/overwrite, additions/deletions, settings, complete-schema preservation PASS\n";
		return 0;
	}
	catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
