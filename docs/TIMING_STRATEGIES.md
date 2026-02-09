# Snes9x Timing for Modern Frontends

This document describes the recommended timing strategy for modern GPU-accelerated Snes9x frontends targeting fast hardware.

## Recommended Approach: Vsync Throttle

For modern systems with powerful GPUs and CPUs, the smoothest experience comes from **vsync-locked frame pacing** with optional frame skipping.

### How It Works

1. **Vsync provides the clock** - The swap/present operation blocks at the display's refresh rate
2. **Throttle prevents busy-wait** - Sleep before swap to avoid burning CPU waiting for vsync
3. **Optional frame skip** - Simple switch to skip rendering when running behind

### Frame Rate Constants

```cpp
// From snes9x.h
#define NTSC_PROGRESSIVE_FRAME_RATE 60.09881389744051
#define PAL_PROGRESSIVE_FRAME_RATE  50.006977968
```

## Throttle Class

A minimal, portable throttle implementation using C++11 `<chrono>`:

```cpp
#include <chrono>
#include <thread>

class FrameThrottle {
public:
    void set_frame_rate(double fps) {
        frame_duration_ns_ = static_cast<long long>(1e9 / fps);
    }

    // Call this AFTER swap/present to advance to next frame time
    void advance() {
        next_frame_time_ += std::chrono::nanoseconds(frame_duration_ns_);
    }

    // Call this BEFORE swap/present to pace the frame rate
    void wait_for_frame() {
        auto now = std::chrono::steady_clock::now();
        auto time_until_next = std::chrono::duration_cast<std::chrono::nanoseconds>(
            next_frame_time_ - now
        ).count();

        // Reset if we're way behind (prevents spiral of death)
        if (time_until_next < -frame_duration_ns_) {
            reset();
            return;
        }

        // Sleep if we're ahead (leave 1ms margin for spin-wait precision)
        if (time_until_next > 1'000'000LL) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(
                time_until_next - 1'000'000LL
            ));
        }

        // Spin-wait for final precision (yield to be CPU-friendly)
        now = std::chrono::steady_clock::now();
        while (now < next_frame_time_) {
            std::this_thread::yield();
            now = std::chrono::steady_clock::now();
        }
    }

    // Combined wait + advance - call this BEFORE swap
    void wait_for_frame_and_rebase_time() {
        wait_for_frame();
        advance();
    }

    // Returns frames behind (positive) or ahead (negative)
    double get_late_frames() const {
        auto now = std::chrono::steady_clock::now();
        auto late_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - next_frame_time_
        ).count();
        return static_cast<double>(late_ns) / frame_duration_ns_;
    }

    void reset() {
        next_frame_time_ = std::chrono::steady_clock::now();
    }

private:
    long long frame_duration_ns_ = 16'666'667LL;  // ~60fps default
    std::chrono::time_point<std::chrono::steady_clock> next_frame_time_;
};
```

## Display Driver Integration

### Basic Usage (No Frame Skip)

```cpp
class DisplayDriver {
    FrameThrottle throttle_;

    void update(uint16_t* buffer, int width, int height) {
        // 1. Render frame to texture
        render_to_texture(buffer, width, height);

        // 2. Pace frame rate before swap
        throttle_.set_frame_rate(Settings.PAL ? PAL_PROGRESSIVE_FRAME_RATE
                                              : NTSC_PROGRESSIVE_FRAME_RATE);
        throttle_.wait_for_frame_and_rebase_time();

        // 3. Present (vsync blocks here if enabled)
        swap_buffers();
    }
};
```

### With Optional Frame Skip

```cpp
class DisplayDriver {
    FrameThrottle throttle_;
    bool frame_skip_enabled_ = false;

    void update(uint16_t* buffer, int width, int height) {
        throttle_.set_frame_rate(Settings.PAL ? PAL_PROGRESSIVE_FRAME_RATE
                                              : NTSC_PROGRESSIVE_FRAME_RATE);

        // Check if we should skip this frame
        if (frame_skip_enabled_) {
            double late = throttle_.get_late_frames();
            if (late >= 1.0) {
                // Skip rendering this frame, but still advance timing
                throttle_.advance();
                swap_buffers();  // Still present previous frame
                return;
            }
        }

        // Render and present
        render_to_texture(buffer, width, height);
        throttle_.wait_for_frame_and_rebase_time();
        swap_buffers();
    }
};
```

## Port Implementation

### S9xSyncSpeed()

For vsync throttle, keep `S9xSyncSpeed()` minimal:

```cpp
void S9xSyncSpeed(void) {
    // Timing handled by display driver at swap time
    // No-op here
}
```

### Audio Settings

Use `DynamicRateControl` for audio sync (not `SoundSync`):

```cpp
Settings.SoundSync = false;
Settings.DynamicRateControl = true;
Settings.DynamicRateLimit = 5;  // 5% max adjustment

// Buffer size: 50-100ms recommended
int buffer_ms = 80;
S9xInitSound(buffer_ms);
```

### Vsync Setup

**Vulkan:**
```cpp
vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;  // Vsync on
swapchainCreateInfo.setPresentMode(present_mode);
```

**OpenGL:**
```cpp
#ifdef _WIN32
    wglSwapIntervalEXT(1);
#elif defined(__APPLE__)
    [context setValues:[NSNumber numberWithInt:1] forParameter:NSOpenGLCPSwapInterval];
#else
    glXSwapInterval(display, 1);
#endif
```

**Metal (macOS):**
```cpp
drawableLayer->displaySyncEnabled = YES;  // Default is YES
```

## Why This Works

1. **Vsync is the master clock** - The display's refresh rate provides accurate 60Hz/50Hz timing
2. **No audio-driven stalling** - Emulation never waits for audio, preventing stutter
3. **Drift correction** - `advance()` adds fixed increments, automatically correcting timing errors
4. **CPU-friendly** - Sleep before spin-wait minimizes CPU usage
5. **Smooth video** - Every frame is displayed (unless frame skip is enabled)

## Key Insight

`advance()` adds to the base time instead of resetting it:

```cpp
// Correct: prevents drift
void advance() {
    next_frame_time_ += frame_duration_ns_;
}

// Wrong: drift accumulates
void advance_wrong() {
    next_frame_time_ = std::chrono::steady_clock::now();  // Don't do this
}
```

If one frame takes 17ms instead of 16.67ms, the next frame's target is still exactly 16.67ms after the previous target. Small timing errors automatically cancel out rather than accumulating.

## Summary

For modern fast hardware:

| Setting | Value |
|---------|-------|
| Timing method | Vsync throttle |
| Frame skip | Off (smoothest) or simple switch |
| Audio sync | `DynamicRateControl = true` |
| SoundSync | `false` |
| Buffer size | 50-100ms |

This provides the smoothest audio and video experience on systems with plenty of CPU/GPU headroom.
