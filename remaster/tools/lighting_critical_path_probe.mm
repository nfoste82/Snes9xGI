// Actual mac-render.mm compute setup/slots/encoders/completion, no emulator/UI.
// Only drawable acquisition is replaced with an offscreen render target.
#define REMASTER_OFFLINE_RENDERER 1
#include "../../macosx/mac-render.mm"
#include <iostream>
#include <stdexcept>
struct SGFX GFX;
struct SSettings Settings;
int videoMode=0;
bool RemasterBenchmarkActive(){return false;}
bool RemasterBenchmarkDeterministicPresentation(){return false;}
RemasterBenchmarkTag GetRemasterBenchmarkTag(){return {};}
void RecordRemasterBenchmarkSample(const RemasterBenchmarkSample &){}
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
#include "lighting_replay_input.h"
static void slotRegression() {
    auto check=[](bool ok,const char *message){if(!ok)throw std::runtime_error(message);};
    auto device=metalDevice;
    auto queue=[device newCommandQueue];
    id<MTLSharedEvent> event=[device newSharedEvent];
    dispatch_group_t retired=dispatch_group_create();
    const int counts[]={0,1,64,64,2,0,128,128,1,0};
    for(int round=0;round<10;round++) {
        id<MTLCommandBuffer> commands[3];id<MTLBuffer> outputs[3],inputs[3];
        std::vector<uint8_t> expected[3];int slots[3];
        for(int n=0;n<3;n++) {
            const int slot=slots[n]=AcquireRemasterResourceSlot(false);check(slot>=0,"acquire");
            auto &r=remasterResources[slot];
            if(round==6) {r.sceneLights=nil;r.sceneLightUploads={};} // dimension/device resource reset contract
            std::vector<RemasterGpuLight> lights(counts[round]);
            for(size_t i=0;i<lights.size();i++) {auto &l=lights[i];
                l.position={float(i),float(n),float(round)};l.radius=8;l.intensity=1;
                l.sampleCount=uint32_t(i%3+1);l.firstSample=uint32_t(i*3);l.color={1,.5f,.25f};}
            if(round==3 || round==7)for(auto &l:lights)l.position.z-=1; // unchanged/reused prefix with one changed record
            if(round==3 && !lights.empty())lights.back().firstSample++;
            RemasterLightingTrace::Frame metrics;
            inputs[n]=UploadRemasterSceneLights(r,lights,&metrics);check(inputs[n],"buffer");
            const size_t bytes=std::max<size_t>(1,lights.size())*sizeof(RemasterGpuLight);
            expected[n].resize(bytes);RemasterGpuLight empty={};
            memcpy(expected[n].data(),lights.empty()?&empty:lights.data(),bytes);
            outputs[n]=[device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
            commands[n]=[queue commandBuffer];[commands[n] encodeWaitForEvent:event value:round+1];
            auto blit=[commands[n] blitCommandEncoder];[blit copyFromBuffer:inputs[n] sourceOffset:0 toBuffer:outputs[n] destinationOffset:0 size:bytes];[blit endEncoding];
            dispatch_group_enter(retired);
            [commands[n] addCompletedHandler:^(id<MTLCommandBuffer>){ReleaseRemasterResourceSlot(slot);dispatch_group_leave(retired);}];
            [commands[n] commit];
        }
        check(AcquireRemasterResourceSlot(false)==-1,"premature slot reuse");
        check(inputs[0]!=inputs[1] && inputs[1]!=inputs[2] && inputs[0]!=inputs[2],"slot alias");
        // All three commands are submitted and blocked by the unsignalled event;
        // no immediate per-frame wait. GPU reads after CPU finishes all uploads.
        event.signaledValue=round+1;
        for(int n=0;n<3;n++) {
            [commands[n] waitUntilCompleted];check(commands[n].status==MTLCommandBufferStatusCompleted,"GPU completion");
            check(!memcmp(outputs[n].contents,expected[n].data(),expected[n].size()),"stale/aliased GPU light bytes");
        }
        dispatch_group_wait(retired,DISPATCH_TIME_FOREVER);
    }
    {int slot=AcquireRemasterResourceSlot(false);{RemasterResourceSlotGuard early(slot);}check(!remasterResources[slot].inUse,"early return release");}
    std::cout<<"PASS 30 actual GPU delayed reads, three slots, wrap/grow/shrink/empty/reset, exact bytes, completion retirement and early return\n";
}
int main(int argc,char **argv) { @autoreleasepool { try {
    if(argc<3)throw std::runtime_error("usage: probe capture frames [synthetic-sphere-count]");
    auto frame=std::make_unique<RemasterFrame>();
    if(!S9xReadRemasterFrame(argv[1],*frame))throw std::runtime_error("capture");
    { const double start=RemasterLightingTrace::now();double sum=0;
      for(int i=0;i<100000;i++)sum+=RemasterLightingTrace::now();
      std::cerr<<"timer_pair_overhead_ns="<<(RemasterLightingTrace::now()-start)*1e9/50000<<" checksum="<<sum<<'\n'; }
    std::unique_ptr<LightingReplayInput> replay;
    if(std::getenv("LIGHTING_REPLAY_ENDFRAME"))replay=std::make_unique<LightingReplayInput>(argv[1]);
    metalDevice=MTLCreateSystemDefaultDevice(); metalCommandQueue=[metalDevice newCommandQueue];
    if(std::getenv("LIGHTING_SLOT_REGRESSION")){slotRegression();return 0;}
    NSError *error=nil;
    NSString *source=[NSString stringWithContentsOfFile:@"macosx/shaders.metal" encoding:NSUTF8StringEncoding error:&error];
    auto library=[metalDevice newLibraryWithSource:source options:nil error:&error];
    if(!library)throw std::runtime_error(error.localizedDescription.UTF8String);
    InitializeRemasterMetalPipelines(library,MTLPixelFormatRGBA8Unorm);
    // A layer object is only a dimensions/config carrier; never nextDrawable.
    metalLayer=[CAMetalLayer layer]; metalLayer.bounds=CGRectMake(0,0,frame->width,frame->height);
    presentationCallbackGate=std::make_shared<RemasterPresentationCallbackGate>();
    auto addLights=[&] { if(argc>3) { frame->lights.clear(); for(int i=0;i<std::atoi(argv[3]);i++) {
        RemasterFrameLight l; l.x=(i%8+.5f)*frame->width/8; l.y=(i/8%8+.5f)*frame->height/8;
        l.z=30; l.radius=8; l.intensity=25; l.red=1;l.green=.8f;l.blue=.6f; frame->lights.push_back(l);
    }}};
    addLights();
    std::cerr<<"actual_renderer=1 offline_drawable=1 device="<<metalDevice.name.UTF8String
        <<" dimensions="<<frame->width<<'x'<<frame->height<<" spheres="<<frame->lights.size()
        <<" bounces="<<int(frame->indirectBounceCount)<<" connections="<<int(frame->samplesPerFrame)<<'\n';
    for(int n=0;n<std::atoi(argv[2]);n++) {
        const char *event=std::getenv("LIGHTING_REPLAY_EVENT");
        if(replay && event) {
            auto &c=replay->context;const auto &p=*replay->captured;
            if(!strcmp(event,"cold"))S9xRemasterInvalidateDungeonContinuity();
            if(!strcmp(event,"profile"))S9xRemasterSetProfile(replay->profile);
            if(!strcmp(event,"camera")) {c.backgroundScrollX+=n%2?1:-1;c.linkX+=n%2?1:-1;}
            if(!strcmp(event,"plane"))c.linkPlane^=1;
            if(!strcmp(event,"collision"))replay->input[16392]^=1;
            if(!strcmp(event,"map"))replay->input[8]^=1;
            if(!strcmp(event,"graphics"))replay->input[24584+c.graphicsBase]^=1;
            (void)p;
        }
        if(replay)replay->run(*frame);
        addLights();
        if(event && !strcmp(event,"light_motion"))for(auto &l:frame->lights)l.x+=n%2;
        if(n==0 && std::getenv("LIGHTING_PATCH_BYTE_CHECK")) {
            RemasterLightingFieldCache fields;fields.update(*frame,!frame->scenePixels.empty());
            auto mesh=RemasterSurfaceMesh::build(frame->width,frame->height,fields.samples,frame->lightingCoordinateScale/255.f);
            for(size_t i=0;i<mesh.size();i++){mesh[i].emissionDepth=fields.depths[i];mesh[i].emissionOmnidirectional=fields.omnidirectional[i];}
            auto a=S9xRemasterEmitterPatches(*frame,fields.emission.data(),fields.participation.data(),mesh);
            auto b=S9xRemasterEmitterPatches(*frame,fields.emission.data(),fields.participation.data(),mesh,&fields.channelSources);
            if(a.size()!=b.size() || (!a.empty() && memcmp(a.data(),b.data(),a.size()*sizeof(a[0]))))throw std::runtime_error("patch byte parity");
            std::cerr<<"direct_patch_byte_compare="<<a.size()*sizeof(RemasterEmitterPatch)<<" equal\n";
        }
        if(!DrawRemasterFrame(*frame,RemasterDebugMode::Original,nullptr,true,RemasterLightingView::Composite,false,false))
            throw std::runtime_error("renderer");
    }
    DrainRemasterPresentation();
    if(const char *prefix=std::getenv("LIGHTING_OUTPUT")) {
        int slot=debugLastResourceSlot;auto &r=remasterResources[slot];
        auto dump=[&](id<MTLTexture> tex,size_t bytes,const char *suffix) {
            std::vector<uint8_t> data(frame->width*frame->height*bytes);
            [tex getBytes:data.data() bytesPerRow:frame->width*bytes fromRegion:MTLRegionMake2D(0,0,frame->width,frame->height) mipmapLevel:0];
            std::ofstream out(std::string(prefix)+suffix,std::ios::binary);out.write((const char*)data.data(),data.size());
        };
        dump(r.direct,8,".direct");dump(r.output,4,".composite");
        if(!S9xWriteRemasterFrame(*frame,std::string(prefix)+".s9xrmf"))throw std::runtime_error("packet output");
    }
    return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;} } }
