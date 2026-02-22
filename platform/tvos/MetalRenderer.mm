#import "MetalRenderer.h"
#include <simd/simd.h>

// Must match gfx.h
static const int kMaxSnesWidth = 512;
static const int kMaxSnesHeight = 478;

@implementation MetalRenderer {
    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    id<MTLRenderPipelineState> _texturePipeline;
    id<MTLRenderPipelineState> _colorPipeline;
    id<MTLTexture> _texture;
    id<MTLSamplerState> _sampler;

    uint32_t _convertedBuffer[512 * 478]; // MAX_SNES_WIDTH * MAX_SNES_HEIGHT
    int _frameWidth;
    int _frameHeight;
    BOOL _hasFrame;

    float _rewindProgress;
    BOOL _rewindVisible;
}

- (instancetype)initWithMTKView:(MTKView *)view {
    self = [super init];
    if (!self) return nil;

    _device = view.device;
    _commandQueue = [_device newCommandQueue];
    _frameWidth = 256;
    _frameHeight = 224;
    _hasFrame = NO;
    _rewindVisible = NO;
    _rewindProgress = 0;

    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;

    // Load shaders from default library (.metal file compiled into app)
    id<MTLLibrary> library = [_device newDefaultLibrary];
    if (!library) {
        NSLog(@"Failed to load Metal shader library");
        return nil;
    }

    // Texture pipeline (game rendering)
    {
        MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = [library newFunctionWithName:@"vertexShader"];
        desc.fragmentFunction = [library newFunctionWithName:@"fragmentShader"];
        desc.colorAttachments[0].pixelFormat = view.colorPixelFormat;

        NSError *error = nil;
        _texturePipeline = [_device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (error) NSLog(@"Texture pipeline error: %@", error);
    }

    // Color overlay pipeline (rewind progress bar)
    {
        MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = [library newFunctionWithName:@"colorVertexShader"];
        desc.fragmentFunction = [library newFunctionWithName:@"colorFragmentShader"];
        desc.colorAttachments[0].pixelFormat = view.colorPixelFormat;
        desc.colorAttachments[0].blendingEnabled = YES;
        desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        NSError *error = nil;
        _colorPipeline = [_device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (error) NSLog(@"Color pipeline error: %@", error);
    }

    // Create texture for SNES framebuffer
    MTLTextureDescriptor *texDesc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                     width:kMaxSnesWidth
                                    height:kMaxSnesHeight
                                 mipmapped:NO];
    texDesc.usage = MTLTextureUsageShaderRead;
    _texture = [_device newTextureWithDescriptor:texDesc];

    // Nearest-neighbor sampler for crisp pixels
    MTLSamplerDescriptor *sampDesc = [[MTLSamplerDescriptor alloc] init];
    sampDesc.minFilter = MTLSamplerMinMagFilterNearest;
    sampDesc.magFilter = MTLSamplerMinMagFilterNearest;
    sampDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sampDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    _sampler = [_device newSamplerStateWithDescriptor:sampDesc];

    return self;
}

- (void)updateFrameWithBuffer:(const uint16_t *)buffer width:(int)width height:(int)height {
    if (!buffer || width <= 0 || height <= 0) return;

    _frameWidth = width;
    _frameHeight = height;

    // Convert RGB555 to BGRA8
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t rgb555 = buffer[y * kMaxSnesWidth + x];
            uint8_t r = ((rgb555 >> 10) & 0x1F) << 3;
            uint8_t g = ((rgb555 >> 5) & 0x1F) << 3;
            uint8_t b = (rgb555 & 0x1F) << 3;
            _convertedBuffer[y * kMaxSnesWidth + x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
        }
    }

    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    [_texture replaceRegion:region
                mipmapLevel:0
                  withBytes:_convertedBuffer
                bytesPerRow:kMaxSnesWidth * sizeof(uint32_t)];
    _hasFrame = YES;
}

- (void)setRewindProgress:(float)progress visible:(BOOL)visible {
    _rewindProgress = progress;
    _rewindVisible = visible;
}

#pragma mark - MTKViewDelegate

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    // Nothing to do
}

- (void)drawInMTKView:(MTKView *)view {
    if (!_hasFrame) return;

    id<MTLCommandBuffer> cmdBuf = [_commandQueue commandBuffer];
    MTLRenderPassDescriptor *rpd = view.currentRenderPassDescriptor;
    if (!rpd) return;

    id<MTLRenderCommandEncoder> enc = [cmdBuf renderCommandEncoderWithDescriptor:rpd];

    // Calculate viewport for 4:3 aspect ratio with letterboxing
    CGSize drawableSize = view.drawableSize;
    float targetAspect = 4.0f / 3.0f;
    float viewAspect = (float)drawableSize.width / (float)drawableSize.height;

    float vpX = 0, vpY = 0, vpW = drawableSize.width, vpH = drawableSize.height;
    if (viewAspect > targetAspect) {
        vpW = drawableSize.height * targetAspect;
        vpX = (drawableSize.width - vpW) / 2.0f;
    } else {
        vpH = drawableSize.width / targetAspect;
        vpY = (drawableSize.height - vpH) / 2.0f;
    }

    MTLViewport viewport = { vpX, vpY, vpW, vpH, 0.0, 1.0 };
    [enc setViewport:viewport];
    [enc setRenderPipelineState:_texturePipeline];
    [enc setFragmentTexture:_texture atIndex:0];
    [enc setFragmentSamplerState:_sampler atIndex:0];

    // Pass texture scale to vertex shader
    struct { float scaleX; float scaleY; } texScale;
    texScale.scaleX = (float)_frameWidth / (float)kMaxSnesWidth;
    texScale.scaleY = (float)_frameHeight / (float)kMaxSnesHeight;
    [enc setVertexBytes:&texScale length:sizeof(texScale) atIndex:0];

    [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

    // Draw rewind progress bar overlay
    if (_rewindVisible) {
        [enc setRenderPipelineState:_colorPipeline];

        MTLViewport fullViewport = { 0, 0, drawableSize.width, drawableSize.height, 0.0, 1.0 };
        [enc setViewport:fullViewport];

        float barHeight = 20.0f;
        float barY = drawableSize.height - barHeight - 20.0f;
        float barWidth = drawableSize.width * 0.8f;
        float barX = (drawableSize.width - barWidth) / 2.0f;
        float filledWidth = barWidth * _rewindProgress;

        auto toNDC = [drawableSize](float x, float y) -> simd_float2 {
            return simd_make_float2(
                (x / drawableSize.width) * 2.0f - 1.0f,
                1.0f - (y / drawableSize.height) * 2.0f
            );
        };

        // Background bar (dark gray)
        {
            simd_float2 tl = toNDC(barX, barY);
            simd_float2 br = toNDC(barX + barWidth, barY + barHeight);
            struct { simd_float2 pos; simd_float4 color; } verts[6] = {
                { {tl.x, tl.y}, {0.2f, 0.2f, 0.2f, 0.8f} },
                { {br.x, tl.y}, {0.2f, 0.2f, 0.2f, 0.8f} },
                { {tl.x, br.y}, {0.2f, 0.2f, 0.2f, 0.8f} },
                { {tl.x, br.y}, {0.2f, 0.2f, 0.2f, 0.8f} },
                { {br.x, tl.y}, {0.2f, 0.2f, 0.2f, 0.8f} },
                { {br.x, br.y}, {0.2f, 0.2f, 0.2f, 0.8f} }
            };
            [enc setVertexBytes:verts length:sizeof(verts) atIndex:0];
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
        }

        // Filled portion (cyan)
        if (filledWidth > 0) {
            simd_float2 tl = toNDC(barX, barY);
            simd_float2 br = toNDC(barX + filledWidth, barY + barHeight);
            struct { simd_float2 pos; simd_float4 color; } verts[6] = {
                { {tl.x, tl.y}, {0.0f, 1.0f, 1.0f, 0.9f} },
                { {br.x, tl.y}, {0.0f, 1.0f, 1.0f, 0.9f} },
                { {tl.x, br.y}, {0.0f, 1.0f, 1.0f, 0.9f} },
                { {tl.x, br.y}, {0.0f, 1.0f, 1.0f, 0.9f} },
                { {br.x, tl.y}, {0.0f, 1.0f, 1.0f, 0.9f} },
                { {br.x, br.y}, {0.0f, 1.0f, 1.0f, 0.9f} }
            };
            [enc setVertexBytes:verts length:sizeof(verts) atIndex:0];
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
        }
    }

    [enc endEncoding];
    [cmdBuf presentDrawable:view.currentDrawable];
    [cmdBuf commit];
}

@end
