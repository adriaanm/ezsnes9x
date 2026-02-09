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
#include "config.h"
#include <set>

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
static bool g_rewindEnabled = true; // Can be disabled via --no-rewind or config
static bool g_rewindSetByCmdLine = false; // Track if --no-rewind was used
static S9xKeyboardMapping g_keyboardMapping;
static std::vector<S9xControllerMapping> g_controllerMappings;
static int g_keyboardPort = -1; // -1 = assign after controllers

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
// Shared rewind state handler
// ---------------------------------------------------------------------------

// Called by both controller and keyboard to manage rewind state
static void UpdateRewindState(bool rewindRequested) {
    if (!g_rewindEnabled)
        return;

    if (rewindRequested && !Emulator::IsRewinding()) {
        Emulator::RewindStartContinuous();
    } else if (!rewindRequested && Emulator::IsRewinding()) {
        Emulator::RewindStop();
    }
}

// ---------------------------------------------------------------------------
// InputManager — GCController mapping to SNES joypad bitmask
// ---------------------------------------------------------------------------

@interface InputManager : NSObject {
    BOOL portUsed[8]; // Track which ports are assigned
}
@property (nonatomic, strong) NSMutableArray<GCController *> *controllers;
@property (nonatomic, assign) int nextAutoPort; // Next available port for auto-assignment
@property (nonatomic, strong) NSMutableDictionary<NSNumber *, NSString *> *portNames; // port -> device name
- (void)setup;
- (void)teardown;
- (int)assignPortForController:(GCController *)controller;
- (BOOL)isPortUsed:(int)port;
- (void)setPortUsed:(int)port used:(BOOL)used;
@end

@implementation InputManager

- (BOOL)isPortUsed:(int)port {
    return (port >= 0 && port < 8) ? portUsed[port] : NO;
}

- (void)setPortUsed:(int)port used:(BOOL)used {
    if (port >= 0 && port < 8)
        portUsed[port] = used;
}

- (void)setup {
    self.controllers = [NSMutableArray array];
    self.portNames = [NSMutableDictionary dictionary];
    self.nextAutoPort = 0;
    for (int i = 0; i < 8; i++)
        portUsed[i] = NO;

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

- (int)assignPortForController:(GCController *)controller {
    // Check if this controller matches any configured mappings
    const char *vendorName = controller.vendorName ? controller.vendorName.UTF8String : "";

    for (const auto &mapping : g_controllerMappings) {
        if (mapping.matching.empty())
            continue;

        // Case-insensitive substring match
        std::string vendor(vendorName);
        std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);
        std::string match = mapping.matching;
        std::transform(match.begin(), match.end(), match.begin(), ::tolower);

        if (vendor.find(match) != std::string::npos) {
            if (mapping.port >= 0 && mapping.port < 8) {
                if ([self isPortUsed:mapping.port]) {
                    printf("[Input] ERROR: Port %d already in use, cannot assign '%s'\n", mapping.port, vendorName);
                    return -1; // Port conflict
                }
                printf("[Input] Matched '%s' -> port %d\n", vendorName, mapping.port);
                [self setPortUsed:mapping.port used:YES];
                self.portNames[@(mapping.port)] = [NSString stringWithUTF8String:vendorName];
                return mapping.port;
            }
        }
    }

    // Auto-assign to next available port
    while (self.nextAutoPort < 8 && [self isPortUsed:self.nextAutoPort]) {
        self.nextAutoPort++;
    }

    if (self.nextAutoPort < 8) {
        int port = self.nextAutoPort;
        [self setPortUsed:port used:YES];
        self.portNames[@(port)] = [NSString stringWithUTF8String:vendorName];
        self.nextAutoPort++;
        printf("[Input] Auto-assigned '%s' -> port %d\n", vendorName, port);
        return port;
    }

    printf("[Input] ERROR: No free ports available for '%s'\n", vendorName);
    return -1; // No ports available
}

- (void)configureController:(GCController *)controller {
    int padIndex = [self assignPortForController:controller];
    if (padIndex < 0) {
        printf("[Input] Skipping controller configuration due to port conflict\n");
        return;
    }

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
        UpdateRewindState(gamepad.leftTrigger.pressed);
    };
}

@end

// ---------------------------------------------------------------------------
// GameView — MTKView subclass that accepts first responder for keyboard input
// ---------------------------------------------------------------------------

static uint16_t g_keyboardButtons = 0;
static bool g_backspaceHeld = false;

// Default keyboard mapping (macOS keycodes)
static void InitDefaultKeyboardMapping() {
    g_keyboardMapping.button_to_keycode["up"]     = 126; // Arrow up
    g_keyboardMapping.button_to_keycode["down"]   = 125; // Arrow down
    g_keyboardMapping.button_to_keycode["left"]   = 123; // Arrow left
    g_keyboardMapping.button_to_keycode["right"]  = 124; // Arrow right
    g_keyboardMapping.button_to_keycode["a"]      = 2;   // D
    g_keyboardMapping.button_to_keycode["b"]      = 7;   // X
    g_keyboardMapping.button_to_keycode["x"]      = 13;  // W
    g_keyboardMapping.button_to_keycode["y"]      = 0;   // A
    g_keyboardMapping.button_to_keycode["l"]      = 12;  // Q
    g_keyboardMapping.button_to_keycode["r"]      = 35;  // P
    g_keyboardMapping.button_to_keycode["start"]  = 36;  // Enter
    g_keyboardMapping.button_to_keycode["select"] = 49;  // Space
}

static void HandleKeyEvent(NSEvent *event, BOOL pressed) {
    int keyCode = event.keyCode;
    uint16_t mask = 0;
    const char *button = nullptr;

    // Handle backspace (51) as rewind trigger
    if (keyCode == 51) {
        g_backspaceHeld = pressed;
        UpdateRewindState(pressed);
        if (g_debug)
            printf("[Input] Backspace %s -> rewind %s\n", pressed ? "DOWN" : "UP", pressed ? "start" : "stop");
        return;
    }

    // Map keycode to SNES button
    for (const auto &pair : g_keyboardMapping.button_to_keycode) {
        if (pair.second == keyCode) {
            button = pair.first.c_str();
            if (pair.first == "up")         mask = SNES_UP_MASK;
            else if (pair.first == "down")  mask = SNES_DOWN_MASK;
            else if (pair.first == "left")  mask = SNES_LEFT_MASK;
            else if (pair.first == "right") mask = SNES_RIGHT_MASK;
            else if (pair.first == "a")     mask = SNES_A_MASK;
            else if (pair.first == "b")     mask = SNES_B_MASK;
            else if (pair.first == "x")     mask = SNES_X_MASK;
            else if (pair.first == "y")     mask = SNES_Y_MASK;
            else if (pair.first == "l")     mask = SNES_TL_MASK;
            else if (pair.first == "r")     mask = SNES_TR_MASK;
            else if (pair.first == "start") mask = SNES_START_MASK;
            else if (pair.first == "select") mask = SNES_SELECT_MASK;
            break;
        }
    }

    if (mask == 0) {
        if (g_debug)
            printf("[Input] Unmapped key code: %d %s\n", keyCode, pressed ? "down" : "up");
        return;
    }

    if (pressed)
        g_keyboardButtons |= mask;
    else
        g_keyboardButtons &= ~mask;

    if (g_debug)
        printf("[Input] Key %s (%d) %s -> port %d buttons=0x%04x\n", button, keyCode, pressed ? "DOWN" : "UP", g_keyboardPort, g_keyboardButtons);

    if (g_keyboardPort >= 0 && g_keyboardPort < 8)
        Emulator::SetButtonState(g_keyboardPort, g_keyboardButtons);
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

    // Set vsync-enabled frame rate to match SNES timing. MTKView with delegate
    // uses the display's refresh rate as the master clock (vsync throttle strategy).
    // The small difference between display rate (typically 60Hz) and SNES rate
    // (60.0988Hz NTSC) is handled by DynamicRateControl audio drift correction.
    mtkView.preferredFramesPerSecond = Settings.PAL ? 50 : 60;
    mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;

    // Ensure vsync is enabled by using timer-based redraw (delegate-driven)
    // rather than manual setNeedsDisplay calls. This is equivalent to
    // CAMetalLayer.displaySyncEnabled = YES.
    mtkView.enableSetNeedsDisplay = NO;

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
        } else if ([args[i] isEqualToString:@"--no-rewind"]) {
            g_rewindEnabled = false;
            g_rewindSetByCmdLine = true;
            printf("[Config] Rewind disabled via command line\n");
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

    // Initialize emulator
    if (!Emulator::Init(configPath.c_str())) {
        NSLog(@"Failed to initialize emulator");
        [NSApp terminate:nil];
        return;
    }

    // Load config and validate
    InitDefaultKeyboardMapping();
    const S9xConfig *config = Emulator::GetConfig();
    if (config) {
        // Validate controller mappings for duplicate ports
        std::set<int> usedPorts;
        for (const auto &mapping : config->controllers) {
            if (mapping.port >= 0 && mapping.port < 8) {
                if (usedPorts.count(mapping.port)) {
                    printf("[Input] ERROR: Config has duplicate controller assignments to port %d\n", mapping.port);
                } else {
                    usedPorts.insert(mapping.port);
                }
            }
        }

        // Check keyboard port doesn't conflict
        if (config->keyboard.port >= 0 && config->keyboard.port < 8) {
            if (usedPorts.count(config->keyboard.port)) {
                printf("[Input] ERROR: Config assigns keyboard to port %d which is also assigned to a controller\n", config->keyboard.port);
            }
        }

        // Load controller mappings
        g_controllerMappings = config->controllers;

        // Load keyboard mapping
        g_keyboardPort = config->keyboard.port;
        if (!config->keyboard.button_to_keycode.empty()) {
            g_keyboardMapping = config->keyboard;
            if (g_debug)
                printf("[Input] Loaded custom keyboard mapping from config\n");
        }

        // Load rewind setting (command line takes precedence)
        if (!g_rewindSetByCmdLine) {
            g_rewindEnabled = config->rewind_enabled;
            if (!g_rewindEnabled)
                printf("[Config] Rewind disabled via config file\n");
        }
    }

    // Apply final rewind setting to emulator (must be before LoadROM)
    Emulator::SetRewindEnabled(g_rewindEnabled);

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

    // Wait for controller discovery (GCController discovery can be delayed)
    // Give it 100ms to discover already-connected controllers
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        // Assign keyboard port after controllers (once at startup, never changes)
        if (g_keyboardPort == -1) {
            // Auto-assign to first free port after controllers
            g_keyboardPort = 0;
            while (g_keyboardPort < 8 && [self.input isPortUsed:g_keyboardPort]) {
                g_keyboardPort++;
            }
            if (g_keyboardPort >= 8) {
                g_keyboardPort = -1; // No free port
                printf("[Input] WARNING: No free port for keyboard\n");
            }
        } else if (g_keyboardPort >= 0 && g_keyboardPort < 8) {
            // User specified a port - check if it's available
            if ([self.input isPortUsed:g_keyboardPort]) {
                printf("[Input] ERROR: Keyboard port %d already in use by controller\n", g_keyboardPort);
                g_keyboardPort = -1;
            }
        }

        // Mark keyboard port as used and add to port names
        if (g_keyboardPort >= 0 && g_keyboardPort < 8) {
            [self.input setPortUsed:g_keyboardPort used:YES];
            self.input.portNames[@(g_keyboardPort)] = @"Keyboard";
            printf("[Input] Keyboard assigned to port %d\n", g_keyboardPort);
        }

        // Print final port mapping summary
        printf("\n[Input] Final port mapping:\n");
        for (int port = 0; port < 8; port++) {
            NSString *name = self.input.portNames[@(port)];
            if (name) {
                printf("  Port %d: %s\n", port, name.UTF8String);
            }
        }
        printf("\n");
    });

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
