// Actual production debug kernel, fed by the production field cache. This is
// fixture-free so --metal always exercises unresolved OBJ normal presentation.
#include "../remaster/runtime/sprite_obstacle.h"
#include "../remaster/runtime/lighting_field_cache.h"

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
	// Same production GPU kernel must display the composed donor field, not
	// stale scene ownership. Optional immutable packet exercises actual gameplay.
	for(int fixture=0;fixture<2;fixture++) {
		RemasterFrame layered=frame;
		if(fixture) {
			const char *path=std::getenv("S9X_REMASTER_POLICY_CAPTURE");if(!path)continue;
			check(S9xReadRemasterFrame(path,layered));
		} else {
			layered.tileInstances[0].source=RemasterSourceType::Background;
			layered.tileInstances[0].sourceIndex=0;layered.assetMetadata[0].hasHeight=false;
			auto donor=layered.tileInstances[0];donor.sourceIndex=1;donor.tileId.hash++;donor.heightOffset=32;
			layered.tileInstances.push_back(donor);auto dm=metadata;dm.tileId=donor.tileId;dm.height.fill(7);layered.assetMetadata.push_back(dm);
			for(size_t p=0;p<64;p++) {
				layered.mainPixels[p].owner=uint32_t(RemasterSourceType::Background)<<24;
				layered.subPixels[p]={uint32_t(RemasterSourceType::Background)<<24|1<<16,2,uint8_t(p)};
			}
			layered.scenePixels=layered.mainPixels;layered.sceneRgb555=layered.originalRgb555;
		}
		RemasterLightingFieldCache fields;fields.update(layered,!layered.scenePixels.empty());
		id<MTLTexture> textures[9];const auto w=layered.width,h=layered.height;
		for(int i=0;i<9;i++) {
			const auto format=i==2?MTLPixelFormatRGBA8Unorm:i==4?MTLPixelFormatRG32Float:MTLPixelFormatRGBA32Float;
			auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
			d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
			textures[i]=[device newTextureWithDescriptor:d];check(textures[i]!=nil);if(i==2)continue;
			if(i==4)[textures[i] replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0 withBytes:fields.height.data() bytesPerRow:w*2*sizeof(float)];
			else {std::vector<float> v(size_t(w)*h*4,0);if(i==5)v=fields.surface;if(i==7)std::fill(v.begin(),v.end(),1);
				[textures[i] replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0 withBytes:v.data() bytesPerRow:w*4*sizeof(float)];}
		}
		Uniforms u={};u.width=w;u.height=h;u.view=5;u.heightPreviewRange={-64,192,0,0};u.heightPreviewMultiplier=8;
		auto cmd=[queue commandBuffer];auto enc=[cmd computeCommandEncoder];[enc setComputePipelineState:pipeline];
		for(int i=0;i<9;i++)[enc setTexture:textures[i] atIndex:i];[enc setBytes:&u length:sizeof(u) atIndex:0];Light light={};[enc setBytes:&light length:sizeof(light) atIndex:1];
		[enc dispatchThreads:MTLSizeMake(w,h,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];[enc endEncoding];[cmd commit];[cmd waitUntilCompleted];check(cmd.status==MTLCommandBufferStatusCompleted);
		std::vector<uint8_t> px(size_t(w)*h*4);[textures[2] getBytes:px.data() bytesPerRow:w*4 fromRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0];
		for(size_t p=0;p<size_t(w)*h;p++)if(fields.height[p*2+1]) {
			const int expected=std::lround(std::max(0.f,std::min(1.f,(fields.height[p*2]*255+64)/256))*255);
			for(int c=0;c<3;c++){check(std::abs(int(px[p*4+c])-expected)<=1);checked++;}
		}
		std::printf("Visible policy GPU Height parity: %ux%u fixture=%d\n",w,h,fixture);
	}
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
