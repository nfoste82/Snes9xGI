#include "layer_editor.h"
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
	std::cout << "Layer flips and shifts: axes, normals, channels, signed heights, edge defaults, isolation and missing data passed\n";
}
