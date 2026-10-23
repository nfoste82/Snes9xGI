#include "../runtime/layer_editor.h"
#include <cstdlib>
#include <iostream>

static void require(bool value, const char *message)
{
	if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

int main()
{
	RemasterAssetMetadata source;
	source.hasHeight = source.hasOcclusion = source.hasNormals = source.hasEmission = source.hasMaterialSelectors = source.hasDoorframeMask = true;
	source.heightSampling = RemasterHeightSampling::Linear;
	source.emissionDepth = 3; source.emissionOmnidirectional = true;
	source.doorframeMask = UINT64_C(1) << 10;
	for (size_t p = 0; p < 64; p++)
	{
		source.height[p] = uint8_t(p);
		source.occlusion[p] = uint8_t(255 - p);
		source.materialSelectors[p] = std::to_string(p);
		for (size_t c = 0; c < 3; c++) source.normalXyz[p * 3 + c] = uint8_t(p * 3 + c);
		for (size_t c = 0; c < 4; c++) source.emissionRgba[p * 4 + c] = uint8_t(p * 4 + c);
	}
	for (bool horizontal : {false, true})
		for (int layer : {RemasterEditorMaterial, RemasterEditorOcclusion, RemasterEditorHeight, RemasterEditorNormal, RemasterEditorEmission, RemasterEditorDoorframe})
		{
			auto result = source;
			require(S9xRemasterFlipMetadataLayer(result, layer, horizontal), "authored layer flips");
			for (size_t p = 0; p < 64; p++)
			{
				const size_t from = horizontal ? (p / 8) * 8 + 7 - p % 8 : (7 - p / 8) * 8 + p % 8;
				require(result.height[p] == source.height[layer == RemasterEditorHeight ? from : p], "height isolation/mirror");
				require(result.occlusion[p] == source.occlusion[layer == RemasterEditorOcclusion ? from : p], "occlusion isolation/mirror");
				require(result.materialSelectors[p] == source.materialSelectors[layer == RemasterEditorMaterial ? from : p], "material isolation/mirror");
				for (size_t c = 0; c < 3; c++)
				{
					const auto value = source.normalXyz[(layer == RemasterEditorNormal ? from : p) * 3 + c];
					const bool reflect = layer == RemasterEditorNormal && c == (horizontal ? 0u : 1u);
					require(result.normalXyz[p * 3 + c] == (reflect ? 255 - value : value), "normal positions and directions reflected; other channels preserved");
				}
				for (size_t c = 0; c < 4; c++) require(result.emissionRgba[p * 4 + c] == source.emissionRgba[(layer == RemasterEditorEmission ? from : p) * 4 + c], "RGBA components preserved");
			}
			require(result.doorframeMask == (layer == RemasterEditorDoorframe ? UINT64_C(1) << (horizontal ? 13 : 50) : source.doorframeMask), "doorframe bit mirror");
			require(result.heightSampling == source.heightSampling && result.emissionDepth == 3 && result.emissionOmnidirectional, "layer options preserved");
			S9xRemasterFlipMetadataLayer(result, layer, horizontal);
			require(result.height == source.height && result.occlusion == source.occlusion && result.normalXyz == source.normalXyz && result.emissionRgba == source.emissionRgba && result.materialSelectors == source.materialSelectors && result.doorframeMask == source.doorframeMask, "double flip restores exact data");
		}
	// Geometry variants use the same array helper directly in the frontend.
	for (bool horizontal : {false, true})
	{
		auto normals = source.normalXyz;
		S9xRemasterFlipNormalLayer(normals, horizontal);
		auto metadata = source;
		S9xRemasterFlipMetadataLayer(metadata, RemasterEditorNormal, horizontal);
		require(normals == metadata.normalXyz, "variant helper matches metadata normal flip");
	}
	RemasterAssetMetadata empty;
	for (bool clockwise : {false, true})
		for (int layer : {RemasterEditorMaterial, RemasterEditorOcclusion, RemasterEditorHeight, RemasterEditorNormal, RemasterEditorEmission, RemasterEditorDoorframe})
		{
			auto original = source;
			original.height[20] = -123;
			original.emissionColorMask = (UINT64_C(1) << 10) | (UINT64_C(1) << 37);
			auto result = original;
			require(S9xRemasterRotateMetadataLayer(result, layer, clockwise), "authored layer rotates");
			for (size_t p = 0; p < 64; p++)
			{
				const size_t from = clockwise ? (7 - p % 8) * 8 + p / 8 : (p % 8) * 8 + 7 - p / 8;
				require(result.height[p] == original.height[layer == RemasterEditorHeight ? from : p], "signed height rotation/isolation");
				require(result.occlusion[p] == original.occlusion[layer == RemasterEditorOcclusion ? from : p], "occlusion rotation/isolation");
				require(result.materialSelectors[p] == original.materialSelectors[layer == RemasterEditorMaterial ? from : p], "material rotation/isolation");
				for (size_t c = 0; c < 3; c++)
				{
					const size_t channel = layer == RemasterEditorNormal && c < 2 ? 1 - c : c;
					const auto value = original.normalXyz[(layer == RemasterEditorNormal ? from : p) * 3 + channel];
					const bool negate = layer == RemasterEditorNormal && c == (clockwise ? 0u : 1u);
					require(result.normalXyz[p * 3 + c] == (negate ? 255 - value : value), "normal quarter-turn positions/directions preserve Z");
				}
				for (size_t c = 0; c < 4; c++) require(result.emissionRgba[p * 4 + c] == original.emissionRgba[(layer == RemasterEditorEmission ? from : p) * 4 + c], "rotation preserves RGBA channels");
				require(((result.emissionColorMask >> p) & 1) == ((original.emissionColorMask >> (layer == RemasterEditorEmission ? from : p)) & 1), "emission color source mask rotates with pixels");
				require(((result.doorframeMask >> p) & 1) == ((original.doorframeMask >> (layer == RemasterEditorDoorframe ? from : p)) & 1), "doorframe mask rotates");
			}
			require(result.heightSampling == original.heightSampling && result.emissionDepth == 3 && result.emissionOmnidirectional, "rotation preserves options");
			const auto restored = [&](const RemasterAssetMetadata &a) {
				return a.height == original.height && a.occlusion == original.occlusion && a.materialSelectors == original.materialSelectors && a.normalXyz == original.normalXyz && a.emissionRgba == original.emissionRgba && a.emissionColorMask == original.emissionColorMask && a.doorframeMask == original.doorframeMask;
			};
			auto inverse = result;
			S9xRemasterRotateMetadataLayer(inverse, layer, !clockwise);
			require(restored(inverse), "opposite rotations restore exact data");
			for (int turn = 1; turn < 4; turn++) S9xRemasterRotateMetadataLayer(result, layer, clockwise);
			require(restored(result), "four quarter turns restore exact data");
			require(!S9xRemasterRotateMetadataLayer(empty, layer, clockwise), "rotation never creates missing layers");
		}
	for (unsigned flip = 0; flip < 4; flip++) for (bool clockwise : {false, true})
	{
		auto result = source.normalXyz;
		S9xRemasterRotateNormalLayer(result, clockwise, flip);
		for (unsigned p = 0; p < 64; p++)
		{
			const unsigned x = p % 8, y = p / 8;
			const unsigned dx = flip & 1 ? 7 - x : x, dy = flip & 2 ? 7 - y : y;
			const unsigned sx = clockwise ? dy : 7 - dy, sy = clockwise ? 7 - dx : dx;
			const unsigned from = (flip & 2 ? 7 - sy : sy) * 8 + (flip & 1 ? 7 - sx : sx);
			require(result[p * 3] == (clockwise ? 255 - source.normalXyz[from * 3 + 1] : source.normalXyz[from * 3 + 1]), "flipped preview rotates positions in displayed direction");
			require(result[p * 3 + 1] == (clockwise ? source.normalXyz[from * 3] : 255 - source.normalXyz[from * 3]), "variant vectors rotate in gameplay space");
			require(result[p * 3 + 2] == source.normalXyz[from * 3 + 2], "variant Z preserved");
		}
	}
	for (int layer : {RemasterEditorArtwork, RemasterEditorMetadata, RemasterEditorObstacleGrouping})
		require(!S9xRemasterRotateMetadataLayer(source, layer, true), "nonpixel layers cannot rotate");
	for (const auto delta : {std::pair<int,int>{0,-1}, {0,1}, {-1,0}, {1,0}})
		for (int layer : {RemasterEditorMaterial, RemasterEditorOcclusion, RemasterEditorHeight, RemasterEditorNormal, RemasterEditorEmission, RemasterEditorDoorframe})
		{
			auto result = source;
			result.height[20] = -123;
			const auto original = result;
			require(S9xRemasterShiftMetadataLayer(result,layer,delta.first,delta.second), "authored layer shifts");
			for (int y=0;y<8;y++) for (int x=0;x<8;x++) {
				const int p=y*8+x, sx=x-delta.first, sy=y-delta.second;
				const bool inside=sx>=0 && sx<8 && sy>=0 && sy<8;
				const int from=inside ? sy*8+sx : 0;
				require(result.height[p] == (layer==RemasterEditorHeight ? (inside ? original.height[from] : 0) : original.height[p]), "signed height shift and zero edge");
				require(result.occlusion[p] == (layer==RemasterEditorOcclusion ? (inside ? original.occlusion[from] : 255) : original.occlusion[p]), "occlusion shift and default edge");
				require(result.materialSelectors[p] == (layer==RemasterEditorMaterial ? (inside ? original.materialSelectors[from] : "") : original.materialSelectors[p]), "material shift and fallback edge");
				for (int c=0;c<3;c++) require(result.normalXyz[p*3+c] == (layer==RemasterEditorNormal ? (inside ? original.normalXyz[from*3+c] : c==2 ? 255 : 128) : original.normalXyz[p*3+c]), "normal translation preserves directions and fills +Z");
				for (int c=0;c<4;c++) require(result.emissionRgba[p*4+c] == (layer==RemasterEditorEmission ? (inside ? original.emissionRgba[from*4+c] : 0) : original.emissionRgba[p*4+c]), "RGBA translation and clear edge");
				const bool expected=layer==RemasterEditorDoorframe ? inside && (original.doorframeMask & (UINT64_C(1)<<from)) : original.doorframeMask & (UINT64_C(1)<<p);
				require(bool(result.doorframeMask & (UINT64_C(1)<<p)) == expected,"mask shift without wrapping");
			}
			require(result.heightSampling==source.heightSampling && result.emissionDepth==3 && result.emissionOmnidirectional,"shift preserves options");
		}
	for (int layer=RemasterEditorArtwork;layer<=RemasterEditorDoorframe;layer++)
		require(!S9xRemasterShiftMetadataLayer(empty,layer,1,0),"shift does not create absent layers");
	require(!S9xRemasterShiftMetadataLayer(source,RemasterEditorArtwork,1,0) && !S9xRemasterShiftMetadataLayer(source,RemasterEditorMetadata,0,1),"nonpixel layers cannot shift");
	require(!S9xRemasterShiftMetadataLayer(source,RemasterEditorHeight,1,1),"diagonal shifts rejected");
	require(!S9xRemasterFlipMetadataLayer(empty, RemasterEditorHeight, true) && !empty.hasHeight, "missing layers remain missing");
	require(!S9xRemasterFlipMetadataLayer(source, RemasterEditorArtwork, true) && !S9xRemasterFlipMetadataLayer(source, RemasterEditorMetadata, false), "nonpixel layers unavailable");
	// The production Tile +/- action operates on the full selection, not focus.
	for (int delta : {-8,-1,1,8})
	{
		RemasterProfile profile;
		profile.gameTitle="Selected height steps"; profile.romSha256=std::string(64,'0');
		const RemasterTileContentId a{101,1,4}, b{102,1,4}, c{103,1,4}, missing{104,1,4};
		for (const auto &id : {a,b,c}) { profile.assets[id]=source; profile.assets[id].tileId=id; }
		profile.assets[b].height.fill(-100);
		profile.assets[a].height[0]=32767; profile.assets[a].height[1]=-32768;
		const auto before=profile;
		require(S9xRemasterStepSelectedTileHeights(profile,{a,b,a,missing},delta),"selected heights change");
		for (int p=0;p<64;p++) {
			require(profile.assets[a].height[p]==std::max(-32768,std::min(32767,int(before.assets.at(a).height[p])+delta)),"all samples adjusted once with signed clamping");
			require(profile.assets[b].height[p]==-100+delta,"second selected tile uses its own heights");
			require(profile.assets[missing].height[p]==delta,"missing height starts at zero");
		}
		require(profile.assets[c].height==before.assets.at(c).height,"unselected heights preserved");
		require(profile.assets[a].normalXyz==source.normalXyz && profile.assets[a].occlusion==source.occlusion && profile.assets[a].heightSampling==source.heightSampling,"other layers and sampling preserved");
		require(profile.assets[missing].hasHeight && profile.assets[missing].tileId==missing,"new layer has identity and authored flag");
		const auto edited=profile;
		std::string saved; std::vector<RemasterProfileDiagnostic> diagnostics; RemasterProfile reloaded;
		auto savedProfile=profile;
		// The flip/shift fixture's synthetic material selectors have no definitions.
		for (auto &entry : savedProfile.assets) {
			entry.second.hasMaterialSelectors=false; entry.second.materialSelectors={};
			entry.second.hasDoorframeMask=false; entry.second.doorframeMask=0;
		}
		const bool roundtrip=S9xRemasterSerializeProfile(savedProfile,saved,diagnostics) && S9xRemasterParseProfile(saved,reloaded,diagnostics);
		if (!roundtrip) for (const auto &diagnostic : diagnostics) std::cerr << diagnostic.message << '\n';
		require(roundtrip,"bulk height profile save/reload");
		require(reloaded.assets.at(a).height==profile.assets.at(a).height && reloaded.assets.at(b).height==profile.assets.at(b).height && reloaded.assets.at(missing).height==profile.assets.at(missing).height,"save retains every selected height layer");
		require(!S9xRemasterStepSelectedTileHeights(profile,{a,b},2),"unsupported step rejected");
		require(profile.assets[a].height==edited.assets.at(a).height,"invalid step leaves selection unchanged");
		profile=before; // Same whole-profile snapshot used by native Undo.
		require(profile.assets[a].height==before.assets.at(a).height && profile.assets[b].height==before.assets.at(b).height && !profile.assets.count(missing),"undo restores full selection and removes new layer");
		profile.assets[a].geometryByFlip=true;
		profile.assets[a].flipGeometry[1]=source;
		require(S9xRemasterStepSelectedTileHeights(profile,{a,b},delta,2),"mixed flip-specific and ordinary selection");
		require(profile.assets[a].height==before.assets.at(a).height && profile.assets[a].flipGeometry[1].height[10]==10+delta && !profile.assets[a].flipGeometry[0].hasHeight,"only selected geometry variant edited");
		profile=before;
		profile.tileContexts[a]["local"].height.fill(50);
		profile.tileContexts[a]["local"].hasHeight=true;
		profile.tileContexts[b]["local"].height.fill(-50);
		profile.tileContexts[b]["local"].hasHeight=true;
		profile.tileContexts[a]["other"].height.fill(99);
		require(S9xRemasterStepSelectedTileHeights(profile,{a,b,c,missing},delta,0,"local"),"context selection edits all available contexts");
		require(profile.tileContexts[a]["local"].height[63]==50+delta && profile.tileContexts[b]["local"].height[63]==-50+delta,"all selected contexts updated");
		require(profile.assets[a].height==before.assets.at(a).height && profile.assets[c].height==before.assets.at(c).height && !profile.assets.count(missing) && profile.tileContexts[a]["other"].height[0]==99,"context edits never fall through or change other contexts");
		profile.assets[a].height.fill(delta>0 ? 32767 : -32768);
		require(!S9xRemasterStepSelectedTileHeights(profile,{a},delta),"fully saturated selection is a no-op");
	}
	std::cout << "Layer flips, rotations, shifts and selected height steps: signed clamping, deduplication, variants, contexts, isolation and undo passed\n";
}
