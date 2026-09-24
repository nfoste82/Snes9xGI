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
    uint32_t sampleIndex, sampleCount, randomSeed;
    simd_float4 cameraDirection;
    simd_float4 debugPositionRadius;
    simd_float4 debugColorIntensity;
};
static_assert(sizeof(Light) == 48 && offsetof(Light, color) == 32, "Metal light ABI");
static_assert(sizeof(Uniforms) == 96 && offsetof(Uniforms, cameraDirection) == 48, "Metal uniforms ABI");

constexpr unsigned width = 128, height = 17, receiverX = 4, receiverY = 8;
constexpr unsigned receiver = receiverY * width + receiverX;
constexpr unsigned emitter = receiverY * width + 26;
enum Field { Source, Occlusion, Surface, Height, Participation, PreviousBounce,
             PreviousIndirect, NextBounce, NextIndirect, Emission, Output, Direct,
             OppositeFacing, FieldCount };
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
            id<MTLComputePipelineState> indirect = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterIndirectBounce"] error:&error];
            require(indirect != nil, error.localizedDescription);
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
                             const Light *debugLight = nullptr) {
                const Field directBindings[] = {Source, Occlusion, Output, Emission,
                    Height, Surface, Direct, Participation, OppositeFacing};
                const Field indirectBindings[] = {Source, Occlusion, Surface, Height,
                    Participation, PreviousBounce, PreviousIndirect, NextBounce, NextIndirect, OppositeFacing};
                // Direct holds the emission seed; NextBounce holds the saved first collision.
                const Field compositeBindings[] = {Source, Direct, NextBounce, NextIndirect, Output, Surface};
                const bool composing = compositeView >= 0;
                const Field *bindings = composing ? compositeBindings : bounce ? indirectBindings : directBindings;
                unsigned count = composing ? 6 : bounce ? 10 : 9;
                id<MTLTexture> textures[10];
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
                    indirectRoughness, 0.65f, sampleIndex, sampleCount, randomSeed,
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
                id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
                require(encoder != nil, @"Could not create compute encoder");
                [encoder setComputePipelineState:composing ? composite : bounce ? indirect : direct];
                for (unsigned i = 0; i < count; ++i)
                    [encoder setTexture:textures[i] atIndex:i];
                [encoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
                if (!bounce && !composing)
                    [encoder setBytes:&light length:sizeof(light) atIndex:1];
                if (bounce && !composing)
                {
                    std::vector<uint32_t> emitters;
                    for (uint32_t pixel = 0; pixel < width * height; ++pixel)
                        if (s[Participation][pixel].x > 0.5f &&
                            (s[PreviousBounce][pixel].x > 0 || s[PreviousBounce][pixel].y > 0 || s[PreviousBounce][pixel].z > 0))
                            emitters.push_back(pixel);
                    const uint32_t emitterCount = static_cast<uint32_t>(emitters.size());
                    if (emitters.empty())
                        emitters.push_back(0);
                    id<MTLBuffer> emitterBuffer = [device newBufferWithBytes:emitters.data()
                        length:emitters.size() * sizeof(uint32_t) options:MTLResourceStorageModeShared];
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
                Uniforms uniforms = {1, 1, 0, 0, 0, 0, 1, 0.65f, 1, 2, 1, {0, 0, -1, 0}, {}, {}};
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
                blocker(visibility, 7, 1, true, 16);
                near("opaque high blocker has zero visibility", visible().bounce, {});
                blocker(visibility, 7, 1, true, 0);
                near("visibility passes over low authored blocker", visible().bounce, simd_float4{1, 1, 1, 1});
                blocker(visibility, 7, 0.5f, true, 16);
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
                near("missing-height blocker remains opaque in visibility", visible().bounce, {});
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
                blocker(distant, 80, 1, false, 0);
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
                near("distant indirect respects missing-height blocker",
                    run(distant, true, 1, 8, -1, nullptr, half).bounce, {});
                blocker(distant, 80, 1, true, 16);
                near("distant indirect respects high authored blocker",
                    run(distant, true, 1, 8, -1, nullptr, half).bounce, {});
                blocker(distant, 80, 1, true, 0);
                near("distant indirect passes over low blocker",
                    run(distant, true, 1, 8, -1, nullptr, half, 0, 0).bounce, clearFar.bounce);
                blocker(distant, 80, 0.5f, true, 16);
                dimmed("distant indirect retains fractional transmittance",
                    run(distant, true, 1, 8, -1, nullptr, half, 0, 0).bounce, clearFar.bounce);
                // A farther endpoint may clear a blocker that hides a nearer, lower one.
                blocker(distant, 80, 1, true, 16);
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
            s[PreviousBounce][emitter] *= 8.0f;
            near("indirect HDR input scales linearly without gamma decoding radiance", run(s, true).bounce,
                paintedReflectance.bounce * 8.0f);
            s[PreviousBounce][emitter] *= 8192.0f;
            const Result hdr = run(s, true);
            near("indirect outgoing HDR radiance is not clamped", hdr.bounce,
                paintedReflectance.bounce * 65536.0f);
            check("HDR regression exercises outgoing radiance above one", hdr.bounce.z > 1.0f,
                hdr.bounce, {});
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
                positive("debug disk lights floor beyond 64 pixels", clearDisk);
                const float d2 = 100 * 100 + 24 * 24;
                const float analytic = 0.9f * 100 * 16 * 24 * 24 / (d2 * (d2 + 1));
                check("small distant disk matches area and inverse-square cosine reference",
                    std::fabs(clearDisk.x - analytic) < analytic * 0.02f, clearDisk, simd_float4{analytic, analytic, analytic, 1});
                light.intensity = 200;
                near("debug intensity is linear and calibrated like authored emission", illuminate().bounce, clearDisk * 8);
                light.intensity = 25;
                light.radius = 2;
                dimmed("smaller physical disk emits less power", illuminate().bounce, clearDisk);
                light.radius = 4;
                for (unsigned y = 0; y < height; y++)
                {
                    const unsigned p = y * width + 80;
                    disk[Occlusion][p] = {1, 1, 0, 0};
                    disk[Height][p] = {40, 1, 0, 0};
                    disk[Surface][p] = {40, 0, 0, 1};
                }
                near("debug disk respects full-height wall", illuminate().bounce, {});
                for (unsigned y = 0; y < height; y++)
                    disk[Surface][y * width + 80].x = 0;
                near("debug disk clears low wall", illuminate().bounce, clearDisk);
                light.color = {1, 0, 0};
                near("debug disk preserves light color", illuminate().bounce, simd_float4{clearDisk.x, 0, 0, 1});
                light.color = {1, 1, 1};
                disk[Surface][receiver].x = 50;
                near("debug disk does not light higher out-of-bounds surface", illuminate().bounce, {});
                disk[Surface][receiver].x = 0;
                disk[Participation][receiver].y = 0;
                near("debug disk honors receives GI", illuminate().bounce, {});
                near("debug visibility ignores receives GI", illuminate(0, 4).bounce, simd_float4{1, 1, 1, 1});
                disk[Participation][receiver].x = 0;
                near("debug disk excludes UI", illuminate().bounce, {});
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
                check("composite displays debug disk without altering scene pixels", glow.x > 1, glow, {});
                near("Direct diagnostic excludes disk self-emission", illuminate(0, 0, 3).accumulated, {});
                disk[Surface][receiver].x = 50;
                const float ambient = std::pow(0.65f, 1.0f / 2.2f);
                near("higher geometry hides debug disk glow", illuminate(0, 0, 0).accumulated, simd_float4{ambient, ambient, ambient, 1});
                disk[Surface][receiver].x = 0;
                light.radius = 0;
                near("zero radius also removes visible glow", illuminate(0, 0, 0).accumulated, simd_float4{ambient, ambient, ambient, 1});
                for (unsigned y = 0; y < height; y++)
                    disk[Occlusion][y * width + 80] = {};
                light.radius = 4096;
                const auto largeDisk = illuminate().bounce;
                const float capped = 0.9f * 100 * 0.95f;
                check("large overhead disk approaches capped hemisphere irradiance",
                    std::fabs(largeDisk.x - capped) < capped * 0.03f, largeDisk, simd_float4{capped, capped, capped, 1});
                for (float radius : {4.0f, 24.0f, 64.0f, 256.0f})
                {
                    light.radius = radius;
                    const float reference = 90 * std::min(0.95f, radius * radius / (radius * radius + 24 * 24));
                    const auto actual = illuminate().bounce;
                    check("overhead disk tracks analytic irradiance across sizes",
                        std::fabs(actual.x - reference) < reference * 0.04f, actual,
                        simd_float4{reference, reference, reference, 1});
                }
                light.radius = 4;
                light.position = {14.5f, receiverY + 0.5f, 32};
                Scene relayDisk = scene();
                relayDisk[PreviousBounce][emitter] = {};
                run(relayDisk, true, 0, 8, -1, &relayDisk, half, 0, 0,
                    0, 1, 1, simd_float3{0, 0, -1}, -1, &light);
                positive("debug disk produces Direct on reflecting surfaces", relayDisk[NextBounce][receiver]);
                relayDisk[PreviousBounce] = relayDisk[NextBounce];
                const auto secondary = run(relayDisk, true, 1, 8, -1, nullptr, half, 0, 0).bounce;
                positive("debug disk Direct seeds secondary reflected light", secondary);
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
                blocker(s, distance, 1, false, 0);
                char name[96];
                std::snprintf(name, sizeof(name), "indirect thin missing-height blocker at distance %u", distance);
                near(name, run(s, true).bounce, {});
            }
            s = scene();
            blocker(s, 7, 1, true, 0);
            near("indirect passes over low authored blocker", run(s, true).bounce, clear.bounce);
            blocker(s, 7, 1, true, 16);
            near("indirect high authored blocker shadows", run(s, true).bounce, {});
            blocker(s, 7, 0.5f, true, 16);
            dimmed("indirect partial blocker between samples dims", run(s, true).bounce, clear.bounce);
            s = scene();
            blocker(s, 6, 0.5f, false, 0);
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
            blocker(s, 7, 1, true, 16);
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
