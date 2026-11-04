// Production conservative visibility atlas, including partial 4x4 edge blocks.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include "../runtime/surface_mesh.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

int main()
{
	@autoreleasepool { try {
		auto check = [](bool value) { if (!value) throw std::runtime_error("visibility atlas mismatch"); };
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		check(device != nil);
		NSError *error = nil;
		NSString *source = [NSString stringWithContentsOfFile:@"macosx/shaders.metal"
			encoding:NSUTF8StringEncoding error:&error];
		source = [source stringByAppendingString:@"\nkernel void atlasVisibilityProbe(texture2d<float, access::read> occlusion [[texture(0)]], texture2d<float, access::read> known [[texture(1)]], texture2d<float, access::read> surface [[texture(2)]], texture2d<float, access::read> atlas [[texture(3)]], const device RemasterMeshCell *mesh [[buffer(0)]], const device float4 *rays [[buffer(1)]], device float *result [[buffer(2)]], uint i [[thread_position_in_grid]]) { result[i] = remasterVisibility(rays[i*2].xyz,rays[i*2+1].xyz,occlusion,known,surface,atlas,mesh); }\n"];
		id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
		check(library != nil);
		auto pipeline = [device newComputePipelineStateWithFunction:
			[library newFunctionWithName:@"remasterBuildVisibilityBlocks"] error:&error];
		check(pipeline != nil);
		auto visibility = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"atlasVisibilityProbe"] error:&error];
		MTLCompileOptions *options = [MTLCompileOptions new];
		options.preprocessorMacros = @{@"REMASTER_REFERENCE_VISIBILITY": @1};
		auto referenceLibrary = [device newLibraryWithSource:source options:options error:&error];
		auto reference = [device newComputePipelineStateWithFunction:[referenceLibrary newFunctionWithName:@"atlasVisibilityProbe"] error:&error];
		check(visibility != nil && reference != nil);
		id<MTLCommandQueue> queue = [device newCommandQueue];
		unsigned checked = 0;
		for (unsigned width : {1u, 3u, 4u, 5u, 19u})
			for (unsigned height : {1u, 4u, 7u, 13u})
			{
				const unsigned count = width * height, blockRows = (height + 3) / 4;
				std::vector<std::array<float, 4>> surface(count), expected(count);
				std::vector<std::array<float, 2>> heights(count);
				std::vector<uint8_t> coverage(count);
				std::vector<RemasterSurfaceMesh::Cell> mesh(count);
				for (unsigned i = 0; i < count; i++)
				{
					float center = float(int(i % 17) - 8) * 0.75f;
					surface[i] = {center, 0, 0, 1};
					heights[i] = {center, i % 5 ? 1.0f : 0.0f};
					coverage[i] = i % 3 == 0 ? 0 : i % 3 == 1 ? 128 : 255;
					auto &p = mesh[i];
					p.corners[0] = center - 0.25f; p.corners[1] = center + 0.5f;
					p.corners[2] = center + 0.25f; p.corners[3] = center;
					p.thickness = i % 2 ? 1 : 0;
					p.solidWall = i % 7 == 0 ? 1 : 0;
					p.wallBase = center - 4;
					float low = center - 0.25f, high = center + 0.5f;
					if (p.solidWall > 0.5f) low = p.wallBase;
					low -= p.thickness * 0.5f; high += p.thickness * 0.5f;
					const float opacity = heights[i][1] >= 0.5f ? coverage[i] / 255.0f : 0;
					expected[i] = {opacity > 0 ? low : INFINITY, opacity > 0 ? high : -INFINITY, center, opacity};
				}
				auto texture = [&](MTLPixelFormat format, unsigned w, unsigned h, const void *bytes, size_t stride) {
					auto descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
					descriptor.storageMode = MTLStorageModeShared;
					descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
					id<MTLTexture> result = [device newTextureWithDescriptor:descriptor]; check(result != nil);
					if (bytes) [result replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0 withBytes:bytes bytesPerRow:stride];
					return result;
				};
				auto occlusion = texture(MTLPixelFormatR8Unorm,width,height,coverage.data(),width);
				auto known = texture(MTLPixelFormatRG32Float,width,height,heights.data(),width*8);
				auto centers = texture(MTLPixelFormatRGBA32Float,width,height,surface.data(),width*16);
				auto atlas = texture(MTLPixelFormatRGBA32Float,width,height+blockRows,nullptr,0);
				auto buffer = [device newBufferWithBytes:mesh.data() length:mesh.size()*sizeof(mesh[0]) options:MTLResourceStorageModeShared];
				auto command = [queue commandBuffer]; auto encoder = [command computeCommandEncoder];
				[encoder setComputePipelineState:pipeline];
				[encoder setTexture:occlusion atIndex:0]; [encoder setTexture:known atIndex:1];
				[encoder setTexture:centers atIndex:2]; [encoder setTexture:atlas atIndex:3];
				[encoder setBuffer:buffer offset:0 atIndex:0];
				[encoder dispatchThreadgroups:MTLSizeMake(((width+3)/4+7)/8,(blockRows+7)/8,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
				[encoder endEncoding]; [command commit]; [command waitUntilCompleted];
				check(command.status == MTLCommandBufferStatusCompleted);
				std::vector<std::array<float,4>> actual(width*(height+blockRows));
				[atlas getBytes:actual.data() bytesPerRow:width*16 fromRegion:MTLRegionMake2D(0,0,width,height+blockRows) mipmapLevel:0];
				for (unsigned i = 0; i < count; i++) for (unsigned c = 0; c < 4; c++)
				{ check(c == 3 ? std::abs(actual[i][c]-expected[i][c]) < 0.000001f : actual[i][c] == expected[i][c]); checked++; }
				for (unsigned by = 0; by < blockRows; by++) for (unsigned bx = 0; bx < (width+3)/4; bx++)
				{
					float low = INFINITY, high = -INFINITY;
					for (unsigned y = by*4; y < std::min(height,by*4+4); y++)
						for (unsigned x = bx*4; x < std::min(width,bx*4+4); x++)
						{ low = std::min(low,expected[y*width+x][0]); high = std::max(high,expected[y*width+x][1]); }
					check(actual[(height+by)*width+bx][0] == low);
					check(actual[(height+by)*width+bx][1] == high); checked += 2;
				}
				// Independent unaccelerated triangle oracle, not the new packed data.
				// Exercise both XY directions, exact grid corners, horizontal/vertical
				// rays, origin/endpoint proper crossings and sub-cell ray intervals.
				constexpr unsigned rayCount = 2048;
				std::vector<simd_float4> rays(rayCount*2);
				uint32_t seed = 0x12345678;
				auto random = [&] { seed = seed*1664525u+1013904223u; return float(seed & 65535)/65536.0f; };
				for (unsigned i = 0; i < rayCount; i++)
				{
					rays[i*2] = {random()*width,random()*height,random()*24-12,0};
					rays[i*2+1] = {random()*width,random()*height,random()*24-12,0};
					if (i%7 == 0) rays[i*2+1].x = rays[i*2].x;
					if (i%7 == 1) rays[i*2+1].y = rays[i*2].y;
					if (i%7 == 2) rays[i*2+1].z = rays[i*2].z;
					if (i%7 == 3) for (unsigned j : {0u,1u}) {
						rays[i*2+j].x = std::floor(rays[i*2+j].x);
						rays[i*2+j].y = std::floor(rays[i*2+j].y);
					}
					if (i%7 == 4) rays[i*2+1] = rays[i*2] + simd_make_float4(0,0,2,0);
				}
				auto rayBuffer = [device newBufferWithBytes:rays.data() length:rays.size()*sizeof(rays[0]) options:MTLResourceStorageModeShared];
				id<MTLBuffer> outputs[2];
				for (unsigned mode = 0; mode < 2; mode++) {
					outputs[mode] = [device newBufferWithLength:rayCount*sizeof(float) options:MTLResourceStorageModeShared];
					auto query = [queue commandBuffer]; auto compute = [query computeCommandEncoder];
					[compute setComputePipelineState:mode ? reference : visibility];
					[compute setTexture:occlusion atIndex:0]; [compute setTexture:known atIndex:1];
					[compute setTexture:centers atIndex:2]; [compute setTexture:atlas atIndex:3];
					[compute setBuffer:buffer offset:0 atIndex:0]; [compute setBuffer:rayBuffer offset:0 atIndex:1];
					[compute setBuffer:outputs[mode] offset:0 atIndex:2];
					[compute dispatchThreads:MTLSizeMake(rayCount,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
					[compute endEncoding]; [query commit]; [query waitUntilCompleted];
					check(query.status == MTLCommandBufferStatusCompleted);
				}
				for (unsigned i = 0; i < rayCount; i++) {
					check(static_cast<float*>(outputs[0].contents)[i] == static_cast<float*>(outputs[1].contents)[i]); checked++;
				}
			}
		std::printf("PASS %u visibility atlas values: partial blocks, signed relief, finite sheets/shells/walls, unknowns and fractional coverage\n", checked);
		return 0;
	} catch (const std::exception &e) { std::fprintf(stderr,"%s\n",e.what()); return 1; } }
}
