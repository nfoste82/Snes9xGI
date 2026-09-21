/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

/***********************************************************************************
  SNES9X for Mac OS (c) Copyright John Stiles

  Snes9x for Mac OS X

  (c) Copyright 2001 - 2011  zones
  (c) Copyright 2002 - 2005  107
  (c) Copyright 2002         PB1400c
  (c) Copyright 2004         Alexander and Sander
  (c) Copyright 2004 - 2005  Steven Seeger
  (c) Copyright 2005         Ryan Vogt
  (c) Copyright 2019         Michael Donald Buckley
 ***********************************************************************************/

#import <Cocoa/Cocoa.h>

#include "snes9x.h"
#include "cheats.h"
#include "memmap.h"
#include "apu.h"
#include "display.h"
#include "blit.h"
#include "remaster/remaster.h"

#include <mutex>
#include <sys/time.h>

#include "mac-prefix.h"
#include "mac-os.h"
#include "mac-screenshot.h"
#include "mac-render.h"

static void S9xInitMetal (void);
static void S9xDeinitMetal(void);
static bool S9xPutImageMetal (int, int, const uint16 *, size_t, const uint32_t *, size_t,
	const uint8_t *, size_t, RemasterDebugMode);

static int					whichBuf          = 0;
static int					textureNum        = 0;
static int					prevBlitWidth, prevBlitHeight;
static int					imageWidth[2], imageHeight[2];
static int					nx                = 2;

typedef struct
{
    vector_float2 position;
    vector_float2 textureCoordinate;
} MetalVertex;

@interface MetalLayerDelegate: NSObject<CALayerDelegate, NSViewLayerContentScaleDelegate>
@end

@implementation MetalLayerDelegate
- (BOOL)layer:(CALayer *)layer shouldInheritContentsScale:(CGFloat)newScale fromWindow:(NSWindow *)window
{
	return YES;
}

@end

CAMetalLayer    			*metalLayer = nil;
MetalLayerDelegate			*layerDelegate = nil;
id<MTLDevice>   			metalDevice = nil;
id<MTLTexture>  			metalTexture = nil;
id<MTLCommandQueue>			metalCommandQueue = nil;
id<MTLRenderPipelineState>	metalPipelineState = nil;

void InitGraphics (void)
{
	if (!S9xBlitFilterInit()      |
		!S9xBlit2xSaIFilterInit() |
		!S9xBlitHQ2xFilterInit()  |
		!S9xBlitNTSCFilterInit())
		QuitWithFatalError(@"render 02");

	switch (videoMode)
	{
		default:
		case VIDEOMODE_NTSC_C:
		case VIDEOMODE_NTSC_TV_C:
			S9xBlitNTSCFilterSet(&snes_ntsc_composite);
			break; 

		case VIDEOMODE_NTSC_S:
		case VIDEOMODE_NTSC_TV_S:
			S9xBlitNTSCFilterSet(&snes_ntsc_svideo);
			break; 

		case VIDEOMODE_NTSC_R:
		case VIDEOMODE_NTSC_TV_R:
			S9xBlitNTSCFilterSet(&snes_ntsc_rgb);
			break; 

		case VIDEOMODE_NTSC_M:
		case VIDEOMODE_NTSC_TV_M:
			S9xBlitNTSCFilterSet(&snes_ntsc_monochrome);
			break;
	}
}

void DeinitGraphics (void)
{
	S9xBlitNTSCFilterDeinit();
	S9xBlitHQ2xFilterDeinit();
	S9xBlit2xSaIFilterDeinit();
	S9xBlitFilterDeinit();
}

void DrawFreezeDefrostScreen (uint8 *draw)
{
	const int w = SNES_WIDTH << 1, h = SNES_HEIGHT << 1;
	S9xPutImageMetal(w, h, (uint16 *)draw, w, nullptr, 0, nullptr, 0, RemasterDebugMode::Original);
}

bool DrawRemasterFrame (const RemasterFrame &frame, RemasterDebugMode debugMode,
	const RemasterTileContentId *selectedTile)
{
	if (frame.width > INT_MAX || frame.height > INT_MAX)
		return false;
	std::vector<uint32_t> owners;
	owners.reserve(frame.mainPixels.size());
	for (const RemasterFramePixel &pixel : frame.mainPixels)
		owners.push_back(pixel.owner);
	std::vector<uint8_t> highlights;
	if (selectedTile)
	{
		highlights.assign(frame.mainPixels.size(), 0);
		for (uint32_t offset : S9xRemasterFrameOccurrences(frame, *selectedTile))
			highlights[offset] = 1;
	}
	return S9xPutImageMetal(static_cast<int>(frame.width), static_cast<int>(frame.height),
		frame.originalRgb555.data(), frame.width, owners.data(), frame.width,
		highlights.empty() ? nullptr : highlights.data(), frame.width, debugMode);
}

static void S9xInitMetal (void)
{
    glScreenW = glScreenBounds.size.width;
    glScreenH = glScreenBounds.size.height;

    metalLayer = (CAMetalLayer *)s9xView.layer;
	layerDelegate = [MetalLayerDelegate new];
	metalLayer.delegate = layerDelegate;
	
    metalDevice = s9xView.device;
			
	metalCommandQueue = [metalDevice newCommandQueue];
	
	NSError *error = nil;
	id<MTLLibrary> defaultLibrary = [metalDevice newDefaultLibraryWithBundle:[NSBundle bundleForClass:[S9xEngine class]] error:&error];

	MTLRenderPipelineDescriptor *pipelineDescriptor = [MTLRenderPipelineDescriptor new];
	pipelineDescriptor.label = @"Snes9x Pipeline";
	pipelineDescriptor.vertexFunction = [defaultLibrary newFunctionWithName:@"vertexShader"];
	pipelineDescriptor.colorAttachments[0].pixelFormat = s9xView.colorPixelFormat;
	pipelineDescriptor.fragmentFunction = [defaultLibrary newFunctionWithName:@"fragmentShader"];
	
	metalPipelineState = [metalDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
	
	if (metalPipelineState == nil)
	{
		NSLog(@"%@",error);
	}
}

static void S9xDeinitMetal (void)
{
	
	metalCommandQueue = nil;
	metalDevice = nil;
	metalTexture = nil;
	metalLayer = nil;
}

void GetGameDisplay (int *w, int *h)
{
	if (w != NULL && h != NULL)
	{
        *w = s9xView.frame.size.width;
		*h = s9xView.frame.size.height;
	}
}

void S9xInitDisplay (int argc, char **argv)
{
    glScreenBounds = s9xView.frame;

	unlimitedCursor = CGPointMake(0.0f, 0.0f);

	imageWidth[0] = imageHeight[0] = 0;
	imageWidth[1] = imageHeight[1] = 0;
	prevBlitWidth = prevBlitHeight = 0;
	whichBuf      = 0;
	textureNum    = 0;

	switch (videoMode)
	{
		case VIDEOMODE_HQ4X:
			nx =  4;
			break;

		case VIDEOMODE_HQ3X:
			nx =  3;
			break;

		case VIDEOMODE_NTSC_C:
		case VIDEOMODE_NTSC_S:
		case VIDEOMODE_NTSC_R:
		case VIDEOMODE_NTSC_M:
			nx = -1;
			break;

		case VIDEOMODE_NTSC_TV_C:
		case VIDEOMODE_NTSC_TV_S:
		case VIDEOMODE_NTSC_TV_R:
		case VIDEOMODE_NTSC_TV_M:
			nx = -2;
			break;

		default:
			nx =  2;
			break;
	}

    S9xInitMetal();

	S9xSetSoundMute(false);
    lastFrame = GetMicroseconds();
}

void S9xDeinitDisplay (void)
{
	S9xSetSoundMute(true);
    S9xDeinitMetal();
}

bool8 S9xInitUpdate (void)
{
	return (true);
}

bool8 S9xDeinitUpdate (int width, int height)
{
	S9xPutImage(width, height);
	return true;
}

bool8 S9xContinueUpdate (int width, int height)
{
	return (true);
}

void S9xPutImage (int width, int height)
{
	for(unsigned int i = 0 ; i < sizeof(watches)/sizeof(*watches) ; i++)
	{
		if(watches[i].on)
		{
			int address = watches[i].address - 0x7E0000;
			const uint8* source;

			if(address < 0x20000)
			{
				source = Memory.RAM + address;
			}
			else if(address < 0x30000)
			{
				source = Memory.SRAM + address - 0x20000;
			}
			else
			{
				source = Memory.FillRAM + address - 0x30000;
			}

			memcpy(&(Cheat.CWatchRAM[address]), source, watches[i].size);
		}
	}

    if (Settings.DisplayFrameRate)
    {
        static int	drawnFrames[60] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        static int	tableIndex = 0;
        int			frameCalc  = 0;

        drawnFrames[tableIndex] = skipFrames;

        if (Settings.TurboMode)
        {
            drawnFrames[tableIndex] = (drawnFrames[tableIndex] + (macFastForwardRate / 2)) / macFastForwardRate;
            if (drawnFrames[tableIndex] == 0)
                drawnFrames[tableIndex] = 1;
        }

        tableIndex = (tableIndex + 1) % 60;

        for (int i = 0; i < 60; i++)
            frameCalc += drawnFrames[i];

		// avoid dividing by 0
		if (frameCalc == 0)
			frameCalc = 1;
		
        IPPU.DisplayedRenderedFrameCount = (Memory.ROMFramesPerSecond * 60) / frameCalc;
    }
	
	S9xPutImageMetal(width, height, GFX.Screen, GFX.RealPPL, S9xRemasterMainOwners(), GFX.RealPPL,
		nullptr, 0, S9xRemasterGetDebugMode());
}


static bool S9xPutImageMetal (int width, int height, const uint16 *buffer16, size_t pitch,
	const uint32_t *owners, size_t ownerPitch, const uint8_t *highlights, size_t highlightPitch,
	RemasterDebugMode debugMode)
{
	static std::mutex renderMutex;
	std::lock_guard<std::mutex> lock(renderMutex);
	static uint8 *buffer = nil;
	static size_t buffer_size = 0;
	if (width <= 0 || height <= 0 || !buffer16 || pitch < static_cast<size_t>(width) ||
		!metalLayer || !metalDevice || !metalCommandQueue || !metalPipelineState)
		return false;
	const size_t requiredSize = static_cast<size_t>(width) * height * 4;

	if (buffer_size != requiredSize)
	{
		uint8 *newBuffer = (uint8 *)realloc(buffer, requiredSize);
		if (!newBuffer)
			return false;
		buffer = newBuffer;
		buffer_size = requiredSize;
	}

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			uint16 pixel = buffer16[y * pitch + x];
			unsigned int red = (pixel & FIRST_COLOR_MASK_RGB555) >> 10;
			unsigned int green = (pixel & SECOND_COLOR_MASK_RGB555) >> 5;
			unsigned int blue = (pixel & THIRD_COLOR_MASK_RGB555);

			red = ( red * 527 + 23 ) >> 6;
			green = ( green * 527 + 23 ) >> 6;
			blue = ( blue * 527 + 23 ) >> 6;

			if (debugMode != RemasterDebugMode::Original)
			{
				uint32 owner = owners && ownerPitch >= static_cast<size_t>(width) ?
					owners[y * ownerPitch + x] : REMASTER_OWNER_UNSUPPORTED;
				unsigned int debugRed, debugGreen, debugBlue;
				if (owner == REMASTER_OWNER_UNSUPPORTED)
				{
					debugRed = 255;
					debugGreen = 0;
					debugBlue = 255;
				}
				else if (owner == REMASTER_OWNER_FORCED_BLANK)
				{
					debugRed = debugGreen = debugBlue = 32;
				}
				else
				{
					uint32 hash = owner * 0x9e3779b1u;
					hash ^= hash >> 16;
					debugRed = 48 + (hash & 0xcf);
					debugGreen = 48 + ((hash >> 8) & 0xcf);
					debugBlue = 48 + ((hash >> 16) & 0xcf);
				}

				if (debugMode == RemasterDebugMode::Overlay)
				{
					red = (red + debugRed) >> 1;
					green = (green + debugGreen) >> 1;
					blue = (blue + debugBlue) >> 1;
				}
				else
				{
					red = debugRed;
					green = debugGreen;
					blue = debugBlue;
				}
			}
			if (highlights && highlightPitch >= static_cast<size_t>(width) && highlights[y * highlightPitch + x])
			{
				red = (red + 255) >> 1;
				green = (green + 255) >> 1;
				blue >>= 1;
			}

			int offset = (y * width + x) * 4;
			buffer[offset++] = (uint8)red;
			buffer[offset++] = (uint8)green;
			buffer[offset++] = (uint8)blue;
			buffer[offset] = 0xFF;
		}
	}
	
	CGSize layerSize = metalLayer.bounds.size;
	
	@autoreleasepool {
		MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor new];
		textureDescriptor.pixelFormat = MTLPixelFormatRGBA8Unorm;
		textureDescriptor.width = width;
		textureDescriptor.height = height;
		
		metalTexture = [metalDevice newTextureWithDescriptor:textureDescriptor];
		
		[metalTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:buffer bytesPerRow:width * 4];
		
		float vWidth = layerSize.width / 2.0;
		float vHeight = layerSize.height / 2.0;
		
		const MetalVertex verticies[] =
		{
			// Pixel positions, Texture coordinates
			{ {  vWidth,  -vHeight },  { 1.f, 1.f } },
			{ { -vWidth,  -vHeight },  { 0.f, 1.f } },
			{ { -vWidth,   vHeight },  { 0.f, 0.f } },
			
			{ {  vWidth,  -vHeight },  { 1.f, 1.f } },
			{ { -vWidth,   vHeight },  { 0.f, 0.f } },
			{ {  vWidth,   vHeight },  { 1.f, 0.f } },
		};
		
		id<MTLBuffer> vertexBuffer = [metalDevice newBufferWithBytes:verticies length:sizeof(verticies) options:MTLResourceStorageModeShared];
		id<MTLBuffer> fragmentBuffer = [metalDevice newBufferWithBytes:&videoMode length:sizeof(videoMode) options:MTLResourceStorageModeShared];
		
		id<MTLCommandBuffer> commandBuffer = [metalCommandQueue commandBuffer];
		commandBuffer.label = @"Snes9x command buffer";
		
		id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
		if (!drawable)
			return false;
		
		MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
		
		renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
		renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
		renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1.0);
		
		if(renderPassDescriptor != nil)
		{
			id<MTLRenderCommandEncoder> renderEncoder =
			[commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
			renderEncoder.label = @"Snes9x render encoder";
			
			vector_uint2 viewportSize = { static_cast<unsigned int>(layerSize.width), static_cast<unsigned int>(layerSize.height) };
			
			CGFloat scale = metalLayer.contentsScale;
			[renderEncoder setViewport:(MTLViewport){0.0, 0.0, layerSize.width * scale, layerSize.height * scale, -1.0, 1.0 }];
			
			[renderEncoder setRenderPipelineState:metalPipelineState];
			
			[renderEncoder setVertexBuffer:vertexBuffer
									offset:0
								   atIndex:0];
			
			[renderEncoder setVertexBytes:&viewportSize
								   length:sizeof(viewportSize)
								  atIndex:1];
			
			[renderEncoder setFragmentTexture:metalTexture atIndex:0];
			[renderEncoder setFragmentBuffer:fragmentBuffer offset:0 atIndex:1];
			
			[renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
			
			[renderEncoder endEncoding];

			[commandBuffer presentDrawable:drawable];
			[commandBuffer commit];
			
			[commandBuffer waitUntilCompleted];
		}
	}
	return true;
}

void S9xTextMode (void)
{
	return;
}

void S9xGraphicsMode (void)
{
	return;
}
