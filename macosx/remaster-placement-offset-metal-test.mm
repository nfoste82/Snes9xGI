#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "remaster/runtime/placement_offset_debug.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#x); std::abort(); } } while(0)
int main() { @autoreleasepool {
 auto device=MTLCreateSystemDefaultDevice(); CHECK(device);
 NSError *error=nil;
 auto source=[NSString stringWithContentsOfFile:@"macosx/shaders.metal" encoding:NSUTF8StringEncoding error:&error];
 auto lib=[device newLibraryWithSource:source options:nil error:&error]; if(!lib)NSLog(@"%@",error); CHECK(lib);
 auto pipeline=[device newComputePipelineStateWithFunction:[lib newFunctionWithName:@"remasterPlacementOffsets"] error:&error]; CHECK(pipeline);
 RemasterFrame f; f.width=80; f.height=8; f.area.id=320;
 f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 f.mainPixels.resize(640); f.subPixels.resize(640);
 const int values[]={-40,-32,-24,-16,-8,0,24,0,0,0};
 for(unsigned t=0;t<10;t++) {
  RemasterFrameTileInstance i; i.tileId={123,1,4}; i.source=RemasterSourceType::Background;i.sourceIndex=1;
  i.savedPlacementOffset=values[t];i.savedPlacementOffsetStatus=t==7?1:t>=8?0:2;
  i.heightOffset=99;i.hasPlacementHeight=true; i.placementHeight.fill(t&1?7:0);
  f.tileInstances.push_back(i); f.contextLocations[t+1]={320,uint16_t(200+t),201,1};
 }
 for(unsigned n=0;n<640;n++) {
  auto &p=f.mainPixels[n]; p.owner=0x02010000;p.instanceId=(n%80)/8+1;p.tilePixel=(n/80)*8+n%8;
  if(n%80>=72) { p.owner=0x03000000; f.tileInstances[9].source=RemasterSourceType::Object; }
 }
 std::vector<float> field; S9xRemasterPlacementOffsetField(f,false,field);
 auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG32Float width:80 height:8 mipmapped:NO];
 auto input=[device newTextureWithDescriptor:d];
 [input replaceRegion:MTLRegionMake2D(0,0,80,8) mipmapLevel:0 withBytes:field.data() bytesPerRow:80*8];
 d.pixelFormat=MTLPixelFormatRGBA8Unorm;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
 auto output=[device newTextureWithDescriptor:d];auto queue=[device newCommandQueue];auto command=[queue commandBuffer];
 auto e=[command computeCommandEncoder];[e setComputePipelineState:pipeline];[e setTexture:input atIndex:0];[e setTexture:output atIndex:1];
 [e dispatchThreads:MTLSizeMake(80,8,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];[e endEncoding];[command commit];[command waitUntilCompleted];
 CHECK(command.status==MTLCommandBufferStatusCompleted);
 std::vector<uint8_t> bytes(640*4);[output getBytes:bytes.data() bytesPerRow:320 fromRegion:MTLRegionMake2D(0,0,80,8) mipmapLevel:0];
 for(unsigned n=0;n<640;n++) {
  unsigned t=n%80/8;
  for(unsigned c=0;c<3;c++) { int expected=t>=8?(c==0?56:c==1?0:71):int(std::round((values[t]+128)*255./256)); CHECK(std::abs(int(bytes[n*4+c])-expected)<=1); }
 }
 for(unsigned t=0;t<10;t++) std::printf("scalar %+d status %u -> GPU RGB %u %u %u\n",values[t],f.tileInstances[t].savedPlacementOffsetStatus,bytes[t*32],bytes[t*32+1],bytes[t*32+2]);
 // All local samples change without any placement field/output change.
 auto original=field;for(auto &i:f.tileInstances)i.placementHeight.fill(-32768);
 S9xRemasterPlacementOffsetField(f,false,field);CHECK(original==field);
 std::puts("PASS production Metal scalar-only view: 640 pixels, signed/default/unsupported; local relief independent");
} }
