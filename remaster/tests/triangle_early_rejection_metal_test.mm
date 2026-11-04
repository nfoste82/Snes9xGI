// Exact production-vs-reference triangle predicate around barycentric borders.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include <vector>
#include <cstdio>
#include <stdexcept>
int main() { @autoreleasepool { try {
    auto check=[](bool x){if(!x)throw std::runtime_error("triangle parity failure");};
    auto device=MTLCreateSystemDefaultDevice(); check(device!=nil);
    NSError *error=nil;
    NSString *source=[NSString stringWithContentsOfFile:@"macosx/shaders.metal" encoding:NSUTF8StringEncoding error:&error];
    source=[source stringByAppendingString:@"\nkernel void triangleProbe(const device float4 *r [[buffer(0)]], device uint *o [[buffer(1)]], uint i [[thread_position_in_grid]]) { o[i]=remasterTriangleHit(r[i*5].xyz,r[i*5+1].xyz,r[i*5+2].xyz,r[i*5+3].xyz,r[i*5+4].xyz,0.0,1.0); }\n"];
    auto queue=[device newCommandQueue];
    std::vector<simd_float4> records;
    const float offsets[]={-0.000004f,-0.0000021f,-0.000002f,-0.0000019f,-0.000001f,0,0.000001f,0.0000019f,0.000002f,0.0000021f,0.000004f,0.5f,1,2};
    uint32_t seed=0x80afc012;
    auto random=[&]{seed=seed*1664525u+1013904223u;return float(seed&65535u)/65536.f;};
    for(unsigned i=0;i<131072;i++) {
        float u=i<196?1+offsets[i%14]:random()*4-1;
        float v=i<196?offsets[i/14]:random()*4-1;
        float scale=i%3==0?0.125f:i%3==1?1:64;
        simd_float3 a={random()*256,random()*224,random()*32-16};
        simd_float3 b=a+simd_make_float3(scale,0,i%4?random()*scale:0),c=a+simd_make_float3(0,scale,i%4?random()*scale:0);
        simd_float3 p=a+(b-a)*u+(c-a)*v;
        simd_float3 ray={random()-0.5f,random()-0.5f,i%2?scale:-scale};
        simd_float3 from=p-ray*0.5f;
        records.push_back(simd_make_float4(from,0));records.push_back(simd_make_float4(ray,0));
        records.push_back(simd_make_float4(a,0));records.push_back(simd_make_float4(b,0));records.push_back(simd_make_float4(c,0));
    }
    unsigned count=unsigned(records.size()/5);
    auto input=[device newBufferWithBytes:records.data() length:records.size()*sizeof(records[0]) options:MTLResourceStorageModeShared];
    id<MTLBuffer> output[2];
    for(unsigned mode=0;mode<2;mode++) {
        MTLCompileOptions *options=[MTLCompileOptions new];
        if(mode)options.preprocessorMacros=@{@"REMASTER_REFERENCE_VISIBILITY":@1};
        auto library=[device newLibraryWithSource:source options:options error:&error];
        if(!library)throw std::runtime_error(error.localizedDescription.UTF8String);
        auto pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"triangleProbe"] error:&error];check(pipeline!=nil);
        output[mode]=[device newBufferWithLength:count*sizeof(uint32_t) options:MTLResourceStorageModeShared];
        auto command=[queue commandBuffer];auto encoder=[command computeCommandEncoder];
        [encoder setComputePipelineState:pipeline];[encoder setBuffer:input offset:0 atIndex:0];[encoder setBuffer:output[mode] offset:0 atIndex:1];
        [encoder dispatchThreads:MTLSizeMake(count,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];[encoder endEncoding];[command commit];[command waitUntilCompleted];
        check(command.status==MTLCommandBufferStatusCompleted);
    }
    for(unsigned i=0;i<count;i++)check(((uint32_t*)output[0].contents)[i]==((uint32_t*)output[1].contents)[i]);
    printf("PASS %u exact triangle predicates, near-u/v borders, signed rays, three scales\n",count);
    return 0;
} catch(const std::exception &e) { fprintf(stderr,"%s\n",e.what());return 1;} } }
