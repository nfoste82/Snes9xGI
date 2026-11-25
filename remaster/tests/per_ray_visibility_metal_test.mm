// Actual shared-cache queue ordering and exact fallback, no external fixtures.
#define main originalProbeMain
#include "../tools/lighting_critical_path_probe.mm"
#undef main

int main() { @autoreleasepool { try {
    metalDevice=MTLCreateSystemDefaultDevice();metalCommandQueue=[metalDevice newCommandQueue];
    NSError *error=nil;
    auto source=[NSString stringWithContentsOfFile:@"macosx/shaders.metal" encoding:NSUTF8StringEncoding error:&error];
    auto library=[metalDevice newLibraryWithSource:source options:nil error:&error];
    if(!library)throw std::runtime_error(error.localizedDescription.UTF8String);
    InitializeRemasterMetalPipelines(library,MTLPixelFormatRGBA8Unorm);
    metalLayer=[CAMetalLayer layer];metalLayer.bounds=CGRectMake(0,0,32,16);
    presentationCallbackGate=std::make_shared<RemasterPresentationCallbackGate>();
    auto check=[](bool ok,const char *label) {
        if(!ok)throw std::runtime_error(label);
        std::cout<<"PASS "<<label<<'\n';
    };
    RemasterFrame frame;frame.width=32;frame.height=16;frame.lightingTrace.room=18;
    frame.indirectBounceCount=0;frame.representativeEmitters=0;
    RemasterMetalResources resources;
    resources.width=32;resources.height=16;
    std::vector<float> surface(512*4,0),height(512*2,0);
    for(size_t i=0;i<512;i++)surface[i*4+3]=1;
    std::vector<uint8_t> occlusion(1024,0);
    std::vector<RemasterSurfaceMesh::Cell> mesh(512);
    const size_t indices[]={0,2,5,7};
    const void *data[]={occlusion.data(),height.data(),surface.data(),mesh.data()};
    const size_t bytes[]={occlusion.size(),height.size()*4,surface.size()*4,mesh.size()*sizeof(mesh[0])};
    for(size_t i=0;i<4;i++) {
        const auto *p=static_cast<const uint8_t *>(data[i]);
        resources.fieldUploads[indices[i]].previous.assign(p,p+bytes[i]);
    }
    auto texture=[&](MTLPixelFormat format,size_t stride,const void *pixels) {
        auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:32 height:16 mipmapped:NO];
        auto texture=[metalDevice newTextureWithDescriptor:descriptor];
        [texture replaceRegion:MTLRegionMake2D(0,0,32,16) mipmapLevel:0 withBytes:pixels bytesPerRow:stride];
        return texture;
    };
    resources.surface=texture(MTLPixelFormatRGBA32Float,32*16,surface.data());
    resources.heightField=texture(MTLPixelFormatRG32Float,32*8,height.data());
    resources.occlusion=texture(MTLPixelFormatRG8Unorm,32*2,occlusion.data());
    resources.mesh=[metalDevice newBufferWithBytes:mesh.data() length:bytes[3] options:MTLResourceStorageModeShared];
    RemasterHiddenDirectSample sample={};sample.position={20,8,3,0};sample.endpoint=sample.position;
    sample.normal={0,0,1,0};sample.radiance={1,1,1,1};
    resources.directSamples=[metalDevice newBufferWithBytes:&sample length:64 options:MTLResourceStorageModeShared];
    std::vector<vector_uint2> sources;
    sources.push_back(vector_uint2{20,0});
    std::vector<RemasterEmitterPatch> patches;
    std::vector<RemasterHiddenDirectSample> hidden;
    auto prepare=[&](id<MTLCommandBuffer> command,bool reuse=false) {
        RemasterGpuStageCounters stages(metalDevice);
        return PrepareRemasterPerRayCache(command,stages,resources,&frame,sources,patches,hidden,uint32_t(sources.size()),false,true,reuse,true);
    };
    auto first=[metalCommandQueue commandBuffer];auto rebuild=prepare(first);
    check(!strcmp(rebuild.outcome,"rebuild") && !rebuild.config.x,"rebuild bypass");
    [first commit];[first waitUntilCompleted];
    id<MTLSharedEvent> event=[metalDevice newSharedEvent];
    auto second=[metalCommandQueue commandBuffer];[second encodeWaitForEvent:event value:1];
    auto a=prepare(second);[second commit];
    auto third=[metalCommandQueue commandBuffer];auto b=prepare(third);[third commit];
    check(a.cache==b.cache,"one cache across pending queue producers");
    check(a.columns!=b.columns && a.changes!=b.changes && a.counters!=b.counters,"immutable per-command inputs");
    check(b.generation.y==a.generation.x,"ordered logical generations across slots");
    event.signaledValue=1;[second waitUntilCompleted];[third waitUntilCompleted];
    check(second.status==MTLCommandBufferStatusCompleted && third.status==MTLCommandBufferStatusCompleted,"pending GPU command ordering");
    // Growth preserves unrelated columns, including their float visibility bits.
    auto retained=b.cache->entries;
    auto seed=[metalCommandQueue commandBuffer];
    auto seedBlit=[seed blitCommandEncoder];
    [seedBlit fillBuffer:retained range:NSMakeRange(0,retained.length) value:0x5a];
    [seedBlit endEncoding];[seed commit];[seed waitUntilCompleted];
    sources.push_back(vector_uint2{21,0});
    RemasterHiddenDirectSample prepared[2]={sample,sample};
    resources.directSamples=[metalDevice newBufferWithBytes:prepared length:sizeof(prepared) options:MTLResourceStorageModeShared];
    auto growth=[metalCommandQueue commandBuffer];auto grown=prepare(growth);
    auto readback=[metalDevice newBufferWithLength:retained.length options:MTLResourceStorageModeShared];
    auto copy=[growth blitCommandEncoder];
    [copy copyFromBuffer:grown.cache->entries sourceOffset:0 toBuffer:readback destinationOffset:0 size:retained.length];
    [copy endEncoding];[growth commit];[growth waitUntilCompleted];
    const auto *oldBytes=static_cast<const uint8_t *>(readback.contents);
    check(grown.cache->entries!=retained && std::all_of(oldBytes,oldBytes+retained.length,
        [](uint8_t byte){return byte==0x5a;}),"source growth preserves existing column bytes");
    auto paint=[metalCommandQueue commandBuffer];
    auto paintBlit=[paint blitCommandEncoder];
    [paintBlit fillBuffer:grown.cache->entries range:NSMakeRange(0,grown.cache->entries.length) value:0x3c];
    [paintBlit endEncoding];[paint commit];[paint waitUntilCompleted];
    auto radiance=sample;radiance.radiance={0.5f,0.25f,0.125f,1};
    prepared[0]=radiance;
    resources.directSamples=[metalDevice newBufferWithBytes:prepared length:sizeof(prepared) options:MTLResourceStorageModeShared];
    auto flicker=[metalCommandQueue commandBuffer];auto flickerDispatch=prepare(flicker);
    check(flickerDispatch.cache==grown.cache && flickerDispatch.config.y==0 && flickerDispatch.config.w==0,
        "radiance-only change preserves cache generation geometry");
    [flicker commit];[flicker waitUntilCompleted];
    mesh[100].corners[0]=8;
    memcpy(resources.fieldUploads[7].previous.data(),mesh.data(),bytes[3]);
    memcpy(resources.mesh.contents,mesh.data(),bytes[3]);
    auto torch=[metalCommandQueue commandBuffer];auto partial=prepare(torch);
    check(partial.config.w==1 && !partial.config.y && partial.cache==grown.cache,
        "one torch geometry cell emits partial invalidation without whole reset");
    [torch commit];[torch waitUntilCompleted];
    auto hit=[metalCommandQueue commandBuffer];auto reuse=prepare(hit,true);
    check(!strcmp(reuse.outcome,"direct-reuse") && !reuse.config.x,"whole Direct reuse skips per-ray dispatch");
    auto abandoned=[metalCommandQueue commandBuffer];auto abandonedDispatch=prepare(abandoned);
    auto next=[metalCommandQueue commandBuffer];auto recovered=prepare(next);
    check(!strcmp(recovered.outcome,"rebuild") && !recovered.config.x,"abandoned producer reset");
    [next commit];[next waitUntilCompleted];
    frame.lightingTrace.room=19;
    auto room=[metalCommandQueue commandBuffer];auto roomDispatch=prepare(room);
    check(!strcmp(roomDispatch.outcome,"rebuild"),"room change bypass");[room commit];[room waitUntilCompleted];
    frame.lightingTrace.scrollY++;
    auto scroll=[metalCommandQueue commandBuffer];auto scrollDispatch=prepare(scroll);
    check(!strcmp(scrollDispatch.outcome,"rebuild"),"scroll bypass without history");[scroll commit];[scroll waitUntilCompleted];
    setenv("S9X_REMASTER_PER_RAY_CACHE_MB","0",1);
    auto cap=[metalCommandQueue commandBuffer];auto capped=prepare(cap);
    check(!strcmp(capped.outcome,"cap") && !capped.config.x && !remasterPerRayCache,"zero cap exact fallback");
    unsetenv("S9X_REMASTER_PER_RAY_CACHE_MB");setenv("S9X_REMASTER_DISABLE_PER_RAY_CACHE","1",1);
    auto disabled=[metalCommandQueue commandBuffer];auto off=prepare(disabled);
    check(!strcmp(off.outcome,"disabled") && !off.config.x,"kill switch fallback");
    unsetenv("S9X_REMASTER_DISABLE_PER_RAY_CACHE");
    setenv("S9X_REMASTER_PER_RAY_CACHE_MB","malformed",1);
    check(RemasterPerRayLimit()==256u*1024*1024,"malformed cap default");
    unsetenv("S9X_REMASTER_PER_RAY_CACHE_MB");
    std::ostringstream json;
    RemasterLiveTrace::Session panel(1);
    auto id=panel.begin(1);
    RemasterLightingTrace::Frame captured;captured.id=id;
    captured.perRayOutcome="active";captured.perRayBytes=123456;
    panel.renderer(captured,true);
    uint32_t counts[256*6]={};counts[0]=20;counts[1]=3;counts[3]=2;counts[5]=1;
    panel.perRay(id,3,17,counts);panel.refresh(id,1);panel.gpu(id,1,1.001,1,0,0,0,4);
    panel.write(json);
    check(json.str().find("\"per_ray_traced\":3")!=std::string::npos &&
        json.str().find("\"per_ray_reused\":17")!=std::string::npos &&
        json.str().find("\"per_ray_sample_changed\":2")!=std::string::npos &&
        json.str().find("\"per_ray_bytes\":123456")!=std::string::npos,"bounded panel pair metrics writer");
    std::cout<<"PASS per-ray visibility lifecycle regression\n";return 0;
} catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;} } }
