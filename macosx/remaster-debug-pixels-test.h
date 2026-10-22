// Actual production debug kernel, fed by the production field cache. This is
// fixture-free so --metal always exercises unresolved OBJ normal presentation.
#include "../remaster/sprite_obstacle.h"
#include "../remaster/lighting_field_cache.h"

static void TestRemasterDebugPixels(id<MTLDevice> device, id<MTLLibrary> library,
	id<MTLCommandQueue> queue)
{
	auto check=[](bool ok){if(!ok)throw std::runtime_error("OBJ debug pixel regression");};
	NSError *error=nil;
	auto pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"remasterDirectLighting"] error:&error];
	check(pipeline!=nil);
	RemasterFrame frame;frame.width=frame.height=8;
	frame.mainPixels.resize(64);frame.subPixels.resize(64);frame.originalRgb555.resize(64);
	RemasterFrameAssetMetadata metadata;metadata.tileId={123,1,4};metadata.hasHeight=metadata.hasNormals=true;
	metadata.height.fill(17);metadata.normalXyz.fill(128);
	for(size_t p=0;p<64;p++){metadata.normalXyz[p*3]=192;metadata.normalXyz[p*3+2]=240;}
	frame.assetMetadata.push_back(metadata);
	RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Object;instance.tileId=metadata.tileId;
	frame.tileInstances.push_back(instance);
	for(size_t p=0;p<64;p++){frame.mainPixels[p].instanceId=1;frame.mainPixels[p].tilePixel=p;}
	RemasterLightingFieldCache cache;
	size_t checked=0;
	for(int flip=0;flip<4;flip++) for(auto status:{RemasterOccurrenceGeometryStatus::Unresolved,
		RemasterOccurrenceGeometryStatus::Resolved,RemasterOccurrenceGeometryStatus::Conflict}) {
		auto &a=frame.tileInstances[0];a.hFlip=flip&1;a.vFlip=flip&2;a.geometryStatus=status;
		a.heightOffset=status==RemasterOccurrenceGeometryStatus::Resolved?24:0;
		a.placementTileRole=status==RemasterOccurrenceGeometryStatus::Resolved?RemasterTileRole::Obstacle:RemasterTileRole::Unknown;
		cache.update(frame,false);
		for(unsigned view:{7u,5u}) {
			id<MTLTexture> textures[9];
			for(int i=0;i<9;i++) {
				const auto format=i==2?MTLPixelFormatRGBA8Unorm:i==4?MTLPixelFormatRG32Float:MTLPixelFormatRGBA32Float;
				auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:8 height:8 mipmapped:NO];
				descriptor.storageMode=MTLStorageModeShared;descriptor.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
				textures[i]=[device newTextureWithDescriptor:descriptor];check(textures[i]!=nil);
				if(i==2)continue;
				if(i==4) [textures[i] replaceRegion:MTLRegionMake2D(0,0,8,8) mipmapLevel:0 withBytes:cache.height.data() bytesPerRow:8*2*sizeof(float)];
				else {
					std::vector<float> values(64*4);
					if(i==5)values=cache.surface;
					if(i==7)for(size_t p=0;p<64;p++)values[p*4]=values[p*4+1]=1;
					[textures[i] replaceRegion:MTLRegionMake2D(0,0,8,8) mipmapLevel:0 withBytes:values.data() bytesPerRow:8*4*sizeof(float)];
				}
			}
			Uniforms uniforms={};uniforms.width=uniforms.height=8;uniforms.view=view;
			uniforms.heightPreviewMultiplier=8;uniforms.heightPreviewRange={0,64,0,0};
			auto command=[queue commandBuffer];auto encoder=[command computeCommandEncoder];
			[encoder setComputePipelineState:pipeline];
			for(int i=0;i<9;i++)[encoder setTexture:textures[i] atIndex:i];
			[encoder setBytes:&uniforms length:sizeof(uniforms) atIndex:0];Light light={};
			[encoder setBytes:&light length:sizeof(light) atIndex:1];
			[encoder dispatchThreads:MTLSizeMake(8,8,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
			[encoder endEncoding];[command commit];[command waitUntilCompleted];
			check(command.status==MTLCommandBufferStatusCompleted);
			std::array<uint8_t,256> pixels;
			[textures[2] getBytes:pixels.data() bytesPerRow:32 fromRegion:MTLRegionMake2D(0,0,8,8) mipmapLevel:0];
			for(size_t p=0;p<64;p++)for(int c=0;c<3;c++) {
				const float expected=view==7?cache.surface[p*4+c+1]*0.5f+0.5f:
					status==RemasterOccurrenceGeometryStatus::Resolved?41.0f/64:
					c==0?0.22f:c==1?0:0.28f;
				check(std::abs(int(pixels[p*4+c])-int(std::lround(expected*255)))<=1);checked++;
			}
		}
	}
	std::printf("Production OBJ debug pixels: %zu channels passed (H/V, resolved/unresolved/conflict, cache refresh)\n",checked);
}
