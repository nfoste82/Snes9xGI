#include "../runtime/profile_merge.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

static void check(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}

int main(int argc, char **argv)
{
	try
	{
		RemasterProfile base;
		base.gameTitle = "Save path";
		base.romSha256 = std::string(64, 'a');
		std::vector<RemasterProfileDiagnostic> errors;
		std::string baseline, diskText, text;
		check(S9xRemasterSerializeProfile(base, baseline, errors), "baseline");
		auto local = base, disk = base;
		local.linkLampBrightness = 300;
		RemasterProfileMergeResult result;
		check(S9xRemasterPrepareProfileSave(baseline, baseline, local, {}, false, result, text, errors), "unchanged disk save");
		check(result.conflicts.empty() && result.saved.linkLampBrightness == 300 && result.saved.schemaVersion >= 32, "fast path schema and data");
		std::string expected;
		check(S9xRemasterSerializeProfile(local, expected, errors) && text == expected, "fast path exact canonical bytes");
		local.cameraDirection = {{0, 0, 0}};
		check(!S9xRemasterPrepareProfileSave(baseline, baseline, local, {}, false, result, text, errors), "fast path validates");
		local = base; local.linkLampBrightness = 300;
		disk.originalSceneContribution = 0.2f;
		check(S9xRemasterSerializeProfile(disk, diskText, errors), "external edit");
		check(S9xRemasterPrepareProfileSave(baseline, diskText, local, {}, false, result, text, errors) &&
			result.conflicts.empty() && result.saved.linkLampBrightness == 300 && result.saved.originalSceneContribution == 0.2f, "disjoint save merge");
		disk.linkLampBrightness = 400;
		check(S9xRemasterSerializeProfile(disk, diskText, errors), "conflicting edit");
		check(S9xRemasterPrepareProfileSave(baseline, diskText, local, {}, false, result, text, errors) &&
			result.conflicts.size() == 1 && result.saved.linkLampBrightness == 400 && result.session.linkLampBrightness == 300, "keep disk retains local conflict");
		auto pending = result.conflicts;
		auto retained = result.session;
		const auto saved = text;
		check(S9xRemasterPrepareProfileSave(saved, saved, retained, pending, false, result, text, errors) &&
			result.conflicts == pending && result.saved.linkLampBrightness == 400, "unchanged disk pending conflicts cannot fast path");
		check(S9xRemasterPrepareProfileSave(saved, saved, retained, pending, true, result, text, errors) &&
			result.saved.linkLampBrightness == 300, "overwrite pending conflict");
		RemasterProfileMergePlan plan;
		check(S9xRemasterPrepareProfileSave(baseline, diskText, local, {}, false, result, text, errors, &plan) &&
			result.conflicts.size() == 1, "prepare reusable conflict plan");
		check(S9xRemasterFinishProfileMerge(plan, false, result, errors) && result.saved.linkLampBrightness == 400 &&
			result.session.linkLampBrightness == 300, "finish plan keep disk");
		check(S9xRemasterFinishProfileMerge(plan, true, result, errors) && result.saved.linkLampBrightness == 300 &&
			result.session.linkLampBrightness == 300, "reuse plan overwrite");
		disk.romSha256 = std::string(64, 'b');
		check(S9xRemasterSerializeProfile(disk, diskText, errors) &&
			!S9xRemasterPrepareProfileSave(baseline, diskText, local, {}, false, result, text, errors), "different disk game rejected");
		// Do not reject a conflict before asking which side to keep: a disk-side
		// material deletion can invalidate a local rule while overwrite is valid.
		base.materials["stone"] = RemasterMaterial{};
		check(S9xRemasterSerializeProfile(base, baseline, errors), "structured baseline");
		local = base; disk = base;
		local.materials["stone"].roughness = 0.25f;
		RemasterProfileRule rule; rule.hasTileHash = true; rule.tileId = {1, 1, 4}; rule.material = "stone";
		local.rules.push_back(rule); disk.materials.erase("stone");
		check(S9xRemasterSerializeProfile(disk, diskText, errors), "material deletion");
		check(S9xRemasterPrepareProfileSave(baseline, diskText, local, {}, false, result, text, errors, &plan) &&
			!result.conflicts.empty(), "inspect before cross-reference validation");
		check(!S9xRemasterFinishProfileMerge(plan, false, result, errors), "invalid keep-disk references rejected");
		check(S9xRemasterFinishProfileMerge(plan, true, result, errors) && result.saved.materials.count("stone"), "valid overwrite still available");
		if (argc > 1)
		{
			check(S9xRemasterLoadProfile(argv[1], base, errors), "load corpus");
			check(S9xRemasterSerializeProfile(base, baseline, errors), "serialize corpus");
			local = base; local.linkLampBrightness += 1;
			auto start = std::chrono::steady_clock::now();
			check(S9xRemasterPrepareProfileSave(baseline, baseline, local, {}, false, result, text, errors), "corpus fast save");
			auto fastEnd = std::chrono::steady_clock::now();
			check(S9xRemasterSerializeProfile(local, expected, errors) && text == expected, "complete corpus preserved");
			RemasterProfile b, d;
			auto oldStart = std::chrono::steady_clock::now();
			check(S9xRemasterParseProfile(baseline, b, errors) && S9xRemasterParseProfile(baseline, d, errors) &&
				S9xRemasterMergeProfiles(b, local, d, {}, false, result, errors, true) &&
				S9xRemasterMergeProfiles(b, local, d, {}, true, result, errors) &&
				S9xRemasterSerializeProfile(result.saved, text, errors) &&
				S9xRemasterSerializeProfile(result.saved, text, errors), "old save preparation");
			auto end = std::chrono::steady_clock::now();
			check(text == expected, "old/new corpus parity");
			std::cout << "Save preparation (" << baseline.size() << " bytes): optimized "
				<< std::chrono::duration<double, std::milli>(fastEnd-start).count() << " ms; previous "
				<< std::chrono::duration<double, std::milli>(end-oldStart).count() << " ms\n";
		}
		if (argc > 2)
		{
			check(S9xRemasterWriteValidatedProfileText(text, argv[2], errors), "atomic text write");
			std::ifstream input(argv[2], std::ios::binary);
			std::string bytes((std::istreambuf_iterator<char>(input)), {});
			check(bytes == text, "writes exact validated bytes");
			check(!S9xRemasterWriteValidatedProfileText(text, std::string(argv[2]) + "/missing", errors), "write failure reported");
		}
		std::cout << "Profile save PASS\n";
	}
	catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
