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
)";

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static NSString *g_romPath = nil;
static bool g_running = false;

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
    for (GCController *c in [GCController controllers]) {
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
    [self configureController:c];
    [self.controllers addObject:c];
}

- (void)controllerDidDisconnect:(NSNotification *)note {
    GCController *c = note.object;
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

        Emulator::SetButtonState(padIndex, buttons);

        // L2/ZL trigger for rewind
        bool rewindHeld = gamepad.leftTrigger.pressed;
        if (rewindHeld && !Emulator::IsRewinding()) {
            Emulator::RewindStart();
        } else if (rewindHeld && Emulator::IsRewinding()) {
            Emulator::RewindStepBack();
        } else if (!rewindHeld && Emulator::IsRewinding()) {
            Emulator::RewindEnd();
        }
    };
}

@end

// ---------------------------------------------------------------------------
// GameViewController — Metal rendering
// ---------------------------------------------------------------------------

@interface GameViewController : NSViewController <MTKViewDelegate>
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLRenderPipelineState> pipelineState;
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

    // Create texture for SNES framebuffer (RGB555 = A1BGR5 on macOS)
    MTLTextureDescriptor *texDesc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatA1BGR5Unorm
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

    // Run one frame of emulation
    Emulator::RunFrame();

    // Upload framebuffer to texture
    int w = Emulator::GetFrameWidth();
    int h = Emulator::GetFrameHeight();
    const uint16_t *fb = Emulator::GetFrameBuffer();

    if (fb && w > 0 && h > 0) {
        MTLRegion region = MTLRegionMake2D(0, 0, w, h);
        [self.texture replaceRegion:region
                        mipmapLevel:0
                          withBytes:fb
                        bytesPerRow:MAX_SNES_WIDTH * sizeof(uint16_t)];
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

    // Determine ROM path
    NSString *romPath = g_romPath;
    if (!romPath) {
        // Try command-line args
        NSArray *args = [[NSProcessInfo processInfo] arguments];
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
    NSArray *args = [[NSProcessInfo processInfo] arguments];
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

    // Create Metal view
    MTKView *mtkView = [[MTKView alloc] initWithFrame:frame device:MTLCreateSystemDefaultDevice()];
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
