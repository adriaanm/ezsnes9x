#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface MetalRenderer : NSObject <MTKViewDelegate>

- (instancetype)initWithMTKView:(MTKView *)view;

/// Upload a new frame from the emulator. Call before drawing.
/// @param buffer  RGB555 pixel data (pitch = MAX_SNES_WIDTH pixels)
/// @param width   Actual frame width in pixels
/// @param height  Actual frame height in pixels
- (void)updateFrameWithBuffer:(const uint16_t *)buffer width:(int)width height:(int)height;

/// Show/hide the rewind progress bar overlay.
- (void)setRewindProgress:(float)progress visible:(BOOL)visible;

@end

NS_ASSUME_NONNULL_END
