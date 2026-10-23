#include "../runtime/profile.h"
#include "../runtime/profile_merge.h"
#include <cstdlib>
#include <iostream>

static void require(bool ok, const char *message)
{
	if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main()
{
	RemasterProfile p;
	p.schemaVersion = 27; p.gameTitle = "Contexts"; p.romSha256 = std::string(64, 'a');
	std::string text; std::vector<RemasterProfileDiagnostic> d;
	require(S9xRemasterSerializeProfile(p, text, d), "legacy serialization");
	require(text.find("tile_contexts") == std::string::npos, "zero serialized context bytes by default");
	const auto legacy = text;
	RemasterAssetMetadata authored;
	authored.hasHeight = authored.hasNormals = true;
	authored.height.fill(-12); authored.normalXyz.fill(64);
	authored.heightSampling = RemasterHeightSampling::Linear;
	authored.normalIgnoreTileTransform = authored.directLightingOppositeFacing = true;
	const auto first = S9xRemasterNewTileContext(authored, true);
	require(first.hasHeight && first.hasNormals && first.height == authored.height && first.normalXyz == authored.normalXyz &&
		first.heightSampling == authored.heightSampling && first.normalIgnoreTileTransform && first.directLightingOppositeFacing,
		"first context preserves existing geometry and options");
	const auto additional = S9xRemasterNewTileContext(authored, false);
	require(!additional.hasHeight && !additional.hasNormals && !additional.normalIgnoreTileTransform && !additional.directLightingOppositeFacing,
		"additional contexts start independent and empty");
	require(authored.height[0] == -12 && authored.normalXyz[0] == 64, "default geometry is not modified");
	RemasterTileContentId id; id.hashVersion=1; id.bitDepth=4; id.hash=123;
	p.tileContexts[id]["Wall"] = {};
	p.tileContexts[id]["Floor"] = {};
	auto &wall=p.tileContexts[id]["Wall"];
	wall.hasHeight=wall.hasNormals=true; wall.height.fill(-16); wall.normalXyz.fill(127);
	wall.heightSampling=RemasterHeightSampling::Linear; wall.normalIgnoreTileTransform=true;
	require(!p.tileContexts[id]["Floor"].hasHeight && !p.tileContexts[id]["Floor"].hasNormals, "new context starts empty");
	require(S9xRemasterSerializeProfile(p,text,d), "context serialization");
	RemasterProfile q;
	require(S9xRemasterParseProfile(text,q,d), "context parse");
	require(q.schemaVersion==28 && q.tileContexts[id].size()==2, "contexts roundtrip");
	RemasterProfileMergeResult merged;
	auto disk = q; disk.tileContexts[id]["Floor"].hasHeight = true;
	auto session = q; session.tileContexts[id]["Wall"].height[0] = -24;
	require(S9xRemasterMergeProfiles(q,session,disk,{},false,merged,d),"context-aware profile merge");
	require(merged.saved.tileContexts[id]["Wall"].height[0]==-24 && merged.saved.tileContexts[id]["Floor"].hasHeight,"independent contexts survive merge/save");
	require(q.tileContexts[id]["Wall"].height==wall.height && q.tileContexts[id]["Wall"].normalXyz==wall.normalXyz && q.tileContexts[id]["Wall"].normalIgnoreTileTransform, "context geometry preserved");
	std::string again; require(S9xRemasterSerializeProfile(q,again,d) && text==again,"stable serialization");
	require(!S9xRemasterValidContextName("None") && !S9xRemasterValidContextName("NONE") && !S9xRemasterValidContextName("") && S9xRemasterValidContextName("Castle Wall"), "reserved and valid names");
	const auto record="\n[[tile_contexts]]\ntile_hash = \"v1:4bpp:000000000000007b\"\nname = \"None\"\n";
	require(!S9xRemasterParseProfile(legacy+record,q,d),"reserved name and old schema rejected");
	require(!S9xRemasterParseProfile(text+"\n[[tile_contexts]]\ntile_hash = \"v1:4bpp:000000000000007b\"\nname = \"Wall\"\n",q,d),"duplicate rejected");
	p.tileContexts.clear(); require(S9xRemasterSerializeProfile(p,text,d) && text==legacy,"removing all contexts restores no-payload profile");
	std::cout << "Tile contexts: sparse storage, independent geometry, signed heights, validation and roundtrip passed\n";
}
