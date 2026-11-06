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
    frame->directionalLight.enabled=true;frame->directionalLight.direction={0,0,-1};
    frame->directionalLight.color={1,.8,.6};frame->directionalLight.brightness=.5f;compare("directional");
    frame->directionalLight.brightness=.25f;compare("directional-change");
    compare("direct-view",RemasterLightingView::DirectContribution);
    compare("visibility-diagnostic",RemasterLightingView::Visibility);
    frame->indirectBounceCount=0;compare("zero-bounce");
    setenv("S9X_REMASTER_DIRECT_SAMPLES","8",1);frame->lights.clear();frame->directionalLight.enabled=false;
    compare("sampled-direct-bypass");unsetenv("S9X_REMASTER_DIRECT_SAMPLES");
    RemasterLightingFieldCache fields;fields.update(*frame,!frame->scenePixels.empty());
    auto mesh=RemasterSurfaceMesh::build(frame->width,frame->height,fields.samples,frame->lightingCoordinateScale/255.f);
    for(size_t i=0;i<mesh.size();i++){mesh[i].emissionDepth=fields.depths[i];mesh[i].emissionOmnidirectional=fields.omnidirectional[i];}
    std::vector<uint16_t> colors=frame->sceneRgb555.empty()?frame->originalRgb555:frame->sceneRgb555;
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
