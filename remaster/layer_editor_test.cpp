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
				for (size_t c = 0; c < 3; c++) require(result.normalXyz[p * 3 + c] == source.normalXyz[(layer == RemasterEditorNormal ? from : p) * 3 + c], "normal components preserved");
				for (size_t c = 0; c < 4; c++) require(result.emissionRgba[p * 4 + c] == source.emissionRgba[(layer == RemasterEditorEmission ? from : p) * 4 + c], "RGBA components preserved");
			}
			require(result.doorframeMask == (layer == RemasterEditorDoorframe ? UINT64_C(1) << (horizontal ? 13 : 50) : source.doorframeMask), "doorframe bit mirror");
			require(result.heightSampling == source.heightSampling && result.emissionDepth == 3 && result.emissionOmnidirectional, "layer options preserved");
			S9xRemasterFlipMetadataLayer(result, layer, horizontal);
			require(result.height == source.height && result.occlusion == source.occlusion && result.normalXyz == source.normalXyz && result.emissionRgba == source.emissionRgba && result.materialSelectors == source.materialSelectors && result.doorframeMask == source.doorframeMask, "double flip restores exact data");
		}
	RemasterAssetMetadata empty;
	require(!S9xRemasterFlipMetadataLayer(empty, RemasterEditorHeight, true) && !empty.hasHeight, "missing layers remain missing");
	require(!S9xRemasterFlipMetadataLayer(source, RemasterEditorArtwork, true) && !S9xRemasterFlipMetadataLayer(source, RemasterEditorMetadata, false), "nonpixel layers unavailable");
	std::cout << "Layer flips: axes, channel preservation, isolation, missing data and round trips passed\n";
}
