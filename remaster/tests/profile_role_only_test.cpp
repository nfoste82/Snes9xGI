#include "../runtime/profile.h"
#include <cassert>
int main()
{
	RemasterProfile p;
	p.schemaVersion=18;
	p.romSha256=std::string(64,'0');
	p.gameTitle="Classification-only test";
	const RemasterTileContentId id={UINT64_C(0x4952263cea5a838f),1,4};
	RemasterAssetMetadata a;a.tileId=id;a.tileRole=RemasterTileRole::Wall;
	p.assets[id]=a;
	std::vector<RemasterProfileDiagnostic> errors;std::string text;
	assert(S9xRemasterSerializeProfile(p,text,errors));
	RemasterProfile parsed;assert(S9xRemasterParseProfile(text,parsed,errors));
	const auto &v=parsed.assets.at(id);
	assert(v.tileRole==RemasterTileRole::Wall && !v.hasHeight && !v.hasNormals && !v.hasOcclusion && !v.hasEmission);
	p.assets[id].tileRole=RemasterTileRole::Unknown;
	assert(!S9xRemasterSerializeProfile(p,text,errors)); // empty entry still invalid
}
