/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

/***********************************************************************************
  SNES9X for Mac OS (c) Copyright John Stiles

  Snes9x for Mac OS X

  (c) Copyright 2001 - 2011  zones
  (c) Copyright 2002 - 2005  107
  (c) Copyright 2002         PB1400c
  (c) Copyright 2004         Alexander and Sander
  (c) Copyright 2004 - 2005  Steven Seeger
  (c) Copyright 2005         Ryan Vogt
  (c) Copyright 2019         Michael Donald Buckley
 ***********************************************************************************/

#import <Cocoa/Cocoa.h>

#include "snes9x.h"
#include "cheats.h"
#include "memmap.h"
#include "apu.h"
#include "display.h"
#include "blit.h"
#include "remaster/remaster.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sys/time.h>
#include <unordered_map>

#include "mac-prefix.h"
#include "mac-os.h"
#include "mac-screenshot.h"
#include "mac-render.h"

typedef struct
{
	vector_float3 position;
	float radius;
	float intensity;
	vector_float3 color;
} RemasterGpuLight;

typedef struct
{
	uint32_t width;
	uint32_t height;
	uint32_t view;
	uint32_t lightCount;
	uint32_t passIndex;
	uint32_t diagnosticStage;
	float indirectRoughness;
	float originalSceneContribution;
	uint32_t sampleIndex;
	uint32_t sampleCount;
	uint32_t randomSeed;
} RemasterLightingUniforms;

struct RemasterRadianceMetrics
{
	double sum[3] = {};
	float peak[3] = {};
	size_t nonzeroPixels = 0;
	size_t significantPixels = 0;
	size_t minX = SIZE_MAX;
	size_t minY = SIZE_MAX;
	size_t maxX = 0;
	size_t maxY = 0;
};

static float RemasterHalfToFloat (uint16_t bits)
{
	__fp16 value;
	static_assert(sizeof(value) == sizeof(bits), "half-float size");
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}

static RemasterRadianceMetrics MeasureRemasterRadiance (id<MTLTexture> texture)
{
	RemasterRadianceMetrics metrics;
	if (!texture || texture.pixelFormat != MTLPixelFormatRGBA16Float)
		return metrics;
	const size_t width = texture.width;
	const size_t height = texture.height;
	std::vector<uint16_t> pixels(width * height * 4);
	[texture getBytes:pixels.data() bytesPerRow:width * 4 * sizeof(uint16_t)
		fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
	for (size_t pixel = 0; pixel < width * height; pixel++)
	{
		bool nonzero = false;
		float maximumChannel = 0.0f;
		for (size_t channel = 0; channel < 3; channel++)
		{
			const float value = RemasterHalfToFloat(pixels[pixel * 4 + channel]);
			if (!std::isfinite(value) || value <= 0.0f)
				continue;
			metrics.sum[channel] += value;
			metrics.peak[channel] = std::max(metrics.peak[channel], value);
			maximumChannel = std::max(maximumChannel, value);
			nonzero = true;
		}
		metrics.nonzeroPixels += nonzero;
		if (maximumChannel >= 1.0f / 4096.0f)
		{
			const size_t x = pixel % width;
			const size_t y = pixel / width;
			metrics.significantPixels++;
			metrics.minX = std::min(metrics.minX, x);
			metrics.minY = std::min(metrics.minY, y);
			metrics.maxX = std::max(metrics.maxX, x);
			metrics.maxY = std::max(metrics.maxY, y);
		}
	}
	return metrics;
}

static void LogRemasterRadianceMetrics (const char *label, uint32_t bounce,
	const RemasterRadianceMetrics &metrics, const RemasterRadianceMetrics *previous = nullptr)
{
	const double delta = previous ?
		(metrics.sum[0] + metrics.sum[1] + metrics.sum[2]) -
		(previous->sum[0] + previous->sum[1] + previous->sum[2]) : 0.0;
	const size_t minX = metrics.significantPixels ? metrics.minX : 0;
	const size_t minY = metrics.significantPixels ? metrics.minY : 0;
	NSLog(@"Remaster GI %s %u: sum=(%.9g, %.9g, %.9g) peak=(%.9g, %.9g, %.9g) nonzero=%zu significant=%zu bounds=(%zu,%zu)-(%zu,%zu) delta=%.9g",
		label, bounce, metrics.sum[0], metrics.sum[1], metrics.sum[2], metrics.peak[0], metrics.peak[1],
		metrics.peak[2], metrics.nonzeroPixels, metrics.significantPixels, minX, minY, metrics.maxX, metrics.maxY, delta);
}

static void S9xInitMetal (void);
static void S9xDeinitMetal(void);
static bool S9xPutImageMetal (int, int, const uint16 *, size_t, const uint32_t *, size_t,
	const uint8_t *, size_t, RemasterDebugMode, const uint8_t * = nullptr,
	const uint8_t * = nullptr, const uint8_t * = nullptr, const float * = nullptr, const uint8_t * = nullptr,
	const uint8_t * = nullptr,
	const std::vector<RemasterGpuLight> * = nullptr, bool = false,
	RemasterLightingView = RemasterLightingView::Composite, uint8_t = 0, float = 1.0f,
	float = 0.65f, uint8_t = 1, bool = true);

static int					whichBuf          = 0;
static int					textureNum        = 0;
static int					prevBlitWidth, prevBlitHeight;
static int					imageWidth[2], imageHeight[2];
static int					nx                = 2;

typedef struct
{
    vector_float2 position;
    vector_float2 textureCoordinate;
} MetalVertex;

@interface MetalLayerDelegate: NSObject<CALayerDelegate, NSViewLayerContentScaleDelegate>
@end

@implementation MetalLayerDelegate
- (BOOL)layer:(CALayer *)layer shouldInheritContentsScale:(CGFloat)newScale fromWindow:(NSWindow *)window
{
	return YES;
}

@end

CAMetalLayer    			*metalLayer = nil;
MetalLayerDelegate			*layerDelegate = nil;
id<MTLDevice>   			metalDevice = nil;
id<MTLTexture>  			metalTexture = nil;
id<MTLCommandQueue>			metalCommandQueue = nil;
id<MTLRenderPipelineState>	metalPipelineState = nil;
id<MTLComputePipelineState>	remasterLightingPipelineState = nil;
id<MTLComputePipelineState>	remasterIndirectPipelineState = nil;
id<MTLComputePipelineState>	remasterCompositePipelineState = nil;
id<MTLComputePipelineState>	remasterSampleAccumulationPipelineState = nil;
id<MTLComputePipelineState>	remasterHighlightPipelineState = nil;
static std::atomic<bool> liveRemasterPresentationEnabled(false);
static std::atomic<RemasterLightingView> liveRemasterLightingView(RemasterLightingView::Composite);
static id<MTLDevice> remasterTextureDevice = nil;
static NSUInteger remasterTextureWidth = 0;
static NSUInteger remasterTextureHeight = 0;
static id<MTLTexture> remasterOcclusionTexture = nil;
static id<MTLTexture> remasterEmissionTexture = nil;
static id<MTLTexture> remasterHeightTexture = nil;
static id<MTLTexture> remasterParticipationTexture = nil;
static id<MTLTexture> remasterOppositeFacingTexture = nil;
static id<MTLTexture> remasterSurfaceTexture = nil;
static id<MTLTexture> remasterOutputTexture = nil;
static id<MTLTexture> remasterDirectTexture = nil;
static id<MTLTexture> remasterBounceTextures[2] = {};
static id<MTLTexture> remasterIndirectTextures[2] = {};
static id<MTLTexture> remasterSampleMeanTextures[2] = {};

void InitGraphics (void)
{
	if (!S9xBlitFilterInit()      |
		!S9xBlit2xSaIFilterInit() |
		!S9xBlitHQ2xFilterInit()  |
		!S9xBlitNTSCFilterInit())
		QuitWithFatalError(@"render 02");

	switch (videoMode)
	{
		default:
		case VIDEOMODE_NTSC_C:
		case VIDEOMODE_NTSC_TV_C:
			S9xBlitNTSCFilterSet(&snes_ntsc_composite);
			break; 

		case VIDEOMODE_NTSC_S:
		case VIDEOMODE_NTSC_TV_S:
			S9xBlitNTSCFilterSet(&snes_ntsc_svideo);
			break; 

		case VIDEOMODE_NTSC_R:
		case VIDEOMODE_NTSC_TV_R:
			S9xBlitNTSCFilterSet(&snes_ntsc_rgb);
			break; 

		case VIDEOMODE_NTSC_M:
		case VIDEOMODE_NTSC_TV_M:
			S9xBlitNTSCFilterSet(&snes_ntsc_monochrome);
			break;
	}
}

void DeinitGraphics (void)
{
	S9xBlitNTSCFilterDeinit();
	S9xBlitHQ2xFilterDeinit();
	S9xBlit2xSaIFilterDeinit();
	S9xBlitFilterDeinit();
}

void DrawFreezeDefrostScreen (uint8 *draw)
{
	const int w = SNES_WIDTH << 1, h = SNES_HEIGHT << 1;
	S9xPutImageMetal(w, h, (uint16 *)draw, w, nullptr, 0, nullptr, 0, RemasterDebugMode::Original);
}

bool DrawRemasterFrame (const RemasterFrame &frame, RemasterDebugMode debugMode,
	const std::vector<RemasterTileContentId> *selectedTiles, bool lighting, RemasterLightingView lightingView)
{
	if (frame.width > INT_MAX || frame.height > INT_MAX ||
		!S9xRemasterFrameHasPixelData(frame, frame.width, frame.height))
		return false;
	// Surface diagnostics are presentation-only, never lighting albedo.
	if (lighting)
		debugMode = RemasterDebugMode::Original;
	std::vector<uint32_t> owners;
	owners.reserve(frame.mainPixels.size());
	for (const RemasterFramePixel &pixel : frame.mainPixels)
		owners.push_back(pixel.owner);
	std::vector<uint8_t> highlights;
	if (selectedTiles && !selectedTiles->empty())
	{
		highlights.assign(frame.mainPixels.size(), 0);
		for (const RemasterTileContentId &tileId : *selectedTiles)
			for (uint32_t offset : S9xRemasterFrameOccurrences(frame, tileId))
				highlights[offset] = 1;
	}
	std::vector<uint8_t> lightingField(frame.mainPixels.size() * 2, 0);
	std::vector<uint8_t> emissionField(frame.mainPixels.size() * 4, 0);
	std::vector<uint8_t> heightField(frame.mainPixels.size() * 2, 0);
	std::vector<uint8_t> participationField(frame.mainPixels.size() * 2, 255);
	std::vector<uint8_t> oppositeFacingField(frame.mainPixels.size(), 0);
	std::vector<float> surfaceField(frame.mainPixels.size() * 4, 0.0f);
	if (lighting && frame.schemaVersion >= 5)
	{
		std::vector<const RemasterFrameAssetMetadata *> instanceMetadata(frame.tileInstances.size(), nullptr);
		for (size_t i = 0; i < frame.tileInstances.size(); i++)
			instanceMetadata[i] = S9xRemasterFrameMetadataForTile(frame, frame.tileInstances[i].tileId);
		std::unordered_map<std::string, const RemasterFrameMaterial *> materials;
		for (const RemasterFrameMaterial &material : frame.materials)
			materials.emplace(material.name, &material);
		for (size_t i = 0; i < frame.mainPixels.size(); i++)
		{
			const RemasterFramePixel &pixel = frame.mainPixels[i];
			if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size() || pixel.tilePixel >= 64)
				continue;
			const RemasterFrameTileInstance &instance = frame.tileInstances[pixel.instanceId - 1];
			const RemasterFrameAssetMetadata *metadata = instanceMetadata[pixel.instanceId - 1];
			const std::string *materialName = nullptr;
			if (metadata && metadata->hasMaterialSelectors && !metadata->materialSelectors[pixel.tilePixel].empty())
				materialName = &metadata->materialSelectors[pixel.tilePixel];
			else if (!instance.material.empty())
				materialName = &instance.material;
			const auto materialEntry = materialName ? materials.find(*materialName) : materials.end();
			const RemasterFrameMaterial *material = materialEntry == materials.end() ? nullptr : materialEntry->second;
			if (material && material->surfaceClass == RemasterSurfaceClass::UserInterface)
				participationField[i * 2] = participationField[i * 2 + 1] = 0;
			else if (material && !material->receivesGi)
				participationField[i * 2 + 1] = 0;
			if (metadata && metadata->hasOcclusion)
			{
				lightingField[i * 2] = metadata->occlusion[pixel.tilePixel];
				lightingField[i * 2 + 1] = 255;
			}
			if (metadata && metadata->directLightingOppositeFacing)
				oppositeFacingField[i] = 255;
			if (metadata && metadata->hasEmission)
				std::copy(metadata->emissionRgba.begin() + pixel.tilePixel * 4,
					metadata->emissionRgba.begin() + pixel.tilePixel * 4 + 4, emissionField.begin() + i * 4);
			if (metadata && metadata->hasHeight)
			{
				heightField[i * 2] = metadata->height[pixel.tilePixel];
				heightField[i * 2 + 1] = 255;
			}
		}
		for (uint32_t y = 0; y < frame.height; y++)
			for (uint32_t x = 0; x < frame.width; x++)
			{
				const size_t i = static_cast<size_t>(y) * frame.width + x;
				const float center = heightField[i * 2] / 255.0f * frame.lightingCoordinateScale;
				const size_t left = static_cast<size_t>(y) * frame.width + (x ? x - 1 : x);
				const size_t right = static_cast<size_t>(y) * frame.width + std::min(frame.width - 1, x + 1);
				const size_t top = static_cast<size_t>(y ? y - 1 : y) * frame.width + x;
				const size_t bottom = static_cast<size_t>(std::min(frame.height - 1, y + 1)) * frame.width + x;
				const float dx = (heightField[right * 2] - heightField[left * 2]) / 255.0f * frame.lightingCoordinateScale;
				const float dy = (heightField[bottom * 2] - heightField[top * 2]) / 255.0f * frame.lightingCoordinateScale;
				const float length = std::sqrt(dx * dx + dy * dy + 4.0f);
				surfaceField[i * 4] = center;
				const RemasterFramePixel &pixel = frame.mainPixels[i];
				const RemasterFrameAssetMetadata *metadata = nullptr;
				const RemasterFrameTileInstance *instance = nullptr;
				if (pixel.instanceId && pixel.instanceId <= frame.tileInstances.size() && pixel.tilePixel < 64)
				{
					instance = &frame.tileInstances[pixel.instanceId - 1];
					metadata = instanceMetadata[pixel.instanceId - 1];
				}
				if (metadata && metadata->hasNormals)
				{
					const size_t offset = pixel.tilePixel * 3;
					float nx = metadata->normalXyz[offset] / 127.5f - 1.0f;
					float ny = metadata->normalXyz[offset + 1] / 127.5f - 1.0f;
					float nz = metadata->normalXyz[offset + 2] / 127.5f - 1.0f;
					S9xRemasterTransformNormalForTileInstance(*instance, nx, ny, nz);
					const float normalLength = std::sqrt(nx * nx + ny * ny + nz * nz);
					surfaceField[i * 4 + 1] = normalLength > 0.0001f ? nx / normalLength : 0.0f;
					surfaceField[i * 4 + 2] = normalLength > 0.0001f ? ny / normalLength : 0.0f;
					surfaceField[i * 4 + 3] = normalLength > 0.0001f ? nz / normalLength : 1.0f;
				}
				else
				{
					surfaceField[i * 4 + 1] = -dx / length;
					surfaceField[i * 4 + 2] = -dy / length;
					surfaceField[i * 4 + 3] = 2.0f / length;
				}
			}
	}
	std::vector<RemasterGpuLight> lights;
	if (lighting)
		for (const RemasterFrameLight &light : S9xRemasterFrameEmissionLights(frame))
			lights.push_back({ { light.x, light.y, light.z }, light.radius, light.intensity,
				{ light.red, light.green, light.blue } });
	return S9xPutImageMetal(static_cast<int>(frame.width), static_cast<int>(frame.height),
		frame.originalRgb555.data(), frame.width, owners.data(), frame.width,
		highlights.empty() ? nullptr : highlights.data(), frame.width, debugMode,
		lightingField.data(), emissionField.data(), heightField.data(), surfaceField.data(), participationField.data(),
		oppositeFacingField.data(), &lights,
		lighting && frame.schemaVersion >= 5,
		lightingView, frame.indirectBounceCount, frame.indirectRoughness, frame.originalSceneContribution,
		frame.samplesPerFrame, frame.sampleAccumulation);
}

void SetLiveRemasterPresentation (bool enabled, RemasterLightingView lightingView)
{
	liveRemasterLightingView.store(lightingView, std::memory_order_relaxed);
	liveRemasterPresentationEnabled.store(enabled, std::memory_order_relaxed);
	S9xRemasterSetLiveFramesEnabled(enabled);
}

static void S9xInitMetal (void)
{
    glScreenW = glScreenBounds.size.width;
    glScreenH = glScreenBounds.size.height;

    metalLayer = (CAMetalLayer *)s9xView.layer;
	layerDelegate = [MetalLayerDelegate new];
	metalLayer.delegate = layerDelegate;
	
    metalDevice = s9xView.device;
			
	metalCommandQueue = [metalDevice newCommandQueue];
	
	NSError *error = nil;
	id<MTLLibrary> defaultLibrary = [metalDevice newDefaultLibraryWithBundle:[NSBundle bundleForClass:[S9xEngine class]] error:&error];

	MTLRenderPipelineDescriptor *pipelineDescriptor = [MTLRenderPipelineDescriptor new];
	pipelineDescriptor.label = @"Snes9x Pipeline";
	pipelineDescriptor.vertexFunction = [defaultLibrary newFunctionWithName:@"vertexShader"];
	pipelineDescriptor.colorAttachments[0].pixelFormat = s9xView.colorPixelFormat;
	pipelineDescriptor.fragmentFunction = [defaultLibrary newFunctionWithName:@"fragmentShader"];
	
	metalPipelineState = [metalDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
	id<MTLFunction> lightingFunction = [defaultLibrary newFunctionWithName:@"remasterDirectLighting"];
	remasterLightingPipelineState = [metalDevice newComputePipelineStateWithFunction:lightingFunction error:&error];
	id<MTLFunction> indirectFunction = [defaultLibrary newFunctionWithName:@"remasterIndirectBounce"];
	remasterIndirectPipelineState = [metalDevice newComputePipelineStateWithFunction:indirectFunction error:&error];
	id<MTLFunction> compositeFunction = [defaultLibrary newFunctionWithName:@"remasterCompositeLighting"];
	remasterCompositePipelineState = [metalDevice newComputePipelineStateWithFunction:compositeFunction error:&error];
	id<MTLFunction> sampleAccumulationFunction = [defaultLibrary newFunctionWithName:@"remasterAccumulateSamples"];
	remasterSampleAccumulationPipelineState = [metalDevice newComputePipelineStateWithFunction:sampleAccumulationFunction error:&error];
	id<MTLFunction> highlightFunction = [defaultLibrary newFunctionWithName:@"remasterSelectedTileHighlight"];
	remasterHighlightPipelineState = [metalDevice newComputePipelineStateWithFunction:highlightFunction error:&error];
	
	if (metalPipelineState == nil)
	{
		NSLog(@"%@",error);
	}
}

static void S9xDeinitMetal (void)
{
	
	metalCommandQueue = nil;
	metalDevice = nil;
	metalTexture = nil;
	remasterLightingPipelineState = nil;
	remasterIndirectPipelineState = nil;
	remasterCompositePipelineState = nil;
	remasterSampleAccumulationPipelineState = nil;
	remasterHighlightPipelineState = nil;
	remasterTextureDevice = nil;
	remasterTextureWidth = remasterTextureHeight = 0;
	remasterOcclusionTexture = remasterEmissionTexture = remasterHeightTexture = nil;
	remasterParticipationTexture = remasterOppositeFacingTexture = remasterSurfaceTexture = nil;
	remasterOutputTexture = remasterDirectTexture = nil;
	remasterBounceTextures[0] = remasterBounceTextures[1] = nil;
	remasterIndirectTextures[0] = remasterIndirectTextures[1] = nil;
	remasterSampleMeanTextures[0] = remasterSampleMeanTextures[1] = nil;
	metalLayer = nil;
}

void GetGameDisplay (int *w, int *h)
{
	if (w != NULL && h != NULL)
	{
        *w = s9xView.frame.size.width;
		*h = s9xView.frame.size.height;
	}
}

void S9xInitDisplay (int argc, char **argv)
{
    glScreenBounds = s9xView.frame;

	unlimitedCursor = CGPointMake(0.0f, 0.0f);

	imageWidth[0] = imageHeight[0] = 0;
	imageWidth[1] = imageHeight[1] = 0;
	prevBlitWidth = prevBlitHeight = 0;
	whichBuf      = 0;
	textureNum    = 0;

	switch (videoMode)
	{
		case VIDEOMODE_HQ4X:
			nx =  4;
			break;

		case VIDEOMODE_HQ3X:
			nx =  3;
			break;

		case VIDEOMODE_NTSC_C:
		case VIDEOMODE_NTSC_S:
		case VIDEOMODE_NTSC_R:
		case VIDEOMODE_NTSC_M:
			nx = -1;
			break;

		case VIDEOMODE_NTSC_TV_C:
		case VIDEOMODE_NTSC_TV_S:
		case VIDEOMODE_NTSC_TV_R:
		case VIDEOMODE_NTSC_TV_M:
			nx = -2;
			break;

		default:
			nx =  2;
			break;
	}

    S9xInitMetal();

	S9xSetSoundMute(false);
    lastFrame = GetMicroseconds();
}

void S9xDeinitDisplay (void)
{
	S9xSetSoundMute(true);
    S9xDeinitMetal();
}

bool8 S9xInitUpdate (void)
{
	return (true);
}

bool8 S9xDeinitUpdate (int width, int height)
{
	S9xPutImage(width, height);
	return true;
}

bool8 S9xContinueUpdate (int width, int height)
{
	return (true);
}

void S9xPutImage (int width, int height)
{
	for(unsigned int i = 0 ; i < sizeof(watches)/sizeof(*watches) ; i++)
	{
		if(watches[i].on)
		{
			int address = watches[i].address - 0x7E0000;
			const uint8* source;

			if(address < 0x20000)
			{
				source = Memory.RAM + address;
			}
			else if(address < 0x30000)
			{
				source = Memory.SRAM + address - 0x20000;
			}
			else
			{
				source = Memory.FillRAM + address - 0x30000;
			}

			memcpy(&(Cheat.CWatchRAM[address]), source, watches[i].size);
		}
	}

    if (Settings.DisplayFrameRate)
    {
        static int	drawnFrames[60] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        static int	tableIndex = 0;
        int			frameCalc  = 0;

        drawnFrames[tableIndex] = skipFrames;

        if (Settings.TurboMode)
        {
            drawnFrames[tableIndex] = (drawnFrames[tableIndex] + (macFastForwardRate / 2)) / macFastForwardRate;
            if (drawnFrames[tableIndex] == 0)
                drawnFrames[tableIndex] = 1;
        }

        tableIndex = (tableIndex + 1) % 60;

        for (int i = 0; i < 60; i++)
            frameCalc += drawnFrames[i];

		// avoid dividing by 0
		if (frameCalc == 0)
			frameCalc = 1;
		
        IPPU.DisplayedRenderedFrameCount = (Memory.ROMFramesPerSecond * 60) / frameCalc;
    }
	
	const RemasterFrame *liveFrame = S9xRemasterCompletedFrame();
	if (liveRemasterPresentationEnabled.load(std::memory_order_relaxed) &&
		width == SNES_WIDTH && height > 0 && liveFrame &&
		S9xRemasterFrameHasPixelData(*liveFrame, static_cast<uint32_t>(width), static_cast<uint32_t>(height)) &&
		DrawRemasterFrame(*liveFrame, S9xRemasterGetDebugMode(), nullptr, true,
			liveRemasterLightingView.load(std::memory_order_relaxed)))
		return;

	S9xPutImageMetal(width, height, GFX.Screen, GFX.RealPPL, S9xRemasterMainOwners(), GFX.RealPPL,
		nullptr, 0, S9xRemasterGetDebugMode());
}


static bool S9xPutImageMetal (int width, int height, const uint16 *buffer16, size_t pitch,
	const uint32_t *owners, size_t ownerPitch, const uint8_t *highlights, size_t highlightPitch,
	RemasterDebugMode debugMode, const uint8_t *occlusion, const uint8_t *emission, const uint8_t *heightField,
	const float *surfaceField, const uint8_t *participation, const uint8_t *oppositeFacing,
	const std::vector<RemasterGpuLight> *lights, bool lighting,
	RemasterLightingView lightingView, uint8_t indirectBounceCount, float indirectRoughness,
	float originalSceneContribution, uint8_t samplesPerFrame, bool sampleAccumulation)
{
	static std::mutex renderMutex;
	std::lock_guard<std::mutex> lock(renderMutex);
	static uint8 *buffer = nil;
	static size_t buffer_size = 0;
	if (width <= 0 || height <= 0 || !buffer16 || pitch < static_cast<size_t>(width) ||
		!metalLayer || !metalDevice || !metalCommandQueue || !metalPipelineState)
		return false;
	const size_t requiredSize = static_cast<size_t>(width) * height * 4;

	if (buffer_size != requiredSize)
	{
		uint8 *newBuffer = (uint8 *)realloc(buffer, requiredSize);
		if (!newBuffer)
			return false;
		buffer = newBuffer;
		buffer_size = requiredSize;
	}

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			uint16 pixel = buffer16[y * pitch + x];
			unsigned int red = (pixel & FIRST_COLOR_MASK_RGB555) >> 10;
			unsigned int green = (pixel & SECOND_COLOR_MASK_RGB555) >> 5;
			unsigned int blue = (pixel & THIRD_COLOR_MASK_RGB555);

			red = ( red * 527 + 23 ) >> 6;
			green = ( green * 527 + 23 ) >> 6;
			blue = ( blue * 527 + 23 ) >> 6;

			if (debugMode != RemasterDebugMode::Original)
			{
				uint32 owner = owners && ownerPitch >= static_cast<size_t>(width) ?
					owners[y * ownerPitch + x] : REMASTER_OWNER_UNSUPPORTED;
				unsigned int debugRed, debugGreen, debugBlue;
				if (owner == REMASTER_OWNER_UNSUPPORTED)
				{
					debugRed = 255;
					debugGreen = 0;
					debugBlue = 255;
				}
				else if (owner == REMASTER_OWNER_FORCED_BLANK)
				{
					debugRed = debugGreen = debugBlue = 32;
				}
				else
				{
					uint32 hash = owner * 0x9e3779b1u;
					hash ^= hash >> 16;
					debugRed = 48 + (hash & 0xcf);
					debugGreen = 48 + ((hash >> 8) & 0xcf);
					debugBlue = 48 + ((hash >> 16) & 0xcf);
				}

				if (debugMode == RemasterDebugMode::Overlay)
				{
					red = (red + debugRed) >> 1;
					green = (green + debugGreen) >> 1;
					blue = (blue + debugBlue) >> 1;
				}
				else
				{
					red = debugRed;
					green = debugGreen;
					blue = debugBlue;
				}
			}

			int offset = (y * width + x) * 4;
			buffer[offset++] = (uint8)red;
			buffer[offset++] = (uint8)green;
			buffer[offset++] = (uint8)blue;
			buffer[offset] = 0xFF;
		}
	}
	
	CGSize layerSize = metalLayer.bounds.size;
	
	@autoreleasepool {
		if (!metalTexture || metalTexture.width != static_cast<NSUInteger>(width) ||
			metalTexture.height != static_cast<NSUInteger>(height))
		{
			MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
				width:width height:height mipmapped:NO];
			metalTexture = [metalDevice newTextureWithDescriptor:textureDescriptor];
		}
		
		[metalTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:buffer bytesPerRow:width * 4];
		
		float vWidth = layerSize.width / 2.0;
		float vHeight = layerSize.height / 2.0;
		
		const MetalVertex verticies[] =
		{
			// Pixel positions, Texture coordinates
			{ {  vWidth,  -vHeight },  { 1.f, 1.f } },
			{ { -vWidth,  -vHeight },  { 0.f, 1.f } },
			{ { -vWidth,   vHeight },  { 0.f, 0.f } },
			
			{ {  vWidth,  -vHeight },  { 1.f, 1.f } },
			{ { -vWidth,   vHeight },  { 0.f, 0.f } },
			{ {  vWidth,   vHeight },  { 1.f, 0.f } },
		};
		
		id<MTLBuffer> vertexBuffer = [metalDevice newBufferWithBytes:verticies length:sizeof(verticies) options:MTLResourceStorageModeShared];
		id<MTLBuffer> fragmentBuffer = [metalDevice newBufferWithBytes:&videoMode length:sizeof(videoMode) options:MTLResourceStorageModeShared];
		
		id<MTLCommandBuffer> commandBuffer = [metalCommandQueue commandBuffer];
		commandBuffer.label = @"Snes9x command buffer";
		id<MTLTexture> presentationTexture = metalTexture;
		const bool measureGi = std::getenv("S9X_REMASTER_GI_METRICS") != nullptr;
		id<MTLTexture> diagnosticDirectTexture = nil;
		id<MTLTexture> diagnosticBounceTextures[16] = {};
		id<MTLTexture> diagnosticIndirectTextures[16] = {};
		id<MTLTexture> diagnosticStageTextures[3] = {};
		uint32_t diagnosticBounceCount = 0;
		if (lighting && occlusion && emission && heightField && surfaceField && participation && oppositeFacing && remasterLightingPipelineState &&
			remasterIndirectPipelineState && remasterCompositePipelineState && remasterSampleAccumulationPipelineState)
		{
			const bool rebuildTextures = remasterTextureDevice != metalDevice ||
				remasterTextureWidth != static_cast<NSUInteger>(width) ||
				remasterTextureHeight != static_cast<NSUInteger>(height);
			if (rebuildTextures)
			{
				remasterTextureDevice = metalDevice;
				remasterTextureWidth = width;
				remasterTextureHeight = height;
				MTLTextureDescriptor *fieldDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG8Unorm
					width:width height:height mipmapped:NO];
				remasterOcclusionTexture = [metalDevice newTextureWithDescriptor:fieldDescriptor];
				remasterHeightTexture = [metalDevice newTextureWithDescriptor:fieldDescriptor];
				remasterParticipationTexture = [metalDevice newTextureWithDescriptor:fieldDescriptor];
				MTLTextureDescriptor *emissionDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
					width:width height:height mipmapped:NO];
				remasterEmissionTexture = [metalDevice newTextureWithDescriptor:emissionDescriptor];
				MTLTextureDescriptor *maskDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
					width:width height:height mipmapped:NO];
				remasterOppositeFacingTexture = [metalDevice newTextureWithDescriptor:maskDescriptor];
				MTLTextureDescriptor *surfaceDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
					width:width height:height mipmapped:NO];
				remasterSurfaceTexture = [metalDevice newTextureWithDescriptor:surfaceDescriptor];
				MTLTextureDescriptor *outputDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
					width:width height:height mipmapped:NO];
				outputDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
				remasterOutputTexture = [metalDevice newTextureWithDescriptor:outputDescriptor];
				MTLTextureDescriptor *radianceDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
					width:width height:height mipmapped:NO];
				radianceDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
				remasterDirectTexture = [metalDevice newTextureWithDescriptor:radianceDescriptor];
				for (size_t i = 0; i < 2; i++)
				{
					remasterBounceTextures[i] = [metalDevice newTextureWithDescriptor:radianceDescriptor];
					remasterIndirectTextures[i] = [metalDevice newTextureWithDescriptor:radianceDescriptor];
					remasterSampleMeanTextures[i] = [metalDevice newTextureWithDescriptor:radianceDescriptor];
				}
			}
			id<MTLTexture> occlusionTexture = remasterOcclusionTexture;
			[occlusionTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:occlusion bytesPerRow:width * 2];
			id<MTLTexture> emissionTexture = remasterEmissionTexture;
			[emissionTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:emission bytesPerRow:width * 4];
			id<MTLTexture> heightTexture = remasterHeightTexture;
			[heightTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:heightField bytesPerRow:width * 2];
			id<MTLTexture> participationTexture = remasterParticipationTexture;
			[participationTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:participation bytesPerRow:width * 2];
			id<MTLTexture> oppositeFacingTexture = remasterOppositeFacingTexture;
			[oppositeFacingTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:oppositeFacing bytesPerRow:width];
			id<MTLTexture> surfaceTexture = remasterSurfaceTexture;
			[surfaceTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:surfaceField bytesPerRow:width * sizeof(float) * 4];
			presentationTexture = remasterOutputTexture;
			MTLTextureDescriptor *radianceDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
				width:width height:height mipmapped:NO];
			radianceDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
			if (measureGi)
				radianceDescriptor.storageMode = MTLStorageModeShared;
			id<MTLTexture> directTexture = measureGi ? [metalDevice newTextureWithDescriptor:radianceDescriptor] : remasterDirectTexture;
			diagnosticDirectTexture = measureGi ? directTexture : nil;
			static uint32_t randomSeed = 0;
			RemasterLightingUniforms uniforms = { static_cast<uint32_t>(width), static_cast<uint32_t>(height),
				static_cast<uint32_t>(lightingView), static_cast<uint32_t>(lights ? lights->size() : 0), 0, 0,
				indirectRoughness, originalSceneContribution, 0, std::max<uint32_t>(1, samplesPerFrame), ++randomSeed };
			id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
			[computeEncoder setComputePipelineState:remasterLightingPipelineState];
			[computeEncoder setTexture:metalTexture atIndex:0];
			[computeEncoder setTexture:occlusionTexture atIndex:1];
			[computeEncoder setTexture:presentationTexture atIndex:2];
			[computeEncoder setTexture:emissionTexture atIndex:3];
			[computeEncoder setTexture:heightTexture atIndex:4];
			[computeEncoder setTexture:surfaceTexture atIndex:5];
			[computeEncoder setTexture:directTexture atIndex:6];
			[computeEncoder setTexture:participationTexture atIndex:7];
			[computeEncoder setTexture:oppositeFacingTexture atIndex:8];
			[computeEncoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
			const RemasterGpuLight emptyLight = {};
			[computeEncoder setBytes:!lights || lights->empty() ? &emptyLight : lights->data()
				length:!lights || lights->empty() ? sizeof(emptyLight) : lights->size() * sizeof(RemasterGpuLight) atIndex:1];
			[computeEncoder dispatchThreads:MTLSizeMake(width, height, 1)
				threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
			[computeEncoder endEncoding];
			if (measureGi && indirectBounceCount > 0)
			{
				id<MTLTexture> discardedPreviousIndirect = [metalDevice newTextureWithDescriptor:radianceDescriptor];
				id<MTLTexture> discardedNextIndirect = [metalDevice newTextureWithDescriptor:radianceDescriptor];
				for (uint32_t stage = 1; stage <= 3; stage++)
				{
					diagnosticStageTextures[stage - 1] = [metalDevice newTextureWithDescriptor:radianceDescriptor];
					uniforms.passIndex = 0;
					uniforms.diagnosticStage = stage;
					id<MTLComputeCommandEncoder> diagnosticEncoder = [commandBuffer computeCommandEncoder];
					[diagnosticEncoder setComputePipelineState:remasterIndirectPipelineState];
					[diagnosticEncoder setTexture:metalTexture atIndex:0];
					[diagnosticEncoder setTexture:occlusionTexture atIndex:1];
					[diagnosticEncoder setTexture:surfaceTexture atIndex:2];
					[diagnosticEncoder setTexture:heightTexture atIndex:3];
					[diagnosticEncoder setTexture:participationTexture atIndex:4];
					[diagnosticEncoder setTexture:directTexture atIndex:5];
					[diagnosticEncoder setTexture:discardedPreviousIndirect atIndex:6];
					[diagnosticEncoder setTexture:diagnosticStageTextures[stage - 1] atIndex:7];
					[diagnosticEncoder setTexture:discardedNextIndirect atIndex:8];
					[diagnosticEncoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
					[diagnosticEncoder dispatchThreads:MTLSizeMake(width, height, 1)
						threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
					[diagnosticEncoder endEncoding];
				}
				uniforms.diagnosticStage = 0;
			}
			if (indirectBounceCount > 0)
			{
				id<MTLTexture> bounceTextures[2] = { remasterBounceTextures[0], remasterBounceTextures[1] };
				id<MTLTexture> indirectTextures[2] = { remasterIndirectTextures[0], remasterIndirectTextures[1] };
				id<MTLTexture> sampleMeanTextures[2] = { remasterSampleMeanTextures[0], remasterSampleMeanTextures[1] };
				id<MTLTexture> finalIndirect = nil;
				for (uint32_t sample = 0; sample < uniforms.sampleCount; sample++)
				{
					uniforms.sampleIndex = sample;
					id<MTLTexture> previousBounce = directTexture;
					for (uint32_t bounce = 0; bounce < indirectBounceCount; bounce++)
					{
						const uint32_t current = bounce & 1;
						const uint32_t previous = current ^ 1;
						uniforms.passIndex = bounce;
						id<MTLComputeCommandEncoder> bounceEncoder = [commandBuffer computeCommandEncoder];
						[bounceEncoder setComputePipelineState:remasterIndirectPipelineState];
						[bounceEncoder setTexture:metalTexture atIndex:0];
						[bounceEncoder setTexture:occlusionTexture atIndex:1];
						[bounceEncoder setTexture:surfaceTexture atIndex:2];
						[bounceEncoder setTexture:heightTexture atIndex:3];
						[bounceEncoder setTexture:participationTexture atIndex:4];
						[bounceEncoder setTexture:previousBounce atIndex:5];
						[bounceEncoder setTexture:indirectTextures[previous] atIndex:6];
						[bounceEncoder setTexture:bounceTextures[current] atIndex:7];
						[bounceEncoder setTexture:indirectTextures[current] atIndex:8];
						[bounceEncoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
						[bounceEncoder dispatchThreads:MTLSizeMake(width, height, 1)
							threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
						[bounceEncoder endEncoding];
						previousBounce = bounceTextures[current];
						if (measureGi && sample == 0 && bounce < 16)
						{
							diagnosticBounceTextures[bounce] = [metalDevice newTextureWithDescriptor:radianceDescriptor];
							diagnosticIndirectTextures[bounce] = [metalDevice newTextureWithDescriptor:radianceDescriptor];
							id<MTLBlitCommandEncoder> diagnosticEncoder = [commandBuffer blitCommandEncoder];
							[diagnosticEncoder copyFromTexture:bounceTextures[current] sourceSlice:0 sourceLevel:0
								sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(width, height, 1)
								toTexture:diagnosticBounceTextures[bounce] destinationSlice:0 destinationLevel:0
								destinationOrigin:MTLOriginMake(0, 0, 0)];
							[diagnosticEncoder copyFromTexture:indirectTextures[current] sourceSlice:0 sourceLevel:0
								sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(width, height, 1)
								toTexture:diagnosticIndirectTextures[bounce] destinationSlice:0 destinationLevel:0
								destinationOrigin:MTLOriginMake(0, 0, 0)];
							[diagnosticEncoder endEncoding];
							diagnosticBounceCount = bounce + 1;
						}
					}
					finalIndirect = indirectTextures[(indirectBounceCount - 1) & 1];
					if (sampleAccumulation)
					{
						const uint32_t current = sample & 1;
						id<MTLComputeCommandEncoder> accumulationEncoder = [commandBuffer computeCommandEncoder];
						[accumulationEncoder setComputePipelineState:remasterSampleAccumulationPipelineState];
						[accumulationEncoder setTexture:finalIndirect atIndex:0];
						[accumulationEncoder setTexture:sampleMeanTextures[current ^ 1] atIndex:1];
						[accumulationEncoder setTexture:sampleMeanTextures[current] atIndex:2];
						[accumulationEncoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
						[accumulationEncoder dispatchThreads:MTLSizeMake(width, height, 1)
							threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
						[accumulationEncoder endEncoding];
						finalIndirect = sampleMeanTextures[current];
					}
				}
				if (lightingView == RemasterLightingView::Composite ||
					lightingView == RemasterLightingView::DirectContribution ||
					lightingView == RemasterLightingView::Difference ||
					lightingView == RemasterLightingView::IndirectContribution ||
					lightingView == RemasterLightingView::DirectAndIndirectContribution)
				{
					id<MTLComputeCommandEncoder> compositeEncoder = [commandBuffer computeCommandEncoder];
					[compositeEncoder setComputePipelineState:remasterCompositePipelineState];
					[compositeEncoder setTexture:metalTexture atIndex:0];
					[compositeEncoder setTexture:directTexture atIndex:1];
					[compositeEncoder setTexture:finalIndirect atIndex:2];
					[compositeEncoder setTexture:presentationTexture atIndex:3];
					[compositeEncoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
					[compositeEncoder dispatchThreads:MTLSizeMake(width, height, 1)
						threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
					[compositeEncoder endEncoding];
				}
			}
		}
		
		// Selection is presentation-only and must never feed lighting albedo or radiance.
		if (highlights && highlightPitch >= static_cast<size_t>(width) && remasterHighlightPipelineState)
		{
			MTLTextureDescriptor *maskDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
				width:width height:height mipmapped:NO];
			maskDescriptor.usage = MTLTextureUsageShaderRead;
			id<MTLTexture> highlightMask = [metalDevice newTextureWithDescriptor:maskDescriptor];
			MTLTextureDescriptor *outputDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
				width:width height:height mipmapped:NO];
			outputDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
			id<MTLTexture> highlightedTexture = [metalDevice newTextureWithDescriptor:outputDescriptor];
			if (!highlightMask || !highlightedTexture)
				return false;
			[highlightMask replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:highlights bytesPerRow:highlightPitch];
			id<MTLComputeCommandEncoder> highlightEncoder = [commandBuffer computeCommandEncoder];
			if (!highlightEncoder)
				return false;
			[highlightEncoder setComputePipelineState:remasterHighlightPipelineState];
			[highlightEncoder setTexture:presentationTexture atIndex:0];
			[highlightEncoder setTexture:highlightMask atIndex:1];
			[highlightEncoder setTexture:highlightedTexture atIndex:2];
			[highlightEncoder dispatchThreadgroups:MTLSizeMake((width + 7) / 8, (height + 7) / 8, 1)
				threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
			[highlightEncoder endEncoding];
			presentationTexture = highlightedTexture;
		}

		id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
		if (!drawable)
			return false;
		
		MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
		
		renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
		renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
		renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1.0);
		
		if(renderPassDescriptor != nil)
		{
			id<MTLRenderCommandEncoder> renderEncoder =
			[commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
			renderEncoder.label = @"Snes9x render encoder";
			
			vector_uint2 viewportSize = { static_cast<unsigned int>(layerSize.width), static_cast<unsigned int>(layerSize.height) };
			
			CGFloat scale = metalLayer.contentsScale;
			[renderEncoder setViewport:(MTLViewport){0.0, 0.0, layerSize.width * scale, layerSize.height * scale, -1.0, 1.0 }];
			
			[renderEncoder setRenderPipelineState:metalPipelineState];
			
			[renderEncoder setVertexBuffer:vertexBuffer
									offset:0
								   atIndex:0];
			
			[renderEncoder setVertexBytes:&viewportSize
								   length:sizeof(viewportSize)
								  atIndex:1];
			
			[renderEncoder setFragmentTexture:presentationTexture atIndex:0];
			[renderEncoder setFragmentBuffer:fragmentBuffer offset:0 atIndex:1];
			
			[renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
			
			[renderEncoder endEncoding];

			[commandBuffer presentDrawable:drawable];
			[commandBuffer commit];
			
			[commandBuffer waitUntilCompleted];
			if (measureGi && diagnosticDirectTexture)
			{
				if (commandBuffer.status != MTLCommandBufferStatusCompleted)
					NSLog(@"Remaster GI metrics unavailable: %@", commandBuffer.error);
				else
				{
					NSLog(@"Remaster GI config: bounces=%u roughness=%.6g original=%.6g samples=%u accumulation=%s lights=%zu",
						unsigned(indirectBounceCount), indirectRoughness, originalSceneContribution,
						unsigned(samplesPerFrame), sampleAccumulation ? "on" : "off", lights ? lights->size() : 0);
					if (lights)
						for (size_t lightIndex = 0; lightIndex < lights->size(); lightIndex++)
						{
							const RemasterGpuLight &light = (*lights)[lightIndex];
							NSLog(@"Remaster GI light %zu: position=(%.3f, %.3f, %.3f) color=(%.6g, %.6g, %.6g) intensity=%.6g radius=%.6g",
								lightIndex, light.position.x, light.position.y, light.position.z, light.color.x, light.color.y,
								light.color.z, light.intensity, light.radius);
						}
					const RemasterRadianceMetrics directMetrics = MeasureRemasterRadiance(diagnosticDirectTexture);
					LogRemasterRadianceMetrics("direct", 0, directMetrics);
					for (uint32_t stage = 0; stage < 3; stage++)
					{
						const RemasterRadianceMetrics stageMetrics = MeasureRemasterRadiance(diagnosticStageTextures[stage]);
						const char *labels[] = { "distance", "cosine", "visible" };
						LogRemasterRadianceMetrics(labels[stage], 1, stageMetrics);
					}
					RemasterRadianceMetrics previousAccumulated;
					for (uint32_t bounce = 0; bounce < diagnosticBounceCount; bounce++)
					{
						const RemasterRadianceMetrics outgoing = MeasureRemasterRadiance(diagnosticBounceTextures[bounce]);
						const RemasterRadianceMetrics accumulated = MeasureRemasterRadiance(diagnosticIndirectTextures[bounce]);
						LogRemasterRadianceMetrics("bounce", bounce + 1, outgoing);
						LogRemasterRadianceMetrics("accumulated", bounce + 1, accumulated, &previousAccumulated);
						previousAccumulated = accumulated;
					}
				}
			}
		}
	}
	return true;
}

void S9xTextMode (void)
{
	return;
}

void S9xGraphicsMode (void)
{
	return;
}
