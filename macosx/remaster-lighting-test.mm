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
struct Uniforms { uint32_t width, height, view, lightCount, passIndex; };
static_assert(sizeof(Light) == 48 && offsetof(Light, color) == 32, "Metal light ABI");
static_assert(sizeof(Uniforms) == 20, "Metal uniforms ABI");

constexpr unsigned width = 32, height = 17, receiverX = 4, receiverY = 8;
constexpr unsigned receiver = receiverY * width + receiverX;
constexpr unsigned emitter = receiverY * width + 26;
enum Field { Source, Occlusion, Surface, Height, Participation, PreviousBounce,
             PreviousIndirect, NextBounce, NextIndirect, Emission, Output, Direct,
             OppositeFacing, FieldCount };
using Image = std::vector<simd_float4>;
using Scene = std::array<Image, FieldCount>;
struct Result { simd_float4 bounce, accumulated; };

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
            id<MTLComputePipelineState> highlight = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterSelectedTileHighlight"] error:&error];
            require(highlight != nil, error.localizedDescription);
            id<MTLCommandQueue> queue = [device newCommandQueue];
            require(queue != nil, @"Could not create Metal command queue");
            std::printf("Device: %s; runtime shaders: macosx/shaders.metal; RGBA32Float\n",
                device.name.UTF8String);

            auto scene = [] {
                Scene s;
                for (auto &image : s)
                    image.assign(width * height, simd_float4{0, 0, 0, 0});
                s[Source].assign(width * height, simd_float4{1, 1, 1, 1});
                s[Surface].assign(width * height, simd_float4{8, 0, 0, 0});
                // Only the receiver and a distant one-pixel radiance patch participate.
                s[Surface][receiver] = {8, 1, 0, 0};
                s[Surface][emitter] = {8, -1, 0, 0};
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
                           float lightZ = 8.0f, int selectedPixel = -1, Scene *readback = nullptr) {
                const Field directBindings[] = {Source, Occlusion, Output, Emission,
                    Height, Surface, Direct, Participation, OppositeFacing};
                const Field indirectBindings[] = {Source, Occlusion, Surface, Height,
                    Participation, PreviousBounce, PreviousIndirect, NextBounce, NextIndirect};
                const Field *bindings = bounce ? indirectBindings : directBindings;
                unsigned count = 9;
                id<MTLTexture> textures[9];
                MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
                    texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                    width:width height:height mipmapped:NO];
                descriptor.storageMode = MTLStorageModeShared;
                descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
                for (unsigned i = 0; i < count; ++i)
                {
                    textures[i] = [device newTextureWithDescriptor:descriptor];
                    require(textures[i] != nil, @"RGBA32Float shared read/write texture unavailable");
                    [textures[i] replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
                        withBytes:s[bindings[i]].data() bytesPerRow:width * sizeof(simd_float4)];
                }
                Uniforms uniforms = {width, height, 0, 1, passIndex};
                Light light = {};
                light.position = {26.5f, receiverY + 0.5f, lightZ};
                light.radius = 64;
                light.intensity = 1;
                light.color = {1, 0.5f, 0.25f};
                id<MTLCommandBuffer> command = [queue commandBuffer];
                id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
                require(encoder != nil, @"Could not create compute encoder");
                [encoder setComputePipelineState:bounce ? indirect : direct];
                for (unsigned i = 0; i < count; ++i)
                    [encoder setTexture:textures[i] atIndex:i];
                [encoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];
                if (!bounce)
                    [encoder setBytes:&light length:sizeof(light) atIndex:1];
                [encoder dispatchThreadgroups:MTLSizeMake(width, height, 1)
                    threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
                [encoder endEncoding];
                id<MTLTexture> presentation = textures[bounce ? 8 : 2];
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
                    id<MTLTexture> highlighted = [device newTextureWithDescriptor:descriptor];
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
                [textures[bounce ? 7 : 6] getBytes:&result.bounce bytesPerRow:sizeof(simd_float4)
                    fromRegion:region mipmapLevel:0];
                [presentation getBytes:&result.accumulated bytesPerRow:sizeof(simd_float4)
                    fromRegion:region mipmapLevel:0];
                if (bounce && readback)
                {
                    [textures[7] getBytes:(*readback)[NextBounce].data() bytesPerRow:width * sizeof(simd_float4)
                        fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
                    [textures[8] getBytes:(*readback)[NextIndirect].data() bytesPerRow:width * sizeof(simd_float4)
                        fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
                }
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
            simd_float4 directClear = run(s, false).bounce;
            positive("direct unblocked point light", directClear);
            blocker(s, 7, 1, false, 0);
            near("direct missing-height full blocker shadows", run(s, false).bounce, {});
            blocker(s, 7, 1, true, 0);
            near("direct passes over low authored blocker", run(s, false).bounce, directClear);
            blocker(s, 7, 1, true, 16);
            near("direct high authored blocker shadows", run(s, false).bounce, {});
            blocker(s, 7, 0.5f, true, 16);
            dimmed("direct partial coverage dims", run(s, false).bounce, directClear);

            s = scene();
            s[Surface][receiver] = {8, -1, 0, 0};
            near("back-facing normal rejects direct light", run(s, false).bounce, {});
            s[OppositeFacing][receiver].x = 1;
            positive("opposite-facing option reverses XY for direct light", run(s, false).bounce);
            near("opposite-facing option does not alter indirect transport", run(s, true).bounce, {});

            s = scene();
            s[Surface][receiver] = {20.0f / 255 * 16, 0, 0, 1};
            Result raisedLight = run(s, false, 0, 8.5f);
            positive("raised torch illuminates front-facing speckled surface", raisedLight.bounce);
            blocker(s, 7, 1, true, 14.0f / 255 * 16);
            near("profile-scale wall lies below receiver-to-torch ray",
                run(s, false, 0, 8.5f).bounce, raisedLight.bounce);
            blocker(s, 7, 1, true, 5);
            near("wall above receiver-to-torch ray blocks direct light",
                run(s, false, 0, 8.5f).bounce, {});

            for (float scale : {8.0f, 255.0f})
            {
                const float dz = 32.0f / 255.0f * scale;
                const float distance = std::sqrt(22.0f * 22.0f + dz * dz);
                const float response = 0.9f * (dz / distance) /
                    (1.0f + 6.0f * distance * distance / (64.0f * 64.0f));
                const simd_float4 expected = {response, response * 0.5f, response * 0.25f, 1};
                for (float normalZ : {1.0f, -1.0f})
                {
                    s = scene();
                    // Height bytes 0 and 32 become world heights before reaching the shader.
                    const float receiverZ = normalZ > 0 ? 0 : dz;
                    s[Height][receiver] = {receiverZ, 1, 0, 0};
                    s[Surface][receiver] = {receiverZ, 0, 0, normalZ};
                    char name[96];
                    std::snprintf(name, sizeof(name), "direct Lambertian RGB scale %.0f normal %+.0fZ", scale, normalZ);
                    near(name, run(s, false, 0, receiverZ + normalZ * dz).bounce, expected);
                    std::snprintf(name, sizeof(name), "direct equal height scale %.0f normal %+.0fZ", scale, normalZ);
                    near(name, run(s, false, 0, receiverZ).bounce, {});
                    std::snprintf(name, sizeof(name), "direct backface height scale %.0f normal %+.0fZ", scale, normalZ);
                    near(name, run(s, false, 0, receiverZ - normalZ * dz).bounce, {});
                }
            }

            s = scene();
            Result clear = run(s, true);
            positive("indirect distant mutually facing radiance patch", clear.bounce);
            near("passIndex 0 ignores garbage previousIndirect", clear.accumulated, clear.bounce);
            s[PreviousBounce][emitter] = {};
            near("indirect needs radiance from previousBounce", run(s, true).bounce, {});
            s = scene();
            s[Surface][receiver] = s[Surface][emitter] = {8, 0, 0, 1};
            near("indirect coplanar +Z surfaces do not exchange light", run(s, true).bounce, {});

            simd_float4 diagonal[2];
            for (int side : {1, -1})
            {
                s = scene();
                s[Participation][emitter] = {};
                s[PreviousBounce][emitter] = {};
                const unsigned patch = int(receiver) + side * (int(width) + 1);
                const float normal = side / std::sqrt(2.0f);
                s[Surface][receiver] = {8, normal, normal, 0};
                s[Surface][patch] = {8, -normal, -normal, 0};
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
            chain[Surface][relay] = {8, -1, 0, 0};
            chain[Participation][relay] = {1, 1, 0, 0};
            for (unsigned y = 2; y <= 4; ++y)
                for (unsigned x = 4; x <= 6; ++x)
                {
                    const unsigned patch = y * width + x;
                    chain[Surface][patch] = {8, 1, 0, 0};
                    chain[Participation][patch] = {1, 1, 0, 0};
                    chain[PreviousBounce][patch] = {1, 0.5f, 0.25f, 1};
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
            near("passIndex 1 preserves new bounce", second.bounce, clear.bounce);
            near("passIndex 1 accumulates exactly prior + new", second.accumulated, prior + clear.bounce);
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
