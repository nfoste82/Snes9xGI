// Actual renderer reuse/invalidation, fixture-free by default; optional pinned replay.
#define main originalProbeMain
#include "../tools/lighting_critical_path_probe.mm"
#undef main

int main(int argc, char **argv) { @autoreleasepool { try {
    auto frame=std::make_unique<RemasterFrame>();
    if (argc > 1) {
        if(!S9xReadRemasterFrame(argv[1],*frame))throw std::runtime_error("capture");
    } else {
        frame->width=16;frame->height=8;frame->area.id=320;
        frame->originalRgb555.assign(128,0x4210);frame->mainPixels.resize(128);frame->subPixels.resize(128);
        RemasterFrameTileInstance instance;instance.tileId={123,1,4};
        instance.source=RemasterSourceType::Background;instance.sourceIndex=1;
        frame->tileInstances={instance};
        RemasterFrameAssetMetadata metadata;metadata.tileId=instance.tileId;
        metadata.hasHeight=true;metadata.height.fill(0);frame->assetMetadata={metadata};
        for(unsigned n=0;n<128;n++){auto &p=frame->mainPixels[n];p.owner=0x02010000;p.instanceId=1;p.tilePixel=(n/16)*8+n%8;}
    }
    metalDevice=MTLCreateSystemDefaultDevice();metalCommandQueue=[metalDevice newCommandQueue];
    NSError *error=nil;id<MTLLibrary> library=nil;
    if(argc>2)library=[metalDevice newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[2]]] error:&error];
    else {
        auto source=[NSString stringWithContentsOfFile:@"macosx/shaders.metal" encoding:NSUTF8StringEncoding error:&error];
        library=[metalDevice newLibraryWithSource:source options:nil error:&error];
    }
    if(!library)throw std::runtime_error(error.localizedDescription.UTF8String);
    InitializeRemasterMetalPipelines(library,MTLPixelFormatRGBA8Unorm);
    metalLayer=[CAMetalLayer layer];metalLayer.bounds=CGRectMake(0,0,frame->width,frame->height);
    presentationCallbackGate=std::make_shared<RemasterPresentationCallbackGate>();
    frame->indirectBounceCount=2;frame->samplesPerFrame=1;
    size_t checks=0;
    auto draw=[&](RemasterLightingView view) {
        if(!DrawRemasterFrame(*frame,RemasterDebugMode::Original,nullptr,true,view,false,false))throw std::runtime_error("renderer");
        DrainRemasterPresentation();auto &r=remasterResources[debugLastResourceSlot];
        std::vector<uint8_t> bytes(frame->width*frame->height*12);
        [r.directResult getBytes:bytes.data() bytesPerRow:frame->width*8 fromRegion:MTLRegionMake2D(0,0,frame->width,frame->height) mipmapLevel:0];
        [r.output getBytes:bytes.data()+frame->width*frame->height*8 bytesPerRow:frame->width*4 fromRegion:MTLRegionMake2D(0,0,frame->width,frame->height) mipmapLevel:0];return bytes;
    };
    auto compare=[&](const char *label,RemasterLightingView view=RemasterLightingView::Composite) {
        unsetenv("S9X_REMASTER_DISABLE_DIRECT_REUSE");auto actual=draw(view);
        remasterRandomSeed--;setenv("S9X_REMASTER_DISABLE_DIRECT_REUSE","1",1);auto expected=draw(view);
        if(actual!=expected)throw std::runtime_error(std::string("parity: ")+label);
        unsetenv("S9X_REMASTER_DISABLE_DIRECT_REUSE");remasterRandomSeed--;draw(view);
        std::cout<<"PASS "<<label<<'\n';checks++;
    };
    compare("cold");for(int n=0;n<6;n++)draw(RemasterLightingView::Composite);
    compare("warm-even-bounce-seed");
    frame->reflectanceBoost+=.125f;compare("reflectance-boost");
    frame->representativeEmitters=frame->representativeEmitters==1?3:1;compare("source-layout");
    RemasterFrameLight l;l.x=3;l.y=4;l.z=3;l.radius=2;l.intensity=25;l.red=1;l.green=.5;l.blue=.25;
    frame->lights.push_back(l);compare("add-sphere");
    frame->lights.back().x+=1;compare("moving-sphere");
    frame->lights.back().hideSource=!frame->lights.back().hideSource;compare("sphere-appearance");
    frame->hybridDirect = {true,0.02f,8};compare("hybrid-enable-next-frame");
    frame->hybridDirect.cutoff = 0.01f;compare("hybrid-cutoff-next-frame");
    frame->hybridDirect.tailSamples = 16;compare("hybrid-tail-next-frame");
    frame->hybridDirect.enabled = false;compare("hybrid-disable-next-frame");
    setenv("S9X_REMASTER_HYBRID_DIRECT","1",1);compare("hybrid-env-enables-profile-off");
    setenv("S9X_REMASTER_HYBRID_TAU","0.02",1);compare("hybrid-env-cutoff");
    setenv("S9X_REMASTER_HYBRID_DIRECT","0",1);compare("hybrid-env-off");
    unsetenv("S9X_REMASTER_HYBRID_DIRECT");unsetenv("S9X_REMASTER_HYBRID_TAU");
    frame->directionalLight.enabled=true;frame->directionalLight.direction={0,0,-1};
    frame->directionalLight.color={1,.8,.6};frame->directionalLight.brightness=.5f;compare("directional");
    frame->directionalLight.brightness=.25f;compare("directional-change");
    compare("direct-view",RemasterLightingView::DirectContribution);
    compare("visibility-diagnostic",RemasterLightingView::Visibility);
    frame->indirectBounceCount=0;compare("zero-bounce");
    setenv("S9X_REMASTER_DIRECT_SAMPLES","8",1);frame->lights.clear();frame->directionalLight.enabled=false;
    compare("sampled-direct-bypass");unsetenv("S9X_REMASTER_DIRECT_SAMPLES");
    frame->lights.push_back(l); // Nonzero transport for sharing/race comparisons.
    RemasterLightingFieldCache fields;fields.update(*frame,!frame->scenePixels.empty());
    auto mesh=RemasterSurfaceMesh::build(frame->width,frame->height,fields.samples,frame->lightingCoordinateScale/255.f);
    for(size_t i=0;i<mesh.size();i++){mesh[i].emissionDepth=fields.depths[i];mesh[i].emissionOmnidirectional=fields.omnidirectional[i];}
    std::vector<uint16_t> colors=frame->sceneRgb555.empty()?frame->originalRgb555:frame->sceneRgb555;
    auto readDirect=[&](int slot) {
        std::vector<uint8_t> bytes(frame->width*frame->height*8);
        [remasterResources[slot].directResult getBytes:bytes.data() bytesPerRow:frame->width*8
            fromRegion:MTLRegionMake2D(0,0,frame->width,frame->height) mipmapLevel:0];
        return bytes;
    };
    auto submit=[&](int slot,bool wait,RemasterLightingTrace::Frame *trace) {
        if(trace)trace->id=RemasterLightingTrace::nextId();
        RemasterLightingTrace::Scope scope(trace,RemasterLightingTrace::Setup);
        if(remasterResources[slot].inUse.exchange(true))throw std::runtime_error("test slot busy");
        if(!S9xPutImageMetal(frame->width,frame->height,colors.data(),frame->width,nullptr,0,nullptr,0,
            RemasterDebugMode::Original,fields.occlusion.data(),fields.emission.data(),fields.height.data(),
            fields.surface.data(),fields.participation.data(),fields.opposite.data(),fields.reflectance.data(),nullptr,true,
            RemasterLightingView::Composite,0,frame->indirectRoughness,frame->originalSceneContribution,
            frame->heightPreviewMultiplier,frame->reflectanceBoost,1,&frame->cameraDirection,slot,wait,&mesh,frame.get(),
            &fields.samples,nullptr,&fields.channelSources))throw std::runtime_error("slot renderer");
    };
    auto require=[&](bool ok,const char *label) {
        if(!ok)throw std::runtime_error(label);
        std::cout<<"PASS "<<label<<'\n';checks++;
    };
    // Explicit slots (ordinary synchronous Draw always acquires slot zero).
    // Warm allocations first, then change one full-key input.
    for(int slot=0;slot<3;slot++)submit(slot,true,nullptr);
    for(bool hybrid:{false,true}) {
        frame->hybridDirect={hybrid,.02f,8};
        fields.emission[3]^=17;
        RemasterLightingTrace::Frame a,b,c;
        submit(0,true,&a);auto expected=readDirect(0);
        submit(1,true,&b);submit(2,true,&c);
        require(std::string(a.directReuse)=="miss" && std::string(b.directReuse)=="shared-copy" &&
            std::string(c.directReuse)=="shared-copy",hybrid?"hybrid B/C shared-copy":"exhaustive B/C shared-copy");
        require(readDirect(1)==expected && readDirect(2)==expected,"cross-slot exact bytes");
        setenv("S9X_REMASTER_DISABLE_DIRECT_REUSE","1",1);submit(2,true,nullptr);
        require(readDirect(2)==expected,"shared-copy versus recomputation");
        unsetenv("S9X_REMASTER_DISABLE_DIRECT_REUSE");
        // Sharing-only switch preserves local hits, but forbids cross-slot copies.
        setenv("S9X_REMASTER_DISABLE_DIRECT_SHARING","1",1);
        fields.emission[3]^=33;submit(0,true,nullptr);submit(1,true,&b);
        require(std::string(b.directReuse)=="miss","sharing kill switch");
        unsetenv("S9X_REMASTER_DISABLE_DIRECT_SHARING");
        // Producer committed but GPU blocked: B must safely accept pending work.
        id<MTLSharedEvent> producerGate=[metalDevice newSharedEvent];
        id<MTLCommandBuffer> block=[metalCommandQueue commandBuffer];
        [block encodeWaitForEvent:producerGate value:1];[block commit];
        fields.emission[3]^=65;submit(0,false,&a);
        dispatch_sync(presentationQueue,^{});
        auto producer=sharedDirectResult->producer;
        id<MTLSharedEvent> consumerGate=[metalDevice newSharedEvent];
        block=[metalCommandQueue commandBuffer];[block encodeWaitForEvent:consumerGate value:1];[block commit];
        submit(1,false,&b);submit(2,false,&c);dispatch_sync(presentationQueue,^{});
        producerGate.signaledValue=1;[producer waitUntilCompleted];
        // Wait only for A's lifetime retirement, not blocked B/C. Then replace
        // its resource while the consumer copies are still pending on the GPU.
        {std::unique_lock<std::mutex> lock(remasterResourceMutex);
            remasterResourceAvailable.wait(lock,[&]{return !remasterResources[0].inUse.load();});}
        expected=readDirect(0);
        auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
            width:frame->width height:frame->height mipmapped:NO];
        descriptor.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
        remasterResources[0].directResult=[metalDevice newTextureWithDescriptor:descriptor];
        std::vector<uint8_t> poison(expected.size(),0xff);
        [remasterResources[0].directResult replaceRegion:MTLRegionMake2D(0,0,frame->width,frame->height)
            mipmapLevel:0 withBytes:poison.data() bytesPerRow:frame->width*8];
        remasterResources[0].directResultValid=false;
        consumerGate.signaledValue=1;DrainRemasterPresentation();
        require(std::string(b.directReuse)=="shared-copy" && std::string(c.directReuse)=="shared-copy" &&
            readDirect(1)==expected && readDirect(2)==expected,"pending producer and overwritten/reallocated producer slot");
        // Verify the actual bounded panel writer, not just a standalone trace.
        RemasterLiveTrace::Session capture(1);
        auto id=capture.begin(1);b.id=id;capture.renderer(b,true);
        std::ostringstream json;capture.write(json);
        require(json.str().find("\"direct_reuse\":\"shared-copy\"")!=std::string::npos &&
            json.str().find(std::string("\"hybrid_direct_enabled\":")+(hybrid?"1":"0"))!=std::string::npos &&
            json.str().find("\"hybrid_direct_tail_samples\":8")!=std::string::npos &&
            json.str().find("\"hybrid_direct_env_overrides\":0")!=std::string::npos,"panel JSONL settings and sharing outcome");
        // Force the production allocation branch while other slots match.
        remasterResources[1].width=0;submit(1,true,&b);
        require(std::string(b.directReuse)=="miss" && std::string(b.directReuseReason)=="allocation" &&
            readDirect(1)==expected,"allocation invalidates sharing");
    }
    auto raw=[&](const char *label) {
        auto run=[&] {
            int slot=AcquireRemasterResourceSlot(true);
            if(!S9xPutImageMetal(frame->width,frame->height,colors.data(),frame->width,nullptr,0,nullptr,0,
                RemasterDebugMode::Original,fields.occlusion.data(),fields.emission.data(),fields.height.data(),
                fields.surface.data(),fields.participation.data(),fields.opposite.data(),fields.reflectance.data(),nullptr,true,
                RemasterLightingView::Composite,0,frame->indirectRoughness,frame->originalSceneContribution,
                frame->heightPreviewMultiplier,frame->reflectanceBoost,1,&frame->cameraDirection,slot,true,&mesh,frame.get(),
                &fields.samples,nullptr,&fields.channelSources))throw std::runtime_error("raw renderer");
            auto &r=remasterResources[slot];std::vector<uint8_t> bytes(frame->width*frame->height*8);
            [r.directResult getBytes:bytes.data() bytesPerRow:frame->width*8 fromRegion:MTLRegionMake2D(0,0,frame->width,frame->height) mipmapLevel:0];return bytes;
        };
        unsetenv("S9X_REMASTER_DISABLE_DIRECT_REUSE");auto actual=run();remasterRandomSeed--;
        setenv("S9X_REMASTER_DISABLE_DIRECT_REUSE","1",1);auto expected=run();
        if(actual!=expected)throw std::runtime_error(std::string("raw parity: ")+label);
        unsetenv("S9X_REMASTER_DISABLE_DIRECT_REUSE");run();std::cout<<"PASS raw "<<label<<'\n';checks++;
    };
    raw("cold");fields.occlusion[0]^=255;raw("occlusion");fields.height[0]+=1;raw("height");
    fields.surface[0]+=1;raw("surface");fields.emission[3]^=255;raw("emission");
    fields.participation[0]^=255;raw("participation");fields.opposite[0]^=255;raw("opposite");
    fields.reflectance[0]+=.25f;raw("reflectance");mesh[0].corners[0]+=1;raw("mesh");colors[0]^=0x421;raw("source-rgb");
    std::cout<<"PASS "<<checks<<" renderer byte comparisons; no expectations edited\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;} } }
