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
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <map>
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
	float heightPreviewMultiplier;
	float padding;
	uint32_t sampleIndex;
	uint32_t sampleCount;
	uint32_t randomSeed;
	float reflectanceBoost;
	vector_float4 cameraDirection;
	vector_float4 debugPositionRadius;
	vector_float4 debugColorIntensity;
	vector_float4 heightPreviewRange;
} RemasterLightingUniforms;

static_assert(sizeof(RemasterLightingUniforms) == 128 &&
	offsetof(RemasterLightingUniforms, cameraDirection) == 64, "Metal uniform ABI");

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
	const uint8_t * = nullptr, const float * = nullptr,
	const std::vector<RemasterGpuLight> * = nullptr, bool = false,
	RemasterLightingView = RemasterLightingView::Composite, uint8_t = 0, float = 1.0f,
	float = 0.65f, float = 8.0f, float = 0.0f, uint8_t = 1, bool = true,
	const std::array<float, 3> * = nullptr, int = -1, bool = true);

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
id<MTLComputePipelineState>	remasterVisibilityBlocksPipelineState = nil;
id<MTLComputePipelineState>	remasterIndirectPipelineState = nil;
id<MTLComputePipelineState>	remasterSourcePowerLeavesPipelineState = nil;
id<MTLComputePipelineState>	remasterSourcePowerReducePipelineState = nil;
id<MTLComputePipelineState>	remasterSampledIndirectPipelineState = nil;
id<MTLComputePipelineState>	remasterCompositePipelineState = nil;
id<MTLComputePipelineState>	remasterHighlightPipelineState = nil;
static std::atomic<bool> liveRemasterPresentationEnabled(false);
static std::atomic<RemasterLightingView> liveRemasterLightingView(RemasterLightingView::Composite);
static std::atomic<uint32_t> remasterHeightPreviewRange { (32u << 16) };
static std::mutex debugLightMutex;
static RemasterDebugLight debugLight;

void SetRemasterDebugLight (const RemasterDebugLight &light)
{
	std::lock_guard<std::mutex> lock(debugLightMutex);
	debugLight = light;
}

RemasterDebugLight GetRemasterDebugLight ()
{
	std::lock_guard<std::mutex> lock(debugLightMutex);
	return debugLight;
}
static constexpr size_t remasterResourceSlotCount = 3;
struct RemasterMetalResources
{
	std::atomic<bool> inUse { false };
	id<MTLDevice> device = nil;
	NSUInteger width = 0;
	NSUInteger height = 0;
	id<MTLTexture> source = nil;
	id<MTLTexture> occlusion = nil;
	id<MTLTexture> emission = nil;
	id<MTLTexture> heightField = nil;
	id<MTLTexture> participation = nil;
	id<MTLTexture> oppositeFacing = nil;
	id<MTLTexture> surface = nil;
	id<MTLTexture> reflectance = nil;
	id<MTLTexture> visibilityBlocks = nil;
	id<MTLTexture> output = nil;
	id<MTLTexture> direct = nil;
	id<MTLTexture> bounce[2] = {};
	id<MTLTexture> indirect[2] = {};
	id<MTLTexture> directMean[2] = {};
	id<MTLBuffer> sourcePower = nil;
	uint32_t sourceLeafCount = 0;
};
static RemasterMetalResources remasterResources[remasterResourceSlotCount];
// Serialize producers and lifecycle changes, not the drawable-waiting worker.
static std::recursive_mutex renderMutex;
static dispatch_queue_t presentationQueue = dispatch_queue_create("org.snes9x.presentation", DISPATCH_QUEUE_SERIAL);
static std::atomic<uint64_t> droppedRemasterPresentations { 0 };
static std::mutex remasterResourceMutex;
static std::condition_variable remasterResourceAvailable;

static int AcquireRemasterResourceSlot (bool wait)
{
	std::unique_lock<std::mutex> lock(remasterResourceMutex);
	auto available = [] {
		for (const RemasterMetalResources &resources : remasterResources)
			if (!resources.inUse.load(std::memory_order_relaxed))
				return true;
		return false;
	};
	if (wait)
		remasterResourceAvailable.wait(lock, available);
	else if (!available())
		return -1;
	for (size_t slot = 0; slot < remasterResourceSlotCount; slot++)
		if (!remasterResources[slot].inUse.exchange(true, std::memory_order_relaxed))
			return static_cast<int>(slot);
	return -1;
}

static void ReleaseRemasterResourceSlot (int slot)
{
	if (slot < 0)
		return;
	{
		std::lock_guard<std::mutex> lock(remasterResourceMutex);
		remasterResources[slot].inUse.store(false, std::memory_order_relaxed);
	}
	remasterResourceAvailable.notify_one();
}

struct RemasterResourceSlotGuard
{
	explicit RemasterResourceSlotGuard (int value) : slot(value) {}
	~RemasterResourceSlotGuard () { ReleaseRemasterResourceSlot(slot); }
	void disarm () { slot = -1; }
	int slot;
};

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
	const std::vector<RemasterTileContentId> *selectedTiles, bool lighting, RemasterLightingView lightingView,
	bool asynchronous)
{
	const bool panelMetrics = S9xRemasterPerformanceMetricsEnabled();
	const auto fieldPreparationStarted = std::chrono::steady_clock::now();
	std::lock_guard<std::recursive_mutex> lock(renderMutex);
	if (asynchronous && !liveRemasterPresentationEnabled.load(std::memory_order_relaxed))
		return true;
	if (frame.width > INT_MAX || frame.height > INT_MAX ||
		!S9xRemasterFrameHasPixelData(frame, frame.width, frame.height))
		return false;
	// Surface diagnostics are presentation-only, never lighting albedo.
	if (lighting)
		debugMode = RemasterDebugMode::Original;
	const int resourceSlot = lighting ? AcquireRemasterResourceSlot(!asynchronous) : -1;
	if (lighting && resourceSlot < 0)
	{
		droppedRemasterPresentations.fetch_add(1, std::memory_order_relaxed);
		return true;
	}
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
	std::vector<float> reflectanceField(frame.mainPixels.size() * 4, 0.0f);
	if (lighting && frame.schemaVersion >= 5)
	{
		std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> metadataByTile;
		for (const RemasterFrameAssetMetadata &metadata : frame.assetMetadata)
			metadataByTile.emplace(metadata.tileId, &metadata);
		std::vector<const RemasterFrameAssetMetadata *> instanceMetadata(frame.tileInstances.size(), nullptr);
		for (size_t i = 0; i < frame.tileInstances.size(); i++)
		{
			const auto found = metadataByTile.find(frame.tileInstances[i].tileId);
			if (found != metadataByTile.end())
				instanceMetadata[i] = found->second;
		}
		std::unordered_map<std::string, const RemasterFrameMaterial *> materials;
		for (const RemasterFrameMaterial &material : frame.materials)
			materials.emplace(material.name, &material);
		for (size_t i = 0; i < frame.mainPixels.size(); i++)
		{
			const RemasterFramePixel &mainPixel = frame.mainPixels[i];
			const bool useSubscreen = !mainPixel.instanceId &&
				static_cast<uint8_t>(mainPixel.owner >> 24) == static_cast<uint8_t>(RemasterSourceType::Backdrop);
			const RemasterFramePixel &pixel = useSubscreen ? frame.subPixels[i] : mainPixel;
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
			if (material && material->hasDiffuseReflectance)
			{
				std::copy(material->diffuseReflectance.begin(), material->diffuseReflectance.end(),
					reflectanceField.begin() + i * 4);
				reflectanceField[i * 4 + 3] = 1.0f;
			}
			lightingField[i * 2] = metadata && metadata->hasOcclusion ? metadata->occlusion[pixel.tilePixel] : 255;
			lightingField[i * 2 + 1] = 255;
			if (metadata && metadata->directLightingOppositeFacing)
				oppositeFacingField[i] = 255;
			if (metadata && metadata->hasEmission)
				std::copy(metadata->emissionRgba.begin() + pixel.tilePixel * 4,
					metadata->emissionRgba.begin() + pixel.tilePixel * 4 + 4, emissionField.begin() + i * 4);
			if ((metadata && metadata->hasHeight) || instance.hasPlacementHeight || instance.heightOffset)
			{
				const unsigned baseHeight = instance.hasPlacementHeight ? instance.placementHeight[pixel.tilePixel] :
					(metadata && metadata->hasHeight ? metadata->height[pixel.tilePixel] : 0);
				heightField[i * 2] = static_cast<uint8_t>(std::min(255u,
					baseHeight + instance.heightOffset));
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
				const RemasterFramePixel &mainPixel = frame.mainPixels[i];
				const bool useSubscreen = !mainPixel.instanceId &&
					static_cast<uint8_t>(mainPixel.owner >> 24) == static_cast<uint8_t>(RemasterSourceType::Backdrop);
				const RemasterFramePixel &pixel = useSubscreen ? frame.subPixels[i] : mainPixel;
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
	if (panelMetrics)
	{
		RemasterState &state = S9xRemasterState();
		std::lock_guard<std::mutex> metricsLock(state.performanceMetricsMutex);
		state.performanceMetrics.lightingFieldMs =
			std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - fieldPreparationStarted).count();
	}
	return S9xPutImageMetal(static_cast<int>(frame.width), static_cast<int>(frame.height),
		frame.originalRgb555.data(), frame.width, owners.data(), frame.width,
		highlights.empty() ? nullptr : highlights.data(), frame.width, debugMode,
		lightingField.data(), emissionField.data(), heightField.data(), surfaceField.data(), participationField.data(),
		oppositeFacingField.data(), reflectanceField.data(), nullptr,
		lighting && frame.schemaVersion >= 5,
		lightingView, frame.indirectBounceCount, frame.indirectRoughness, frame.originalSceneContribution,
		frame.heightPreviewMultiplier, frame.reflectanceBoost, frame.samplesPerFrame, frame.sampleAccumulation, &frame.cameraDirection,
		resourceSlot, !asynchronous);
}

void SetLiveRemasterPresentation (bool enabled, RemasterLightingView lightingView)
{
	std::lock_guard<std::recursive_mutex> lock(renderMutex);
	liveRemasterLightingView.store(lightingView, std::memory_order_relaxed);
	liveRemasterPresentationEnabled.store(enabled, std::memory_order_relaxed);
	S9xRemasterSetLiveFramesEnabled(enabled);
}

void SetRemasterHeightPreviewRange (uint16_t minimum, uint16_t maximum)
{
	const uint32_t low = std::min<uint32_t>(255, minimum);
	const uint32_t high = std::min<uint32_t>(256, std::max<uint32_t>(low + 1, maximum));
	remasterHeightPreviewRange.store((high << 16) | low, std::memory_order_relaxed);
}

static void S9xInitMetal (void)
{
	std::lock_guard<std::recursive_mutex> lock(renderMutex);
	if (metalCommandQueue)
		S9xDeinitMetal();
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
	id<MTLFunction> visibilityBlocksFunction = [defaultLibrary newFunctionWithName:@"remasterBuildVisibilityBlocks"];
	remasterVisibilityBlocksPipelineState = [metalDevice newComputePipelineStateWithFunction:visibilityBlocksFunction error:&error];
	id<MTLFunction> indirectFunction = [defaultLibrary newFunctionWithName:@"remasterIndirectBounce"];
	remasterIndirectPipelineState = [metalDevice newComputePipelineStateWithFunction:indirectFunction error:&error];
	remasterSourcePowerLeavesPipelineState = [metalDevice newComputePipelineStateWithFunction:
		[defaultLibrary newFunctionWithName:@"remasterBuildSourcePowerLeaves"] error:&error];
	remasterSourcePowerReducePipelineState = [metalDevice newComputePipelineStateWithFunction:
		[defaultLibrary newFunctionWithName:@"remasterReduceSourcePower"] error:&error];
	remasterSampledIndirectPipelineState = [metalDevice newComputePipelineStateWithFunction:
		[defaultLibrary newFunctionWithName:@"remasterSampledIndirectBounce"] error:&error];
	id<MTLFunction> compositeFunction = [defaultLibrary newFunctionWithName:@"remasterCompositeLighting"];
	remasterCompositePipelineState = [metalDevice newComputePipelineStateWithFunction:compositeFunction error:&error];
	id<MTLFunction> highlightFunction = [defaultLibrary newFunctionWithName:@"remasterSelectedTileHighlight"];
	remasterHighlightPipelineState = [metalDevice newComputePipelineStateWithFunction:highlightFunction error:&error];
	
	if (metalPipelineState == nil)
	{
		NSLog(@"%@",error);
	}
}

static void S9xDeinitMetal (void)
{
	std::lock_guard<std::recursive_mutex> lock(renderMutex);
	// No producer can reserve a slot while draining queued presentation and GPU work.
	dispatch_sync(presentationQueue, ^{});
	int acquiredSlots[remasterResourceSlotCount];
	for (size_t i = 0; i < remasterResourceSlotCount; i++)
		acquiredSlots[i] = AcquireRemasterResourceSlot(true);
	for (int slot : acquiredSlots)
	{
		RemasterMetalResources &resources = remasterResources[slot];
		resources.device = nil;
		resources.width = resources.height = 0;
		resources.source = resources.occlusion = resources.emission = resources.heightField = nil;
		resources.participation = resources.oppositeFacing = resources.surface = resources.reflectance = resources.output = nil;
		resources.visibilityBlocks = nil;
		resources.direct = resources.bounce[0] = resources.bounce[1] = nil;
		resources.indirect[0] = resources.indirect[1] = nil;
		resources.directMean[0] = resources.directMean[1] = nil;
		resources.sourcePower = nil;
		resources.sourceLeafCount = 0;
	}
	for (int slot : acquiredSlots)
		ReleaseRemasterResourceSlot(slot);
	metalCommandQueue = nil;
	metalDevice = nil;
	metalTexture = nil;
	remasterLightingPipelineState = nil;
	remasterVisibilityBlocksPipelineState = nil;
	remasterIndirectPipelineState = nil;
	remasterSourcePowerLeavesPipelineState = nil;
	remasterSourcePowerReducePipelineState = nil;
	remasterSampledIndirectPipelineState = nil;
	remasterCompositePipelineState = nil;
	remasterHighlightPipelineState = nil;
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
			liveRemasterLightingView.load(std::memory_order_relaxed), true))
		return;

	S9xPutImageMetal(width, height, GFX.Screen, GFX.RealPPL, S9xRemasterMainOwners(), GFX.RealPPL,
		nullptr, 0, S9xRemasterGetDebugMode());
}


static bool S9xPutImageMetal (int width, int height, const uint16 *buffer16, size_t pitch,
	const uint32_t *owners, size_t ownerPitch, const uint8_t *highlights, size_t highlightPitch,
	RemasterDebugMode debugMode, const uint8_t *occlusion, const uint8_t *emission, const uint8_t *heightField,
	const float *surfaceField, const uint8_t *participation, const uint8_t *oppositeFacing,
	const float *reflectance, const std::vector<RemasterGpuLight> *lights, bool lighting,
	RemasterLightingView lightingView, uint8_t indirectBounceCount, float indirectRoughness,
	float originalSceneContribution, float heightPreviewMultiplier, float reflectanceBoost,
	uint8_t samplesPerFrame, bool sampleAccumulation,
	const std::array<float, 3> *cameraDirection, int resourceSlot, bool waitForCompletion)
{
	RemasterResourceSlotGuard resourceGuard(resourceSlot);
	std::lock_guard<std::recursive_mutex> lock(renderMutex);
	const bool panelMetrics = S9xRemasterPerformanceMetricsEnabled();
	const auto lightingPreparationStarted = std::chrono::steady_clock::now();
	double directEncodeMs = 0.0;
	double indirectEncodeMs = 0.0;
	double accumulationEncodeMs = 0.0;
	double compositeEncodeMs = 0.0;
	static uint8 *buffer = nil;
	static size_t buffer_size = 0;
	if (width <= 0 || height <= 0 || !buffer16 || pitch < static_cast<size_t>(width) ||
		!metalLayer || !metalDevice || !metalCommandQueue || !metalPipelineState ||
		(!waitForCompletion && resourceSlot < 0))
		return false;
	vector_float3 normalizedCameraDirection = cameraDirection ? vector_float3{ (*cameraDirection)[0], (*cameraDirection)[1],
		(*cameraDirection)[2] } : vector_float3{ 0.0f, 0.0f, -1.0f };
	normalizedCameraDirection = simd_normalize(normalizedCameraDirection);
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
		RemasterMetalResources *resources = resourceSlot >= 0 ? &remasterResources[resourceSlot] : nullptr;
		id<MTLTexture> sourceTexture = resources ? resources->source : metalTexture;
		if (!sourceTexture || (resources && resources->device != metalDevice) ||
			sourceTexture.width != static_cast<NSUInteger>(width) ||
			sourceTexture.height != static_cast<NSUInteger>(height))
		{
			MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
				width:width height:height mipmapped:NO];
			sourceTexture = [metalDevice newTextureWithDescriptor:textureDescriptor];
			if (resources)
				resources->source = sourceTexture;
			else
				metalTexture = sourceTexture;
		}
		
		[sourceTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:buffer bytesPerRow:width * 4];
		
		float vWidth = layerSize.width / 2.0;
		float vHeight = layerSize.height / 2.0;
		
		const std::array<MetalVertex, 6> verticies = {{
			// Pixel positions, Texture coordinates
			{ {  vWidth,  -vHeight },  { 1.f, 1.f } },
			{ { -vWidth,  -vHeight },  { 0.f, 1.f } },
			{ { -vWidth,   vHeight },  { 0.f, 0.f } },
			
			{ {  vWidth,  -vHeight },  { 1.f, 1.f } },
			{ { -vWidth,   vHeight },  { 0.f, 0.f } },
			{ {  vWidth,   vHeight },  { 1.f, 0.f } },
		}};
		
		id<MTLCommandBuffer> commandBuffer = [metalCommandQueue commandBuffer];
		if (!commandBuffer)
			return false;
		commandBuffer.label = @"Snes9x command buffer";
		id<MTLTexture> presentationTexture = sourceTexture;
		const bool measureGi = std::getenv("S9X_REMASTER_GI_METRICS") != nullptr;
		id<MTLTexture> diagnosticDirectTexture = nil;
		id<MTLTexture> diagnosticBounceTextures[16] = {};
		id<MTLTexture> diagnosticIndirectTextures[16] = {};
		id<MTLTexture> diagnosticStageTextures[3] = {};
		uint32_t diagnosticBounceCount = 0;
		if (lighting && occlusion && emission && heightField && surfaceField && participation && oppositeFacing && reflectance && remasterLightingPipelineState &&
			remasterVisibilityBlocksPipelineState && remasterIndirectPipelineState && remasterSourcePowerLeavesPipelineState &&
			remasterSourcePowerReducePipelineState && remasterSampledIndirectPipelineState &&
			remasterCompositePipelineState)
		{
			if (!resources)
				return false;
			const bool rebuildTextures = !resources->visibilityBlocks || resources->device != metalDevice ||
				resources->width != static_cast<NSUInteger>(width) ||
				resources->height != static_cast<NSUInteger>(height);
			if (rebuildTextures)
			{
				resources->device = metalDevice;
				resources->width = width;
				resources->height = height;
				resources->sourceLeafCount = 1;
				while (resources->sourceLeafCount < static_cast<uint32_t>(width * height))
					resources->sourceLeafCount *= 2;
				resources->sourcePower = [metalDevice newBufferWithLength:
					static_cast<NSUInteger>(resources->sourceLeafCount) * 2 * sizeof(float)
					options:MTLResourceStorageModePrivate];
				MTLTextureDescriptor *fieldDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG8Unorm
					width:width height:height mipmapped:NO];
				resources->occlusion = [metalDevice newTextureWithDescriptor:fieldDescriptor];
				resources->heightField = [metalDevice newTextureWithDescriptor:fieldDescriptor];
				resources->participation = [metalDevice newTextureWithDescriptor:fieldDescriptor];
				MTLTextureDescriptor *emissionDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
					width:width height:height mipmapped:NO];
				resources->emission = [metalDevice newTextureWithDescriptor:emissionDescriptor];
				MTLTextureDescriptor *maskDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
					width:width height:height mipmapped:NO];
				resources->oppositeFacing = [metalDevice newTextureWithDescriptor:maskDescriptor];
				MTLTextureDescriptor *surfaceDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
					width:width height:height mipmapped:NO];
				resources->surface = [metalDevice newTextureWithDescriptor:surfaceDescriptor];
				resources->reflectance = [metalDevice newTextureWithDescriptor:surfaceDescriptor];
				MTLTextureDescriptor *visibilityBlocksDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG32Float
					width:(width + 7) / 8 height:(height + 7) / 8 mipmapped:NO];
				visibilityBlocksDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
				resources->visibilityBlocks = [metalDevice newTextureWithDescriptor:visibilityBlocksDescriptor];
				MTLTextureDescriptor *outputDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
					width:width height:height mipmapped:NO];
				outputDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
				resources->output = [metalDevice newTextureWithDescriptor:outputDescriptor];
				MTLTextureDescriptor *radianceDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
					width:width height:height mipmapped:NO];
				radianceDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
				resources->direct = [metalDevice newTextureWithDescriptor:radianceDescriptor];
				for (size_t i = 0; i < 2; i++)
				{
					resources->bounce[i] = [metalDevice newTextureWithDescriptor:radianceDescriptor];
					resources->indirect[i] = [metalDevice newTextureWithDescriptor:radianceDescriptor];
					resources->directMean[i] = [metalDevice newTextureWithDescriptor:radianceDescriptor];
				}
			}
			id<MTLTexture> occlusionTexture = resources->occlusion;
			[occlusionTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:occlusion bytesPerRow:width * 2];
			id<MTLTexture> emissionTexture = resources->emission;
			[emissionTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:emission bytesPerRow:width * 4];
			id<MTLTexture> heightTexture = resources->heightField;
			[heightTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:heightField bytesPerRow:width * 2];
			id<MTLTexture> participationTexture = resources->participation;
			[participationTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:participation bytesPerRow:width * 2];
			id<MTLTexture> oppositeFacingTexture = resources->oppositeFacing;
			[oppositeFacingTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:oppositeFacing bytesPerRow:width];
			id<MTLTexture> surfaceTexture = resources->surface;
			[surfaceTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:surfaceField bytesPerRow:width * sizeof(float) * 4];
			id<MTLTexture> reflectanceTexture = resources->reflectance;
			[reflectanceTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
				withBytes:reflectance bytesPerRow:width * sizeof(float) * 4];
			id<MTLTexture> visibilityBlocksTexture = resources->visibilityBlocks;
			if (!visibilityBlocksTexture || !resources->sourcePower)
				return false;
			id<MTLComputeCommandEncoder> visibilityBlocksEncoder = [commandBuffer computeCommandEncoder];
			if (!visibilityBlocksEncoder)
				return false;
			[visibilityBlocksEncoder setComputePipelineState:remasterVisibilityBlocksPipelineState];
			[visibilityBlocksEncoder setTexture:occlusionTexture atIndex:0];
			[visibilityBlocksEncoder setTexture:heightTexture atIndex:1];
			[visibilityBlocksEncoder setTexture:surfaceTexture atIndex:2];
			[visibilityBlocksEncoder setTexture:visibilityBlocksTexture atIndex:3];
			[visibilityBlocksEncoder dispatchThreadgroups:MTLSizeMake((visibilityBlocksTexture.width + 7) / 8,
				(visibilityBlocksTexture.height + 7) / 8, 1)
				threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
			[visibilityBlocksEncoder endEncoding];
			presentationTexture = resources->output;
			MTLTextureDescriptor *radianceDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
				width:width height:height mipmapped:NO];
			radianceDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
			if (measureGi)
				radianceDescriptor.storageMode = MTLStorageModeShared;
			id<MTLTexture> directTexture = measureGi ? [metalDevice newTextureWithDescriptor:radianceDescriptor] : resources->direct;
			// Keep every visible emitting pixel, with no proxy clustering or distance cutoff.
			std::vector<uint32_t> emitterPixels;
			for (uint32_t pixel = 0; pixel < static_cast<uint32_t>(width * height); pixel++)
				if (participation[pixel * 2] > 127 && emission[pixel * 4 + 3] &&
					(emission[pixel * 4] || emission[pixel * 4 + 1] || emission[pixel * 4 + 2]))
					emitterPixels.push_back(pixel);
			const uint32_t emitterCount = static_cast<uint32_t>(emitterPixels.size());
			const uint32_t emptyEmitter = 0;
			id<MTLBuffer> emitterBuffer = [metalDevice newBufferWithBytes:emitterCount ? emitterPixels.data() : &emptyEmitter
				length:std::max<size_t>(1, emitterCount) * sizeof(uint32_t) options:MTLResourceStorageModeShared];
			static uint32_t randomSeed = 0;
			RemasterLightingUniforms uniforms = { static_cast<uint32_t>(width), static_cast<uint32_t>(height),
				static_cast<uint32_t>(lightingView), 0, 0, 0, indirectRoughness, originalSceneContribution,
				heightPreviewMultiplier, 0.0f, 0,
					std::max<uint32_t>(1, samplesPerFrame), ++randomSeed, reflectanceBoost, { normalizedCameraDirection.x,
					normalizedCameraDirection.y, normalizedCameraDirection.z, 0.0f }, {}, {} };
			const uint32_t previewRange = remasterHeightPreviewRange.load(std::memory_order_relaxed);
			uniforms.heightPreviewRange = { static_cast<float>(previewRange & 0xffff),
				static_cast<float>(previewRange >> 16), 0.0f, 0.0f };
			const RemasterDebugLight light = GetRemasterDebugLight();
			if (light.enabled)
			{
				uniforms.debugPositionRadius = { light.x * width, light.y * height, light.height, light.radius };
				uniforms.debugColorIntensity = { std::pow(light.red, 2.2f), std::pow(light.green, 2.2f),
					std::pow(light.blue, 2.2f), light.intensity / 25.0f };
			}
			id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
			const auto directEncodeStarted = std::chrono::steady_clock::now();
			[computeEncoder setComputePipelineState:remasterLightingPipelineState];
			[computeEncoder setTexture:sourceTexture atIndex:0];
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
			directEncodeMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - directEncodeStarted).count();
			if (measureGi)
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
					[diagnosticEncoder setTexture:sourceTexture atIndex:0];
					[diagnosticEncoder setTexture:occlusionTexture atIndex:1];
					[diagnosticEncoder setTexture:surfaceTexture atIndex:2];
					[diagnosticEncoder setTexture:heightTexture atIndex:3];
					[diagnosticEncoder setTexture:participationTexture atIndex:4];
					[diagnosticEncoder setTexture:directTexture atIndex:5];
					[diagnosticEncoder setTexture:discardedPreviousIndirect atIndex:6];
					[diagnosticEncoder setTexture:diagnosticStageTextures[stage - 1] atIndex:7];
					[diagnosticEncoder setTexture:discardedNextIndirect atIndex:8];
					[diagnosticEncoder setTexture:oppositeFacingTexture atIndex:9];
					[diagnosticEncoder setTexture:visibilityBlocksTexture atIndex:10];
					[diagnosticEncoder setTexture:reflectanceTexture atIndex:11];
					[diagnosticEncoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
					[diagnosticEncoder setBuffer:emitterBuffer offset:0 atIndex:1];
					[diagnosticEncoder setBytes:&emitterCount length:sizeof(emitterCount) atIndex:2];
					[diagnosticEncoder dispatchThreads:MTLSizeMake(width, height, 1)
						threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
					[diagnosticEncoder endEncoding];
				}
				uniforms.diagnosticStage = 0;
			}
			{
				id<MTLTexture> bounceTextures[2] = { resources->bounce[0], resources->bounce[1] };
				id<MTLTexture> indirectTextures[2] = { resources->indirect[0], resources->indirect[1] };
				id<MTLTexture> directMeanTextures[2] = { resources->directMean[0], resources->directMean[1] };
				id<MTLTexture> finalIndirect = nil;
				id<MTLTexture> finalDirect = nil;
				const bool visibilityView = lightingView == RemasterLightingView::Visibility;
				const bool directOnly = lightingView == RemasterLightingView::DirectContribution && !measureGi;
				const uint32_t transportBounceCount = visibilityView || directOnly ? 0 : indirectBounceCount;
				// With no indirect transport, all samples are the same deterministic direct result.
					// The profile count now means connections per receiver within this frame.
					const uint32_t transportSampleCount = 1;
					const uint32_t connectionCount = uniforms.sampleCount;
				for (uint32_t sample = 0; sample < transportSampleCount; sample++)
				{
						uniforms.sampleIndex = sample;
						uniforms.passIndex = 0;
						uniforms.diagnosticStage = visibilityView ? 4 : 0;
						uniforms.sampleCount = 1;
						uniforms.padding = 1.0f; // Camera-independent Lambertian Direct source field.
					if (sample == 0)
					{
						finalDirect = measureGi ? [metalDevice newTextureWithDescriptor:radianceDescriptor] : directMeanTextures[0];
						diagnosticDirectTexture = measureGi ? finalDirect : nil;
						id<MTLComputeCommandEncoder> directEncoder = [commandBuffer computeCommandEncoder];
						const auto directEncodeStarted = std::chrono::steady_clock::now();
						[directEncoder setComputePipelineState:remasterIndirectPipelineState];
						[directEncoder setTexture:sourceTexture atIndex:0];
						[directEncoder setTexture:occlusionTexture atIndex:1];
						[directEncoder setTexture:surfaceTexture atIndex:2];
						[directEncoder setTexture:heightTexture atIndex:3];
						[directEncoder setTexture:participationTexture atIndex:4];
						[directEncoder setTexture:directTexture atIndex:5];
						[directEncoder setTexture:indirectTextures[1] atIndex:6];
						[directEncoder setTexture:finalDirect atIndex:7];
						[directEncoder setTexture:indirectTextures[0] atIndex:8];
						[directEncoder setTexture:oppositeFacingTexture atIndex:9];
						[directEncoder setTexture:visibilityBlocksTexture atIndex:10];
						[directEncoder setTexture:reflectanceTexture atIndex:11];
						[directEncoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
						[directEncoder setBuffer:emitterBuffer offset:0 atIndex:1];
						[directEncoder setBytes:&emitterCount length:sizeof(emitterCount) atIndex:2];
						[directEncoder dispatchThreads:MTLSizeMake(width, height, 1)
							threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
						[directEncoder endEncoding];
						directEncodeMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - directEncodeStarted).count();
					}

					// Direct is deterministic and need not be recomputed for each indirect sample.
					uniforms.diagnosticStage = 0;
					uniforms.sampleCount = connectionCount;
					id<MTLTexture> previousBounce = finalDirect;
					for (uint32_t bounce = 0; bounce < transportBounceCount; bounce++)
					{
						const uint32_t current = (bounce + 1) & 1;
						const uint32_t previous = current ^ 1;
						uniforms.passIndex = bounce + 1;
						id<MTLComputeCommandEncoder> leavesEncoder = [commandBuffer computeCommandEncoder];
						[leavesEncoder setComputePipelineState:remasterSourcePowerLeavesPipelineState];
						[leavesEncoder setTexture:previousBounce atIndex:0];
						[leavesEncoder setTexture:participationTexture atIndex:1];
						[leavesEncoder setBuffer:resources->sourcePower offset:0 atIndex:0];
						[leavesEncoder setBytes:&resources->sourceLeafCount length:sizeof(uint32_t) atIndex:1];
						[leavesEncoder dispatchThreads:MTLSizeMake(resources->sourceLeafCount, 1, 1)
							threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
						[leavesEncoder endEncoding];
						for (uint32_t firstNode = resources->sourceLeafCount / 2; firstNode; firstNode /= 2)
						{
							id<MTLComputeCommandEncoder> reduceEncoder = [commandBuffer computeCommandEncoder];
							[reduceEncoder setComputePipelineState:remasterSourcePowerReducePipelineState];
							[reduceEncoder setBuffer:resources->sourcePower offset:0 atIndex:0];
							[reduceEncoder setBytes:&firstNode length:sizeof(firstNode) atIndex:1];
							[reduceEncoder dispatchThreads:MTLSizeMake(firstNode, 1, 1)
								threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
							[reduceEncoder endEncoding];
						}
						id<MTLComputeCommandEncoder> bounceEncoder = [commandBuffer computeCommandEncoder];
						const auto indirectEncodeStarted = std::chrono::steady_clock::now();
						[bounceEncoder setComputePipelineState:remasterSampledIndirectPipelineState];
						[bounceEncoder setTexture:sourceTexture atIndex:0];
						[bounceEncoder setTexture:occlusionTexture atIndex:1];
						[bounceEncoder setTexture:surfaceTexture atIndex:2];
						[bounceEncoder setTexture:heightTexture atIndex:3];
						[bounceEncoder setTexture:participationTexture atIndex:4];
						[bounceEncoder setTexture:previousBounce atIndex:5];
						[bounceEncoder setTexture:indirectTextures[previous] atIndex:6];
						[bounceEncoder setTexture:bounceTextures[current] atIndex:7];
						[bounceEncoder setTexture:indirectTextures[current] atIndex:8];
						[bounceEncoder setTexture:visibilityBlocksTexture atIndex:10];
						[bounceEncoder setTexture:reflectanceTexture atIndex:11];
						[bounceEncoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
						[bounceEncoder setBuffer:resources->sourcePower offset:0 atIndex:1];
						[bounceEncoder setBytes:&resources->sourceLeafCount length:sizeof(uint32_t) atIndex:2];
						[bounceEncoder dispatchThreads:MTLSizeMake(width, height, 1)
							threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
						[bounceEncoder endEncoding];
						indirectEncodeMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - indirectEncodeStarted).count();
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
					finalIndirect = indirectTextures[transportBounceCount & 1];
				}
				if (lightingView == RemasterLightingView::Composite ||
					visibilityView ||
					lightingView == RemasterLightingView::DirectContribution ||
					lightingView == RemasterLightingView::Difference ||
					lightingView == RemasterLightingView::IndirectContribution ||
					lightingView == RemasterLightingView::DirectAndIndirectContribution)
				{
					id<MTLComputeCommandEncoder> compositeEncoder = [commandBuffer computeCommandEncoder];
					const auto compositeEncodeStarted = std::chrono::steady_clock::now();
					[compositeEncoder setComputePipelineState:remasterCompositePipelineState];
					[compositeEncoder setTexture:sourceTexture atIndex:0];
					[compositeEncoder setTexture:directTexture atIndex:1];
					[compositeEncoder setTexture:finalDirect atIndex:2];
					[compositeEncoder setTexture:finalIndirect atIndex:3];
					[compositeEncoder setTexture:presentationTexture atIndex:4];
					[compositeEncoder setTexture:surfaceTexture atIndex:5];
					[compositeEncoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
					[compositeEncoder dispatchThreads:MTLSizeMake(width, height, 1)
						threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
					[compositeEncoder endEncoding];
					compositeEncodeMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - compositeEncodeStarted).count();
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

		if (panelMetrics)
		{
			RemasterState &state = S9xRemasterState();
			std::lock_guard<std::mutex> metricsLock(state.performanceMetricsMutex);
			state.performanceMetrics.lightingPreparationMs =
				std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - lightingPreparationStarted).count();
			state.performanceMetrics.directEncodeMs = directEncodeMs;
			state.performanceMetrics.indirectEncodeMs = indirectEncodeMs;
			state.performanceMetrics.accumulationEncodeMs = accumulationEncodeMs;
			state.performanceMetrics.compositeEncodeMs = compositeEncodeMs;
		}

		// Only retained Metal objects and copied values cross this boundary. Frame
		// pixels and temporary CPU fields have already been uploaded on the producer.
		CAMetalLayer *presentationLayer = metalLayer;
		id<MTLRenderPipelineState> presentationPipeline = metalPipelineState;
		const int presentationVideoMode = videoMode;
		const CGFloat presentationScale = metalLayer.contentsScale;
		const bool logFrameMetrics = std::getenv("S9X_REMASTER_FRAME_METRICS") != nullptr;
		const bool measureFrames = panelMetrics || logFrameMetrics;
		const auto queuedAt = std::chrono::steady_clock::now();
		bool (^present)(void) = ^bool {
			@autoreleasepool {
				RemasterResourceSlotGuard queuedGuard(waitForCompletion ? -1 : resourceSlot);
				const auto drawableStart = std::chrono::steady_clock::now();
				id<CAMetalDrawable> drawable = [presentationLayer nextDrawable];
				const auto drawableEnd = std::chrono::steady_clock::now();
				if (!drawable)
					return false;
				MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
				renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
				renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
				renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
				id<MTLRenderCommandEncoder> renderEncoder =
					[commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
				if (!renderEncoder)
					return false;
				renderEncoder.label = @"Snes9x render encoder";
				vector_uint2 viewportSize = { static_cast<unsigned int>(layerSize.width), static_cast<unsigned int>(layerSize.height) };
				[renderEncoder setViewport:(MTLViewport){0.0, 0.0, layerSize.width * presentationScale,
					layerSize.height * presentationScale, -1.0, 1.0}];
				[renderEncoder setRenderPipelineState:presentationPipeline];
				[renderEncoder setVertexBytes:verticies.data() length:sizeof(verticies) atIndex:0];
				[renderEncoder setVertexBytes:&viewportSize length:sizeof(viewportSize) atIndex:1];
				[renderEncoder setFragmentTexture:presentationTexture atIndex:0];
				[renderEncoder setFragmentBytes:&presentationVideoMode length:sizeof(presentationVideoMode) atIndex:1];
				[renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
				[renderEncoder endEncoding];
				[commandBuffer presentDrawable:drawable];
				if (measureFrames)
				{
					const double queuedMs = std::chrono::duration<double, std::milli>(drawableStart - queuedAt).count();
					const double drawableMs = std::chrono::duration<double, std::milli>(drawableEnd - drawableStart).count();
					[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> completed) {
						double gpuMs = 0.0;
						if (@available(macOS 10.15, *))
							if (completed.GPUEndTime > completed.GPUStartTime)
								gpuMs = (completed.GPUEndTime - completed.GPUStartTime) * 1000.0;
						const uint64_t dropped = droppedRemasterPresentations.exchange(0);
						if (panelMetrics)
						{
							RemasterState &state = S9xRemasterState();
							std::lock_guard<std::mutex> metricsLock(state.performanceMetricsMutex);
							state.performanceMetrics.presentationQueueMs = queuedMs;
							state.performanceMetrics.drawableMs = drawableMs;
							state.performanceMetrics.gpuFrameMs = gpuMs;
							state.performanceMetrics.droppedPresentations = dropped;
						}
						if (logFrameMetrics)
							if (@available(macOS 10.15, *))
								NSLog(@"Remaster frame: queued=%.3fms drawable=%.3fms gpu=%.3fms dropped=%llu",
									queuedMs, drawableMs, gpuMs, static_cast<unsigned long long>(dropped));
					}];
					if (panelMetrics)
						if (@available(macOS 10.15.4, *))
						{
							[drawable addPresentedHandler:^(id<MTLDrawable> presentedDrawable) {
								const double presentedTime = presentedDrawable.presentedTime;
								if (presentedTime <= 0.0)
									return;
								RemasterState &state = S9xRemasterState();
								std::lock_guard<std::mutex> metricsLock(state.performanceMetricsMutex);
								if (state.performanceLastPresentedTime > 0.0 && presentedTime > state.performanceLastPresentedTime)
									state.performanceMetrics.presentedFps =
										1.0 / (presentedTime - state.performanceLastPresentedTime);
								state.performanceLastPresentedTime = presentedTime;
							}];
						}
				}
				if (!waitForCompletion)
				{
					const int completedSlot = resourceSlot;
					[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
						ReleaseRemasterResourceSlot(completedSlot);
					}];
					queuedGuard.disarm();
				}
				[commandBuffer commit];
				return true;
			}
		};
		if (!waitForCompletion)
		{
			// Slots bound queued + GPU work to three frames. Drawable starvation can
			// drop video frames, but can no longer stop emulation or audio production.
			resourceGuard.disarm();
			dispatch_async(presentationQueue, ^{ present(); });
			return true;
		}
		__block bool presented = false;
		dispatch_sync(presentationQueue, ^{ presented = present(); });
		if (!presented)
			return false;
		[commandBuffer waitUntilCompleted];
		if (measureGi && diagnosticDirectTexture)
		{
			if (commandBuffer.status != MTLCommandBufferStatusCompleted)
				NSLog(@"Remaster GI metrics unavailable: %@", commandBuffer.error);
			else
			{
				NSLog(@"Remaster GI config: bounces=%u roughness=%.6g camera=(%.6g, %.6g, %.6g) original=%.6g samples=%u accumulation=%s",
					unsigned(indirectBounceCount), indirectRoughness, normalizedCameraDirection.x,
					normalizedCameraDirection.y, normalizedCameraDirection.z,
					originalSceneContribution, unsigned(samplesPerFrame), sampleAccumulation ? "on" : "off");
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
