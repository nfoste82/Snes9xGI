// Run from the repository root. Immutable production-frame Direct microprofile.
// xcrun clang++ -std=c++17 -O2 -fobjc-arc tools/profile_remaster_direct.mm \
//   -framework Foundation -framework Metal -o /path/to/profile-remaster-direct
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <vector>

struct Uniforms {
    uint32_t width, height, view, lightCount, passIndex, diagnosticStage;
    float roughness, originalContribution, heightMultiplier, padding;
    uint32_t sampleIndex, sampleCount, randomSeed;
    float reflectanceBoost;
    simd_float4 camera, sphere, color, heightRange;
};
static_assert(sizeof(Uniforms) == 128, "Uniform ABI");

static void require(bool ok, NSString *message) {
    if (!ok) throw std::runtime_error(message ? message.UTF8String : "Metal failure");
}
static uint64_t hash(const void *data, size_t length) {
    uint64_t h = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < length; ++i) h = (h ^ ((const uint8_t *)data)[i]) * UINT64_C(1099511628211);
    return h;
}
static NSString *replace(NSString *source, NSString *before, NSString *after) {
    require([source containsString:before], [@"Shader anchor missing: " stringByAppendingString:before]);
    return [source stringByReplacingOccurrencesOfString:before withString:after];
}

// Diagnostic clones leave the production helpers and indirect kernels intact.
// Per-invocation counters use thread-local integers and one final buffer store;
// no global atomic contention. Counted timings are not shipping timings.
static NSString *instrument(NSString *source) {
    NSRange start = [source rangeOfString:@"static bool remasterTriangleHit("];
    NSRange end = [source rangeOfString:@"// Complete binary sum tree."];
    require(start.location != NSNotFound && end.location > start.location, @"Visibility anchors missing");
    NSString *helpers = [source substringWithRange:NSMakeRange(start.location, end.location - start.location)];
    helpers = replace(helpers, @"remasterTriangleHit", @"profileTriangleHit");
    helpers = replace(helpers, @"remasterMeshHit", @"profileMeshHit");
    helpers = replace(helpers, @"remasterVisibility", @"profileVisibility");
    helpers = replace(helpers, @"float entry, float exit)", @"float entry, float exit, thread uint *counts)");
    helpers = replace(helpers, @"entry, exit)", @"entry, exit, counts)");
    helpers = replace(helpers, @"const device RemasterMeshCell *mesh)\n{", @"const device RemasterMeshCell *mesh, thread uint *counts)\n{");
    helpers = replace(helpers, @"float3 e1 = b - a", @"counts[5]++;\n\tfloat3 e1 = b - a");
    helpers = replace(helpers, @"float low = min(center", @"counts[3]++;\n\tfloat low = min(center");
    helpers = replace(helpers, @"if (min(z0, z1) > high || max(z0, z1) < low) return false;",
        @"if (min(z0, z1) > high || max(z0, z1) < low) { counts[4]++; return false; }");
    helpers = replace(helpers, @"float3 segment = to - from;", @"counts[0]++;\n\tfloat3 segment = to - from;");
    helpers = replace(helpers, @"0.0, min(1.0, min(next.x, next.y))))", @"0.0, min(1.0, min(next.x, next.y)), counts))");
    helpers = replace(helpers, @"testedBlock = block;", @"counts[2]++;\n\t\t\ttestedBlock = block;");
    helpers = replace(helpers, @"while (any(cell != endpoint))\n\t{", @"while (any(cell != endpoint))\n\t{\n\t\tcounts[1]++;");
    helpers = replace(helpers, @"if (emptyBlock)\n\t\t{", @"if (emptyBlock)\n\t\t{\n\t\t\tcounts[6]++;");
    helpers = replace(helpers, @"visibility *= 1.0 - coverage;", @"{ counts[7]++; visibility *= 1.0 - coverage; }");
    NSString *kernel = [source substringFromIndex:[source rangeOfString:@"kernel void remasterIndirectBounce("].location];
    NSRange kernelEnd = [kernel rangeOfString:@"kernel void remasterReduceDirectPartials("];
    require(kernelEnd.location != NSNotFound, @"Direct reduction anchor missing");
    NSString *body = [kernel substringToIndex:kernelEnd.location];
    NSString *original = body;
    body = replace(body, @"device float4 *directPartials [[buffer(5)]],", @"device float4 *directPartials [[buffer(5)]],\n\tdevice uint *profileCounts [[buffer(6)]],");
    body = replace(body, @"uint2 pixel = grid.xy;", @"uint2 pixel = grid.xy;\n\tuint counts[8] = {};");
    body = replace(body, @"remasterVisibility(receiverRayEndpoint, sourceRayEndpoint,\n\t\t\t\tocclusion, heightField, surfaceField, visibilityBlocks, mesh)",
        @"profileVisibility(receiverRayEndpoint, sourceRayEndpoint,\n\t\t\t\tocclusion, heightField, surfaceField, visibilityBlocks, mesh, counts)");
    body = replace(body, @"directPartials[(grid.z * uniforms.height + pixel.y) * uniforms.width + pixel.x] = float4(incoming, totalFormFactor);",
        @"uint base = ((grid.z * uniforms.height + pixel.y) * uniforms.width + pixel.x) * 8;\n\t\tfor (uint i = 0; i < 8; i++) profileCounts[base + i] = counts[i];\n\t\tdirectPartials[(grid.z * uniforms.height + pixel.y) * uniforms.width + pixel.x] = float4(incoming, totalFormFactor);");
    source = replace(source, original, body);
    return [source stringByReplacingOccurrencesOfString:@"kernel void remasterIndirectBounce("
        withString:[helpers stringByAppendingString:@"\nkernel void remasterIndirectBounce("]];
}

int main(int argc, const char **argv) {
    @autoreleasepool {
        try {
            require(argc == 2 || argc == 3, @"Usage: profile-remaster-direct SNAPSHOT_DIRECTORY [THREADS_XxY]");
            unsigned threadsX = 8, threadsY = 8;
            if (argc == 3) require(sscanf(argv[2], "%ux%u", &threadsX, &threadsY) == 2 &&
                threadsX && threadsY && threadsX <= 32 && threadsY <= 8 && threadsX * threadsY <= 64,
                @"Layout must fit production benchmark bounds (x<=32,y<=8,total<=64)");
            NSString *directory = [NSString stringWithUTF8String:argv[1]];
            auto read = [&](NSString *name) {
                NSData *data = [NSData dataWithContentsOfFile:[directory stringByAppendingPathComponent:name]];
                require(data != nil, [@"Missing snapshot file " stringByAppendingString:name]);
                return data;
            };
            NSData *uniformData = read(@"uniforms.bin");
            require(uniformData.length == sizeof(Uniforms), @"Uniform snapshot ABI mismatch");
            Uniforms u;
            memcpy(&u, uniformData.bytes, sizeof(u));
            std::printf("uniforms_fnv=%016llx\n", (unsigned long long)hash(uniformData.bytes, uniformData.length));
            require(u.width > 0 && u.width <= 512 && u.height > 0 && u.height <= 512 &&
                u.passIndex == 0 && u.color.w == 0, @"Requires bounded Direct snapshot without sphere");
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            require(device != nil, @"No Metal GPU");
            NSError *error = nil;
            NSString *source = [NSString stringWithContentsOfFile:@"macosx/shaders.metal" encoding:NSUTF8StringEncoding error:&error];
            require(source != nil, error.localizedDescription);
            std::printf("Device=%s shader_fnv=%016llx snapshot=%s\n", device.name.UTF8String,
                (unsigned long long)hash(source.UTF8String, strlen(source.UTF8String)), argv[1]);
            if (@available(macOS 10.15, *))
                for (id<MTLCounterSet> set in device.counterSets) {
                    std::printf("Public counter set: %s\n", set.name.UTF8String);
                    for (id<MTLCounter> counter in set.counters) std::printf("  %s\n", counter.name.UTF8String);
                }
            auto library = [&](NSString *text) {
                id<MTLLibrary> result = [device newLibraryWithSource:text options:nil error:&error];
                require(result != nil, error.localizedDescription);
                return result;
            };
            id<MTLLibrary> libs[4];
            libs[0] = library(source);
            libs[1] = library(replace(source, @"float3 segment = to - from;", @"return 1.0;\n\tfloat3 segment = to - from;"));
            libs[2] = library(replace(source, @"float entry, float exit)\n{\n\tfloat low = min(center", @"float entry, float exit)\n{\n\treturn false;\n\tfloat low = min(center"));
            libs[3] = library(instrument(source));
            auto pipeline = [&](id<MTLLibrary> lib, NSString *name, bool constants) {
                MTLFunctionConstantValues *values = [MTLFunctionConstantValues new];
                bool yes = true;
                [values setConstantValue:&yes type:MTLDataTypeBool atIndex:0];
                [values setConstantValue:&yes type:MTLDataTypeBool atIndex:1];
                id<MTLFunction> fn = constants ? [lib newFunctionWithName:name constantValues:values error:&error] : [lib newFunctionWithName:name];
                require(fn != nil, error.localizedDescription);
                id<MTLComputePipelineState> p = [device newComputePipelineStateWithFunction:fn error:&error];
                require(p != nil, error.localizedDescription);
                return p;
            };
            id<MTLComputePipelineState> transport[7];
            for (unsigned i = 0; i < 4; i++) {
                transport[i] = pipeline(libs[i], @"remasterIndirectBounce", true);
                std::printf("pipeline %u width=%lu max_threads=%lu static_tg_bytes=%lu\n", i,
                    (unsigned long)transport[i].threadExecutionWidth, (unsigned long)transport[i].maxTotalThreadsPerThreadgroup,
                    (unsigned long)transport[i].staticThreadgroupMemoryLength);
            }
            for (unsigned i = 4; i < 7; i++) transport[i] = pipeline(libs[0], @"remasterSampledDirect", false);
            auto build = pipeline(libs[0], @"remasterBuildVisibilityBlocks", false);
            auto seed = pipeline(libs[0], @"remasterDirectLighting", false);
            auto prepare = pipeline(libs[0], @"remasterPrepareDirectSamples", false);
            auto reduce = pipeline(libs[0], @"remasterReduceDirectPartials", false);
            size_t pixels = size_t(u.width) * u.height;
            auto buffer = [&](NSString *name, size_t stride, size_t count) {
                NSData *data = read(name);
                require(data.length == stride * count, [@"Invalid snapshot length: " stringByAppendingString:name]);
                std::printf("input %s bytes=%lu fnv=%016llx\n", name.UTF8String, (unsigned long)data.length,
                    (unsigned long long)hash(data.bytes, data.length));
                id<MTLBuffer> result = [device newBufferWithBytes:data.bytes length:data.length options:MTLResourceStorageModeShared];
                require(result != nil, @"Buffer allocation failed");
                return result;
            };
            NSData *sourceData = read(@"sources.bin");
            require(sourceData.length > 0 && sourceData.length % 8 == 0, @"Empty/malformed source list");
            uint32_t count = uint32_t(sourceData.length / 8), batches = (count + 7) / 8;
            require(pixels * batches * 16 <= 64 * 1024 * 1024, @"Snapshot exceeds production scratch bound");
            id<MTLBuffer> sources = buffer(@"sources.bin", 8, count);
            NSData *meshData = read(@"mesh.bin"), *surfaceData = read(@"surface.bin");
            NSData *occlusionData = read(@"occlusion.bin"), *heightData = read(@"height.bin");
            require(meshData.length == pixels * 48, @"Invalid mesh snapshot");
            id<MTLBuffer> mesh = [device newBufferWithBytes:meshData.bytes length:meshData.length options:MTLResourceStorageModeShared];
            require(mesh != nil, @"Mesh allocation failed");
            std::printf("input mesh.bin bytes=%lu fnv=%016llx\n", (unsigned long)meshData.length,
                (unsigned long long)hash(meshData.bytes, meshData.length));
            id<MTLBuffer> samples = [device newBufferWithLength:count * 64 options:MTLResourceStorageModePrivate];
            size_t partialBytes = pixels * batches * 16;
            id<MTLBuffer> partials = [device newBufferWithLength:partialBytes options:MTLResourceStorageModeShared];
            id<MTLBuffer> counts = [device newBufferWithLength:pixels * batches * 8 * 4 options:MTLResourceStorageModeShared];
            require(samples && partials && counts, @"Scratch allocation failed");
            auto texture = [&](MTLPixelFormat format, unsigned w, unsigned h, NSString *name, unsigned bpp) {
                MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
                d.storageMode = MTLStorageModeShared;
                d.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
                id<MTLTexture> t = [device newTextureWithDescriptor:d];
                require(t != nil, @"Texture allocation failed");
                if (name) {
                    NSData *data = [name isEqualToString:@"surface.bin"] ? surfaceData :
                        [name isEqualToString:@"occlusion.bin"] ? occlusionData :
                        [name isEqualToString:@"height.bin"] ? heightData : read(name);
                    require(data.length == size_t(w) * h * bpp, [@"Invalid texture snapshot: " stringByAppendingString:name]);
                    std::printf("input %s bytes=%lu fnv=%016llx\n", name.UTF8String, (unsigned long)data.length,
                        (unsigned long long)hash(data.bytes, data.length));
                    [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:data.bytes bytesPerRow:w * bpp];
                }
                return t;
            };
            id<MTLTexture> t[12];
            t[0] = texture(MTLPixelFormatRGBA8Unorm, u.width, u.height, @"source.bin", 4);
            t[1] = texture(MTLPixelFormatRG8Unorm, u.width, u.height, @"occlusion.bin", 2);
            t[2] = texture(MTLPixelFormatRGBA32Float, u.width, u.height, @"surface.bin", 16);
            t[3] = texture(MTLPixelFormatRG8Unorm, u.width, u.height, @"height.bin", 2);
            t[4] = texture(MTLPixelFormatRG8Unorm, u.width, u.height, @"participation.bin", 2);
            for (unsigned i = 5; i <= 8; i++) t[i] = texture(MTLPixelFormatRGBA16Float, u.width, u.height, nil, 8);
            t[9] = texture(MTLPixelFormatR8Unorm, u.width, u.height, @"opposite.bin", 1);
            t[10] = texture(MTLPixelFormatRG32Float, (u.width + 3) / 4, (u.height + 3) / 4, nil, 8);
            t[11] = texture(MTLPixelFormatRGBA32Float, u.width, u.height, @"reflectance.bin", 16);
            auto emission = texture(MTLPixelFormatRGBA8Unorm, u.width, u.height, @"emission.bin", 4);
            auto output = texture(MTLPixelFormatRGBA8Unorm, u.width, u.height, nil, 4);
            auto queue = [device newCommandQueue];
            auto setup = [queue commandBuffer];
            auto e = [setup computeCommandEncoder];
            [e setComputePipelineState:build];
            [e setTexture:t[1] atIndex:0]; [e setTexture:t[3] atIndex:1];
            [e setTexture:t[2] atIndex:2]; [e setTexture:t[10] atIndex:3];
            [e setBuffer:mesh offset:0 atIndex:0];
            [e dispatchThreads:MTLSizeMake(t[10].width, t[10].height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
            [e endEncoding];
            e = [setup computeCommandEncoder];
            [e setComputePipelineState:seed];
            id<MTLTexture> seeds[] = {t[0], t[1], output, emission, t[3], t[2], t[5], t[4], t[9]};
            for (unsigned i = 0; i < 9; i++) [e setTexture:seeds[i] atIndex:i];
            [e setBytes:&u length:sizeof(u) atIndex:0];
            simd_float4 zero[4] = {};
            [e setBytes:zero length:sizeof(zero) atIndex:1];
            [e dispatchThreads:MTLSizeMake(u.width, u.height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
            [e endEncoding];
            e = [setup computeCommandEncoder];
            [e setComputePipelineState:prepare];
            [e setTexture:t[1] atIndex:0]; [e setTexture:t[3] atIndex:1];
            [e setTexture:t[2] atIndex:2]; [e setTexture:t[5] atIndex:3];
            [e setBuffer:sources offset:0 atIndex:0]; [e setBuffer:samples offset:0 atIndex:1];
            [e setBuffer:mesh offset:0 atIndex:2]; [e setBytes:&count length:4 atIndex:3];
            [e dispatchThreads:MTLSizeMake(count, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
            [e endEncoding]; [setup commit]; [setup waitUntilCompleted];
            require(setup.status == MTLCommandBufferStatusCompleted, setup.error.localizedDescription);
            std::printf("Pinned %ux%u samples=%u batches=%u threads=%ux%u; transport-only command buffers, no stage counters\n",
                u.width, u.height, count, batches, threadsX, threadsY);
            const char *names[] = {"production", "visibility-disabled", "mesh-tests-disabled", "work-counted", "sampled-direct-16", "sampled-direct-32", "sampled-direct-64"};
            std::vector<double> times[7];
            std::vector<double> sampledMeans[3];
            for (auto &mean : sampledMeans) mean.resize(pixels * 4, 0.0);
            std::vector<uint8_t> baseline;
            // Rotate variant order each round to reduce monotonic timing drift.
            for (unsigned round = 0; round < 16; round++)
                for (unsigned j = 0; j < 7; j++) {
                    unsigned variant = (round + j) % 7;
                    memset(counts.contents, 0, counts.length);
                    auto cmd = [queue commandBuffer];
                    auto encoder = [cmd computeCommandEncoder];
                    [encoder setComputePipelineState:transport[variant]];
                    for (unsigned i = 0; i < 12; i++) [encoder setTexture:t[i] atIndex:i];
                    Uniforms dispatchUniforms = u;
                    if (variant >= 4) {
                        dispatchUniforms.sampleCount = 16u << (variant - 4);
                        dispatchUniforms.randomSeed = u.randomSeed + round * 7919;
                    }
                    [encoder setBytes:&dispatchUniforms length:sizeof(dispatchUniforms) atIndex:0];
                    [encoder setBuffer:sources offset:0 atIndex:1]; [encoder setBytes:&count length:4 atIndex:2];
                    [encoder setBuffer:mesh offset:0 atIndex:3]; [encoder setBuffer:samples offset:0 atIndex:4];
                    [encoder setBuffer:partials offset:0 atIndex:5];
                    if (variant == 3) [encoder setBuffer:counts offset:0 atIndex:6];
                    [encoder dispatchThreads:MTLSizeMake(u.width, u.height, variant >= 4 ? 1 : batches) threadsPerThreadgroup:MTLSizeMake(threadsX, threadsY, 1)];
                    [encoder endEncoding]; [cmd commit]; [cmd waitUntilCompleted];
                    require(cmd.status == MTLCommandBufferStatusCompleted, cmd.error.localizedDescription);
                    double ms = (cmd.GPUEndTime - cmd.GPUStartTime) * 1000;
                    require(ms > 0, @"GPU timestamps unavailable");
                    if (round >= 4) times[variant].push_back(ms);
                    if (round >= 4 && variant >= 4) {
                        std::vector<uint16_t> image(pixels * 4);
                        [t[7] getBytes:image.data() bytesPerRow:u.width * 8 fromRegion:MTLRegionMake2D(0, 0, u.width, u.height) mipmapLevel:0];
                        for (size_t p = 0; p < image.size(); p++) {
                            _Float16 value;
                            memcpy(&value, &image[p], sizeof(value));
                            require(std::isfinite(float(value)), @"Nonfinite sampled Direct output");
                            sampledMeans[variant - 4][p] += double(value) / 12.0;
                        }
                    }
                    if (round == 0 && variant == 0) baseline.assign((uint8_t *)partials.contents, (uint8_t *)partials.contents + partialBytes);
                    if (variant == 3) require(memcmp(baseline.data(), partials.contents, partialBytes) == 0,
                        @"Instrumentation changed production float32 partials");
                    if (round == 15 && variant == 3) {
                        uint64_t totals[8] = {};
                        const auto *values = (const uint32_t *)counts.contents;
                        for (size_t p = 0; p < pixels * batches; p++) for (unsigned c = 0; c < 8; c++) totals[c] += values[p * 8 + c];
                        const char *labels[] = {"visibility-rays", "DDA-iterations", "envelope-tests", "mesh-tests", "mesh-z-rejects", "triangle-tests", "empty-block-iterations", "mesh-hits"};
                        for (unsigned c = 0; c < 8; c++) std::printf("count %-24s %llu\n", labels[c], (unsigned long long)totals[c]);
                        std::printf("partials_exact=true fnv=%016llx\n", (unsigned long long)hash(partials.contents, partialBytes));
                        auto check = [queue commandBuffer];
                        auto r = [check computeCommandEncoder];
                        [r setComputePipelineState:reduce];
                        [r setBuffer:partials offset:0 atIndex:0]; [r setBytes:&batches length:4 atIndex:1];
                        [r setBytes:&u length:sizeof(u) atIndex:2];
                        [r setTexture:t[7] atIndex:0]; [r setTexture:t[8] atIndex:1];
                        [r setTexture:t[0] atIndex:2]; [r setTexture:t[11] atIndex:3]; [r setTexture:t[6] atIndex:4];
                        [r dispatchThreads:MTLSizeMake(u.width, u.height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
                        [r endEncoding]; [check commit]; [check waitUntilCompleted];
                        require(check.status == MTLCommandBufferStatusCompleted, check.error.localizedDescription);
                        std::vector<uint8_t> direct(pixels * 8);
                        [t[7] getBytes:direct.data() bytesPerRow:u.width * 8 fromRegion:MTLRegionMake2D(0, 0, u.width, u.height) mipmapLevel:0];
                        NSData *expected = [NSData dataWithContentsOfFile:[directory stringByAppendingPathComponent:@"direct-result.bin"]];
                        if (expected) {
                            require(expected.length == direct.size() && memcmp(expected.bytes, direct.data(), direct.size()) == 0,
                                @"Offline Direct differs from application's pinned GPU result");
                            std::puts("application_direct_half_exact=true");
                        } else std::puts("application_direct_half_check=skipped (direct-result.bin absent)");
                    }
                }
            for (unsigned i = 0; i < 7; i++) {
                std::sort(times[i].begin(), times[i].end());
                std::printf("%-24s median_ms=%.3f min=%.3f p95=%.3f n=%zu\n", names[i],
                    times[i][times[i].size()/2], times[i].front(), times[i].back(), times[i].size());
            }
            NSData *expected = [NSData dataWithContentsOfFile:[directory stringByAppendingPathComponent:@"direct-result.bin"]];
            if (expected && expected.length == pixels * 8) {
                const auto *half = (const uint16_t *)expected.bytes;
                for (unsigned variant = 0; variant < 3; variant++) {
                    double squaredError = 0.0, squaredReference = 0.0;
                    for (size_t p = 0; p < pixels * 4; p++) if (p % 4 != 3) {
                        _Float16 value;
                        memcpy(&value, &half[p], sizeof(value));
                        double error = sampledMeans[variant][p] - double(value);
                        squaredError += error * error;
                        squaredReference += double(value) * double(value);
                    }
                    std::printf("%s mean_12_frames_relative_rms=%.6f\n", names[variant + 4],
                        std::sqrt(squaredError / std::max(1e-30, squaredReference)));
                }
            }
            std::puts("Scratch is shared for readback (production partials are private); source uses runtime compilation.\n"
                      "Disabled variants change visibility, control flow, register allocation and compiler dead-code elimination.\n"
                      "Their time deltas are diagnostic sensitivity, not additive component timings or valid rendering modes.");
        } catch (const std::exception &error) {
            std::fprintf(stderr, "%s\n", error.what());
            return 1;
        }
    }
    return 0;
}
