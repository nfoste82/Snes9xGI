// Standalone, run from the repository root; optional argv[1] selects shader source.
// xcrun clang++ -std=c++17 -O2 -fobjc-arc -Wall -Wextra macosx/remaster-lighting-benchmark.mm \
//   -framework Foundation -framework Metal -o <temporary-directory>/remaster-lighting-benchmark
// Measures one indirect bounce, not total frame time. Scenes are synthetic, not captures.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

struct Uniforms
{
    uint32_t width, height, view, lightCount, passIndex, diagnosticStage;
    float indirectRoughness, originalSceneContribution, heightPreviewMultiplier, padding;
    uint32_t sampleIndex, sampleCount, randomSeed;
    simd_float4 cameraDirection, debugPositionRadius, debugColorIntensity;
};
static_assert(sizeof(Uniforms) == 112 && offsetof(Uniforms, cameraDirection) == 64,
    "Metal uniforms ABI");

static uint64_t checksum(const void *data, size_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    const auto *bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; ++i)
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash;
}

int main(int argc, const char *argv[])
{
    @autoreleasepool
    {
        try
        {
            auto require = [](bool ok, NSString *message) {
                if (!ok)
                    throw std::runtime_error(message ? message.UTF8String : "Metal operation failed");
            };
            require(argc <= 2, @"Usage: remaster-lighting-benchmark [shader.metal]");
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            require(device != nil, @"No Metal device available");
            NSError *error = nil;
            NSString *path = argc == 2 ? [NSString stringWithUTF8String:argv[1]] : @"macosx/shaders.metal";
            // Retain one immutable source snapshot throughout all scenes, even if the file changes.
            NSString *source = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:&error];
            require(source != nil, error.localizedDescription);
            NSData *sourceBytes = [source dataUsingEncoding:NSUTF8StringEncoding];
            id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
            require(library != nil, error.localizedDescription);
            MTLCompileOptions *options = [MTLCompileOptions new];
            options.preprocessorMacros = @{@"REMASTER_REFERENCE_VISIBILITY": @1};
            id<MTLLibrary> referenceLibrary = [device newLibraryWithSource:source options:options error:&error];
            require(referenceLibrary != nil, error.localizedDescription);
            id<MTLComputePipelineState> pipelines[2];
            for (unsigned variant = 0; variant < 2; ++variant)
            {
                id<MTLFunction> function = [(variant == 0 ? referenceLibrary : library)
                    newFunctionWithName:@"remasterIndirectBounce"];
                require(function != nil, @"Missing remasterIndirectBounce kernel");
                pipelines[variant] = [device newComputePipelineStateWithFunction:function error:&error];
                require(pipelines[variant] != nil, error.localizedDescription);
                require(pipelines[variant].maxTotalThreadsPerThreadgroup >= 64, @"8x8 threadgroups unavailable");
            }
            id<MTLFunction> buildFunction = [library newFunctionWithName:@"remasterBuildVisibilityBlocks"];
            require(buildFunction != nil, @"Missing remasterBuildVisibilityBlocks kernel");
            id<MTLComputePipelineState> buildPipeline = [device newComputePipelineStateWithFunction:buildFunction error:&error];
            require(buildPipeline != nil, error.localizedDescription);
            id<MTLCommandQueue> queue = [device newCommandQueue];
            require(queue != nil, @"Could not create command queue");

            constexpr unsigned width = 256, height = 224, pixels = width * height;
            constexpr unsigned warmups = 2, repeats = 9;
            constexpr uint32_t seed = 0x5eed1234;
            Uniforms uniforms = {width, height, 0, 0, 1, 0, 1.0f, 0.65f, 8.0f, 0.0f,
                0, 1, seed, {0, 0, -1, 0}, {}, {}};
            uint32_t zero = 0;
            id<MTLBuffer> constants = [device newBufferWithBytes:&uniforms length:sizeof(uniforms)
                options:MTLResourceStorageModeShared];
            id<MTLBuffer> emptyEmitters = [device newBufferWithBytes:&zero length:sizeof(zero)
                options:MTLResourceStorageModeShared];
            require(constants != nil && emptyEmitters != nil, @"Could not create constant buffers");
            std::printf("Device: %s; OS: %s\nShader: %s; source_fnv1a=%016llx\n",
                device.name.UTF8String, NSProcessInfo.processInfo.operatingSystemVersionString.UTF8String,
                path.UTF8String, static_cast<unsigned long long>(checksum(sourceBytes.bytes, sourceBytes.length)));
            std::printf("SYNTHETIC bowl scenes (not real captures); %ux%u; threads=8x8; passIndex=1; "
                "sampleCount=1; seed=0x%08x; warmups=%u; repeats=%u\n",
                width, height, seed, warmups, repeats);
            std::printf("Production formats: RGBA8Unorm source, RG8Unorm fields, RGBA32Float surface, "
                "RGBA16Float radiance, R8Unorm facing, R32Float blocks; shared storage\n");
            std::printf("Reference macro REMASTER_REFERENCE_VISIBILITY=1; exact RGB comparison; "
                "block build timed once per scene, separately from indirect medians\n");
            std::fflush(stdout);

            const char *names[] = {"open-long-paths", "known-low-blockers", "tall-opaque", "fractional-unknown"};
            bool mismatch = false;
            for (unsigned scene = 0; scene < 4; ++scene)
            {
                @autoreleasepool
                {
                    std::vector<uint8_t> painted(pixels * 4), occlusion(pixels * 2), heights(pixels * 2),
                        participation(pixels * 2, 255), facing(pixels, 0);
                    std::vector<simd_float4> surfaces(pixels);
                    std::vector<__fp16> radiance(pixels * 4), black(pixels * 4, 0);
                    uint32_t random = seed;
                    unsigned covered = 0, unknown = 0;
                    for (unsigned y = 0; y < height; ++y)
                        for (unsigned x = 0; x < width; ++x)
                        {
                            unsigned p = y * width + x;
                            float dx = x + 0.5f - width * 0.5f, dy = y + 0.5f - height * 0.5f;
                            // Convex height graph: chords lie above the bowl, with inward-facing
                            // normals at both endpoints. Long rays reach visibility, not cosine rejection.
                            float z = 16.0f + 0.003f * (dx * dx + dy * dy);
                            simd_float3 normal = simd_normalize(simd_float3{-0.006f * dx, -0.006f * dy, 1});
                            bool stripe = (x % 64 == 31 && y > 16 && y < height - 16) ||
                                (y % 64 == 31 && x > 16 && x < width - 16);
                            uint8_t coverage = scene == 0 ? 0 : 255;
                            bool known = true;
                            if (scene == 1 && stripe)
                                z = 0.0f; // Opaque, authored, but below every bowl-to-bowl ray.
                            if (scene == 2 && stripe)
                                z = 192.0f;
                            if (scene == 3 && stripe)
                            {
                                coverage = (x + y) % 3 == 0 ? 96 : 160;
                                known = (x + y) % 2 == 0;
                                z = known ? 192.0f : 0.0f;
                            }
                            surfaces[p] = {z, normal.x, normal.y, normal.z};
                            occlusion[p * 2] = coverage;
                            occlusion[p * 2 + 1] = 255;
                            heights[p * 2] = static_cast<uint8_t>(std::min(z, 255.0f));
                            heights[p * 2 + 1] = known ? 255 : 0;
                            covered += coverage != 0;
                            unknown += !known;
                            for (unsigned c = 0; c < 3; ++c)
                            {
                                random ^= random << 13;
                                random ^= random >> 17;
                                random ^= random << 5;
                                painted[p * 4 + c] = 160 + (random & 63);
                                // Deliberate dense positive radiance seed, not a simulated direct pass.
                                radiance[p * 4 + c] = 0.25f + float((random >> 8) & 1023) / 1024.0f;
                            }
                            painted[p * 4 + 3] = 255;
                            radiance[p * 4 + 3] = 1;
                        }

                    const MTLPixelFormat formats[] = {MTLPixelFormatRGBA8Unorm, MTLPixelFormatRG8Unorm,
                        MTLPixelFormatRGBA32Float, MTLPixelFormatRG8Unorm, MTLPixelFormatRG8Unorm,
                        MTLPixelFormatRGBA16Float, MTLPixelFormatRGBA16Float, MTLPixelFormatRGBA16Float,
                        MTLPixelFormatRGBA16Float, MTLPixelFormatR8Unorm};
                    const void *uploads[] = {painted.data(), occlusion.data(), surfaces.data(), heights.data(),
                        participation.data(), radiance.data(), black.data(), nullptr, nullptr, facing.data()};
                    const unsigned bytesPerPixel[] = {4, 2, 16, 2, 2, 8, 8, 8, 8, 1};
                    id<MTLTexture> textures[10];
                    for (unsigned i = 0; i < 10; ++i)
                    {
                        MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                            formats[i] width:width height:height mipmapped:NO];
                        descriptor.storageMode = MTLStorageModeShared;
                        descriptor.usage = i == 7 || i == 8 ? MTLTextureUsageShaderWrite : MTLTextureUsageShaderRead;
                        textures[i] = [device newTextureWithDescriptor:descriptor];
                        require(textures[i] != nil, @"Could not create production-format texture");
                        if (uploads[i])
                            [textures[i] replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
                                withBytes:uploads[i] bytesPerRow:width * bytesPerPixel[i]];
                    }
                    constexpr unsigned blockWidth = (width + 7) / 8, blockHeight = (height + 7) / 8;
                    MTLTextureDescriptor *blockDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                        MTLPixelFormatR32Float width:blockWidth height:blockHeight mipmapped:NO];
                    blockDescriptor.storageMode = MTLStorageModeShared;
                    blockDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
                    id<MTLTexture> blocks = [device newTextureWithDescriptor:blockDescriptor];
                    require(blocks != nil, @"Could not create visibility blocks");
                    id<MTLCommandBuffer> build = [queue commandBuffer];
                    id<MTLComputeCommandEncoder> builder = [build computeCommandEncoder];
                    require(builder != nil, @"Could not create block encoder");
                    [builder setComputePipelineState:buildPipeline];
                    [builder setTexture:textures[1] atIndex:0];
                    [builder setTexture:textures[3] atIndex:1];
                    [builder setTexture:textures[2] atIndex:2];
                    [builder setTexture:blocks atIndex:3];
                    [builder dispatchThreadgroups:MTLSizeMake((blockWidth + 7) / 8, (blockHeight + 7) / 8, 1)
                        threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
                    [builder endEncoding];
                    [build commit];
                    [build waitUntilCompleted];
                    require(build.status == MTLCommandBufferStatusCompleted, build.error.localizedDescription);
                    double buildMs = (build.GPUEndTime - build.GPUStartTime) * 1000.0;
                    require(std::isfinite(buildMs) && buildMs > 0, @"Block GPU timestamps unavailable");
                    std::printf("%s block_build_ms=%.6f covered=%u unknown=%u\n",
                        names[scene], buildMs, covered, unknown);
                    std::vector<__fp16> reference;
                    double medians[2];
                    for (unsigned variant = 0; variant < 2; ++variant)
                    {
                    // Pre-encode all dispatches. No resource allocations, uploads, blits, or
                    // readback in measured command buffers, and no ping-pong of the fixed seed.
                    id<MTLCommandBuffer> commands[warmups + repeats];
                    for (unsigned i = 0; i < warmups + repeats; ++i)
                    {
                        commands[i] = [queue commandBuffer];
                        id<MTLComputeCommandEncoder> encoder = [commands[i] computeCommandEncoder];
                        require(encoder != nil, @"Could not create compute encoder");
                        [encoder setComputePipelineState:pipelines[variant]];
                        for (unsigned binding = 0; binding < 10; ++binding)
                            [encoder setTexture:textures[binding] atIndex:binding];
                        [encoder setTexture:blocks atIndex:10];
                        [encoder setBuffer:constants offset:0 atIndex:0];
                        [encoder setBuffer:emptyEmitters offset:0 atIndex:1];
                        [encoder setBuffer:emptyEmitters offset:0 atIndex:2];
                        [encoder dispatchThreadgroups:MTLSizeMake(width / 8, height / 8, 1)
                            threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
                        [encoder endEncoding];
                    }
                    std::array<double, repeats> times;
                    for (unsigned i = 0; i < warmups + repeats; ++i)
                    {
                        [commands[i] commit];
                        [commands[i] waitUntilCompleted];
                        require(commands[i].status == MTLCommandBufferStatusCompleted, commands[i].error.localizedDescription);
                        double milliseconds = (commands[i].GPUEndTime - commands[i].GPUStartTime) * 1000.0;
                        require(std::isfinite(milliseconds) && milliseconds > 0, @"GPU timestamps unavailable");
                        if (i >= warmups)
                            times[i - warmups] = milliseconds;
                    }
                    std::sort(times.begin(), times.end());
                    medians[variant] = times[repeats / 2];
                    std::vector<__fp16> bounce(pixels * 4), indirect(pixels * 4);
                    [textures[7] getBytes:bounce.data() bytesPerRow:width * 8
                        fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
                    [textures[8] getBytes:indirect.data() bytesPerRow:width * 8
                        fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
                    unsigned nonzero = 0;
                    double sum = 0;
                    for (unsigned p = 0; p < pixels; ++p)
                    {
                        bool positive = false;
                        for (unsigned c = 0; c < 3; ++c)
                        {
                            float value = bounce[p * 4 + c];
                            require(std::isfinite(value) && value >= 0, @"Invalid output radiance");
                            positive |= value > 0;
                            sum += value;
                        }
                        require(bounce[p * 4 + 3] == 1, @"Unwritten output pixel");
                        nonzero += positive;
                    }
                    require(nonzero > 0, @"Scene produced no positive radiance");
                    require(std::memcmp(bounce.data(), indirect.data(), bounce.size() * sizeof(__fp16)) == 0,
                        @"passIndex=1 bounce/indirect outputs differ");
                    std::printf("%-22s %-11s median_ms=%.3f min_ms=%.3f max_ms=%.3f "
                        "checksum=%016llx nonzero=%u/%u rgb_sum=%.9f\n",
                        names[scene], variant == 0 ? "reference" : "accelerated", times[repeats / 2], times.front(), times.back(),
                        static_cast<unsigned long long>(checksum(bounce.data(), bounce.size() * sizeof(__fp16))),
                        nonzero, pixels, sum);
                    if (variant == 0)
                        reference = bounce;
                    else
                    {
                        unsigned differences = 0, first = 0;
                        float maxAbsolute = 0, maxRelative = 0;
                        for (unsigned p = 0; p < pixels; ++p)
                            for (unsigned c = 0; c < 3; ++c)
                            {
                                unsigned index = p * 4 + c;
                                float expected = reference[index], actual = bounce[index];
                                float absolute = std::abs(actual - expected);
                                // One minimum half subnormal keeps relative error defined at zero.
                                float relative = absolute / std::max(std::abs(expected), 0x1p-24f);
                                maxAbsolute = std::max(maxAbsolute, absolute);
                                maxRelative = std::max(maxRelative, relative);
                                if (actual != expected)
                                {
                                    if (differences == 0)
                                        first = index;
                                    ++differences;
                                }
                            }
                        std::printf("%s comparison differences=%u/%u max_abs=%.9g max_rel=%.9g "
                            "speedup=%.3fx with_build=%.3fx\n", names[scene], differences, pixels * 3,
                            maxAbsolute, maxRelative, medians[0] / medians[1], medians[0] / (medians[1] + buildMs));
                        if (differences)
                            std::printf("MISMATCH first_pixel=(%u,%u) channel=%u reference=%.9g accelerated=%.9g\n",
                                (first / 4) % width, (first / 4) / width, first % 4,
                                float(reference[first]), float(bounce[first]));
                        mismatch |= differences != 0;
                    }
                    std::fflush(stdout);
                    }
                }
            }
            require(!mismatch, @"Reference/accelerated RGB mismatch (exact comparison required)");
            return 0;
        }
        catch (const std::exception &error)
        {
            std::fprintf(stderr, "FAIL: %s\n", error.what());
            return 1;
        }
    }
}
