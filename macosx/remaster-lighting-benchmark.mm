// Standalone, run from the repository root; optional argv[1] selects shader source.
// xcrun clang++ -std=c++17 -O2 -fobjc-arc -Wall -Wextra macosx/remaster-lighting-benchmark.mm \
//   -framework Foundation -framework Metal -o <temporary-directory>/remaster-lighting-benchmark
// Measures dense bounces and the complete sampled one-bounce pipeline, including
// source and visibility updates. Separately times each stage to locate bottlenecks.
// Scenes are synthetic, not captures.
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
#include "../remaster/surface_mesh.h"

struct Uniforms
{
    uint32_t width, height, view, lightCount, passIndex, diagnosticStage;
    float indirectRoughness, originalSceneContribution, heightPreviewMultiplier, padding;
	uint32_t sampleIndex, sampleCount, randomSeed;
	float reflectanceBoost;
    simd_float4 cameraDirection, debugPositionRadius, debugColorIntensity, heightPreviewRange;
};
static_assert(sizeof(Uniforms) == 128 && offsetof(Uniforms, cameraDirection) == 64,
    "Metal uniforms ABI");

static uint64_t checksum(const void *data, size_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    const auto *bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; ++i)
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash;
}

static void printGpuTimes(const char *scene, uint32_t connections, const char *stage,
    std::vector<double> times, bool direct = false)
{
    std::sort(times.begin(), times.end());
    const auto percentile = [&](unsigned percentage) {
        // Nearest-rank percentile, with the rank rounded up.
        return times[(times.size() * percentage + 99) / 100 - 1];
    };
    std::printf("%-22s %s=%u %-16s gpu_ms median=%.3f p95=%.3f p99=%.3f max=%.3f\n",
        scene, direct ? "direct samples" : "sampled connections", connections, stage,
        times[times.size() / 2], percentile(95), percentile(99), times.back());
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
            require(argc <= 2, @"Usage: remaster-lighting-benchmark [shader.metal | --direct]");
            const bool directBenchmark = argc == 2 && std::strcmp(argv[1], "--direct") == 0;
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            require(device != nil, @"No Metal device available");
            NSError *error = nil;
            NSString *path = argc == 2 && !directBenchmark ? [NSString stringWithUTF8String:argv[1]] : @"macosx/shaders.metal";
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
            MTLFunctionConstantValues *referenceConstants = [MTLFunctionConstantValues new];
            bool unprepared = false;
            [referenceConstants setConstantValue:&unprepared type:MTLDataTypeBool atIndex:0];
            for (unsigned variant = 0; variant < 2; ++variant)
            {
                id<MTLFunction> function = [(variant == 0 ? referenceLibrary : library)
                    newFunctionWithName:@"remasterIndirectBounce" constantValues:referenceConstants error:&error];
                require(function != nil, @"Missing remasterIndirectBounce kernel");
                pipelines[variant] = [device newComputePipelineStateWithFunction:function error:&error];
                require(pipelines[variant] != nil, error.localizedDescription);
                require(pipelines[variant].maxTotalThreadsPerThreadgroup >= 64, @"8x8 threadgroups unavailable");
            }
            id<MTLFunction> buildFunction = [library newFunctionWithName:@"remasterBuildVisibilityBlocks"];
            require(buildFunction != nil, @"Missing remasterBuildVisibilityBlocks kernel");
            id<MTLComputePipelineState> buildPipeline = [device newComputePipelineStateWithFunction:buildFunction error:&error];
            require(buildPipeline != nil, error.localizedDescription);
            id<MTLComputePipelineState> powerLeaves = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterBuildSourcePowerLeaves"] error:&error];
            id<MTLComputePipelineState> powerReduce = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterReduceSourcePower"] error:&error];
            id<MTLComputePipelineState> sampledPipeline = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterSampledIndirectBounce" constantValues:referenceConstants error:&error] error:&error];
            require(powerLeaves && powerReduce && sampledPipeline, error.localizedDescription);
            id<MTLComputePipelineState> preparePipeline = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterPrepareDirectSamples"] error:&error];
            require(preparePipeline != nil, error.localizedDescription);
            MTLFunctionConstantValues *directConstants = [MTLFunctionConstantValues new];
            bool specialized = true;
            [directConstants setConstantValue:&specialized type:MTLDataTypeBool atIndex:0];
            id<MTLComputePipelineState> directTransport = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterIndirectBounce" constantValues:directConstants error:&error] error:&error];
            require(directTransport != nil, error.localizedDescription);
            [directConstants setConstantValue:&specialized type:MTLDataTypeBool atIndex:1];
            id<MTLComputePipelineState> batchedTransport = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterIndirectBounce" constantValues:directConstants error:&error] error:&error];
            id<MTLComputePipelineState> reduceDirect = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"remasterReduceDirectPartials"] error:&error];
            require(batchedTransport && reduceDirect, error.localizedDescription);
            const bool batched = getenv("S9X_REMASTER_TEST_BATCHED") != nullptr;
            if (directBenchmark) std::printf("Prepared production direct batching: %s (8 samples/batch)\n", batched ? "enabled" : "disabled");
            id<MTLCommandQueue> queue = [device newCommandQueue];
            require(queue != nil, @"Could not create command queue");

            constexpr unsigned width = 256, height = 224, pixels = width * height;
            constexpr unsigned warmups = 2, repeats = 9;
            constexpr unsigned sampledWarmups = 3, sampledRepeats = 51;
            constexpr uint32_t seed = 0x5eed1234;
            Uniforms uniforms = {width, height, 0, 0, 1, 0, 1.0f, 0.65f, 8.0f, 0.0f,
                0, 1, seed, 0.0f, {0, 0, -1, 0}, {}, {}};
            uint32_t zero = 0;
            id<MTLBuffer> constants = [device newBufferWithBytes:&uniforms length:sizeof(uniforms)
                options:MTLResourceStorageModeShared];
            id<MTLBuffer> emptyEmitters = [device newBufferWithBytes:&zero length:sizeof(zero)
                options:MTLResourceStorageModeShared];
            require(constants != nil && emptyEmitters != nil, @"Could not create constant buffers");
            std::printf("Device: %s; OS: %s\nShader: %s; source_fnv1a=%016llx\n",
                device.name.UTF8String, NSProcessInfo.processInfo.operatingSystemVersionString.UTF8String,
                path.UTF8String, static_cast<unsigned long long>(checksum(sourceBytes.bytes, sourceBytes.length)));
            if (directBenchmark)
                std::printf("SYNTHETIC direct scenes (not real captures); %ux%u; threads=8x8; "
                    "128 emitters; mixed planar/depth=4; passIndex=0; Lambertian; "
                    "original versus prepared including preparation; 3 warmups + 11 measurements\n", width, height);
            else std::printf("SYNTHETIC bowl scenes (not real captures); %ux%u; threads=8x8; passIndex=1; "
                "dense sampleCount=1; sampled connections=4/8/16; seed=0x%08x; dense=%u+%u; sampled=%u+%u\n",
                width, height, seed, warmups, repeats, sampledWarmups, sampledRepeats);
            std::printf("Production formats: RGBA8Unorm source, RG8Unorm fields, RGBA32Float surface, "
                "RGBA16Float radiance, R8Unorm facing, RG32Float blocks; shared storage\n");
            std::printf("Reference macro REMASTER_REFERENCE_VISIBILITY=1; exact RGB comparison; "
                "sampled stages timed in separate command buffers; complete pipeline remains authoritative\n");
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
					std::vector<float> reflectance(pixels * 4, 0.0f);
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

					std::vector<RemasterSurfaceMesh::Sample> meshSamples(pixels);
					for (unsigned p = 0; p < pixels; p++)
					{
						meshSamples[p].height = surfaces[p].x;
						meshSamples[p].known = heights[p * 2 + 1] != 0;
						meshSamples[p].coverage = occlusion[p * 2] / 255.0f;
						meshSamples[p].sheet = true; meshSamples[p].domain = 1;
					}
					auto mesh = RemasterSurfaceMesh::build(width, height, meshSamples, 0.01f);
					if (directBenchmark)
						for (unsigned p = 0; p < pixels; p++) mesh[p].emissionDepth = p % 3 == 0 ? 4.0f : 0.0f;
					id<MTLBuffer> meshBuffer = [device newBufferWithBytes:mesh.data() length:mesh.size() * sizeof(mesh[0])
						options:MTLResourceStorageModeShared];
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
                    constexpr unsigned blockWidth = (width + 3) / 4, blockHeight = (height + 3) / 4;
                    MTLTextureDescriptor *blockDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                        MTLPixelFormatRG32Float width:blockWidth height:blockHeight mipmapped:NO];
                    blockDescriptor.storageMode = MTLStorageModeShared;
                    blockDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
					id<MTLTexture> blocks = [device newTextureWithDescriptor:blockDescriptor];
					require(blocks != nil, @"Could not create visibility blocks");
					MTLTextureDescriptor *reflectanceDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
						MTLPixelFormatRGBA32Float width:width height:height mipmapped:NO];
					reflectanceDescriptor.storageMode = MTLStorageModeShared;
					id<MTLTexture> reflectanceTexture = [device newTextureWithDescriptor:reflectanceDescriptor];
					require(reflectanceTexture != nil, @"Could not create compatibility reflectance texture");
					[reflectanceTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
						withBytes:reflectance.data() bytesPerRow:width * sizeof(float) * 4];
                    id<MTLCommandBuffer> build = [queue commandBuffer];
                    id<MTLComputeCommandEncoder> builder = [build computeCommandEncoder];
                    require(builder != nil, @"Could not create block encoder");
                    [builder setComputePipelineState:buildPipeline];
                    [builder setTexture:textures[1] atIndex:0];
                    [builder setTexture:textures[3] atIndex:1];
                    [builder setTexture:textures[2] atIndex:2];
                    [builder setTexture:blocks atIndex:3];
                    [builder setBuffer:meshBuffer offset:0 atIndex:0];
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
                    if (directBenchmark)
                    {
                        // Same full-resolution geometry/formats as above, but a bounded
                        // list of authored sources, including outward-depth sources.
                        std::vector<uint32_t> emitters;
                        std::vector<simd_uint2> sources;
                        for (uint32_t i = 0; i < 128; i++)
                        {
                            const uint32_t p = (i * 443 + 211) % pixels;
                            emitters.push_back(p);
                            for (uint32_t s = 0, n = mesh[p].emissionDepth > 0 ? 4 : 1; s < n; s++)
                                sources.push_back(simd_uint2{p, s});
                        }
                        id<MTLBuffer> emitterBuffer = [device newBufferWithBytes:emitters.data()
                            length:emitters.size() * sizeof(uint32_t) options:MTLResourceStorageModeShared];
                        id<MTLBuffer> sourceBuffer = [device newBufferWithBytes:sources.data()
                            length:sources.size() * sizeof(simd_uint2) options:MTLResourceStorageModeShared];
                        id<MTLBuffer> sampleBuffer = [device newBufferWithLength:sources.size() * 64
                            options:MTLResourceStorageModePrivate];
                        const uint32_t sourceCount = static_cast<uint32_t>(sources.size());
                        const uint32_t batchCount = (sourceCount + 7) / 8;
                        id<MTLBuffer> partials = [device newBufferWithLength:size_t(pixels) * batchCount * 16 options:MTLResourceStorageModePrivate];
                        std::vector<__fp16> expected(pixels * 4), actual(pixels * 4);
                        for (unsigned prepared = 0; prepared < 2; prepared++)
                        {
                            Uniforms directUniforms = uniforms;
                            directUniforms.passIndex = 0;
                            directUniforms.padding = prepared ? 2.0f : 1.0f;
                            const uint32_t count = prepared ? sourceCount : static_cast<uint32_t>(emitters.size());
                            std::vector<double> times;
                            for (unsigned repeat = 0; repeat < 14; repeat++)
                            {
                                id<MTLCommandBuffer> command = [queue commandBuffer];
                                if (prepared)
                                {
                                    id<MTLComputeCommandEncoder> prepare = [command computeCommandEncoder];
                                    [prepare setComputePipelineState:preparePipeline];
                                    [prepare setTexture:textures[1] atIndex:0];
                                    [prepare setTexture:textures[3] atIndex:1];
                                    [prepare setTexture:textures[2] atIndex:2];
                                    [prepare setTexture:textures[5] atIndex:3];
                                    [prepare setBuffer:sourceBuffer offset:0 atIndex:0];
                                    [prepare setBuffer:sampleBuffer offset:0 atIndex:1];
                                    [prepare setBuffer:meshBuffer offset:0 atIndex:2];
                                    [prepare setBytes:&sourceCount length:sizeof(sourceCount) atIndex:3];
                                    [prepare dispatchThreads:MTLSizeMake(sourceCount, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
                                    [prepare endEncoding];
                                }
                                id<MTLComputeCommandEncoder> transport = [command computeCommandEncoder];
                                [transport setComputePipelineState:prepared ? (batched ? batchedTransport : directTransport) : pipelines[1]];
                                for (unsigned binding = 0; binding < 10; binding++)
                                    [transport setTexture:textures[binding] atIndex:binding];
                                [transport setTexture:blocks atIndex:10];
                                [transport setTexture:reflectanceTexture atIndex:11];
                                [transport setBytes:&directUniforms length:sizeof(directUniforms) atIndex:0];
                                [transport setBuffer:prepared ? sourceBuffer : emitterBuffer offset:0 atIndex:1];
                                [transport setBytes:&count length:sizeof(count) atIndex:2];
                                [transport setBuffer:meshBuffer offset:0 atIndex:3];
                                [transport setBuffer:sampleBuffer offset:0 atIndex:4];
                                [transport setBuffer:partials offset:0 atIndex:5];
                                [transport dispatchThreads:MTLSizeMake(width, height, prepared && batched ? batchCount : 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
                                [transport endEncoding];
                                if (prepared && batched)
                                {
                                    id<MTLComputeCommandEncoder> reduce = [command computeCommandEncoder];
                                    [reduce setComputePipelineState:reduceDirect];
                                    [reduce setBuffer:partials offset:0 atIndex:0];
                                    [reduce setBytes:&batchCount length:sizeof(batchCount) atIndex:1];
                                    [reduce setBytes:&directUniforms length:sizeof(directUniforms) atIndex:2];
                                    [reduce setTexture:textures[7] atIndex:0];
                                    [reduce setTexture:textures[8] atIndex:1];
                                    [reduce setTexture:textures[0] atIndex:2];
                                    [reduce setTexture:reflectanceTexture atIndex:3];
                                    [reduce setTexture:textures[6] atIndex:4];
                                    [reduce dispatchThreads:MTLSizeMake(width, height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
                                    [reduce endEncoding];
                                }
                                [command commit];
                                [command waitUntilCompleted];
                                require(command.status == MTLCommandBufferStatusCompleted, command.error.localizedDescription);
                                const double ms = (command.GPUEndTime - command.GPUStartTime) * 1000.0;
                                require(std::isfinite(ms) && ms > 0, @"Direct GPU timestamp unavailable");
                                if (repeat >= 3) times.push_back(ms);
                            }
                            printGpuTimes(names[scene], sourceCount, prepared ? "direct-prepared" : "direct-original", times, true);
                            auto &output = prepared ? actual : expected;
                            [textures[7] getBytes:output.data() bytesPerRow:width * 8
                                fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
                        }
                        unsigned differences = 0;
                        float maximumAbsolute = 0, maximumRelative = 0;
                        unsigned outsideTolerance = 0;
                        for (unsigned i = 0; i < pixels * 4; i++)
                        {
                            const float a = actual[i], e = expected[i];
                            const float absolute = std::abs(a - e);
                            differences += a != e;
                            maximumAbsolute = std::max(maximumAbsolute, absolute);
                            maximumRelative = std::max(maximumRelative, absolute / std::max(std::abs(e), 0x1p-24f));
                            // Specialization changes compiler scheduling/rounding; allow one
                            // half-float rounding step, never missing light or nonfinite output.
                            outsideTolerance += !std::isfinite(a) || absolute > std::max(0x1p-24f, std::abs(e) / 1024.0f);
                        }
                        std::printf("%s direct emitters=%zu samples=%u exact_half_differences=%u max_abs=%.9g max_rel=%.9g outside_one_half_step=%u\n",
                            names[scene], emitters.size(), sourceCount, differences, maximumAbsolute, maximumRelative, outsideTolerance);
                        mismatch |= outsideTolerance != 0;
                        continue;
                    }
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
						[encoder setTexture:reflectanceTexture atIndex:11];
                        [encoder setBuffer:constants offset:0 atIndex:0];
                        [encoder setBuffer:emptyEmitters offset:0 atIndex:1];
                        [encoder setBuffer:emptyEmitters offset:0 atIndex:2];
                        [encoder setBuffer:meshBuffer offset:0 atIndex:3];
                        [encoder setBuffer:emptyEmitters offset:0 atIndex:4];
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
                    constexpr uint32_t leafCount = 65536;
                    id<MTLBuffer> sourcePower = [device newBufferWithLength:leafCount * 2 * sizeof(float)
                        options:MTLResourceStorageModePrivate];
                    require(sourcePower != nil, @"Could not allocate source distribution");
                    for (uint32_t connections : {4u, 8u, 16u})
                    {
                        Uniforms sampledUniforms = uniforms;
                        sampledUniforms.sampleCount = connections;
                        id<MTLBuffer> sampledConstants = [device newBufferWithBytes:&sampledUniforms
                            length:sizeof(sampledUniforms) options:MTLResourceStorageModeShared];
                        const auto encodeBlocks = [&](id<MTLCommandBuffer> command) {
                            id<MTLComputeCommandEncoder> blockEncoder = [command computeCommandEncoder];
                            require(blockEncoder != nil, @"Could not encode visibility blocks");
                            [blockEncoder setComputePipelineState:buildPipeline];
                            [blockEncoder setBuffer:meshBuffer offset:0 atIndex:0];
                            [blockEncoder setTexture:textures[1] atIndex:0];
                            [blockEncoder setTexture:textures[3] atIndex:1];
                            [blockEncoder setTexture:textures[2] atIndex:2];
                            [blockEncoder setTexture:blocks atIndex:3];
                            [blockEncoder dispatchThreads:MTLSizeMake(blockWidth, blockHeight, 1)
                                threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
                            [blockEncoder endEncoding];
                        };
                        const auto encodeSource = [&](id<MTLCommandBuffer> command) {
                            id<MTLComputeCommandEncoder> leaves = [command computeCommandEncoder];
                            require(leaves != nil, @"Could not encode source leaves");
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
                                require(reduce != nil, @"Could not encode source reduction");
                                [reduce setComputePipelineState:powerReduce];
                                [reduce setBuffer:sourcePower offset:0 atIndex:0];
                                [reduce setBytes:&firstNode length:sizeof(firstNode) atIndex:1];
                                [reduce dispatchThreads:MTLSizeMake(firstNode, 1, 1)
                                    threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
                                [reduce endEncoding];
                            }
                        };
                        const auto encodeBounce = [&](id<MTLCommandBuffer> command) {
                            id<MTLComputeCommandEncoder> bounceEncoder = [command computeCommandEncoder];
                            require(bounceEncoder != nil, @"Could not encode sampled bounce");
                            [bounceEncoder setComputePipelineState:sampledPipeline];
                            for (unsigned binding = 0; binding < 9; ++binding)
                                [bounceEncoder setTexture:textures[binding] atIndex:binding];
                            [bounceEncoder setTexture:blocks atIndex:10];
                            [bounceEncoder setTexture:reflectanceTexture atIndex:11];
                            [bounceEncoder setBuffer:sampledConstants offset:0 atIndex:0];
                            [bounceEncoder setBuffer:sourcePower offset:0 atIndex:1];
                            [bounceEncoder setBuffer:meshBuffer offset:0 atIndex:3];
                            [bounceEncoder setBytes:&leafCount length:sizeof(leafCount) atIndex:2];
                            [bounceEncoder dispatchThreads:MTLSizeMake(width, height, 1)
                                threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
                            [bounceEncoder endEncoding];
                        };
                        const auto measureStage = [&](const char *label, unsigned stage) {
                            std::vector<double> times;
                            times.reserve(sampledRepeats);
                            for (unsigned i = 0; i < sampledWarmups + sampledRepeats; ++i)
                            {
                                id<MTLCommandBuffer> command = [queue commandBuffer];
                                require(command != nil, @"Could not create sampled command buffer");
                                if (stage == 0 || stage == 1)
                                    encodeBlocks(command);
                                if (stage == 0 || stage == 2)
                                    encodeSource(command);
                                if (stage == 0 || stage == 3)
                                    encodeBounce(command);
                                [command commit];
                                [command waitUntilCompleted];
                                require(command.status == MTLCommandBufferStatusCompleted,
                                    command.error.localizedDescription);
                                double milliseconds = (command.GPUEndTime - command.GPUStartTime) * 1000.0;
                                require(std::isfinite(milliseconds) && milliseconds > 0,
                                    @"Sampled GPU timestamp unavailable");
                                if (i >= sampledWarmups)
                                    times.push_back(milliseconds);
                            }
                            printGpuTimes(names[scene], connections, label, std::move(times));
                        };
                        // Run the combined case first to populate blocks and source power.
                        // Isolated stage timings include command-buffer overhead and need not add up
                        // to the complete duration; only the combined case tests the 5 ms budget.
                        measureStage("complete", 0);
                        measureStage("visibility", 1);
                        measureStage("source", 2);
                        measureStage("transport+resolve", 3);
                    }
                    std::fflush(stdout);
                }
            }
            require(!mismatch, directBenchmark ? @"Direct RGB mismatch exceeds one half-float step" :
                @"Reference/accelerated RGB mismatch (exact comparison required)");
            return 0;
        }
        catch (const std::exception &error)
        {
            std::fprintf(stderr, "FAIL: %s\n", error.what());
            return 1;
        }
    }
}
