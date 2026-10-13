// Standalone production shader replay. Run from repo root, no application target.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include "../remaster/lighting_field_cache.h"
#include "../remaster/emitter_patches.h"
#include "../remaster/sphere_samples.h"
#include "../remaster/direct_batches.h"
#include "../remaster/hidden_emitter_samples.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
struct Light {simd_float3 position;float radius,intensity;uint32_t sampleCount,firstSample;simd_float3 color;};
struct Uniforms {uint32_t width,height,view,lightCount,passIndex,diagnosticStage;float roughness,original,multiplier,padding;uint32_t sampleIndex,sampleCount,seed;float boost;simd_float4 camera,debugPosition,debugColor,range;};
static_assert(sizeof(Light)==48 && sizeof(Uniforms)==128,"ABI");
static void check(bool ok,const char *what){if(!ok)throw std::runtime_error(what);}
int main(int argc,char **argv){@autoreleasepool {try {
 check(argc>=2,"usage: probe own-packets...");auto device=MTLCreateSystemDefaultDevice();check(device,"Metal device");auto queue=[device newCommandQueue];NSError *error=nil;
 auto source=[NSString stringWithContentsOfFile:@"macosx/shaders.metal" encoding:NSUTF8StringEncoding error:&error];check(source,"shader source");
 if(std::getenv("ALTTP_STAIR_UNOCCLUDED"))source=[source stringByReplacingOccurrencesOfString:@"return visibility;" withString:@"return 1.0;" ];
 id<MTLLibrary> libraries[2];for(int ref=0;ref<2;ref++){auto options=[MTLCompileOptions new];if(ref)options.preprocessorMacros=@{@"REMASTER_REFERENCE_VISIBILITY":@1};libraries[ref]=[device newLibraryWithSource:source options:options error:&error];if(!libraries[ref])throw std::runtime_error(error.localizedDescription.UTF8String);}
 for(int arg=1;arg<argc;arg++) {
 auto f=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(argv[arg],*f),"packet");const int w=f->width,h=f->height;const size_t count=w*h;
 if(std::getenv("ALTTP_STAIR_SETTINGS051")){f->originalSceneContribution=0;f->indirectBounceCount=0;}
 const bool scene=f->scenePixels.size()==count && f->sceneRgb555.size()==count;RemasterLightingFieldCache fields;fields.update(*f,scene);
 auto mesh=RemasterSurfaceMesh::build(w,h,fields.samples,f->lightingCoordinateScale/255.0f);
 for(size_t n=0;n<count;n++){mesh[n].emissionDepth=fields.depths[n];mesh[n].emissionOmnidirectional=fields.omnidirectional[n];if(!fields.authoredNormals[n])std::copy_n(mesh[n].normal,3,fields.surface.begin()+n*4+1);}
 // Diagnostic only: remove coverage before block construction and visibility;
 // does not mutate packet geometry, normals or source heights.
 if(std::getenv("ALTTP_STAIR_UNOCCLUDED"))for(size_t n=0;n<count;n++)fields.occlusion[n*2]=0;
 std::vector<uint8_t> rgb(count*4);const auto &colors=scene?f->sceneRgb555:f->originalRgb555;
 for(size_t n=0;n<count;n++){auto c=S9xRemasterHeldLampHudPixel(*f,n)?f->originalRgb555[n]:colors[n];for(int k=0;k<3;k++)rgb[n*4+k]=(((c>>(10-k*5))&31)*527+23)>>6;rgb[n*4+3]=255;}
 auto texture=[&](MTLPixelFormat format,const void *data,size_t stride,int tw=0,int th=0){auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:tw?tw:w height:th?th:h mipmapped:NO];d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;d.storageMode=MTLStorageModeShared;auto t=[device newTextureWithDescriptor:d];if(data)[t replaceRegion:MTLRegionMake2D(0,0,t.width,t.height) mipmapLevel:0 withBytes:data bytesPerRow:stride];return t;};
 auto buffer=[&](const void *data,size_t bytes){return data?[device newBufferWithBytes:data length:std::max<size_t>(1,bytes) options:MTLResourceStorageModeShared]:[device newBufferWithLength:std::max<size_t>(1,bytes) options:MTLResourceStorageModeShared];};
 auto src=texture(MTLPixelFormatRGBA8Unorm,rgb.data(),w*4),occ=texture(MTLPixelFormatRG8Unorm,fields.occlusion.data(),w*2),height=texture(MTLPixelFormatRG32Float,fields.height.data(),w*8),surface=texture(MTLPixelFormatRGBA32Float,fields.surface.data(),w*16),participation=texture(MTLPixelFormatRG8Unorm,fields.participation.data(),w*2),emission=texture(MTLPixelFormatRGBA8Unorm,fields.emission.data(),w*4),opposite=texture(MTLPixelFormatR8Unorm,fields.opposite.data(),w),reflectance=texture(MTLPixelFormatRGBA32Float,fields.reflectance.data(),w*16);
 auto blocks=texture(MTLPixelFormatRG32Float,nullptr,0,(w+3)/4,(h+3)/4);auto meshBuffer=buffer(mesh.data(),mesh.size()*sizeof(mesh[0]));
 auto pipeline=[&](int ref,NSString *name,MTLFunctionConstantValues *constants=nil){auto function=constants?[libraries[ref] newFunctionWithName:name constantValues:constants error:&error]:[libraries[ref] newFunctionWithName:name];auto p=[device newComputePipelineStateWithFunction:function error:&error];if(!p)throw std::runtime_error(error.localizedDescription.UTF8String);return p;};
 std::vector<float> baseline[2][2];
  for(int ref=0;ref<2;ref++)for(int full=0;full<2;full++)for(int clustered=0;clustered<2;clustered++)for(int batchMode=0;batchMode<2;batchMode++) {
  if(std::getenv("ALTTP_STAIR_FAST_ROI") && (ref || full || !clustered || !batchMode))continue;
  const unsigned connections=std::getenv("S9X_REMASTER_DIRECT_SAMPLES")?std::strtoul(std::getenv("S9X_REMASTER_DIRECT_SAMPLES"),nullptr,10):0;
  const bool sampled=connections>0,collapsed=sampled && !std::getenv("S9X_REMASTER_EXPANDED_SAMPLED_DEPTH");
  const bool batched=!sampled && batchMode;
 std::vector<Light> lights;uint32_t sphereCount=0;for(auto &l:f->lights){Light a={};a.position={l.x,l.y,l.z};a.radius=l.radius;a.intensity=l.intensity/25;a.color={std::pow(l.red,2.2f),std::pow(l.green,2.2f),std::pow(l.blue,2.2f)};a.sampleCount=full?128:S9xRemasterSphereSamples(l.x,l.y,l.radius,w,h);a.firstSample=sphereCount;sphereCount+=a.sampleCount;lights.push_back(a);}Light emptyLight={};auto lightBuffer=buffer(lights.empty()?&emptyLight:lights.data(),std::max<size_t>(1,lights.size())*sizeof(Light));
 auto patches=clustered?S9xRemasterEmitterPatches(*f,fields.emission.data(),fields.participation.data(),fields.surface.data(),mesh):std::vector<RemasterEmitterPatch>{};RemasterEmitterPatch dummy={};auto patchBuffer=buffer(patches.empty()?&dummy:patches.data(),std::max<size_t>(1,patches.size())*sizeof(dummy));
  const uint32_t sourceFlag=collapsed?0x80000000u:0;
  std::vector<simd_uint2> records;if(clustered){for(size_t p=0;p<patches.size();p++){auto pixel=patches[p].members[0];bool depth=(patches[p].mode&1)?(patches[p].mode&2):mesh[pixel].emissionDepth>0;for(unsigned s=0;s<(!collapsed && depth?4u:1u);s++)records.push_back(simd_uint2{pixel,uint32_t(p*4+s)|sourceFlag});}}
  else for(size_t n=0;n<count;n++)if(fields.participation[n*2]>127 && fields.emission[n*4+3] && (fields.emission[n*4] || fields.emission[n*4+1] || fields.emission[n*4+2]))for(unsigned s=0;s<(!collapsed && mesh[n].emissionDepth>0?4u:1u);s++)records.push_back(simd_uint2{uint32_t(n),s|sourceFlag});
 const uint32_t visibleRecordCount=records.size();
   const auto hidden=S9xRemasterHiddenDirectSamples(*f,collapsed,clustered && !std::getenv("S9X_REMASTER_UNCLUSTERED_HIDDEN"));
  uint32_t hiddenStratum=0;for(const auto &s:hidden)records.push_back(simd_uint2{UINT32_MAX,s.normal[3]<0?hiddenStratum++%4:0});
 simd_uint2 dummyRecord={};auto recordsBuffer=buffer(records.empty()?&dummyRecord:records.data(),std::max<size_t>(1,records.size())*8);auto sampleBuffer=buffer(nullptr,std::max<size_t>(1,records.size())*64);uint32_t recordCount=records.size();
 auto batches=S9xRemasterDirectBatches(count,recordCount+sphereCount);auto partials=buffer(nullptr,batched?batches.bytes:64);
 auto seed=texture(MTLPixelFormatRGBA16Float,nullptr,0),direct=texture(MTLPixelFormatRGBA16Float,nullptr,0),zero=texture(MTLPixelFormatRGBA16Float,nullptr,0),indirect=texture(MTLPixelFormatRGBA16Float,nullptr,0),output=texture(MTLPixelFormatRGBA8Unorm,nullptr,0);
 std::vector<uint16_t> zeros(count*4);[zero replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0 withBytes:zeros.data() bytesPerRow:w*8];
  Uniforms u={};u.width=w;u.height=h;u.lightCount=lights.size();u.roughness=f->indirectRoughness;u.original=f->originalSceneContribution;u.multiplier=f->heightPreviewMultiplier;u.sampleCount=sampled?connections:batched?batches.samples:1;u.seed=17;u.boost=f->reflectanceBoost;u.camera={f->cameraDirection[0],f->cameraDirection[1],f->cameraDirection[2],0};u.range={0,50,float(sphereCount),float(full)};
 auto command=[queue commandBuffer];auto dispatch=[&](id<MTLComputeCommandEncoder> e,int x,int y,int z=1){[e dispatchThreads:MTLSizeMake(x,y,z) threadsPerThreadgroup:MTLSizeMake(8,8,1)];[e endEncoding];};
 auto e=[command computeCommandEncoder];[e setComputePipelineState:pipeline(ref,@"remasterBuildVisibilityBlocks")];[e setTexture:occ atIndex:0];[e setTexture:height atIndex:1];[e setTexture:surface atIndex:2];[e setTexture:blocks atIndex:3];[e setBuffer:meshBuffer offset:0 atIndex:0];dispatch(e,(w+3)/4,(h+3)/4);
 e=[command computeCommandEncoder];[e setComputePipelineState:pipeline(ref,@"remasterDirectLighting")];id<MTLTexture> seedTextures[]={src,occ,output,emission,height,surface,seed,participation,opposite};for(int n=0;n<9;n++)[e setTexture:seedTextures[n] atIndex:n];[e setBytes:&u length:sizeof(u) atIndex:0];[e setBuffer:lightBuffer offset:0 atIndex:1];dispatch(e,w,h);
 if(visibleRecordCount){e=[command computeCommandEncoder];[e setComputePipelineState:pipeline(ref,clustered?@"remasterPrepareEmitterPatches":@"remasterPrepareDirectSamples")];id<MTLTexture> ts[]={occ,height,surface,seed};for(int n=0;n<4;n++)[e setTexture:ts[n] atIndex:n];[e setBuffer:recordsBuffer offset:0 atIndex:0];[e setBuffer:sampleBuffer offset:0 atIndex:1];[e setBuffer:meshBuffer offset:0 atIndex:2];[e setBytes:&visibleRecordCount length:4 atIndex:3];if(clustered)[e setBuffer:patchBuffer offset:0 atIndex:4];[e dispatchThreads:MTLSizeMake(visibleRecordCount,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];[e endEncoding];}
 if(!hidden.empty()) {auto hiddenBuffer=buffer(hidden.data(),hidden.size()*64);auto copy=[command blitCommandEncoder];[copy copyFromBuffer:hiddenBuffer sourceOffset:0 toBuffer:sampleBuffer destinationOffset:visibleRecordCount*64 size:hidden.size()*64];[copy endEncoding];}
 auto constants=[MTLFunctionConstantValues new];bool yes=true,no=false;[constants setConstantValue:&yes type:MTLDataTypeBool atIndex:0];[constants setConstantValue:batched?&yes:&no type:MTLDataTypeBool atIndex:1];[constants setConstantValue:clustered?&yes:&no type:MTLDataTypeBool atIndex:3];[constants setConstantValue:full?&no:&yes type:MTLDataTypeBool atIndex:4];[constants setConstantValue:clustered?&yes:&no type:MTLDataTypeBool atIndex:6];
  u.padding=2;e=[command computeCommandEncoder];[e setComputePipelineState:sampled?pipeline(ref,@"remasterSampledDirect"):pipeline(ref,@"remasterIndirectBounce",constants)];id<MTLTexture> ts[]={src,occ,surface,height,participation,seed,zero,direct,indirect,opposite,blocks,reflectance};for(int n=0;n<12;n++)[e setTexture:ts[n] atIndex:n];[e setBytes:&u length:sizeof(u) atIndex:0];[e setBuffer:recordsBuffer offset:0 atIndex:1];[e setBytes:&recordCount length:4 atIndex:2];[e setBuffer:meshBuffer offset:0 atIndex:3];[e setBuffer:sampleBuffer offset:0 atIndex:4];[e setBuffer:batched?partials:sampleBuffer offset:0 atIndex:5];[e setBuffer:clustered?patchBuffer:recordsBuffer offset:0 atIndex:6];[e setBuffer:clustered?patchBuffer:recordsBuffer offset:0 atIndex:7];[e setBuffer:lightBuffer offset:0 atIndex:8];dispatch(e,w,h,batched?batches.count:1);
 if(batched){e=[command computeCommandEncoder];[e setComputePipelineState:pipeline(ref,@"remasterReduceDirectPartials")];[e setBuffer:partials offset:0 atIndex:0];[e setBytes:&batches.count length:4 atIndex:1];[e setBytes:&u length:sizeof(u) atIndex:2];id<MTLTexture> rt[]={direct,indirect,src,reflectance,zero};for(int n=0;n<5;n++)[e setTexture:rt[n] atIndex:n];dispatch(e,w,h);}
 e=[command computeCommandEncoder];[e setComputePipelineState:pipeline(ref,@"remasterCompositeLighting")];id<MTLTexture> ct[]={src,seed,direct,indirect,output,surface};for(int n=0;n<6;n++)[e setTexture:ct[n] atIndex:n];[e setBytes:&u length:sizeof(u) atIndex:0];[e setBuffer:lightBuffer offset:0 atIndex:1];dispatch(e,w,h);
 [command commit];[command waitUntilCompleted];if(command.error)throw std::runtime_error(command.error.localizedDescription.UTF8String);
 std::vector<__fp16> result(count*4);[direct getBytes:result.data() bytesPerRow:w*8 fromRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0];size_t lit=0,invalid=0;double sum=0;float maximum=0;for(size_t n=0;n<count;n++){float m=std::max(float(result[n*4]),std::max(float(result[n*4+1]),float(result[n*4+2])));invalid+=!std::isfinite(m);if(std::isfinite(m)){lit+=m>1e-6;sum+=m;maximum=std::max(maximum,m);}}
 std::vector<uint8_t> composite(count*4);[output getBytes:composite.data() bytesPerRow:w*4 fromRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0];size_t outputNonzero=0,significant=0;int outputMax=0;for(size_t n=0;n<count;n++){int v=std::max(composite[n*4],std::max(composite[n*4+1],composite[n*4+2]));outputNonzero+=v>0;significant+=v>=16;outputMax=std::max(outputMax,v);}
 float difference=0;
 if(!std::getenv("ALTTP_STAIR_FAST_ROI")) {
 if(!ref && !batched){baseline[full][clustered].resize(count*3);for(size_t n=0;n<count;n++)for(int k=0;k<3;k++)baseline[full][clustered][n*3+k]=float(result[n*4+k]);}
 else for(size_t n=0;n<count;n++)for(int k=0;k<3;k++)difference=std::max(difference,std::fabs(float(result[n*4+k])-baseline[full][clustered][n*3+k]));
 // Empty patch transport must be identical to nonclustered source transport;
 // authored visible-patch approximations are separately reported, not equated.
 if(clustered && patches.empty())for(size_t n=0;n<count;n++)for(int k=0;k<3;k++)difference=std::max(difference,std::fabs(float(result[n*4+k])-baseline[full][0][n*3+k]));
 }
 if(std::getenv("ALTTP_STAIR_FAST_ROI")) {
  for(const char *region:{"upper_flat","lower_flat","stair","link_box"}) {
   size_t pixels=0,on=0,ge16=0;double total=0;float peak=0,minZ=INFINITY,maxZ=-INFINITY,normalZ=0;
   const auto &pxs=scene?f->scenePixels:f->mainPixels;
   for(size_t n=0;n<count;n++) {
    const auto &px=pxs[n];if(!px.instanceId || px.instanceId>f->tileInstances.size())continue;
    const auto &i=f->tileInstances[px.instanceId-1];auto m=S9xRemasterFrameMetadataForTile(*f,i.tileId);
    const bool stair=i.assetGroup=="stair_treads" || (m && (m->tileRole==RemasterTileRole::StairTop || m->tileRole==RemasterTileRole::StairBottom));
    const bool flat=i.source==RemasterSourceType::Background && i.sourceIndex==1 && m && m->tileRole==RemasterTileRole::Floor && fields.surface[n*4+3]>.99;
    bool selected=std::string(region)=="upper_flat"?flat && fields.surface[n*4]>18:
     std::string(region)=="lower_flat"?flat && std::fabs(fields.surface[n*4])<.01:
     std::string(region)=="stair"?stair:n%w>=112 && n%w<144 && n/w>=88 && n/w<144;
    if(!selected || fields.participation[n*2]<128)continue;
    float z=fields.surface[n*4],value=std::max(float(result[n*4]),std::max(float(result[n*4+1]),float(result[n*4+2])));
    pixels++;on+=value>1e-6;total+=value;peak=std::max(peak,value);minZ=std::min(minZ,z);maxZ=std::max(maxZ,z);normalZ+=fields.surface[n*4+3];ge16+=std::max(composite[n*4],std::max(composite[n*4+1],composite[n*4+2]))>=16;
   }
   std::cout<<"ROI capture="<<argv[arg]<<" region="<<region<<" unoccluded="<<bool(std::getenv("ALTTP_STAIR_UNOCCLUDED"))<<" settings051="<<bool(std::getenv("ALTTP_STAIR_SETTINGS051"))<<" pixels="<<pixels<<" lit="<<on<<" max="<<peak<<" sum="<<total<<" z="<<minZ<<','<<maxZ<<" mean_nz="<<(pixels?normalZ/pixels:0)<<" composite_ge16="<<ge16<<'\n';
  }
 }
 if(!ref && !full && clustered && batched)if(const char *directory=std::getenv("ALTTP_STAIR_METAL_IMAGES")) {
  const std::string path=argv[arg],name=path.substr(path.find_last_of('/')+1);
  auto ppm=[&](const std::string &suffix,const std::vector<uint8_t> &pixels){std::ofstream out(std::string(directory)+"/"+name+suffix,std::ios::binary);check(bool(out),"image output directory");out<<"P6\n"<<w<<' '<<h<<"\n255\n";for(size_t n=0;n<count;n++)out.write(reinterpret_cast<const char *>(pixels.data()+n*4),3);};
  ppm("-gpu-composite.ppm",composite);ppm("-stock.ppm",rgb);
 }
 check(!invalid && lit>0 && outputNonzero>0,"actual packet shader blackout/nonfinite radiance");check(difference<0.001,"serial/batch/reference or empty-patch transport disagreement");
  std::ostringstream line;line<<"capture="<<argv[arg]<<" connections="<<connections<<" reference="<<ref<<" full="<<full<<" clustered="<<clustered<<" batched="<<batched<<" patches="<<patches.size()<<" records="<<recordCount<<" spheres="<<lights.size()<<" sphere_samples="<<sphereCount<<" lit="<<lit<<" max="<<maximum<<" sum="<<sum<<" invalid="<<invalid<<" oracle_max_difference="<<difference<<" output_nonzero="<<outputNonzero<<" output_ge16="<<significant<<" output_max_byte="<<outputMax<<" original_contribution="<<f->originalSceneContribution<<" indirect_bounces="<<int(f->indirectBounceCount)<<" gpu_ms="<<(command.GPUEndTime-command.GPUStartTime)*1000;
 std::cout<<line.str()<<'\n';if(const char *path=std::getenv("ALTTP_STAIR_METAL_RESULTS")){std::ofstream log(path,std::ios::app);log<<line.str()<<'\n';}
 }
 }
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}}
