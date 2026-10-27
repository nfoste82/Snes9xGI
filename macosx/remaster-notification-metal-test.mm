// Run from the repository root. Compile with -std=c++17 -fobjc-arc
// and -framework Foundation -framework Metal.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include "remaster-notification.h"
#include <cassert>
#include <cstdio>

int main()
{
	@autoreleasepool {
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		assert(device);
		NSError *error = nil;
		NSString *source = [NSString stringWithContentsOfFile:@"macosx/shaders.metal"
			encoding:NSUTF8StringEncoding error:&error];
		assert(source);
		id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
		if (!library) NSLog(@"%@", error);
		assert(library);
		MTLRenderPipelineDescriptor *pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"vertexShader"];
		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"fragmentShader"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
		id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
		assert(pipeline);
		constexpr unsigned width = 256, height = 224;
		const auto notification = RemasterNotificationPixels("Capture written: frame_room061.s9xrmf",
			width, height, 0x7fff, false, false);
		MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
			width:width height:height mipmapped:NO];
		descriptor.usage = MTLTextureUsageShaderRead;
		id<MTLTexture> text = [device newTextureWithDescriptor:descriptor];
		id<MTLTexture> background = [device newTextureWithDescriptor:descriptor];
		descriptor.usage = MTLTextureUsageRenderTarget;
		id<MTLTexture> output = [device newTextureWithDescriptor:descriptor];
		[text replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
			withBytes:notification.data() bytesPerRow:width * 4];
		struct Vertex { simd_float2 position, uv; };
		const Vertex vertices[] = {
			{{128,-112},{1,1}}, {{-128,-112},{0,1}}, {{-128,112},{0,0}},
			{{128,-112},{1,1}}, {{-128,112},{0,0}}, {{128,112},{1,0}}};
		const simd_uint2 viewport = {width, height};
		id<MTLCommandQueue> queue = [device newCommandQueue];
		// Exercise final presentation over dark/direct, brighter combined, and
		// colored/composite inputs, both filtering modes, and notification expiry.
		for (unsigned shade : {0u, 80u, 160u})
			for (int videoMode : {0, 1})
				for (int enabled : {0, 1})
				{
					std::vector<uint8_t> scene(width * height * 4), result(scene.size());
					for (size_t i = 0; i < scene.size(); i += 4)
					{ scene[i] = shade; scene[i+1] = shade/2; scene[i+2] = shade/4; scene[i+3] = 255; }
					[background replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0
						withBytes:scene.data() bytesPerRow:width * 4];
					id<MTLCommandBuffer> command = [queue commandBuffer];
					MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
					pass.colorAttachments[0].texture = output;
					pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
					pass.colorAttachments[0].storeAction = MTLStoreActionStore;
					id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
					[encoder setRenderPipelineState:pipeline];
					[encoder setVertexBytes:vertices length:sizeof(vertices) atIndex:0];
					[encoder setVertexBytes:&viewport length:sizeof(viewport) atIndex:1];
					[encoder setFragmentTexture:background atIndex:0];
					[encoder setFragmentTexture:enabled ? text : nil atIndex:1];
					[encoder setFragmentBytes:&videoMode length:sizeof(videoMode) atIndex:1];
					[encoder setFragmentBytes:&enabled length:sizeof(enabled) atIndex:2];
					[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
					[encoder endEncoding];
					[command commit];
					[command waitUntilCompleted];
					assert(command.status == MTLCommandBufferStatusCompleted);
					[output getBytes:result.data() bytesPerRow:width * 4
						fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
					for (size_t i = 0; i < result.size(); i += 4)
						for (size_t c = 0; c < 4; ++c)
							assert(result[i+c] == (enabled && notification[i+3] ? notification[i+c] : scene[i+c]));
				}
		std::puts("PASS production Metal notification presentation: 12 cases, all pixels exact");
	}
}
