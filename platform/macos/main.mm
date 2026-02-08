/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
               This file is licensed under the Snes9x License.
  For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

// macOS frontend — single-file Metal + AVAudioEngine + GCController app.
// No menus or UI beyond the game window. ROM path from command-line or drag-drop.

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <AVFoundation/AVFoundation.h>
#import <GameController/GameController.h>

#include "emulator.h"
#include "snes9x.h"
#include "gfx.h"
#include "apu/apu.h"

// ---------------------------------------------------------------------------
// Forward declarations for port stubs and frame-size helper
// ---------------------------------------------------------------------------

extern void EmulatorSetFrameSize(int width, int height);

// ---------------------------------------------------------------------------
// Metal shader source — simple passthrough to blit texture to screen
// ---------------------------------------------------------------------------

static NSString *const kShaderSource = @R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
};

struct TexScale {
    float scaleX;
    float scaleY;
};

vertex VertexOut vertexShader(uint vid [[vertex_id]],
                               constant TexScale &scale [[buffer(0)]]) {
    // Fullscreen triangle pair
    float2 positions[] = {
        float2(-1, -1), float2( 1, -1), float2(-1,  1),
        float2(-1,  1), float2( 1, -1), float2( 1,  1)
    };
    float2 texCoords[] = {
        float2(0, 1), float2(1, 1), float2(0, 0),
        float2(0, 0), float2(1, 1), float2(1, 0)
    };
    VertexOut out;
    out.position = float4(positions[vid], 0, 1);
    out.texCoord = texCoords[vid] * float2(scale.scaleX, scale.scaleY);
    return out;
}

fragment float4 fragmentShader(VertexOut in [[stage_in]],
                               texture2d<float> tex [[texture(0)]],
                               sampler samp [[sampler(0)]]) {
    return tex.sample(samp, in.texCoord);
}

// Overlay shader for solid color rectangles
struct ColorVertex {
    float2 position [[attribute(0)]];
    float4 color [[attribute(1)]];
};

struct ColorVertexOut {
    float4 position [[position]];
    float4 color;
};

vertex ColorVertexOut colorVertexShader(uint vid [[vertex_id]],
                                         constant ColorVertex *vertices [[buffer(0)]]) {
    ColorVertexOut out;
    out.position = float4(vertices[vid].position, 0, 1);
    out.color = vertices[vid].color;
    return out;
}

fragment float4 colorFragmentShader(ColorVertexOut in [[stage_in]]) {
    return in.color;
}
)";

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static NSString *g_romPath = nil;
static bool g_running = false;
static bool g_debug = false;

// ---------------------------------------------------------------------------
// AudioEngine — pulls samples from S9xMixSamples via AVAudioEngine source node
// ---------------------------------------------------------------------------

@interface AudioEngine : NSObject
@property (nonatomic, strong) AVAudioEngine *engine;
- (void)start;
- (void)stop;
@end

@implementation AudioEngine

- (void)start {
    self.engine = [[AVAudioEngine alloc] init];

    double sampleRate = Settings.SoundPlaybackRate ? Settings.SoundPlaybackRate : 48000;
    AVAudioFormat *format = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                                            sampleRate:sampleRate
                                                              channels:2
                                                           interleaved:YES];

    AVAudioSourceNode *srcNode = [[AVAudioSourceNode alloc]
        initWithFormat:format
        renderBlock:^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp,
                              AVAudioFrameCount frameCount, AudioBufferList *outputData) {
            (void)isSilence;
            (void)timestamp;
            for (UInt32 i = 0; i < outputData->mNumberBuffers; i++) {
                AudioBuffer *buf = &outputData->mBuffers[i];
                int sampleCount = buf->mDataByteSize / 2; // 16-bit samples
                S9xMixSamples((uint8 *)buf->mData, sampleCount);
            }
            return noErr;
        }];

    [self.engine attachNode:srcNode];
    [self.engine connect:srcNode to:self.engine.mainMixerNode format:format];

    NSError *error = nil;
    [self.engine startAndReturnError:&error];
    if (error) {
        NSLog(@"AudioEngine start error: %@", error);
    }
}

- (void)stop {
    [self.engine stop];
    self.engine = nil;
}

@end

// ---------------------------------------------------------------------------
// InputManager — GCController mapping to SNES joypad bitmask
// ---------------------------------------------------------------------------

@interface InputManager : NSObject
@property (nonatomic, strong) NSMutableArray<GCController *> *controllers;
- (void)setup;
- (void)teardown;
@end

@implementation InputManager

- (void)setup {
    self.controllers = [NSMutableArray array];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(controllerDidConnect:)
               name:GCControllerDidConnectNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(controllerDidDisconnect:)
               name:GCControllerDidDisconnectNotification
             object:nil];

    // Pick up already-connected controllers
    NSArray<GCController *> *existing = [GCController controllers];
    printf("[Input] Found %lu already-connected controller(s)\n", (unsigned long)existing.count);
    for (GCController *c in existing) {
        printf("[Input] Controller: %s (extended=%s)\n",
               c.vendorName ? c.vendorName.UTF8String : "unknown",
               c.extendedGamepad ? "yes" : "no");
        [self configureController:c];
        [self.controllers addObject:c];
    }
}

- (void)teardown {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    self.controllers = nil;
}

- (void)controllerDidConnect:(NSNotification *)note {
    GCController *c = note.object;
    printf("[Input] Controller CONNECTED: %s (extended=%s)\n",
           c.vendorName ? c.vendorName.UTF8String : "unknown",
           c.extendedGamepad ? "yes" : "no");
    [self configureController:c];
    [self.controllers addObject:c];
}

- (void)controllerDidDisconnect:(NSNotification *)note {
    GCController *c = note.object;
    printf("[Input] Controller DISCONNECTED: %s\n",
           c.vendorName ? c.vendorName.UTF8String : "unknown");
    [self.controllers removeObject:c];
}

- (void)configureController:(GCController *)controller {
    int padIndex = (int)self.controllers.count; // 0 for first controller
    if (padIndex > 7) padIndex = 7;

    GCExtendedGamepad *gp = controller.extendedGamepad;
    if (!gp) return;

    gp.valueChangedHandler = ^(GCExtendedGamepad *gamepad, GCControllerElement *element) {
        (void)element;
        uint16_t buttons = 0;

        if (gamepad.dpad.up.pressed)      buttons |= SNES_UP_MASK;
        if (gamepad.dpad.down.pressed)    buttons |= SNES_DOWN_MASK;
        if (gamepad.dpad.left.pressed)    buttons |= SNES_LEFT_MASK;
        if (gamepad.dpad.right.pressed)   buttons |= SNES_RIGHT_MASK;

        // Map face buttons: SNES layout
        // GC A (right) -> SNES A, GC B (bottom) -> SNES B
        // GC X (top)   -> SNES X, GC Y (left)   -> SNES Y
        if (gamepad.buttonA.pressed)      buttons |= SNES_A_MASK;
        if (gamepad.buttonB.pressed)      buttons |= SNES_B_MASK;
        if (gamepad.buttonX.pressed)      buttons |= SNES_X_MASK;
        if (gamepad.buttonY.pressed)      buttons |= SNES_Y_MASK;

        if (gamepad.leftShoulder.pressed)  buttons |= SNES_TL_MASK;
        if (gamepad.rightShoulder.pressed) buttons |= SNES_TR_MASK;

        // Menu button -> Start, Options button -> Select
        if (gamepad.buttonMenu.pressed)    buttons |= SNES_START_MASK;
        if (gamepad.buttonOptions && gamepad.buttonOptions.pressed)
            buttons |= SNES_SELECT_MASK;

        if (g_debug)
            printf("[Input] Controller pad%d buttons=0x%04x\n", padIndex, buttons);
        Emulator::SetButtonState(padIndex, buttons);

        // L2/ZL trigger for rewind
        bool rewindHeld = gamepad.leftTrigger.pressed;
        if (rewindHeld && !Emulator::IsRewinding()) {
            Emulator::RewindStartContinuous();
        } else if (!rewindHeld && Emulator::IsRewinding()) {
            Emulator::RewindStop();
        }
    };
}

@end

// ---------------------------------------------------------------------------
// GameView — MTKView subclass that accepts first responder for keyboard input
// ---------------------------------------------------------------------------

static uint16_t g_keyboardButtons = 0;

static void HandleKeyEvent(NSEvent *event, BOOL pressed) {
    uint16_t mask = 0;
    const char *name = "?";

    switch (event.keyCode) {
        case 126: mask = SNES_UP_MASK;     name = "Up";     break;
        case 125: mask = SNES_DOWN_MASK;   name = "Down";   break;
        case 123: mask = SNES_LEFT_MASK;   name = "Left";   break;
        case 124: mask = SNES_RIGHT_MASK;  name = "Right";  break;
        case 13:  mask = SNES_UP_MASK;     name = "W/Up";   break;
        case 0:   mask = SNES_LEFT_MASK;   name = "A/Left"; break;
        case 1:   mask = SNES_DOWN_MASK;   name = "S/Down"; break;
        case 2:   mask = SNES_RIGHT_MASK;  name = "D/Right";break;
        case 37:  mask = SNES_A_MASK;      name = "L/A";    break;
        case 40:  mask = SNES_B_MASK;      name = "K/B";    break;
        case 34:  mask = SNES_X_MASK;      name = "I/X";    break;
        case 31:  mask = SNES_Y_MASK;      name = "O/Y";    break;
        case 38:  mask = SNES_Y_MASK;      name = "J/Y";    break;
        case 3:   mask = SNES_TL_MASK;     name = "F/TL";   break;
        case 35:  mask = SNES_TR_MASK;     name = "P/TR";   break;
        case 36:  mask = SNES_START_MASK;  name = "Enter/Start"; break;
        case 49:  mask = SNES_SELECT_MASK; name = "Space/Select"; break;
        case 48:  mask = SNES_SELECT_MASK; name = "Tab/Select"; break;
        default:
            if (g_debug)
                printf("[Input] Unmapped key code: %d %s\n", event.keyCode, pressed ? "down" : "up");
            return;
    }

    if (pressed)
        g_keyboardButtons |= mask;
    else
        g_keyboardButtons &= ~mask;

    if (g_debug)
        printf("[Input] Key %s %s -> buttons=0x%04x\n", name, pressed ? "DOWN" : "UP", g_keyboardButtons);
    Emulator::SetButtonState(0, g_keyboardButtons);
}

@interface GameView : MTKView
@end

@implementation GameView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent *)event {
    HandleKeyEvent(event, YES);
}

- (void)keyUp:(NSEvent *)event {
    HandleKeyEvent(event, NO);
}

- (void)mouseDown:(NSEvent *)event {
    (void)event;
    Settings.StopEmulation = !Settings.StopEmulation;
    if (g_debug)
        printf("[Input] Mouse click -> pause=%s\n", Settings.StopEmulation ? "YES" : "NO");
}

@end

// ---------------------------------------------------------------------------
// GameViewController — Metal rendering
// ---------------------------------------------------------------------------

@interface GameViewController : NSViewController <MTKViewDelegate>
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLRenderPipelineState> pipelineState;
@property (nonatomic, strong) id<MTLRenderPipelineState> colorPipelineState;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) id<MTLTexture> texture;
@property (nonatomic, strong) id<MTLSamplerState> sampler;
@end

@implementation GameViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    self.device = MTLCreateSystemDefaultDevice();
    MTKView *mtkView = (MTKView *)self.view;
    mtkView.device = self.device;
    mtkView.delegate = self;
    mtkView.preferredFramesPerSecond = Settings.PAL ? 50 : 60;
    mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;

    self.commandQueue = [self.device newCommandQueue];

    // Compile shaders
    NSError *error = nil;
    id<MTLLibrary> library = [self.device newLibraryWithSource:kShaderSource options:nil error:&error];
    if (error) {
        NSLog(@"Shader compile error: %@", error);
        return;
    }

    MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = [library newFunctionWithName:@"vertexShader"];
    desc.fragmentFunction = [library newFunctionWithName:@"fragmentShader"];
    desc.colorAttachments[0].pixelFormat = mtkView.colorPixelFormat;

    self.pipelineState = [self.device newRenderPipelineStateWithDescriptor:desc error:&error];
    if (error) {
        NSLog(@"Pipeline error: %@", error);
        return;
    }

    // Create color overlay pipeline (for progress bar)
    MTLRenderPipelineDescriptor *colorDesc = [[MTLRenderPipelineDescriptor alloc] init];
    colorDesc.vertexFunction = [library newFunctionWithName:@"colorVertexShader"];
    colorDesc.fragmentFunction = [library newFunctionWithName:@"colorFragmentShader"];
    colorDesc.colorAttachments[0].pixelFormat = mtkView.colorPixelFormat;
    colorDesc.colorAttachments[0].blendingEnabled = YES;
    colorDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    colorDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    colorDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    colorDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    colorDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    colorDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    self.colorPipelineState = [self.device newRenderPipelineStateWithDescriptor:colorDesc error:&error];
    if (error) {
        NSLog(@"Color pipeline error: %@", error);
        return;
    }

    // Create texture for SNES framebuffer
    // We'll use BGRA8 and convert RGB555->RGBA manually
    MTLTextureDescriptor *texDesc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                     width:MAX_SNES_WIDTH
                                    height:MAX_SNES_HEIGHT
                                 mipmapped:NO];
    texDesc.usage = MTLTextureUsageShaderRead;
    self.texture = [self.device newTextureWithDescriptor:texDesc];

    // Nearest-neighbor sampler for crisp pixels
    MTLSamplerDescriptor *sampDesc = [[MTLSamplerDescriptor alloc] init];
    sampDesc.minFilter = MTLSamplerMinMagFilterNearest;
    sampDesc.magFilter = MTLSamplerMinMagFilterNearest;
    sampDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sampDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    self.sampler = [self.device newSamplerStateWithDescriptor:sampDesc];
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    (void)view;
    (void)size;
}

- (void)drawInMTKView:(MTKView *)view {
    if (!g_running || Settings.StopEmulation)
        return;

    // Run one frame of emulation (or rewind tick)
    if (Emulator::IsRewinding()) {
        Emulator::RewindTick();
    } else {
        Emulator::RunFrame();
    }

    // Upload framebuffer to texture
    int w = Emulator::GetFrameWidth();
    int h = Emulator::GetFrameHeight();
    const uint16_t *fb = Emulator::GetFrameBuffer();

    if (fb && w > 0 && h > 0) {
        // Convert RGB555 to BGRA8
        static uint32_t convertedBuffer[MAX_SNES_WIDTH * MAX_SNES_HEIGHT];
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                uint16_t rgb555 = fb[y * MAX_SNES_WIDTH + x];
                // Extract RGB555: 0RRRRRGGGGGBBBBB
                uint8_t r = ((rgb555 >> 10) & 0x1F) << 3; // 5->8 bit
                uint8_t g = ((rgb555 >> 5) & 0x1F) << 3;
                uint8_t b = (rgb555 & 0x1F) << 3;
                // Pack as BGRA8
                convertedBuffer[y * MAX_SNES_WIDTH + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
            }
        }

        MTLRegion region = MTLRegionMake2D(0, 0, w, h);
        [self.texture replaceRegion:region
                        mipmapLevel:0
                          withBytes:convertedBuffer
                        bytesPerRow:MAX_SNES_WIDTH * sizeof(uint32_t)];
    }

    // Render
    id<MTLCommandBuffer> cmdBuf = [self.commandQueue commandBuffer];
    MTLRenderPassDescriptor *rpd = view.currentRenderPassDescriptor;
    if (!rpd) return;

    id<MTLRenderCommandEncoder> enc = [cmdBuf renderCommandEncoderWithDescriptor:rpd];

    // Calculate viewport for 4:3 aspect ratio with letterboxing
    CGSize drawableSize = view.drawableSize;
    float targetAspect = 4.0f / 3.0f;
    float viewAspect = (float)drawableSize.width / (float)drawableSize.height;

    float vpX = 0, vpY = 0, vpW = drawableSize.width, vpH = drawableSize.height;
    if (viewAspect > targetAspect) {
        // Window is wider than 4:3 — pillarbox
        vpW = drawableSize.height * targetAspect;
        vpX = (drawableSize.width - vpW) / 2.0f;
    } else {
        // Window is taller than 4:3 — letterbox
        vpH = drawableSize.width / targetAspect;
        vpY = (drawableSize.height - vpH) / 2.0f;
    }

    MTLViewport viewport = { vpX, vpY, vpW, vpH, 0.0, 1.0 };
    [enc setViewport:viewport];
    [enc setRenderPipelineState:self.pipelineState];
    [enc setFragmentTexture:self.texture atIndex:0];
    [enc setFragmentSamplerState:self.sampler atIndex:0];

    // Pass texture scale to vertex shader
    struct { float scaleX; float scaleY; } texScale;
    texScale.scaleX = (float)w / (float)MAX_SNES_WIDTH;
    texScale.scaleY = (float)h / (float)MAX_SNES_HEIGHT;
    [enc setVertexBytes:&texScale length:sizeof(texScale) atIndex:0];

    [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

    // Draw rewind progress bar overlay if rewinding
    if (Emulator::IsRewinding()) {
        int depth = Emulator::GetRewindBufferDepth();
        int pos = Emulator::GetRewindPosition();
        if (depth > 0 && pos >= 0) {
            // Switch to color pipeline
            [enc setRenderPipelineState:self.colorPipelineState];

            // Reset viewport to full screen for overlay
            MTLViewport fullViewport = { 0, 0, drawableSize.width, drawableSize.height, 0.0, 1.0 };
            [enc setViewport:fullViewport];

            // Progress bar dimensions (in screen coordinates)
            float barHeight = 20.0f;
            float barY = drawableSize.height - barHeight - 20.0f; // 20px from bottom
            float barWidth = drawableSize.width * 0.8f; // 80% of screen width
            float barX = (drawableSize.width - barWidth) / 2.0f; // Centered

            float progress = (float)pos / (float)(depth - 1); // 0.0 (oldest) to 1.0 (newest)
            float filledWidth = barWidth * progress;

            // Convert screen coords to NDC (-1 to 1)
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
                    { {tl.x, tl.y}, {0.2f, 0.2f, 0.2f, 0.8f} }, // top-left
                    { {br.x, tl.y}, {0.2f, 0.2f, 0.2f, 0.8f} }, // top-right
                    { {tl.x, br.y}, {0.2f, 0.2f, 0.2f, 0.8f} }, // bottom-left
                    { {tl.x, br.y}, {0.2f, 0.2f, 0.2f, 0.8f} }, // bottom-left
                    { {br.x, tl.y}, {0.2f, 0.2f, 0.2f, 0.8f} }, // top-right
                    { {br.x, br.y}, {0.2f, 0.2f, 0.2f, 0.8f} }  // bottom-right
                };
                [enc setVertexBytes:verts length:sizeof(verts) atIndex:0];
                [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
            }

            // Filled portion (cyan)
            if (filledWidth > 0) {
                simd_float2 tl = toNDC(barX, barY);
                simd_float2 br = toNDC(barX + filledWidth, barY + barHeight);

                struct { simd_float2 pos; simd_float4 color; } verts[6] = {
                    { {tl.x, tl.y}, {0.0f, 1.0f, 1.0f, 0.9f} }, // cyan
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
    }

    [enc endEncoding];

    [cmdBuf presentDrawable:view.currentDrawable];
    [cmdBuf commit];
}

@end

// ---------------------------------------------------------------------------
// AppDelegate
// ---------------------------------------------------------------------------

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) NSWindow *window;
@property (nonatomic, strong) GameViewController *viewController;
@property (nonatomic, strong) AudioEngine *audio;
@property (nonatomic, strong) InputManager *input;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;

    // Parse command-line args
    NSArray *args = [[NSProcessInfo processInfo] arguments];
    for (NSUInteger i = 1; i < args.count; i++) {
        if ([args[i] isEqualToString:@"--debug"]) {
            g_debug = true;
            printf("[Debug] Debug mode enabled\n");
        }
    }

    // Determine ROM path
    NSString *romPath = g_romPath;
    if (!romPath) {
        // Try command-line args
        for (NSUInteger i = 1; i < args.count; i++) {
            NSString *arg = args[i];
            if (![arg hasPrefix:@"-"]) {
                romPath = arg;
                break;
            }
        }
    }

    if (!romPath) {
        // Open file dialog
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        panel.allowedContentTypes = @[];
        panel.allowsOtherFileTypes = YES;
        panel.title = @"Select SNES ROM";
        if ([panel runModal] != NSModalResponseOK) {
            [NSApp terminate:nil];
            return;
        }
        romPath = panel.URL.path;
    }

    // Initialize emulator
    std::string configPath;
    for (NSUInteger i = 1; i < args.count; i++) {
        if ([args[i] isEqualToString:@"--config"] && i + 1 < args.count) {
            configPath = [args[i + 1] UTF8String];
            break;
        }
    }

    if (!Emulator::Init(configPath.c_str())) {
        NSLog(@"Failed to initialize emulator");
        [NSApp terminate:nil];
        return;
    }

    if (!Emulator::LoadROM([romPath UTF8String])) {
        NSLog(@"Failed to load ROM: %@", romPath);
        Emulator::Shutdown();
        [NSApp terminate:nil];
        return;
    }

    printf("Loaded ROM: %s\n", Emulator::GetROMName());

    // Create window
    NSRect frame = NSMakeRect(0, 0, 800, 600);
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                       NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = [NSString stringWithFormat:@"Snes9x — %s", Emulator::GetROMName()];
    [self.window center];

    // Create Metal view (GameView subclass for keyboard input)
    GameView *mtkView = [[GameView alloc] initWithFrame:frame device:MTLCreateSystemDefaultDevice()];
    self.viewController = [[GameViewController alloc] init];
    self.viewController.view = mtkView;
    [self.viewController viewDidLoad];

    self.window.contentView = mtkView;
    self.window.contentViewController = self.viewController;

    // Start audio
    self.audio = [[AudioEngine alloc] init];
    [self.audio start];

    // Set up input
    self.input = [[InputManager alloc] init];
    [self.input setup];

    g_running = true;

    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:mtkView];
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    (void)notification;
    g_running = false;
    [self.audio stop];
    [self.input teardown];
    Emulator::Shutdown();
}

- (void)applicationDidBecomeActive:(NSNotification *)notification {
    (void)notification;
    if (g_running)
        Emulator::Resume();
}

- (void)applicationWillResignActive:(NSNotification *)notification {
    (void)notification;
    if (g_running)
        Emulator::Suspend();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    (void)sender;
    return YES;
}

- (BOOL)application:(NSApplication *)sender openFile:(NSString *)filename {
    (void)sender;
    g_romPath = filename;
    return YES;
}

@end

// ---------------------------------------------------------------------------
// Port interface stubs — platform-specific (macOS)
// ---------------------------------------------------------------------------

void S9xInitDisplay(int argc, char **argv)
{
    (void)argc;
    (void)argv;
}

void S9xDeinitDisplay()
{
}

bool8 S9xInitUpdate()
{
    return TRUE;
}

bool8 S9xDeinitUpdate(int width, int height)
{
    EmulatorSetFrameSize(width, height);
    return TRUE;
}

bool8 S9xContinueUpdate(int width, int height)
{
    EmulatorSetFrameSize(width, height);
    return TRUE;
}

void S9xSyncSpeed()
{
    // MTKView's preferredFramesPerSecond handles timing
}

void S9xProcessEvents(bool8 block)
{
    (void)block;
    // GCController is callback-driven
}

void S9xPutImage(int width, int height)
{
    (void)width;
    (void)height;
    // Metal draws from GFX.Screen directly in drawInMTKView
}

bool8 S9xOpenSoundDevice()
{
    return TRUE;
}

void S9xInitInputDevices()
{
    // GCController setup handled by InputManager
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        AppDelegate *delegate = [[AppDelegate alloc] init];
        app.delegate = delegate;

        [app run];
    }
    return 0;
}
