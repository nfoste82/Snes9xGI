// Run from the repository root (no Xcode target):
// xcrun clang++ -std=c++17 -fobjc-arc -Wall -Wextra macosx/remaster-lighting-test.mm \
//   -framework Foundation -framework Metal -o /tmp/remaster-lighting-test
// /tmp/remaster-lighting-test
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "../remaster/indirect_lighting_reference.h"
#include "../remaster/surface_mesh_material.h"

// Match the constant-buffer ABI in shaders.metal, including float3 alignment.
struct Light
{
    simd_float3 position;
    float radius;
    float intensity;
    simd_float3 color;
};
struct Uniforms
{
	uint32_t width, height, view, lightCount, passIndex, diagnosticStage;
	float indirectRoughness, originalSceneContribution;
	float heightPreviewMultiplier, padding;
	uint32_t sampleIndex, sampleCount, randomSeed;
	float reflectanceBoost;
    simd_float4 cameraDirection;
    simd_float4 debugPositionRadius;
    simd_float4 debugColorIntensity;
	simd_float4 heightPreviewRange;
};
static_assert(sizeof(Light) == 48 && offsetof(Light, color) == 32, "Metal light ABI");
static_assert(sizeof(Uniforms) == 128 && offsetof(Uniforms, cameraDirection) == 64, "Metal uniforms ABI");

constexpr unsigned width = 128, height = 17, receiverX = 4, receiverY = 8;
constexpr unsigned receiver = receiverY * width + receiverX;
constexpr unsigned emitter = receiverY * width + 26;
enum Field { Source, Occlusion, Surface, Height, Participation, PreviousBounce,
			 PreviousIndirect, NextBounce, NextIndirect, Emission, Output, Direct,
			 OppositeFacing, Reflectance, FieldCount };
using Image = std::vector<simd_float4>;
using Scene = std::array<Image, FieldCount>;
struct Result { simd_float4 bounce, accumulated; };

static uint16_t floatToHalf(float value)
{
    __fp16 half = value;
    uint16_t bits;
    std::memcpy(&bits, &half, sizeof(bits));
    return bits;
}

static float halfToFloat(uint16_t bits)
{
    __fp16 half;
    std::memcpy(&half, &bits, sizeof(bits));
    return half;
}

int main()
{
    @autoreleasepool
    {
        try
        {
            auto require = [](bool ok, NSString *message) {
                if (!ok)
                    throw std::runtime_error(message ? message.UTF8String : "Metal operation failed");
            };
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            require(device != nil, @"No Metal device available");
            NSError *error = nil;
            NSString *source = [NSString stringWithContentsOfFile:@"macosx/shaders.metal"
                encoding:NSUTF8StringEncoding error:&error];
            require(source != nil, error.localizedDescription);
            id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
            require(library != nil, error.localizedDescription);
            id<MTLComputePipelineState> direct = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterDirectLighting"] error:&error];
            require(direct != nil, error.localizedDescription);
            MTLFunctionConstantValues *referenceConstants = [MTLFunctionConstantValues new];
            bool unprepared = false;
            [referenceConstants setConstantValue:&unprepared type:MTLDataTypeBool atIndex:0];
            id<MTLComputePipelineState> indirect = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterIndirectBounce" constantValues:referenceConstants error:&error] error:&error];
            require(indirect != nil, error.localizedDescription);
            id<MTLComputePipelineState> prepareDirect = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterPrepareDirectSamples"] error:&error];
            require(prepareDirect != nil, error.localizedDescription);
            MTLFunctionConstantValues *directConstants = [MTLFunctionConstantValues new];
            bool specialized = true;
            [directConstants setConstantValue:&specialized type:MTLDataTypeBool atIndex:0];
            id<MTLComputePipelineState> directTransport = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterIndirectBounce" constantValues:directConstants error:&error] error:&error];
            require(directTransport != nil, error.localizedDescription);
            id<MTLComputePipelineState> powerLeaves = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterBuildSourcePowerLeaves"] error:&error];
            id<MTLComputePipelineState> powerReduce = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterReduceSourcePower"] error:&error];
            id<MTLComputePipelineState> sampledIndirect = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterSampledIndirectBounce"] error:&error];
            require(powerLeaves && powerReduce && sampledIndirect, error.localizedDescription);
            id<MTLComputePipelineState> buildVisibilityBlocks = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterBuildVisibilityBlocks"] error:&error];
            require(buildVisibilityBlocks != nil, error.localizedDescription);
            MTLCompileOptions *referenceOptions = [MTLCompileOptions new];
            referenceOptions.preprocessorMacros = @{@"REMASTER_REFERENCE_VISIBILITY": @1};
            id<MTLLibrary> referenceLibrary = [device newLibraryWithSource:source options:referenceOptions error:&error];
            require(referenceLibrary != nil, error.localizedDescription);
            id<MTLComputePipelineState> indirectReference = [device newComputePipelineStateWithFunction:
                [referenceLibrary newFunctionWithName:@"remasterIndirectBounce" constantValues:referenceConstants error:&error] error:&error];
            require(indirectReference != nil, error.localizedDescription);
            id<MTLComputePipelineState> composite = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterCompositeLighting"] error:&error];
            require(composite != nil, error.localizedDescription);
            id<MTLComputePipelineState> accumulate = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterAccumulateSamples"] error:&error];
            require(accumulate != nil, error.localizedDescription);
            id<MTLComputePipelineState> highlight = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterSelectedTileHighlight"] error:&error];
            require(highlight != nil, error.localizedDescription);
            id<MTLCommandQueue> queue = [device newCommandQueue];
            require(queue != nil, @"Could not create Metal command queue");
            std::printf("Device: %s; runtime shaders: macosx/shaders.metal; RGBA32Float and production RGBA16Float\n",
                device.name.UTF8String);

            auto scene = [] {
                Scene s;
                for (auto &image : s)
                    image.assign(width * height, simd_float4{0, 0, 0, 0});
                s[Source].assign(width * height, simd_float4{1, 1, 1, 1});
                s[Surface].assign(width * height, simd_float4{8, 0, 0, 0});
                // Only the receiver and a distant one-pixel radiance patch participate.
                s[Surface][receiver] = {8, 0.98f, 0, 0.198997f};
                s[Surface][emitter] = {8, -0.98f, 0, 0.198997f};
                s[Participation][receiver] = s[Participation][emitter] = {1, 1, 0, 0};
                s[PreviousBounce][emitter] = {1, 0.5f, 0.25f, 1};
                s[PreviousIndirect].assign(width * height, simd_float4{91, 37, 13, 1});
                return s;
            };
            auto blocker = [](Scene &s, unsigned distance, float coverage, bool authored, float z) {
                unsigned pixel = receiver + distance;
                s[Occlusion][pixel] = {coverage, 1, 0, 0};
                s[Height][pixel] = {z, authored ? 1.0f : 0.0f, 0, 0};
                s[Surface][pixel] = {z, 0, 0, 0};
            };
            auto run = [&](const Scene &s, bool bounce, unsigned passIndex = 0,
                           float lightZ = 8.0f, int selectedPixel = -1, Scene *readback = nullptr,
                            bool productionRadiance = false, unsigned diagnosticStage = 0, float indirectRoughness = 1.0f,
                            unsigned sampleIndex = 0, unsigned sampleCount = 1, unsigned randomSeed = 1,
                             simd_float3 cameraDirection = {0, 0, -1}, int compositeView = -1,
                              const Light *debugLight = nullptr, bool referenceVisibility = false,
                               bool sampled = false, bool lambertianDirect = false, float reflectanceBoost = 0.0f,
                               const std::vector<RemasterSurfaceMesh::Cell> *customMesh = nullptr) {
                const Field directBindings[] = {Source, Occlusion, Output, Emission,
                    Height, Surface, Direct, Participation, OppositeFacing};
				const Field indirectBindings[] = {Source, Occlusion, Surface, Height,
					Participation, PreviousBounce, PreviousIndirect, NextBounce, NextIndirect, OppositeFacing, Reflectance};
                // Direct holds the emission seed; NextBounce holds the saved first collision.
                const Field compositeBindings[] = {Source, Direct, NextBounce, NextIndirect, Output, Surface};
                const bool composing = compositeView >= 0;
                const Field *bindings = composing ? compositeBindings : bounce ? indirectBindings : directBindings;
				unsigned count = composing ? 6 : bounce ? 11 : 9;
				id<MTLTexture> textures[11];
                for (unsigned i = 0; i < count; ++i)
                {
                    const bool radiance = bindings[i] == PreviousBounce || bindings[i] == PreviousIndirect ||
                        bindings[i] == NextBounce || bindings[i] == NextIndirect || bindings[i] == Direct;
                    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
                        texture2DDescriptorWithPixelFormat:productionRadiance && radiance ?
                            MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA32Float
                        width:width height:height mipmapped:NO];
                    descriptor.storageMode = MTLStorageModeShared;
                    descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
                    textures[i] = [device newTextureWithDescriptor:descriptor];
                    require(textures[i] != nil, @"Shared read/write texture unavailable");
                    if (productionRadiance && radiance)
                    {
                        std::vector<uint16_t> encoded(width * height * 4);
                        for (unsigned pixel = 0; pixel < width * height; ++pixel)
                            for (unsigned channel = 0; channel < 4; ++channel)
                                encoded[pixel * 4 + channel] = floatToHalf(s[bindings[i]][pixel][channel]);
                        [textures[i] replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
                            withBytes:encoded.data() bytesPerRow:width * 4 * sizeof(uint16_t)];
                    }
                    else
                        [textures[i] replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
                            withBytes:s[bindings[i]].data() bytesPerRow:width * sizeof(simd_float4)];
                }
                Uniforms uniforms = {width, height, composing ? unsigned(compositeView) : 0, 1, passIndex, diagnosticStage,
					indirectRoughness, 0.65f, 8.0f, lambertianDirect ? 1.0f : 0.0f, sampleIndex, sampleCount, randomSeed, reflectanceBoost,
                    {cameraDirection.x, cameraDirection.y, cameraDirection.z, 0}, {}, {}};
                if (debugLight)
                {
                    uniforms.debugPositionRadius = {debugLight->position.x, debugLight->position.y,
                        debugLight->position.z, debugLight->radius};
                    uniforms.debugColorIntensity = {std::pow(debugLight->color.x, 2.2f),
                        std::pow(debugLight->color.y, 2.2f), std::pow(debugLight->color.z, 2.2f),
                        debugLight->intensity / 25.0f};
                }
                Light light = {};
                light.position = {26.5f, receiverY + 0.5f, lightZ};
                light.radius = 64;
                light.intensity = 1;
                light.color = {1, 0.5f, 0.25f};
                id<MTLCommandBuffer> command = [queue commandBuffer];
                id<MTLTexture> visibilityBlocks = nil;
                id<MTLBuffer> sourcePower = nil;
				std::vector<RemasterSurfaceMesh::Cell> mesh(width * height);
				for (unsigned p = 0; p < width * height; p++)
					for (float &corner : mesh[p].corners) corner = s[Surface][p].x;
				if (customMesh) mesh = *customMesh;
				id<MTLBuffer> meshBuffer = [device newBufferWithBytes:mesh.data() length:mesh.size() * sizeof(mesh[0])
					options:MTLResourceStorageModeShared];
                uint32_t leafCount = 1;
                if (bounce && !composing)
                {
                    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
                        texture2DDescriptorWithPixelFormat:MTLPixelFormatRG32Float
                        width:(width + 7) / 8 height:(height + 7) / 8 mipmapped:NO];
                    descriptor.storageMode = MTLStorageModePrivate;
                    descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
                    visibilityBlocks = [device newTextureWithDescriptor:descriptor];
                    require(visibilityBlocks != nil, @"Visibility block texture unavailable");
                    id<MTLComputeCommandEncoder> blocks = [command computeCommandEncoder];
                    require(blocks != nil, @"Could not create visibility block encoder");
                    [blocks setComputePipelineState:buildVisibilityBlocks];
                    [blocks setTexture:textures[1] atIndex:0];
                    [blocks setTexture:textures[3] atIndex:1];
                    [blocks setTexture:textures[2] atIndex:2];
                    [blocks setTexture:visibilityBlocks atIndex:3];
					[blocks setBuffer:meshBuffer offset:0 atIndex:0];
                    [blocks dispatchThreads:MTLSizeMake((width + 7) / 8, (height + 7) / 8, 1)
                        threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
                    [blocks endEncoding];
                }
                if (bounce && sampled && !composing)
                {
                    while (leafCount < width * height)
                        leafCount *= 2;
                    sourcePower = [device newBufferWithLength:leafCount * 2 * sizeof(float)
                        options:MTLResourceStorageModeShared];
                    require(sourcePower != nil, @"Source power buffer unavailable");
                    id<MTLComputeCommandEncoder> leaves = [command computeCommandEncoder];
                    [leaves setComputePipelineState:powerLeaves];
                    [leaves setTexture:textures[5] atIndex:0];
                    [leaves setTexture:textures[4] atIndex:1];
                    [leaves setBuffer:sourcePower offset:0 atIndex:0];
                    [leaves setBytes:&leafCount length:sizeof(leafCount) atIndex:1];
                    [leaves dispatchThreads:MTLSizeMake(leafCount, 1, 1)
                        threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
                    [leaves endEncoding];
                    for (uint32_t firstNode = leafCount / 2; firstNode; firstNode /= 2)
                    {
                        id<MTLComputeCommandEncoder> reduce = [command computeCommandEncoder];
                        [reduce setComputePipelineState:powerReduce];
                        [reduce setBuffer:sourcePower offset:0 atIndex:0];
                        [reduce setBytes:&firstNode length:sizeof(firstNode) atIndex:1];
                        [reduce dispatchThreads:MTLSizeMake(firstNode, 1, 1)
                            threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
                        [reduce endEncoding];
                    }
                }
                id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
                require(encoder != nil, @"Could not create compute encoder");
                [encoder setComputePipelineState:composing ? composite :
                    bounce ? (sampled ? sampledIndirect : referenceVisibility ? indirectReference :
                        passIndex == 0 && lambertianDirect && diagnosticStage == 0 ? directTransport : indirect) : direct];
				for (unsigned i = 0; i < count; ++i)
					[encoder setTexture:textures[i] atIndex:bindings[i] == Reflectance ? 11 : i];
                if (bounce && !composing)
                    [encoder setBuffer:meshBuffer offset:0 atIndex:3];
                if (bounce && !composing)
                    [encoder setTexture:visibilityBlocks atIndex:10];
                [encoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
                if (!bounce && !composing)
                    [encoder setBytes:&light length:sizeof(light) atIndex:1];
                if (bounce && !composing && sampled)
                {
                    [encoder setBuffer:sourcePower offset:0 atIndex:1];
                    [encoder setBytes:&leafCount length:sizeof(leafCount) atIndex:2];
                }
                else if (bounce && !composing)
                {
                    std::vector<uint32_t> emitters;
                    for (uint32_t pixel = 0; pixel < width * height; ++pixel)
                        if (s[Participation][pixel].x > 0.5f &&
                            (s[PreviousBounce][pixel].x > 0 || s[PreviousBounce][pixel].y > 0 || s[PreviousBounce][pixel].z > 0))
                            emitters.push_back(pixel);
                    const bool prepared = passIndex == 0 && !referenceVisibility;
                    std::vector<simd_uint2> sources;
                    if (prepared)
                        for (uint32_t pixel : emitters)
                            for (uint32_t sample = 0, count = mesh[pixel].emissionDepth > 0 ? 4 : 1; sample < count; sample++)
                                sources.push_back(simd_uint2{pixel, sample});
                    const uint32_t emitterCount = static_cast<uint32_t>(prepared ? sources.size() : emitters.size());
                    if (emitters.empty())
                        emitters.push_back(0);
                    id<MTLBuffer> emitterBuffer = [device newBufferWithBytes:emitters.data()
                        length:emitters.size() * sizeof(uint32_t) options:MTLResourceStorageModeShared];
                    id<MTLBuffer> sampleBuffer = [device newBufferWithLength:std::max<size_t>(1, sources.size()) * 64
                        options:MTLResourceStorageModePrivate];
                    if (prepared)
                    {
                        // A separate command completes source preparation before this test's
                        // already-open transport encoder; the app uses ordered encoders.
                        if (sources.empty()) sources.push_back(simd_uint2{0, 0});
                        emitterBuffer = [device newBufferWithBytes:sources.data() length:sources.size() * sizeof(simd_uint2)
                            options:MTLResourceStorageModeShared];
                        if (emitterCount)
                        {
                            id<MTLCommandBuffer> preparation = [queue commandBuffer];
                            id<MTLComputeCommandEncoder> prepare = [preparation computeCommandEncoder];
                            [prepare setComputePipelineState:prepareDirect];
                            [prepare setTexture:textures[1] atIndex:0];
                            [prepare setTexture:textures[3] atIndex:1];
                            [prepare setTexture:textures[2] atIndex:2];
                            [prepare setTexture:textures[5] atIndex:3];
                            [prepare setBuffer:emitterBuffer offset:0 atIndex:0];
                            [prepare setBuffer:sampleBuffer offset:0 atIndex:1];
                            [prepare setBuffer:meshBuffer offset:0 atIndex:2];
                            [prepare setBytes:&emitterCount length:sizeof(emitterCount) atIndex:3];
                            [prepare dispatchThreads:MTLSizeMake(emitterCount, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
                            [prepare endEncoding];
                            [preparation commit];
                            [preparation waitUntilCompleted];
                            require(preparation.status == MTLCommandBufferStatusCompleted, preparation.error.localizedDescription);
                        }
                        // padding retains the existing Lambertian/non-Lambertian switch.
                        uniforms.padding = lambertianDirect ? 2.0f : -2.0f;
                        [encoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
                    }
                    [encoder setBuffer:sampleBuffer offset:0 atIndex:4];
                    [encoder setBuffer:emitterBuffer offset:0 atIndex:1];
                    [encoder setBytes:&emitterCount length:sizeof(emitterCount) atIndex:2];
                }
                [encoder dispatchThreadgroups:MTLSizeMake(width, height, 1)
                    threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
                [encoder endEncoding];
                id<MTLTexture> presentation = textures[composing ? 4 : bounce ? 8 : 2];
                if (selectedPixel >= 0)
                {
                    std::array<uint8_t, width * height> mask = {};
                    mask[selectedPixel] = 1;
                    MTLTextureDescriptor *maskDescriptor = [MTLTextureDescriptor
                        texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                        width:width height:height mipmapped:NO];
                    id<MTLTexture> maskTexture = [device newTextureWithDescriptor:maskDescriptor];
                    [maskTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
                        withBytes:mask.data() bytesPerRow:width];
                    MTLTextureDescriptor *highlightDescriptor = [MTLTextureDescriptor
                        texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                        width:width height:height mipmapped:NO];
                    highlightDescriptor.storageMode = MTLStorageModeShared;
                    highlightDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
                    id<MTLTexture> highlighted = [device newTextureWithDescriptor:highlightDescriptor];
                    id<MTLComputeCommandEncoder> overlay = [command computeCommandEncoder];
                    [overlay setComputePipelineState:highlight];
                    [overlay setTexture:presentation atIndex:0];
                    [overlay setTexture:maskTexture atIndex:1];
                    [overlay setTexture:highlighted atIndex:2];
                    [overlay dispatchThreadgroups:MTLSizeMake((width + 7) / 8, (height + 7) / 8, 1)
                        threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
                    [overlay endEncoding];
                    presentation = highlighted;
                }
                [command commit];
                [command waitUntilCompleted];
                require(command.status == MTLCommandBufferStatusCompleted,
                    command.error.localizedDescription);
                Result result;
                MTLRegion region = MTLRegionMake2D(receiverX, receiverY, 1, 1);
                const unsigned radianceBinding = composing ? 2 : bounce ? 7 : 6;
                if (productionRadiance)
                {
                    uint16_t encoded[4];
                    [textures[radianceBinding] getBytes:encoded bytesPerRow:sizeof(encoded)
                        fromRegion:region mipmapLevel:0];
                    for (unsigned channel = 0; channel < 4; ++channel)
                        result.bounce[channel] = halfToFloat(encoded[channel]);
                }
                else
                    [textures[radianceBinding] getBytes:&result.bounce bytesPerRow:sizeof(simd_float4)
                        fromRegion:region mipmapLevel:0];
                if (presentation.pixelFormat == MTLPixelFormatRGBA16Float)
                {
                    uint16_t encoded[4];
                    [presentation getBytes:encoded bytesPerRow:sizeof(encoded)
                        fromRegion:region mipmapLevel:0];
                    for (unsigned channel = 0; channel < 4; ++channel)
                        result.accumulated[channel] = halfToFloat(encoded[channel]);
                }
                else
                    [presentation getBytes:&result.accumulated bytesPerRow:sizeof(simd_float4)
                        fromRegion:region mipmapLevel:0];
                if (readback)
                {
                    for (unsigned binding = 0; binding < count; ++binding)
                    {
                        const Field field = bindings[binding];
                        if (composing ? field != Output : bounce ?
                            (field != NextBounce && field != NextIndirect) : (field != Direct && field != Output))
                            continue;
                        if (textures[binding].pixelFormat == MTLPixelFormatRGBA16Float)
                        {
                            std::vector<uint16_t> encoded(width * height * 4);
                            [textures[binding] getBytes:encoded.data() bytesPerRow:width * 4 * sizeof(uint16_t)
                                fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
                            for (unsigned pixel = 0; pixel < width * height; ++pixel)
                                for (unsigned channel = 0; channel < 4; ++channel)
                                    (*readback)[field][pixel][channel] = halfToFloat(encoded[pixel * 4 + channel]);
                        }
                        else
                            [textures[binding] getBytes:(*readback)[field].data() bytesPerRow:width * sizeof(simd_float4)
                                fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
                    }
                }
                return result;
            };
            auto average = [&](simd_float4 first, simd_float4 second) {
                MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
                    texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:1 height:1 mipmapped:NO];
                descriptor.storageMode = MTLStorageModeShared;
                descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
                id<MTLTexture> sample = [device newTextureWithDescriptor:descriptor];
                id<MTLTexture> previous = [device newTextureWithDescriptor:descriptor];
                id<MTLTexture> output = [device newTextureWithDescriptor:descriptor];
                [sample replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&second bytesPerRow:sizeof(second)];
                [previous replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&first bytesPerRow:sizeof(first)];
				Uniforms uniforms = {1, 1, 0, 0, 0, 0, 1, 0.65f, 8.0f, 0.0f, 1, 2, 1, 0.0f,
                    {0, 0, -1, 0}, {}, {}};
                id<MTLCommandBuffer> command = [queue commandBuffer];
                id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
                [encoder setComputePipelineState:accumulate];
                [encoder setTexture:sample atIndex:0];
                [encoder setTexture:previous atIndex:1];
                [encoder setTexture:output atIndex:2];
                [encoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
                [encoder dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
                [encoder endEncoding];
                [command commit];
                [command waitUntilCompleted];
                require(command.status == MTLCommandBufferStatusCompleted, command.error.localizedDescription);
                simd_float4 result;
                [output getBytes:&result bytesPerRow:sizeof(result) fromRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0];
                return result;
            };

            unsigned checks = 0, failures = 0;
            auto check = [&](const char *name, bool ok, simd_float4 actual, simd_float4 reference) {
                ++checks;
                if (!ok)
                {
                    ++failures;
                    std::fprintf(stderr, "FAIL %s: RGB=(%.9g, %.9g, %.9g), reference=(%.9g, %.9g, %.9g)\n",
                        name, actual.x, actual.y, actual.z, reference.x, reference.y, reference.z);
                }
            };
            auto near = [&](const char *name, simd_float4 actual, simd_float4 expected) {
                bool ok = true;
                for (unsigned c = 0; c < 3; ++c)
                    ok &= std::isfinite(actual[c]) &&
                        std::fabs(actual[c] - expected[c]) <= 1e-6f + std::fabs(expected[c]) * 1e-5f;
                check(name, ok, actual, expected);
            };
            auto positive = [&](const char *name, simd_float4 actual) {
                bool ok = true;
                for (unsigned c = 0; c < 3; ++c)
                    ok &= std::isfinite(actual[c]) && actual[c] > 1e-5f;
                check(name, ok, actual, {});
            };
            auto dimmed = [&](const char *name, simd_float4 actual, simd_float4 unblocked) {
                bool ok = true;
                for (unsigned c = 0; c < 3; ++c)
                    ok &= std::isfinite(actual[c]) && actual[c] > 1e-6f && actual[c] < unblocked[c] * 0.99f;
                check(name, ok, actual, unblocked);
            };

            Scene s = scene();
            near("direct field excludes generated point-light proxies", run(s, false).bounce, {});
            s[Emission][receiver] = {0.25f, 0.5f, 0.75f, 50.0f / 255.0f};
            positive("direct field retains per-pixel emission seed", run(s, false).bounce);

            // Authored normal(255,128,128) retains a small camera-facing +Z.
            // Put the source to the left: it faces the receiver, but the receiver faces away.
            constexpr unsigned wallSource = receiver - 2;
            const simd_float3 wallNormal = simd_normalize(simd_float3{1, 1.0f / 255, 1.0f / 255});
            Scene wall = scene();
            wall[Participation][emitter] = {};
            wall[PreviousBounce].assign(width * height, simd_float4{});
            wall[Surface][receiver] = wall[Surface][wallSource] =
                {8, wallNormal.x, wallNormal.y, wallNormal.z};
            wall[Participation][wallSource] = {1, 1, 0, 0};
            wall[PreviousBounce][wallSource] = {1, 0.5f, 0.25f, 1};
            near("co-oriented walls reject direct collision with opposite facing off", run(wall, true).bounce, {});
            wall[OppositeFacing][receiver].x = 1;
            const Result oppositeWall = run(wall, true);
            positive("co-oriented walls receive direct collision with opposite facing on", oppositeWall.bounce);
            near("opposite-facing first collision does not accumulate indirect", oppositeWall.accumulated, {});
            s = wall;
            s[OppositeFacing][receiver].x = 0;
            s[Surface][receiver].y *= -1;
            s[Surface][receiver].z *= -1;
            near("opposite receiver uses mirrored XY normal including Oren-Nayar response",
                oppositeWall.bounce, run(s, true).bounce);
            near("passIndex 1 ignores opposite receiver toggle", run(wall, true, 1).bounce, {});
            wall[OppositeFacing][receiver].x = 0;
            near("passIndex 1 stays black with toggle off", run(wall, true, 1).bounce, {});
            wall[OppositeFacing][receiver].x = 1;
            s = wall;
            s[Surface][wallSource].y *= -1;
            s[OppositeFacing][wallSource].x = 1;
            near("opposite facing never rescues a rejected source cosine", run(s, true).bounce, {});

            for (bool half : {false, true})
            {
                simd_float4 collisions[2] = {};
                for (unsigned level = 0; level < 2; ++level)
                {
                    const float intensity = level == 0 ? 10.0f : 200.0f;
                    s = wall;
                    // Discard synthetic radiance and feed the actual emission-kernel readback.
                    s[PreviousBounce].assign(width * height, simd_float4{});
                    s[Emission][wallSource] = {1, 0.75f, 0.5f, intensity / 255.0f};
                    run(s, false, 0, 8, -1, &s, half);
                    positive("emission readback contains authored seed", s[Direct][wallSource]);
                    near("emission seed does not contain receiver lighting", s[Direct][receiver], {});
                    s[PreviousBounce] = s[Direct];
                    const Result collision = run(s, true, 0, 8, -1, &s, half);
                    collisions[level] = collision.bounce;
                    positive("actual emission seed illuminates co-oriented receiver", collision.bounce);
                    bool noIndirect = true;
                    for (const auto &pixel : s[NextIndirect])
                        noIndirect &= pixel.x == 0 && pixel.y == 0 && pixel.z == 0;
                    check("mandatory collision clears entire indirect field", noIndirect, collision.accumulated, {});
                    s[OppositeFacing][receiver].x = 0;
                    near("actual emission collision stays black with opposite facing off",
                        run(s, true, 0, 8, -1, nullptr, half).bounce, {});
                }
                bool linear = true;
                for (unsigned c = 0; c < 3; ++c)
                    linear &= std::isfinite(collisions[1][c]) && collisions[0][c] > 0 &&
                        std::fabs(collisions[1][c] / collisions[0][c] - 20.0f) < (half ? 0.08f : 0.001f);
                check(half ? "RGBA16Float intensity 10 to 200 scales collision by 20" :
                    "intensity 10 to 200 scales collision by 20", linear, collisions[1], collisions[0] * 20);
            }

            s = scene();
            Result clear = run(s, true);
            positive("indirect distant mutually facing radiance patch", clear.bounce);

            for (bool half : {false, true})
            {
                Scene visibility = scene();
                visibility[Emission][emitter] = {1, 0.75f, 0.5f, 10.0f / 255.0f};
                run(visibility, false, 0, 8, -1, &visibility, half);
                visibility[PreviousBounce] = visibility[Direct];
                auto visible = [&] {
                    return run(visibility, true, 0, 8, -1, &visibility, half, 4);
                };
                near("unblocked emission path has full visibility", visible().bounce, simd_float4{1, 1, 1, 1});
                near("visibility diagnostic leaves indirect empty", visible().accumulated, {});
                visibility[Surface][receiver] = visibility[Surface][emitter] = {8, 0, 0, 1};
                near("coplanar normals still reject actual direct light",
                    run(visibility, true, 0, 8, -1, nullptr, half).bounce, {});
                near("visibility does not confuse facing rejection with occlusion", visible().bounce, simd_float4{1, 1, 1, 1});
                visibility[Source][receiver] = {0, 0, 0, 1};
                visibility[Participation][receiver].y = 0;
                near("visibility ignores albedo and receivesGi", visible().bounce, simd_float4{1, 1, 1, 1});
                blocker(visibility, 7, 1, true, 8);
                near("opaque intersecting voxel has zero visibility", visible().bounce, {});
                blocker(visibility, 7, 1, true, 0);
                near("visibility passes over low authored blocker", visible().bounce, simd_float4{1, 1, 1, 1});
                blocker(visibility, 7, 0.5f, true, 8);
                near("fractional blocker preserves half visibility", visible().bounce, simd_float4{0.5f, 0.5f, 0.5f, 1});
                const Result presented = run(visibility, false, 0, 8, -1, nullptr, half,
                    0, 1, 0, 1, 1, simd_float3{0, 0, -1}, 2);
                near("Visibility view presents transmittance in cyan without exposure or gamma",
                    presented.accumulated, simd_float4{0, 0.425f, 0.5f, 1});
                visibility[Direct][receiver].w = 0;
                visibility[Source][receiver] = {0.2f, 0.4f, 0.6f, 1};
                near("Visibility presentation preserves UI pixels",
                    run(visibility, false, 0, 8, -1, nullptr, half, 0, 1, 0, 1, 1, simd_float3{0, 0, -1}, 2).accumulated,
                    visibility[Source][receiver]);
                blocker(visibility, 7, 1, false, 0);
                near("missing-height blocker does not occlude in visibility", visible().bounce,
                    simd_float4{1, 1, 1, 1});
                blocker(visibility, 7, 0, false, 0);
                visibility[Participation][emitter].x = 0;
                near("visibility excludes UI emitters", visible().bounce, {});
                visibility[Participation][emitter].x = 1;
                visibility[PreviousBounce][emitter] = {};
                near("no sampled emission yields zero visibility", visible().bounce, {});
            }
            for (bool half : {false, true})
            {
                Scene distant = scene();
                distant[PreviousBounce][emitter] = {};
                constexpr unsigned farEmitter = receiver + 100;
                distant[Surface][farEmitter] = distant[Surface][emitter];
                distant[Participation][farEmitter] = {1, 1, 0, 0};
                distant[Emission][farEmitter] = {1, 1, 1, 200.0f / 255.0f};
                run(distant, false, 0, 8, -1, &distant, half);
                distant[PreviousBounce] = distant[Direct];
                const Result far = run(distant, true, 0, 8, -1, nullptr, half, 0, 0);
                positive("single-pixel emitter illuminates beyond old 64-pixel cutoff", far.bounce);
                const float expected = 0.9f * 800.0f * 0.98f * 0.98f / (float(M_PI) * 10001.0f);
                check("direct uses unit pixel area and inverse-square distance", std::fabs(far.bounce.x - expected) <
                    expected * (half ? 0.002f : 1e-5f), far.bounce, simd_float4{expected, expected, expected, 1});
                near("far emitter visibility is fully clear", run(distant, true, 0, 8, -1, nullptr, half, 4).bounce,
                    simd_float4{1, 1, 1, 1});
                near("direct enumeration is independent of jitter seed", run(distant, true, 0, 8, -1, nullptr,
                    half, 0, 0, 3, 8, 97).bounce, far.bounce);
                blocker(distant, 80, 1, true, 8);
                near("visibility traverses blockers beyond old range", run(distant, true, 0, 8, -1, nullptr, half, 4).bounce, {});
                near("far blocker shadows direct", run(distant, true, 0, 8, -1, nullptr, half).bounce, {});
                blocker(distant, 80, 0, false, 0);
                distant[Surface][receiver] = {50, 0, 0, 1};
                distant[Surface][farEmitter].x = 32;
                near("higher +Z out-of-bounds surface rejects lower emitter despite clear visibility",
                    run(distant, true, 0, 8, -1, nullptr, half).bounce, {});
                near("height-facing rejection is not shadowing", run(distant, true, 0, 8, -1, nullptr, half, 4).bounce,
                    simd_float4{1, 1, 1, 1});
                distant[Surface][receiver].x = 0;
                positive("height-zero floor receives direct light from elevated emitter",
                    run(distant, true, 0, 8, -1, nullptr, half).bounce);
                // Distance 3 falls between the old radial stencil's samples at 2 and 6.
                distant[PreviousBounce][farEmitter] = {};
                constexpr unsigned tinyEmitter = receiver + 3;
                distant[Surface][receiver] = s[Surface][receiver];
                distant[Surface][tinyEmitter] = s[Surface][emitter];
                distant[Participation][tinyEmitter] = {1, 1, 0, 0};
                distant[PreviousBounce][tinyEmitter] = {1, 1, 1, 1};
                positive("direct finds tiny emitter between stencil cells", run(distant, true, 0, 8, -1, nullptr, half).bounce);
            }
            for (bool half : {false, true})
            {
                Scene distant = scene();
                distant[PreviousBounce][emitter] = {};
                // Last in-frame radial sample, 122 pixels from the receiver.
                constexpr unsigned patch = receiver + 122;
                distant[Participation][patch] = {1, 1, 0, 0};
                distant[Surface][patch] = distant[Surface][emitter];
                distant[PreviousBounce][patch] = {1, 1, 1, 1};
                const Result clearFar = run(distant, true, 1, 8, -1, nullptr, half, 0, 0);
                positive("indirect reaches radiance near frame edge beyond 64 pixels", clearFar.bounce);
                near("distant indirect accumulates once", clearFar.accumulated, clearFar.bounce);
                blocker(distant, 80, 1, false, 0);
                near("distant indirect passes missing-height blocker",
                    run(distant, true, 1, 8, -1, nullptr, half, 0, 0).bounce, clearFar.bounce);
                blocker(distant, 80, 1, true, 8);
                near("distant indirect respects intersecting authored voxel",
                    run(distant, true, 1, 8, -1, nullptr, half).bounce, {});
                blocker(distant, 80, 1, true, 0);
                near("distant indirect passes over low blocker",
                    run(distant, true, 1, 8, -1, nullptr, half, 0, 0).bounce, clearFar.bounce);
                blocker(distant, 80, 0.5f, true, 8);
                dimmed("distant indirect retains fractional transmittance",
                    run(distant, true, 1, 8, -1, nullptr, half, 0, 0).bounce, clearFar.bounce);
                // A farther endpoint may clear a blocker that hides a nearer, lower one.
                blocker(distant, 80, 1, true, 8);
                constexpr unsigned hiddenPatch = receiver + 90;
                distant[Participation][hiddenPatch] = {1, 1, 0, 0};
                distant[Surface][hiddenPatch] = distant[Surface][emitter];
                distant[PreviousBounce][hiddenPatch] = {1, 1, 1, 1};
                distant[Surface][patch].x = 40;
                positive("blocked lower connection does not terminate search for higher radiance",
                    run(distant, true, 1, 8, -1, nullptr, half, 0, 0).bounce);
            }
            s[OppositeFacing][receiver].x = 1;
            near("opposite facing retains valid original receiver response", run(s, true).bounce, clear.bounce);
            s[OppositeFacing][receiver].x = 0;
            const Result indirectClear = run(s, true, 1);
            s[OppositeFacing][receiver].x = 1;
            near("passIndex 1 ignores toggle on an illuminated receiver", run(s, true, 1).bounce, indirectClear.bounce);
            s[OppositeFacing][receiver].x = 0;
            Scene sloped = scene();
            sloped[Surface][receiver] = {8, 0.5f, 0, 0.8660254f};
            sloped[Surface][emitter] = {40, 0, 0, -1};
            const simd_float4 originalResponse = run(sloped, true).bounce;
            sloped[Surface][receiver].y *= -1;
            const simd_float4 mirroredResponse = run(sloped, true).bounce;
            positive("max-response fixture illuminates both receiver orientations", mirroredResponse);
            sloped[OppositeFacing][receiver].x = 1;
            simd_float4 maximumResponse = {};
            for (unsigned c = 0; c < 3; ++c)
                maximumResponse[c] = std::fmax(originalResponse[c], mirroredResponse[c]);
            near("opposite facing takes maximum full cosine-times-Oren-Nayar response, not sum",
                run(sloped, true).bounce, maximumResponse);
            for (unsigned y = receiverY - 2; y <= receiverY + 2; y++)
                for (unsigned x = 23; x <= 29; x++)
                {
                    const unsigned pixel = y * width + x;
                    s[Surface][pixel] = {8, -1, 0, 0};
                    s[Participation][pixel] = {1, 1, 0, 0};
                    s[PreviousBounce][pixel] = {float(x - 22) / 7, 0.5f, 0.25f, 1};
                }
            Result stochasticA = run(s, true, 1, 8, -1, nullptr, false, 0, 1, 0, 4, 11);
            Result stochasticB = run(s, true, 1, 8, -1, nullptr, false, 0, 1, 1, 4, 11);
            check("stochastic samples jitter independently", stochasticA.bounce.x != stochasticB.bounce.x,
                stochasticA.bounce, stochasticB.bounce);
            simd_float4 stochasticMean = average(stochasticA.bounce, stochasticB.bounce);
            near("sample accumulation computes an incremental mean", stochasticMean,
                (stochasticA.bounce + stochasticB.bounce) * 0.5f);
			s[Surface][emitter] = {8, -0.5f, 0, 0.8660254f};
			Result diffuseAngled = run(s, true, 0, 8, -1, nullptr, false, 0, 1.0f);
			Result shinyAngled = run(s, true, 0, 8, -1, nullptr, false, 0, 0.0f);
			check("surface roughness changes camera-facing diffuse scattering", shinyAngled.bounce.x > 0.0f &&
				std::fabs(shinyAngled.bounce.x - diffuseAngled.bounce.x) > 1e-6f, shinyAngled.bounce, diffuseAngled.bounce);
			near("camera forward magnitude is normalized", run(s, true, 0, 8, -1, nullptr, false, 0, 1.0f,
				0, 1, 1, simd_float3{0, 0, -4}).bounce, diffuseAngled.bounce);
			near("camera behind the surface rejects its reflected radiance", run(s, true, 0, 8, -1, nullptr,
				false, 0, 1.0f, 0, 1, 1, simd_float3{0, 0, 1}).bounce, {});
			s = scene();
            Result distanceStage = run(s, true, 0, 8, -1, nullptr, false, 1);
            Result cosineStage = run(s, true, 0, 8, -1, nullptr, false, 2);
            Result visibleStage = run(s, true, 0, 8, -1, nullptr, false, 3);
            bool diagnosticOrdering = true;
            for (unsigned channel = 0; channel < 3; ++channel)
                diagnosticOrdering &= distanceStage.bounce[channel] >= cosineStage.bounce[channel] &&
                    cosineStage.bounce[channel] >= visibleStage.bounce[channel] &&
                    visibleStage.bounce[channel] > clear.bounce[channel];
            check("indirect diagnostics attribute distance, cosine, visibility, and albedo in order",
                diagnosticOrdering, clear.bounce, distanceStage.bounce);
            s[Source][receiver] = {0, 0, 0, 1};
            near("indirect pre-albedo diagnostic ignores black receiver", run(s, true, 0, 8, -1,
                nullptr, false, 3).bounce, visibleStage.bounce);
            near("indirect black receiver absorbs diagnosed incoming light", run(s, true).bounce, {});
			s = scene();
			s[Source][receiver] = {0.25f, 0.5f, 0.75f, 1};
			Result paintedReflectance = run(s, true);
            const simd_float4 reflectance = {0.9f * std::pow(0.25f, 2.2f),
                0.9f * std::pow(0.5f, 2.2f), 0.9f * std::pow(0.75f, 2.2f), 1};
            near("indirect applies linear RGB receiver albedo once", paintedReflectance.bounce,
                visibleStage.bounce * reflectance);
			s[Source][emitter] = {0, 0, 0, 1};
			near("indirect does not reapply source albedo to outgoing radiance", run(s, true).bounce,
				paintedReflectance.bounce);
			s[Reflectance][receiver] = {0.25f, 0.5f, 0.75f, 1};
			near("authored diffuse reflectance is linear and overrides artwork", run(s, true).bounce,
				visibleStage.bounce * simd_float4{0.25f, 0.5f, 0.75f, 1});
			near("pre-albedo diagnostic ignores authored diffuse reflectance", run(s, true, 0, 8, -1,
				nullptr, false, 3).bounce, visibleStage.bounce);
			s[Reflectance][receiver] = {0, 0, 0, 1};
			near("authored black diffuse reflectance absorbs energy", run(s, true).bounce, {});
			near("boosted black remains absorbing", run(s, true, 0, 8, -1, nullptr, false, 0,
				1.0f, 0, 1, 1, simd_float3{0, 0, -1}, -1, nullptr, false, false, false, 4.0f).bounce, {});
			s[Reflectance][receiver] = {0.1f, 0.2f, 0.4f, 1};
			const Result lowBase = run(s, true);
			const Result lowBoost = run(s, true, 0, 8, -1, nullptr, false, 0,
				1.0f, 0, 1, 1, simd_float3{0, 0, -1}, -1, nullptr, false, false, false, 2.0f);
			const float lowBrightness = 0.1f * 0.2126f + 0.2f * 0.7152f + 0.4f * 0.0722f;
			const float lowScale = 1.0f + 2.0f * (1.0f - lowBrightness) * (1.0f - lowBrightness);
			near("boost lifts dark receiver without changing RGB ratios", lowBoost.bounce, lowBase.bounce * lowScale);
			const Result sampledBase = run(s, true, 0, 8, -1, nullptr, false, 0,
				1.0f, 0, 64, 7, simd_float3{0, 0, -1}, -1, nullptr, false, true);
			const Result sampledBoost = run(s, true, 0, 8, -1, nullptr, false, 0,
				1.0f, 0, 64, 7, simd_float3{0, 0, -1}, -1, nullptr, false, true, false, 2.0f);
			positive("sampled receiver has incoming light for boost test", sampledBase.bounce);
			near("sampled bounce applies the same color-preserving lift", sampledBoost.bounce,
				sampledBase.bounce * lowScale);
			s[Reflectance][receiver] = {0.7f, 0.8f, 0.9f, 1};
			const Result brightBase = run(s, true);
			const Result brightBoost = run(s, true, 0, 8, -1, nullptr, false, 0,
				1.0f, 0, 1, 1, simd_float3{0, 0, -1}, -1, nullptr, false, false, false, 2.0f);
			check("bright receiver receives less relative lift", lowBoost.bounce.x / lowBase.bounce.x >
				brightBoost.bounce.x / brightBase.bounce.x, lowBoost.bounce, brightBoost.bounce);
			near("boost caps bright authored reflectance below full reflection", run(s, true, 0, 8, -1,
				nullptr, false, 0, 1.0f, 0, 1, 1, simd_float3{0, 0, -1}, -1,
				nullptr, false, false, false, 4.0f).bounce,
				visibleStage.bounce * simd_float4{0.95f * 0.7f / 0.9f,
					0.95f * 0.8f / 0.9f, 0.95f, 1});
			s[Reflectance][receiver] = {1, 1, 1, 1};
			near("authored white never reflects all incoming radiance at zero boost",
				run(s, true).bounce, visibleStage.bounce * simd_float4{0.95f, 0.95f, 0.95f, 1});
			s[Reflectance][receiver] = {};
			s[PreviousBounce][emitter] *= 8.0f;
            near("indirect HDR input scales linearly without gamma decoding radiance", run(s, true).bounce,
                paintedReflectance.bounce * 8.0f);
            s[PreviousBounce][emitter] *= 8192.0f;
            const Result hdr = run(s, true);
            near("indirect outgoing HDR radiance is not clamped", hdr.bounce,
                paintedReflectance.bounce * 65536.0f);
            check("HDR regression exercises outgoing radiance above one", hdr.bounce.z > 1.0f,
                hdr.bounce, {});
            Scene sampledScene = scene();
            sampledScene[Reflectance][receiver] = {0.8f, 0.6f, 0.4f, 1};
            auto sampled = [&](const Scene &input, simd_float3 camera = {0, 0, -1}) {
                return run(input, true, 1, 8, -1, nullptr, false, 0, 1.0f,
                    0, 8192, 17, camera, -1, nullptr, false, true).bounce;
            };
            const simd_float4 sampledResult = sampled(sampledScene);
            const float transfer = (0.98f * 0.98f) / (float(M_PI) * 22.0f * 22.0f);
            const simd_float4 expectedSampled = {0.8f * transfer, 0.3f * transfer, 0.1f * transfer, 1};
            bool sampledConverges = true;
            for (unsigned channel = 0; channel < 3; ++channel)
                sampledConverges &= std::fabs(sampledResult[channel] - expectedSampled[channel]) <
                    expectedSampled[channel] * 0.06f;
            check("sampled mixture converges to finite-patch Lambertian exchange",
                sampledConverges, sampledResult, expectedSampled);
            Scene threeSources = sampledScene;
            const unsigned secondSource = receiverY * width + 40;
            const unsigned thirdSource = receiverY * width + 60;
            threeSources[Surface][secondSource] = threeSources[Surface][thirdSource] =
                {8, -0.98f, 0, 0.198997f};
            threeSources[Participation][secondSource] = threeSources[Participation][thirdSource] = {1, 1, 0, 0};
            threeSources[PreviousBounce][secondSource] = {0.25f, 1.0f, 0.1f, 1};
            threeSources[PreviousBounce][thirdSource] = {0.1f, 0.2f, 1.5f, 1};
            blocker(threeSources, 28, 0.5f, true, 8);
            RemasterIndirectReference::Scene oracle;
            oracle.width = width;
            oracle.height = height;
            oracle.patches.push_back({{receiverX + 0.5, receiverY + 0.5, 8},
                {0.98, 0, 0.198997}, {0.8, 0.6, 0.4}, {}, 1, true, true});
            for (unsigned sourceX : {26u, 40u, 60u})
            {
                const simd_float4 radiance = threeSources[PreviousBounce][receiverY * width + sourceX];
                oracle.patches.push_back({{sourceX + 0.5, receiverY + 0.5, 8},
                    {-0.98, 0, 0.198997}, {}, {radiance.x, radiance.y, radiance.z}, 1, true, true});
            }
            oracle.blockers.push_back({receiverX + 28, int(receiverY), 8, 0.5, true});
            const RemasterIndirectReference::Rgb exact = RemasterIndirectReference::exhaustiveBounce(oracle)[0];
            const simd_float4 threeSourceResult = run(threeSources, true, 1, 8, -1, nullptr, false, 0,
                1.0f, 0, 16384, 17, simd_float3{0, 0, -1}, -1, nullptr, false, true).bounce;
            const simd_float4 expectedThreeSources = {float(exact.r), float(exact.g), float(exact.b), 1};
            bool threeSourcesConverge = true;
            for (unsigned channel = 0; channel < 3; ++channel)
                threeSourcesConverge &= std::fabs(threeSourceResult[channel] - expectedThreeSources[channel]) <
                    expectedThreeSources[channel] * 0.08f;
            check("sampled colored sources and fractional blocker match exhaustive oracle",
                threeSourcesConverge, threeSourceResult, expectedThreeSources);
            near("sampled diffuse transport ignores camera direction", sampled(sampledScene, simd_float3{0, 0, 1}), sampledResult);
            const simd_float4 directForward = run(sampledScene, true, 0, 8, -1, nullptr, false, 0,
                1.0f, 0, 1, 17, simd_float3{0, 0, -1}, -1, nullptr, false, false, true).bounce;
            const simd_float4 directReverse = run(sampledScene, true, 0, 8, -1, nullptr, false, 0,
                1.0f, 0, 1, 17, simd_float3{0, 0, 1}, -1, nullptr, false, false, true).bounce;
            positive("Lambertian Direct source field receives light", directForward);
            near("Lambertian Direct source field ignores camera direction", directReverse, directForward);
            sampledScene[Reflectance][receiver] = {0, 0, 0, 1};
            near("sampled authored black absorbs all incoming light", sampled(sampledScene), {});
            sampledScene[Reflectance][receiver] = {0.8f, 0.6f, 0.4f, 1};
            blocker(sampledScene, 7, 1, true, 8);
            near("sampled connection respects opaque height-aware blocker", sampled(sampledScene), {});
            s = scene();
            s[PreviousBounce][emitter] = {1, 1, 1, 1};
            s[Source][receiver] = {1, 0, 0, 1};
            near("white light reflects red from a red surface", run(s, true).bounce,
                simd_float4{clear.bounce.x, 0, 0, 1});
            s[PreviousBounce][emitter] = {1, 0, 0, 1};
            s[Source][receiver] = {0, 1, 0, 1};
            near("green surface absorbs red light without inventing green", run(s, true).bounce, {});
            s[Source][receiver] = {0, 0, 0, 1};
            s[Emission][receiver] = {0.25f, 0.5f, 0.75f, 50.0f / 255.0f};
            near("self emission decodes color once and ignores absorbing albedo", run(s, false).bounce,
                reflectance * (200.0f / 0.9f));
            near("passIndex 0 classifies first collision as direct", clear.accumulated, {});
            s[PreviousBounce][emitter] = {};
            near("indirect needs radiance from previousBounce", run(s, true).bounce, {});
            s = scene();
            s[Surface][receiver] = s[Surface][emitter] = {8, 0, 0, 1};
            near("indirect coplanar +Z surfaces do not exchange light", run(s, true).bounce, {});

            Scene edgeWall = scene();
            edgeWall[Surface][receiver] = {8, 1, 0, 0};
            const float edgeExpected = 0.9f * 0.98f / (float(M_PI) * (22 * 22 + 1));
            near("edge-on Lambertian wall retains finite grazing radiance",
                run(edgeWall, true, 0, 8, -1, nullptr, false, 0, 0).bounce,
                simd_float4{edgeExpected, edgeExpected * 0.5f, edgeExpected * 0.25f, 1});
            positive("edge-on rough wall retains finite grazing radiance", run(edgeWall, true).bounce);
            edgeWall[Surface][receiver] = {8, 0.98f, 0, -0.198997f};
            near("actual camera backface remains rejected", run(edgeWall, true).bounce, {});

            for (bool half : {false, true})
            {
                Scene torch = scene();
                torch[Surface][receiver] = {0, 0, 0, 1};
                torch[Surface][emitter] = {0, 0, 0, 1};
                std::vector<RemasterSurfaceMesh::Cell> mesh(width * height);
                for (unsigned p = 0; p < width * height; p++)
                    for (float &z : mesh[p].corners) z = torch[Surface][p].x;
                auto illuminate = [&] {
                    return run(torch, true, 0, 8, -1, nullptr, half, 0, 0, 0, 1, 1,
                        simd_float3{0, 0, -1}, -1, nullptr, false, false, true, 0, &mesh).bounce;
                };
                near("zero emission depth retains planar coplanar rejection", illuminate(), {});
                mesh[emitter].emissionDepth = 4;
                const auto volume = illuminate();
                positive("emission depth lights coplanar floor from elevated samples", volume);
                check("emission depth preserves source hue", volume.x > volume.y && volume.y > volume.z, volume, {});
                // Close an elevated wall ramp to its floor base. A horizontal
                // ray below the face must block, while an isolated rail stays open.
                torch = scene();
                const unsigned barrier = receiver + 10;
                torch[Surface][barrier] = {20, 0, 0, 1};
                torch[Occlusion][barrier] = {1, 1, 0, 0};
                torch[Height][barrier] = {20, 1, 0, 0};
                for (unsigned p = 0; p < width * height; p++)
                {
                    mesh[p] = {};
                    for (float &z : mesh[p].corners) z = torch[Surface][p].x;
                }
                auto visibility = [&](bool reference) {
                    return run(torch, true, 0, 8, -1, nullptr, half, 4, 0, 0, 1, 1,
                        simd_float3{0, 0, -1}, -1, nullptr, reference, false, true, 0, &mesh).bounce;
                };
                near("raised finite rail remains open underneath", visibility(false), simd_float4{1, 1, 1, 1});
                mesh[barrier].solidWall = 1; mesh[barrier].wallBase = 0; mesh[barrier].thickness = 0;
                near("solid wall closure blocks under-ramp leakage", visibility(false), {});
                near("wall closure envelope matches reference", visibility(true), {});
                mesh[barrier].wallBase = 10;
                near("upper-floor wall base does not fill lower underpass", visibility(false), simd_float4{1, 1, 1, 1});
            }
            // Regression: authoring lateral normals on an elevated unclassified
            // jail bar must not turn it into a wall down to Z0. Use the production
            // material classifier and mesh builder, with a touching wall seed.
            for (bool half : {false, true})
            {
                for (auto surfaceClass : {RemasterSurfaceClass::Unclassified, RemasterSurfaceClass::Prop,
                    RemasterSurfaceClass::WallTop, RemasterSurfaceClass::WallFace})
                {
                    Scene jail = scene();
                    const unsigned bar = receiver + 10, wall = bar - width;
                    std::vector<RemasterSurfaceMesh::Sample> samples(width * height);
                    for (unsigned p : {bar, wall})
                    {
                        jail[Height][p] = {50, 1, 0, 0};
                        jail[Occlusion][p] = {1, 1, 0, 0};
                        samples[p].height = 50; samples[p].known = true; samples[p].coverage = 1;
                        RemasterSurfaceMesh::classify(samples[p], RemasterSourceType::Background, 1,
                            p == wall ? RemasterSurfaceClass::WallFace : surfaceClass, "", p + 1);
                    }
                    // A radius-2 sphere can see around a one-pixel pillar.
                    // Continue the divider across the viewport for this test.
                    for (unsigned y = 0; y < height; y++)
                    {
                        const unsigned p = y * width + bar % width;
                        if (p == bar) continue;
                        samples[p] = samples[bar];
                        jail[Height][p] = jail[Height][bar];
                        jail[Occlusion][p] = jail[Occlusion][bar];
                        jail[Surface][p] = {50, 0, 0, 1};
                    }
                    const auto mesh = RemasterSurfaceMesh::build(width, height, samples, 1);
                    auto visibility = [&](float z, bool reference) {
                        jail[Surface][receiver] = {z, 0, 0, 1};
                        jail[Surface][emitter] = {z, 0, 0, 1};
                        return run(jail, true, 0, 8, -1, nullptr, half, 4, 0, 0, 1, 1,
                            simd_float3{0, 0, -1}, -1, nullptr, reference, false, true, 0, &mesh).bounce;
                    };
                    const bool structural = surfaceClass == RemasterSurfaceClass::WallTop || surfaceClass == RemasterSurfaceClass::WallFace;
                    for (simd_float3 normal : {simd_float3{0, 0, 1}, simd_float3{1, 0, 0}, simd_float3{0, -1, 0}})
                    {
                        jail[Surface][bar] = {50, normal.x, normal.y, normal.z};
                        jail[Surface][wall] = {50, 1, 0, 0};
                        for (bool reference : {false, true})
                        {
                            near("normals do not change elevated detail underpass or structural closure",
                                visibility(0, reference), structural ? simd_float4{} : simd_float4{1, 1, 1, 1});
                            near("elevated bar remains opaque at its actual height", visibility(50, reference), {});
                            near("light passes above finite bar and structural wall", visibility(52, reference), simd_float4{1, 1, 1, 1});
                            jail[Surface][receiver] = {0, 0, 0, 1};
                            Light light = {};
                            light.position = {26.5f, receiverY + 0.5f, 5};
                            light.radius = 2; light.intensity = 1000; light.color = {1, 1, 1};
                            const auto direct = run(jail, true, 0, 8, -1, nullptr, half, 0, 0, 0, 1, 1,
                                simd_float3{0, 0, -1}, -1, &light, reference, false, true, 0, &mesh).bounce;
                            if (structural) near("structural closure still blocks height-5 radius-2 floor sphere light", direct, {});
                            else positive("floor sphere light passes below normal-authored jail bar", direct);
                            if (structural)
                            {
                                // Structural semantics do not authorize replacing
                                // user-authored zero opacity with an opaque blocker.
                                for (unsigned y = 0; y < height; y++) jail[Occlusion][y * width + bar % width].x = 0;
                                positive("transparent structural wall preserves authored light transmission",
                                    run(jail, true, 0, 8, -1, nullptr, half, 0, 0, 0, 1, 1,
                                        simd_float3{0, 0, -1}, -1, &light, reference, false, true, 0, &mesh).bounce);
                                for (unsigned y = 0; y < height; y++) jail[Occlusion][y * width + bar % width].x = 1;
                            }
                        }
                    }
                }
            }
            // Flat height-6 tiled props must not shadow themselves when artwork
            // normals tilt away from the geometric tabletop normal. Separate draw
            // domains are intentional: this does not assume whole-table ownership.
            for (bool half : {false, true}) for (bool reference : {false, true})
                for (auto material : {RemasterSurfaceClass::Unclassified, RemasterSurfaceClass::Prop})
                {
                    Scene table = scene();
                    table[PreviousBounce].assign(width * height, {});
                    std::vector<RemasterSurfaceMesh::Sample> samples(width * height);
                    for (unsigned y = 0; y < height; y++) for (unsigned x = 0; x < width; x++)
                    {
                        const unsigned p = y * width + x;
                        table[Surface][p] = {6, 0, 0, 1};
                        table[Height][p] = {6, 1, 0, 0};
                        table[Occlusion][p] = {1, 1, 0, 0};
                        table[Participation][p] = {1, 1, 0, 0};
                        samples[p].height = 6; samples[p].known = true; samples[p].coverage = 1;
                        RemasterSurfaceMesh::classify(samples[p], RemasterSourceType::Background, 1,
                            material, "", 1 + (y / 8) * (width / 8) + x / 8);
                    }
                    const auto mesh = RemasterSurfaceMesh::build(width, height, samples, 200.0f / 255);
                    // Strong sideways relief normal: the former voxel-face bias
                    // moved only 0.375 in Z, still inside the one-unit shell.
                    table[Surface][receiver] = {6, 0.8f, 0, 0.6f};
                    Light light = {};
                    light.position = {100.5f, receiverY + 0.5f, 8};
                    light.radius = 0.25f; light.intensity = 10000; light.color = {1, 1, 1};
                    auto illuminate = [&](const Scene &s, unsigned stage, const Light *sphere = nullptr) {
                        return run(s, true, 0, 8, -1, nullptr, half, stage, 0, 0, 1, 1,
                            simd_float3{0, 0, -1}, -1, sphere, reference, false, true, 0, &mesh).bounce;
                    };
                    Scene clear = table; clear[Occlusion].assign(width * height, {});
                    positive("tilted tabletop receives shallow sphere light", illuminate(table, 0, &light));
                    near("coplanar tiled shells do not shadow tilted tabletop", illuminate(table, 0, &light), illuminate(clear, 0, &light));
                    near("tabletop sphere visibility has no tile seams", illuminate(table, 4, &light), simd_float4{1, 1, 1, 1});
                    Scene transparentReceiver = table;
                    transparentReceiver[Occlusion][receiver].x = 0;
                    near("transparent tabletop receiver uses the same geometric face as opaque neighbors",
                        illuminate(transparentReceiver, 0, &light), illuminate(table, 0, &light));
                    near("transparent tabletop endpoint does not enter coplanar opaque shell",
                        illuminate(transparentReceiver, 4, &light), simd_float4{1, 1, 1, 1});
                    table[PreviousBounce][emitter] = {1, 0.5f, 0.25f, 1};
                    table[Surface][emitter] = {6, -0.8f, 0, 0.6f};
                    near("coplanar authored shell endpoints have clear visibility", illuminate(table, 4), simd_float4{1, 1, 1, 1});
                    positive("coplanar authored shells retain indirect transport", illuminate(table, 0));
                    Scene clearIndirect = table; clearIndirect[Occlusion].assign(width * height, {});
                    near("coplanar indirect transport has no false seam attenuation", illuminate(table, 0), illuminate(clearIndirect, 0));
                    // Below-table source is not evidence of a seam: a tilted
                    // shading normal can face it while the actual opaque top blocks.
                    table[PreviousBounce].assign(width * height, {});
                    light.position.z = 4;
                    positive("tilted artwork faces a below-table source without occlusion", illuminate(clear, 0, &light));
                    near("opaque tabletop blocks a source below its surface", illuminate(table, 0, &light), {});
                    light.position = {receiverX + 0.5f, receiverY + 0.5f, 4};
                    near("below-table source crossing only receiver cell remains blocked", illuminate(table, 4, &light), {});
                }
            // Connected triangle surfaces must illuminate smoothly, but still
            // Plateau-to-lower-edge reconstruction must not invent a ridge that
            // intercepts shallow rays on the plateau (fresh table capture).
            for (bool half : {false, true}) for (bool reference : {false, true})
            {
                Scene tabletop = scene(); tabletop[PreviousBounce].assign(width * height, {});
                std::vector<RemasterSurfaceMesh::Sample> samples(width * height);
                for (unsigned y = 0; y < height; y++) for (unsigned x = 0; x < width; x++)
                {
                    unsigned p = y * width + x;
                    float z = x < 8 ? 6 : 5.75f;
                    tabletop[Surface][p] = {z, 0, 0, 1};
                    tabletop[Occlusion][p] = {1, 1, 0, 0};
                    tabletop[Height][p] = {z, 1, 0, 0};
                    tabletop[Participation][p] = {1, 1, 0, 0};
                    samples[p].height = z; samples[p].known = true; samples[p].coverage = 1; samples[p].domain = 11;
                }
                auto mesh = RemasterSurfaceMesh::build(width, height, samples, 0.25f);
                Light light = {}; light.position = {100.5f, receiverY + 0.5f, 6.8f};
                light.radius = 0.05f; light.intensity = 10000; light.color = {1, 1, 1};
                auto illuminate = [&](const Scene &s, unsigned stage) {
                    return run(s, true, 0, 8, -1, nullptr, half, stage, 0, 0, 1, 1,
                        simd_float3{0, 0, -1}, -1, &light, reference, false, true, 0, &mesh).bounce;
                };
                Scene clear = tabletop; clear[Occlusion].assign(width * height, {});
                near("lower tabletop edge does not raise a shadow-casting plateau ridge", illuminate(tabletop, 4), simd_float4{1, 1, 1, 1});
                positive("shallow sphere illuminates plateau beside lower edge", illuminate(tabletop, 0));
                near("plateau lower edge preserves direct lighting", illuminate(tabletop, 0), illuminate(clear, 0));
                // Transparent source/receiver face positioning must not bypass a
                // real opaque crossing. A light below the top remains blocked.
                tabletop[Occlusion][receiver].x = 0;
                light.position.z = 4;
                near("transparent plateau receiver still respects opaque below-top crossing", illuminate(tabletop, 4), {});
            }
            // Connected triangle surfaces must illuminate smoothly, but still
            // block rays crossing from the opposite side of a perspective wall.
            for (bool half : {false, true})
            {
                Scene ramp = scene();
                ramp[PreviousBounce].assign(width * height, {});
                std::vector<RemasterSurfaceMesh::Sample> samples(width * height);
                for (unsigned y = 0; y < height; y++) for (unsigned x = 0; x < width; x++)
                {
                    const unsigned p = y * width + x;
                    const float z = (x + 0.5f) * 0.5f;
                    ramp[Surface][p] = {z, -0.4472136f, 0, 0.8944272f};
                    ramp[Occlusion][p] = {1, 1, 0, 0};
                    ramp[Height][p] = {z, 1, 0, 0};
                    ramp[Participation][p] = {1, 1, 0, 0};
                    samples[p].height = z; samples[p].known = true;
                    samples[p].coverage = 1; samples[p].sheet = true; samples[p].domain = 2;
                }
                const auto mesh = RemasterSurfaceMesh::build(width, height, samples, 0.25f);
                Light light = {};
                light.position = {100.5f, receiverY + 0.5f, 50.75f};
                light.radius = 0.1f; light.intensity = 10000; light.color = {1, 1, 1};
                auto illuminate = [&](const Scene &s, unsigned stage, bool reference = false) {
                    return run(s, true, 0, 8, -1, nullptr, half, stage, 0, 0, 1, 1,
                        simd_float3{0, 0, -1}, -1, &light, reference, false, true, 0, &mesh).bounce;
                };
                Scene clear = ramp; clear[Occlusion].assign(width * height, {});
                positive("shallow light reaches continuous perspective wall", illuminate(ramp, 0));
                near("connected wall does not staircase-shadow itself", illuminate(ramp, 0), illuminate(clear, 0));
                near("continuous wall visibility is clear", illuminate(ramp, 4), simd_float4{1, 1, 1, 1});
                near("continuous wall acceleration parity", illuminate(ramp, 0), illuminate(ramp, 0, true));
                light.position.z = 40;
                near("perspective wall blocks light crossing to opposite side", illuminate(ramp, 4), {});
                // A separate raised rail remains finite, with a gap underneath.
                samples[receiver + 45].sheet = false; samples[receiver + 45].domain = 300;
                samples[receiver + 45].height = 40;
                const auto railMesh = RemasterSurfaceMesh::build(width, height, samples, 0.25f);
                check("rail is finite while wall remains a connected sheet", railMesh[receiver + 45].thickness == 1 &&
                    railMesh[receiver + 44].thickness == 0, {}, {});
            }
            // Real floors are opaque continuous voxel sheets, not isolated
            // non-occluding receiver points. Shallow rays must escape their face.
            for (bool half : {false, true})
            {
                Scene floor = scene();
                floor[PreviousBounce][emitter] = {};
                for (unsigned p = 0; p < width * height; p++)
                {
                    floor[Surface][p] = {0, 0, 0, 1};
                    floor[Height][p] = {0, 1, 0, 0};
                    floor[Occlusion][p] = {1, 1, 0, 0};
                    floor[Participation][p] = {1, 1, 0, 0};
                }
                Light light = {};
                light.position = {90.5f, receiverY + 0.5f, 15};
                light.radius = 5;
                light.intensity = 25;
                light.color = {1, 1, 1};
                auto illuminate = [&](const Scene &s, const Light *debug, unsigned stage = 0, bool reference = false,
                                      bool sampled = false) {
                    return run(s, true, sampled ? 1 : 0, 8, -1, nullptr, half, stage, 0,
                        0, sampled ? 4096 : 1, 1, simd_float3{0, 0, -1}, -1, debug, reference, sampled, true);
                };
                Scene transparent = floor;
                transparent[Occlusion].assign(width * height, {});
                const simd_float4 sphereClear = illuminate(transparent, &light).bounce;
                positive("distant sphere illuminates opaque continuous floor", illuminate(floor, &light).bounce);
                near("opaque floor does not shadow its own sphere illumination", illuminate(floor, &light).bounce, sphereClear);
                near("unaccelerated opaque floor sphere illumination agrees", illuminate(floor, &light, 0, true).bounce, sphereClear);
                near("floor sphere visibility diagnostic stays clear", illuminate(floor, &light, 4).bounce,
                    simd_float4{1, 1, 1, 1});
                // A downward-facing authored emitter also sits within a sheet
                // of its own opaque voxels: both connection endpoints must exit.
                const unsigned torch = receiverY * width + 90;
                for (unsigned y = receiverY - 1; y <= receiverY + 1; y++)
                    for (unsigned x = 89; x <= 91; x++)
                    {
                        const unsigned p = y * width + x;
                        floor[Surface][p] = {15, 0, 0, -1};
                        floor[Height][p] = {15, 1, 0, 0};
                    }
                floor[PreviousBounce][torch] = {100, 50, 25, 1};
                transparent = floor;
                transparent[Occlusion].assign(width * height, {});
                const simd_float4 torchClear = illuminate(transparent, nullptr).bounce;
                positive("authored elevated emitter illuminates opaque continuous floor", illuminate(floor, nullptr).bounce);
                near("floor and emitter sheets do not self-shadow Direct", illuminate(floor, nullptr).bounce, torchClear);
                near("floor and emitter sheets do not self-shadow indirect", illuminate(floor, nullptr, 0, false, true).bounce,
                    illuminate(transparent, nullptr, 0, false, true).bounce);
                // A separate raised sheet really intersects the shallow path.
                for (unsigned y = 0; y < height; y++)
                {
                    const unsigned p = y * width + 45;
                    floor[Surface][p] = {7.5f, 0, 0, 1};
                    floor[Height][p] = {7.5f, 1, 0, 0};
                }
                near("raised blocker still shadows authored floor illumination", illuminate(floor, nullptr).bounce, {});
                dimmed("raised blocker still shadows sphere floor illumination", illuminate(floor, &light).bounce, sphereClear);
            }
            for (bool half : {false, true})
            {
                Scene sphere = scene();
                sphere[PreviousBounce][emitter] = {};
                Light light = {};
                light.position = {26.5f, receiverY + 0.5f, 8};
                light.radius = 12;
                light.intensity = 25;
                light.color = {1, 1, 1};
                auto illuminate = [&](int view = -1) {
                    return run(sphere, view < 0, 0, 8, -1, nullptr, half, 0, 0,
                        0, 1, 1, simd_float3{0, 0, -1}, view, &light, false, false, true);
                };
                sphere[Surface][receiver] = {16, 0, 0, 1};
                positive("sphere upper extent lights receiver above center", illuminate().bounce);
                light.radius = 2;
                near("sphere entirely below upward receiver cannot illuminate it", illuminate().bounce, {});
                light.radius = 12;
                sphere[Surface][receiver] = {8, 1, 0, 0};
                positive("sphere emits sideways at center height", illuminate().bounce);
                sphere[Surface][receiver] = {24, 0, 0, -1};
                positive("sphere emits upward onto downward receiver", illuminate().bounce);
                sphere[Surface][receiver] = {-8, 0, 0, 1};
                positive("sphere emits downward onto upward receiver", illuminate().bounce);
                light.position = {receiverX + 0.5f, receiverY + 0.5f, 8};
                light.radius = 4;
                sphere[Surface][receiver] = {10, 0, 0, 1};
                sphere[Direct][receiver] = {0, 0, 0, 1};
                check("sphere front surface remains visible above center height",
                    illuminate(0).accumulated.x > 1, illuminate(0).accumulated, {});
                sphere[Surface][receiver].x = 13;
                const float ambient = std::pow(0.65f, 1.0f / 2.2f);
                near("geometry above sphere front surface hides glow", illuminate(0).accumulated,
                    simd_float4{ambient, ambient, ambient, 1});
                sphere[Surface][receiver] = {8, 0, 0, 1};
                const simd_float4 centered = illuminate().bounce;
                positive("receiver at sphere center has finite illumination", centered);
                check("sphere center illumination is bounded", centered.x <= 85.6f, centered, {});
                light.radius = 0.05f;
                light.position = {26.5f, receiverY + 0.5f, 24};
                positive("tiny distant sphere is not lost by area sampling", illuminate().bounce);
            }
            for (bool half : {false, true})
            {
                Scene disk = scene();
                disk[PreviousBounce][emitter] = {};
                disk[Surface][receiver] = {0, 0, 0, 1};
                Light light = {};
                light.position = {104.5f, receiverY + 0.5f, 24};
                light.radius = 4;
                light.intensity = 25;
                light.color = {1, 1, 1};
                auto illuminate = [&](unsigned pass = 0, unsigned stage = 0, int view = -1) {
                    return run(disk, view < 0, pass, 8, -1, nullptr, half, stage, 0,
                        0, 1, 1, simd_float3{0, 0, -1}, view, &light);
                };
                const simd_float4 clearDisk = illuminate().bounce;
                positive("debug sphere lights floor beyond 64 pixels", clearDisk);
                const float d2 = 100 * 100 + 24 * 24;
                const float analytic = 0.9f * 100 * 16 * 24 / std::pow(d2, 1.5f);
                check("small distant sphere matches solid-angle cosine reference",
                    std::fabs(clearDisk.x - analytic) < analytic * 0.02f, clearDisk, simd_float4{analytic, analytic, analytic, 1});
                light.intensity = 200;
                near("debug intensity is linear and calibrated like authored emission", illuminate().bounce, clearDisk * 8);
                light.intensity = 25;
                light.radius = 2;
                dimmed("smaller physical sphere emits less power", illuminate().bounce, clearDisk);
                light.radius = 4;
                for (unsigned y = 0; y < height; y++)
                {
                    const unsigned p = y * width + 80;
                    disk[Occlusion][p] = {1, 1, 0, 0};
                    disk[Height][p] = {40, 1, 0, 0};
                    disk[Surface][p] = {40, 0, 0, 1};
                }
                near("debug sphere passes beneath raised opaque rail", illuminate().bounce, clearDisk);
                for (unsigned y = 0; y < height; y++)
                    disk[Surface][y * width + 80].x = 18.12f;
                dimmed("debug sphere is shadowed by intersecting opaque voxels", illuminate().bounce, clearDisk);
                light.radius = 0.1f;
                near("small debug sphere is fully blocked by intersecting rail", illuminate().bounce, {});
                light.radius = 4;
                for (unsigned y = 0; y < height; y++)
                    disk[Surface][y * width + 80].x = 0;
                near("debug sphere clears low wall", illuminate().bounce, clearDisk);
                light.color = {1, 0, 0};
                near("debug sphere preserves light color", illuminate().bounce, simd_float4{clearDisk.x, 0, 0, 1});
                light.color = {1, 1, 1};
                disk[Surface][receiver].x = 50;
                near("debug sphere does not light higher out-of-bounds surface", illuminate().bounce, {});
                disk[Surface][receiver].x = 0;
                disk[Participation][receiver].y = 0;
                near("debug sphere honors receives GI", illuminate().bounce, {});
                near("debug visibility ignores receives GI", illuminate(0, 4).bounce, simd_float4{1, 1, 1, 1});
                disk[Participation][receiver].x = 0;
                near("debug sphere excludes UI", illuminate().bounce, {});
                disk[Participation][receiver] = {1, 1, 0, 0};
                near("debug light is not reinjected into later bounces", illuminate(1).bounce, {});
                light.intensity = 0;
                near("zero debug intensity disables illumination", illuminate().bounce, {});
                light.intensity = 25;
                light.radius = 0;
                near("zero debug radius has zero power", illuminate().bounce, {});
                light.radius = 2;
                light.position.x = receiverX + 0.5f;
                disk[Direct][receiver] = {0, 0, 0, 1};
                const auto glow = illuminate(0, 0, 0).accumulated;
                check("composite displays debug sphere without altering scene pixels", glow.x > 1, glow, {});
                near("Direct diagnostic excludes sphere self-emission", illuminate(0, 0, 3).accumulated, {});
                disk[Surface][receiver].x = 50;
                const float ambient = std::pow(0.65f, 1.0f / 2.2f);
                near("higher geometry hides debug sphere glow", illuminate(0, 0, 0).accumulated, simd_float4{ambient, ambient, ambient, 1});
                disk[Surface][receiver].x = 0;
                light.radius = 0;
                near("zero radius also removes visible glow", illuminate(0, 0, 0).accumulated, simd_float4{ambient, ambient, ambient, 1});
                for (unsigned y = 0; y < height; y++)
                    disk[Occlusion][y * width + 80] = {};
                light.radius = 4096;
                const auto largeDisk = illuminate().bounce;
                const float capped = 0.9f * 100 * 0.95f;
                check("receiver inside sphere approaches capped hemisphere irradiance",
                    std::fabs(largeDisk.x - capped) < capped * 0.03f, largeDisk, simd_float4{capped, capped, capped, 1});
                for (float radius : {4.0f, 24.0f, 64.0f, 256.0f})
                {
                    light.radius = radius;
                    const float reference = 90 * std::min(0.95f, radius >= 24 ? 1.0f : radius * radius / (24 * 24));
                    const auto actual = illuminate().bounce;
                    check("overhead sphere tracks analytic irradiance across sizes",
                        std::fabs(actual.x - reference) < reference * 0.04f, actual,
                        simd_float4{reference, reference, reference, 1});
                }
                light.radius = 4;
                light.position = {14.5f, receiverY + 0.5f, 32};
                Scene relayDisk = scene();
                relayDisk[PreviousBounce][emitter] = {};
                run(relayDisk, true, 0, 8, -1, &relayDisk, half, 0, 0,
                    0, 1, 1, simd_float3{0, 0, -1}, -1, &light);
                positive("debug sphere produces Direct on reflecting surfaces", relayDisk[NextBounce][receiver]);
                relayDisk[PreviousBounce] = relayDisk[NextBounce];
                const auto secondary = run(relayDisk, true, 1, 8, -1, nullptr, half, 0, 0).bounce;
                positive("debug sphere Direct seeds secondary reflected light", secondary);
                near("secondary light does not reinject enabled debug emitter",
                    run(relayDisk, true, 1, 8, -1, nullptr, half, 0, 0, 0, 1, 1,
                        simd_float3{0, 0, -1}, -1, &light).bounce, secondary);
            }

            simd_float4 diagonal[2];
            for (int side : {1, -1})
            {
                s = scene();
                s[Participation][emitter] = {};
                s[PreviousBounce][emitter] = {};
                const unsigned patch = int(receiver) + side * (int(width) + 1);
                const float normal = side * std::sqrt(0.48f);
                s[Surface][receiver] = {8, normal, normal, 0.2f};
                s[Surface][patch] = {8, -normal, -normal, 0.2f};
                s[Participation][patch] = {1, 1, 0, 0};
                s[PreviousBounce][patch] = {1, 0.5f, 0.25f, 1};
                diagonal[side > 0 ? 0 : 1] = run(s, true).bounce;
            }
            positive("indirect diagonal +(1,1) has energy", diagonal[0]);
            positive("indirect diagonal -(1,1) has energy", diagonal[1]);
            near("indirect mirrored diagonal energy matches", diagonal[1], diagonal[0]);

            // A is a 3x3 patch so both stencil origins sample A -> B. C cannot see
            // A's +X-facing radiance, but B faces both A and C and relays it.
            Scene chain = scene();
            chain[Participation][emitter] = {};
            chain[PreviousBounce][emitter] = {};
            constexpr unsigned relay = receiverY * width + 10;
            chain[Surface][relay] = {8, -0.98f, 0, 0.198997f};
            chain[Participation][relay] = {1, 1, 0, 0};
            for (unsigned y = 2; y <= 4; ++y)
                for (unsigned x = 4; x <= 6; ++x)
                {
                    const unsigned patch = y * width + x;
                    chain[Surface][patch] = {8, 0.98f, 0, 0.198997f};
                    chain[Participation][patch] = {1, 1, 0, 0};
                    chain[PreviousBounce][patch] = {1, 0.5f, 0.25f, 1};
                }
            // Exercise production view composition with real seed and collision fields.
            // Checking every pixel includes emissive A, directly lit B, and indirectly lit C.
            for (bool half : {false, true})
                for (unsigned extraBounces : {0u, 1u})
                {
                    s = chain;
                    for (unsigned pixel = 0; pixel < width * height; ++pixel)
                        if (chain[PreviousBounce][pixel].x > 0)
                            s[Emission][pixel] = {1, 0.75f, 0.5f, 200.0f / 255.0f};
                    s[PreviousBounce].assign(width * height, simd_float4{});
                    run(s, false, 0, 8, -1, &s, half);
                    s[PreviousBounce] = s[Direct];
                    run(s, true, 0, 8, -1, &s, half);
                    const Image firstCollision = s[NextBounce];
                    positive("emission chain lights relay in mandatory collision", firstCollision[relay]);
                    if (extraBounces)
                    {
                        s[PreviousBounce] = firstCollision;
                        s[PreviousIndirect] = s[NextIndirect];
                        run(s, true, 1, 8, -1, &s, half);
                        positive("emission chain reaches receiver with one extra bounce", s[NextIndirect][receiver]);
                        near("one extra bounce accumulates only second collision",
                            s[NextIndirect][receiver], s[NextBounce][receiver]);
                        s[NextBounce] = firstCollision;
                    }
                    else
                        near("zero extra bounces leave indirect black", s[NextIndirect][receiver], {});
                    for (unsigned view : {3u, 6u, 8u, 0u})
                    {
                        run(s, false, 0, 8, -1, &s, half, 0, 1, 0, 1, 1, simd_float3{0, 0, -1}, view);
                        bool matches = true;
                        simd_float4 expectedReceiver = {};
                        for (unsigned pixel = 0; pixel < width * height; ++pixel)
                            for (unsigned c = 0; c < 3; ++c)
                            {
                                float linear = view == 3 ? firstCollision[pixel][c] * 4 :
                                    view == 6 ? s[NextIndirect][pixel][c] * 4 :
                                    view == 8 ? (firstCollision[pixel][c] + s[NextIndirect][pixel][c]) * 4 :
                                    std::pow(s[Source][pixel][c], 2.2f) * 0.65f + s[Direct][pixel][c] +
                                        firstCollision[pixel][c] + s[NextIndirect][pixel][c];
                                const float expected = s[Direct][pixel].w < 0.5f ? s[Source][pixel][c] :
                                    std::pow(linear, 1.0f / 2.2f);
                                matches &= std::isfinite(s[Output][pixel][c]) &&
                                    std::fabs(s[Output][pixel][c] - expected) <= 1e-6f + expected * 1e-5f;
                                if (pixel == receiver)
                                    expectedReceiver[c] = expected;
                            }
                        char name[128];
                        std::snprintf(name, sizeof(name), "%s view %u with %u extra bounces counts seed/direct/indirect once",
                            half ? "RGBA16Float" : "RGBA32Float", view, extraBounces);
                        check(name, matches, s[Output][receiver], expectedReceiver);
                    }
                }
            s = chain;
            Image sum(width * height, simd_float4{});
            simd_float4 oneBounce = {}, firstPeak = {}, previousPeak = {1, 0.5f, 0.25f, 0};
            for (unsigned pass = 0; pass < 16; ++pass)
            {
                Result result = run(s, true, pass, 8, -1, &s);
                simd_float4 peak = {};
                bool accumulatedMatches = true, bounded = true;
                for (unsigned pixel = 0; pixel < width * height; ++pixel)
                {
					if (pass > 0)
						sum[pixel] += s[NextBounce][pixel];
                    for (unsigned c = 0; c < 3; ++c)
                    {
                        const float value = s[NextBounce][pixel][c];
                        bounded &= std::isfinite(value) && value >= 0 &&
                            value <= previousPeak[c] * (0.9f * 0.95f) + 1e-7f;
                        peak[c] = std::fmax(peak[c], value);
                        accumulatedMatches &= std::isfinite(s[NextIndirect][pixel][c]) &&
                            std::fabs(s[NextIndirect][pixel][c] - sum[pixel][c]) <=
                                1e-6f + std::fabs(sum[pixel][c]) * 1e-5f;
                    }
                }
                char name[96];
                std::snprintf(name, sizeof(name), "chained pass %u full accumulation equals bounce sum", pass + 1);
                check(name, accumulatedMatches, result.accumulated, sum[receiver]);
                std::snprintf(name, sizeof(name), "chained pass %u white-albedo energy is bounded", pass + 1);
                check(name, bounded, peak, previousPeak * (0.9f * 0.95f));
                if (pass == 0)
                {
                    oneBounce = result.accumulated;
                    firstPeak = peak;
                    near("chained A cannot reach C in first bounce", result.bounce, {});
                    positive("chained A reaches B in first bounce", s[NextBounce][relay]);
                    for (unsigned y = 2; y <= 4; ++y)
                        for (unsigned x = 4; x <= 6; ++x)
                            near("chained initial seed is not copied into next bounce", s[NextBounce][y * width + x], {});
                }
                if (pass == 1)
                    positive("chained A -> B -> C reaches C in second bounce", result.bounce);
                previousPeak = peak;
                // Feed only the last outgoing field, never the seed or accumulated light.
                s[PreviousBounce].swap(s[NextBounce]);
                s[PreviousIndirect].swap(s[NextIndirect]);
            }
            positive("chained sixteen bounces reach beyond one bounce",
                s[PreviousIndirect][receiver] - oneBounce);
            check("chained energy decays without reseeding", previousPeak.x < firstPeak.x * 0.001f &&
                previousPeak.y < firstPeak.y * 0.001f && previousPeak.z < firstPeak.z * 0.001f,
                previousPeak, firstPeak);
			Scene exhaustedChain = s;
			s = chain;
			Result smoothFirst = run(s, true, 0, 8, -1, &s, false, 0, 0.5f);
			s[PreviousBounce].swap(s[NextBounce]);
			s[PreviousIndirect].swap(s[NextIndirect]);
			Result smoothSecond = run(s, true, 1, 8, -1, &s, false, 0, 0.5f);
			positive("smooth chained transport reaches C on second bounce", smoothSecond.bounce);
			check("smooth second bounce increases accumulated light", smoothSecond.accumulated.x > smoothFirst.accumulated.x,
				smoothSecond.accumulated, smoothFirst.accumulated);
			s = std::move(exhaustedChain);
			s[PreviousBounce].assign(width * height, simd_float4{});
            Result exhausted = run(s, true, 16);
            near("chained zero previous field cannot reseed light", exhausted.bounce, {});
            near("chained zero previous field preserves accumulation", exhausted.accumulated, sum[receiver]);

            s = chain;
            s[Source][relay] = {0, 0, 0, 1};
            run(s, true, 0, 8, -1, &s);
            near("chained black B absorbs first bounce", s[NextBounce][relay], {});
            s[PreviousBounce].swap(s[NextBounce]);
            s[PreviousIndirect].swap(s[NextIndirect]);
            Result absorbed = run(s, true, 1);
            near("chained black B prevents second-order reach to C", absorbed.bounce, {});
            near("chained black B leaves C accumulation dark", absorbed.accumulated, {});

            // Colored A -> B -> C must multiply the two receiver reflectances,
            // not grayscale them, decode radiance again, or apply B twice.
            for (bool half : {false, true})
            {
                s = chain;
                run(s, true, 0, 8, -1, &s, half);
                const simd_float4 whiteRelay = s[NextBounce][relay];
                s[PreviousBounce].swap(s[NextBounce]);
                s[PreviousIndirect].swap(s[NextIndirect]);
                const Result whiteSecond = run(s, true, 1, 8, -1, &s, half);
                s = chain;
                s[Source][relay] = {1, 0.5f, 0.25f, 1};
                s[Source][receiver] = {0.5f, 1, 0.75f, 1};
                run(s, true, 0, 8, -1, &s, half);
                const simd_float4 coloredRelay = s[NextBounce][relay];
                s[PreviousBounce].swap(s[NextBounce]);
                s[PreviousIndirect].swap(s[NextIndirect]);
                const Result coloredSecond = run(s, true, 1, 8, -1, &s, half);
                simd_float4 expectedRelay = {}, expectedSecond = {};
                bool relayMatches = true, secondMatches = true;
                for (unsigned c = 0; c < 3; ++c)
                {
                    const float relayRatio = std::pow(s[Source][relay][c], 2.2f);
                    const float receiverRatio = std::pow(s[Source][receiver][c], 2.2f);
                    expectedRelay[c] = whiteRelay[c] * relayRatio;
                    expectedSecond[c] = whiteSecond.bounce[c] * relayRatio * receiverRatio;
                    const float relativeTolerance = half ? 0.005f : 1e-5f;
                    const float absoluteTolerance = half ? 1e-7f : 1e-9f;
                    relayMatches &= std::isfinite(coloredRelay[c]) && coloredRelay[c] > 0 &&
                        std::fabs(coloredRelay[c] - expectedRelay[c]) <=
                            absoluteTolerance + expectedRelay[c] * relativeTolerance;
                    secondMatches &= std::isfinite(coloredSecond.bounce[c]) && coloredSecond.bounce[c] > 0 &&
                        std::fabs(coloredSecond.bounce[c] - expectedSecond[c]) <=
                            absoluteTolerance + expectedSecond[c] * relativeTolerance;
                }
                check(half ? "RGBA16Float colored relay applies RGB absorption" : "colored relay applies RGB absorption",
                    relayMatches, coloredRelay, expectedRelay);
                check(half ? "RGBA16Float second bounce carries product of material colors" :
                    "second bounce carries product of material colors", secondMatches, coloredSecond.bounce, expectedSecond);
                near("colored second bounce accumulates once", coloredSecond.accumulated, coloredSecond.bounce);
            }

            // Repeat the chain using the production RGBA16Float radiance format. This
            // catches later bounces being lost to ping-pong precision or accumulation.
            s = chain;
            simd_float4 halfAccumulations[16] = {};
            for (unsigned pass = 0; pass < 16; ++pass)
            {
                Result result = run(s, true, pass, 8, -1, &s, true);
                halfAccumulations[pass] = result.accumulated;
                s[PreviousBounce].swap(s[NextBounce]);
                s[PreviousIndirect].swap(s[NextIndirect]);
            }
            positive("RGBA16Float chain reaches C on second bounce", halfAccumulations[1]);
            bool halfAccumulationGrows = false;
            for (unsigned channel = 0; channel < 3; ++channel)
                halfAccumulationGrows |= halfAccumulations[15][channel] >
                    halfAccumulations[0][channel] + std::fmax(1e-6f, halfAccumulations[0][channel] * 0.001f);
            check("RGBA16Float sixteen-bounce accumulation differs from one bounce",
                halfAccumulationGrows, halfAccumulations[15], halfAccumulations[0]);
            bool halfMonotonic = true;
            for (unsigned pass = 1; pass < 16; ++pass)
                for (unsigned channel = 0; channel < 3; ++channel)
                    halfMonotonic &= halfAccumulations[pass][channel] + 1e-6f >=
                        halfAccumulations[pass - 1][channel];
            check("RGBA16Float accumulated radiance is monotonic", halfMonotonic,
                halfAccumulations[15], halfAccumulations[0]);

            // Sparse GI samples are at 2, 6, 10, ...; these blockers are strictly between them.
            for (unsigned distance : {3u, 7u, 11u, 15u, 19u})
            {
                s = scene();
                blocker(s, distance, 1, true, 8);
                char name[96];
                std::snprintf(name, sizeof(name), "indirect thin authored blocker at distance %u", distance);
                near(name, run(s, true).bounce, {});
            }
            s = scene();
            blocker(s, 7, 1, true, 0);
            near("indirect passes over low authored blocker", run(s, true).bounce, clear.bounce);
            blocker(s, 7, 1, true, 8);
            near("indirect intersecting authored voxel shadows", run(s, true).bounce, {});
            blocker(s, 7, 0.5f, true, 8);
            dimmed("indirect partial blocker between samples dims", run(s, true).bounce, clear.bounce);
            s = scene();
            blocker(s, 6, 0.5f, true, 8);
            dimmed("indirect partial blocker at a distance sample dims", run(s, true).bounce, clear.bounce);

            s = scene();
            simd_float4 prior = {0.125f, 0.25f, 0.5f, 1};
            s[PreviousIndirect][receiver] = prior;
            Result second = run(s, true, 1);
            near("passIndex 1 preserves new bounce", second.bounce, indirectClear.bounce);
            near("passIndex 1 starts indirect accumulation with second collision", second.accumulated, indirectClear.bounce);
            s[Participation][receiver].y = 0;
            Result disabled = run(s, true);
            near("receivesGi false produces zero bounce", disabled.bounce, {});
            near("receivesGi false produces zero initial indirect", disabled.accumulated, {});

            s = scene();
            s[Surface][receiver] = {0, 0, 0, 1};
            s[Surface][emitter] = {8, -1, 0, 0};
            positive("elevated inward-facing wall bounces onto lower floor", run(s, true).bounce);
            blocker(s, 7, 1, true, 8.0f * 7 / 22);
            near("intervening wall blocks wall-to-floor indirect", run(s, true).bounce, {});

            s = scene();
            for (bool bounce : {false, true})
            {
                Result plain = run(s, bounce, 0, 8, -1, nullptr, true);
                Result selected = run(s, bounce, 0, 8, receiver, nullptr, true);
                near("half radiance with float presentation preserves selected radiance", selected.bounce, plain.bounce);
                near("half radiance with float presentation reads highlighted output correctly", selected.accumulated,
                    (plain.accumulated + simd_float4{1, 1, 0, 1}) * 0.5f);
            }
            for (bool bounce : {false, true})
            {
                Result plain = run(s, bounce);
                Result selected = run(s, bounce, 0, 8, receiver);
                near(bounce ? "selection leaves indirect radiance unchanged" : "selection leaves direct radiance unchanged",
                    selected.bounce, plain.bounce);
                near("selection tints only presentation", selected.accumulated,
                    (plain.accumulated + simd_float4{1, 1, 0, 1}) * 0.5f);
                near("unselected presentation pixels unchanged", run(s, bounce, 0, 8, emitter).accumulated,
                    plain.accumulated);
            }

            // Finite voxel semantics, independent of normal and albedo response.
            for (bool half : {false, true})
                for (bool reference : {false, true})
                {
                    Scene voxels = scene();
                    auto visible = [&] {
                        return run(voxels, true, 0, 8, -1, nullptr, half, 4, 1,
                            0, 1, 1, simd_float3{0, 0, -1}, -1, nullptr, reference).bounce;
                    };
                    const simd_float4 full = {1, 1, 1, 1};
                    blocker(voxels, 7, 1, true, 8);
                    near("ray intersects opaque voxel center", visible(), {});
                    blocker(voxels, 7, 1, true, 16);
                    near("ray passes below opaque voxel", visible(), full);
                    blocker(voxels, 7, 1, true, 0);
                    near("ray passes above opaque voxel", visible(), full);
                    blocker(voxels, 7, 1, true, 8.5f);
                    near("ray tangent to voxel bottom does not block", visible(), full);
                    blocker(voxels, 7, 1, true, 7.5f);
                    near("ray tangent to voxel top does not block", visible(), full);
                    blocker(voxels, 7, 1, true, 8.49f);
                    near("ray just inside voxel blocks", visible(), {});
                    blocker(voxels, 7, 0.5f, true, 8);
                    near("fractional voxel coverage attenuates once", visible(), simd_float4{0.5f, 0.5f, 0.5f, 1});
                    blocker(voxels, 7, 1, false, 8);
                    near("missing-height voxel does not block", visible(), full);
                    blocker(voxels, 7, 0, true, 8);
                    near("transparent voxel does not block", visible(), full);
                    blocker(voxels, 0, 1, true, 8);
                    blocker(voxels, 22, 1, true, 8);
                    near("endpoint voxels do not self-shadow", visible(), full);
                    // These endpoints now have face geometry, not blanket cell
                    // exclusion. Remove them before changing endpoint heights;
                    // the following cases isolate the intervening voxel only.
                    blocker(voxels, 0, 0, true, 8);
                    blocker(voxels, 22, 0, true, 8);
                    blocker(voxels, 7, 1, true, 7);
                    voxels[Surface][receiver].x = 0;
                    voxels[Surface][emitter].x = 22;
                    near("ascending ray crosses finite Z slab", visible(), {});
                    blocker(voxels, 7, 1, true, 9);
                    voxels[Surface][receiver].x = 16;
                    voxels[Surface][emitter].x = -6;
                    near("descending ray crosses finite Z slab", visible(), {});
                    blocker(voxels, 7, 1, true, 8);
                    voxels[Surface][receiver].x = 8;
                    voxels[Surface][emitter].x = 30;
                    near("Z crossing outside voxel XY is clear", visible(), full);
                    voxels = scene();
                    blocker(voxels, 6, 1, true, 0);
                    blocker(voxels, 7, 1, true, 16);
                    near("block height envelope is not solid geometry", visible(), full);
                    // A horizontal rail and a vertical bar both block their own
                    // intersections; the gap between them remains transmissive.
                    blocker(voxels, 10, 1, true, 8);
                    near("opaque grid rail casts a shadow", visible(), {});
                    blocker(voxels, 10, 0, true, 8);
                    blocker(voxels, 12, 1, true, 8);
                    near("opaque grid bar casts a shadow", visible(), {});
                    blocker(voxels, 12, 0, true, 8);
                    near("grid opening transmits light", visible(), full);
                }

            // Compare the entire frame against the unaccelerated pixel DDA, including the
            // partial block at y=16. Report the first differing pixel per dispatch.
            auto compareVisibility = [&](const Scene &input, const char *label, unsigned pass,
                                          unsigned stage, unsigned sample, unsigned samples, unsigned seed,
                                          const Light *debugLight = nullptr,
                                          const std::vector<RemasterSurfaceMesh::Cell> *mesh = nullptr) {
                Scene optimized = input, reference = input;
                run(input, true, pass, 8, -1, &optimized, false, stage, 1, sample, samples, seed,
                    simd_float3{0, 0, -1}, -1, debugLight, false, false, false, 0, mesh);
                run(input, true, pass, 8, -1, &reference, false, stage, 1, sample, samples, seed,
                    simd_float3{0, 0, -1}, -1, debugLight, true, false, false, 0, mesh);
                unsigned mismatch = width * height;
                for (unsigned p = 0; p < width * height && mismatch == width * height; ++p)
                    for (unsigned c = 0; c < 4; ++c)
                    {
                        const float actual = optimized[NextBounce][p][c], expected = reference[NextBounce][p][c];
                        if (!std::isfinite(actual) || !std::isfinite(expected) ||
                            std::fabs(actual - expected) > 1e-9f + std::fabs(expected) * 1e-5f)
                            mismatch = p;
                    }
                char name[192];
                std::snprintf(name, sizeof(name), "%s pass=%u stage=%u sample=%u/%u seed=%u pixel=(%u,%u)",
                    label, pass, stage, sample, samples, seed, mismatch % width, mismatch / width);
                const unsigned p = mismatch < width * height ? mismatch : receiver;
                check(name, mismatch == width * height, optimized[NextBounce][p], reference[NextBounce][p]);
            };
            const unsigned visibilityChecks = checks;
            // Both XY directions, axis-aligned rays, exact and near corners, and
            // starts/ends on either side of an 8-pixel boundary.
            const unsigned endpoints[][4] = {
                {0, 0, 127, 0}, {127, 16, 0, 16}, {7, 0, 7, 16}, {8, 16, 8, 0},
                {0, 0, 16, 16}, {16, 16, 0, 0}, {7, 16, 23, 0}, {23, 0, 7, 16},
                {0, 0, 127, 16}, {127, 16, 0, 0}, {7, 7, 24, 16}, {24, 16, 7, 7}
            };
            for (unsigned ray = 0; ray < sizeof(endpoints) / sizeof(endpoints[0]); ++ray)
                for (int slope : {-1, 0, 1})
                    for (unsigned kind = 0; kind < 5; ++kind)
                    {
                        Scene fixture = scene();
                        fixture[Participation].assign(width * height, simd_float4{});
                        fixture[PreviousBounce].assign(width * height, simd_float4{});
                        const auto &xy = endpoints[ray];
                        const unsigned a = xy[1] * width + xy[0], b = xy[3] * width + xy[2];
                        const simd_float3 normal = simd_normalize(simd_float3{
                            float(int(xy[2]) - int(xy[0])), float(int(xy[3]) - int(xy[1])), 4});
                        fixture[Surface][a] = {16, normal.x, normal.y, normal.z};
                        fixture[Surface][b] = {16 + slope * 12.0f, -normal.x, -normal.y, normal.z};
                        fixture[Participation][a] = fixture[Participation][b] = {1, 1, 0, 0};
                        fixture[PreviousBounce][b] = {8, 4, 2, 1};
                        for (unsigned y = 0; y < height; ++y)
                            for (unsigned x = 0; x < width; ++x)
                            {
                                const unsigned p = y * width + x;
                                if (p == a || p == b || kind == 0 ||
                                    (x % 8 != 0 && x % 8 != 7 && y != 7 && y != 8 && y != 15 && y != 16))
                                    continue;
                                fixture[Occlusion][p] = {kind == 3 ? 0.25f : 1.0f, 1, 0, 0};
                                fixture[Height][p] = {0, kind == 4 ? 0.0f : 1.0f, 0, 0};
                                fixture[Surface][p].x = kind == 1 ? -0.25f : 16.125f;
                            }
                        char label[96];
                        std::snprintf(label, sizeof(label), "visibility ray=%u z-slope=%d blocker=%u", ray, slope, kind);
                        compareVisibility(fixture, label, 0, 4, 0, 1, 1);
                        compareVisibility(fixture, label, 0, 0, 0, 1, 1);
                    }
            for (unsigned seed : {1u, 19u, 97u, 65537u})
            {
                Scene randomScene = scene();
                uint32_t state = seed;
                auto random = [&] {
                    state ^= state << 13;
                    state ^= state >> 17;
                    state ^= state << 5;
                    return state;
                };
                for (unsigned p = 0; p < width * height; ++p)
                {
                    const float z = float(int(random() % 161) - 32) * 0.25f;
                    const simd_float3 normal = simd_normalize(simd_float3{
                        float(int(random() % 201) - 100), float(int(random() % 201) - 100), 20});
                    randomScene[Surface][p] = {z, normal.x, normal.y, normal.z};
                    randomScene[Height][p] = {z, random() % 7 == 0 ? 0.0f : 1.0f, 0, 0};
                    randomScene[Occlusion][p] = {random() % 9 == 0 ? float(1 + random() % 4) * 0.25f : 0, 1, 0, 0};
                    randomScene[Participation][p] = {random() % 4 == 0 ? 1.0f : 0.0f, 1, 0, 0};
                    randomScene[PreviousBounce][p] = random() % 3 == 0 ? simd_float4{8, 4, 2, 1} : simd_float4{};
                }
                for (unsigned pass : {0u, 1u})
                    for (unsigned samples : {1u, 4u})
                        for (unsigned sample = 0; sample < (samples == 1 ? 1u : 2u); ++sample)
                            compareVisibility(randomScene, "randomized visibility", pass, 0, sample, samples, seed);
                compareVisibility(randomScene, "randomized visibility diagnostic", 0, 4, 0, 1, seed);
                std::vector<RemasterSurfaceMesh::Sample> samples(width * height);
                for (unsigned p = 0; p < width * height; p++)
                {
                    samples[p].height = randomScene[Surface][p].x;
                    samples[p].known = randomScene[Height][p].y > 0.5f;
                    samples[p].coverage = randomScene[Occlusion][p].x;
                    samples[p].sheet = p % 3 != 0;
                    samples[p].domain = p % 3 + 1;
                }
                const auto mesh = RemasterSurfaceMesh::build(width, height, samples, 0.25f);
                compareVisibility(randomScene, "mixed triangle visibility", 0, 4, 0, 1, seed, nullptr, &mesh);
                compareVisibility(randomScene, "mixed triangle transport", 0, 0, 0, 1, seed, nullptr, &mesh);
            }
            // Sphere samples retain fractional XYZ endpoints, including outside the frame.
            const simd_float3 diskPositions[] = {
                {64.125f, 8.375f, 16}, {-4.25f, 8.125f, 16}, {132.125f, 8.375f, 16},
                {64.375f, -4.125f, 16}, {64.125f, 21.375f, 16}
            };
            for (unsigned position = 0; position < sizeof(diskPositions) / sizeof(diskPositions[0]); ++position)
                for (int slope : {-1, 1})
                    for (unsigned kind = 0; kind < 3; ++kind)
                    {
                        Scene disk = scene();
                        disk[PreviousBounce].assign(width * height, simd_float4{});
                        disk[Participation].assign(width * height, simd_float4{1, 1, 0, 0});
                        const simd_float3 normal = simd_normalize(simd_float3{-slope * 0.25f, 0, 1});
                        for (unsigned y = 0; y < height; ++y)
                            for (unsigned x = 0; x < width; ++x)
                            {
                                const unsigned p = y * width + x;
                                const float z = 16 + slope * (float(x) - 64) * 0.25f;
                                disk[Surface][p] = {z, normal.x, normal.y, normal.z};
                                disk[Height][p] = {z, 1, 0, 0};
                                if (kind && (x == 7 || x == 64 || x == 120 || y == 7 || y == 16))
                                {
                                    disk[Occlusion][p] = {kind == 1 ? 0.25f : 1.0f, 1, 0, 0};
                                    disk[Surface][p].x += 4.125f;
                                    disk[Height][p].y = kind == 2 ? 0.0f : 1.0f;
                                }
                            }
                        Light light = {};
                        light.position = diskPositions[position];
                        light.radius = 3.75f;
                        light.intensity = 25;
                        light.color = {1, 0.75f, 0.5f};
                        char label[96];
                        std::snprintf(label, sizeof(label), "sphere visibility position=%u slope=%d blocker=%u",
                            position, slope, kind);
                        for (unsigned stage : {4u, 0u})
                            compareVisibility(disk, label, 0, stage, 0, 1, 1, &light);
                    }
            std::printf("Visibility reference comparisons: %u\n", checks - visibilityChecks);
            std::printf("%u/%u checks passed; %u failed\n", checks - failures, checks, failures);
            return failures ? 1 : 0;
        }
        catch (const std::exception &error)
        {
            std::fprintf(stderr, "Test setup/execution failed: %s\n", error.what());
            return 2;
        }
    }
}
