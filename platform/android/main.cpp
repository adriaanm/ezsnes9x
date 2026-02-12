/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
               This file is licensed under the Snes9x License.
  For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

// Android frontend — single-file NativeActivity + OpenGL ES 3.0 + Oboe app.
// Targets ARM64 gaming handhelds (Retroid Pocket, Anbernic) with physical gamepads.
// ROM path from intent (file manager / launcher) or fallback path.

#include <android/log.h>
#include <android/input.h>
#include <android_native_app_glue.h>
#include <jni.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <oboe/Oboe.h>

#include "emulator.h"
#include "snes9x.h"
#include "gfx.h"
#include "apu/apu.h"

#include <cstring>
#include <string>
#include <cmath>
#include <chrono>
#include <thread>

#define LOG_TAG "snes9x"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

extern void EmulatorSetFrameSize(int width, int height);

// ---------------------------------------------------------------------------
// Frame Throttle
// ---------------------------------------------------------------------------
// Vsync-locked frame pacing using C++11 <chrono> for smooth emulation timing.

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
    // Useful for detecting when we're running too slowly
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

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static struct android_app *g_app = nullptr;
static bool g_running   = false;
static bool g_has_focus = false;
static bool g_rewinding = false;
static bool g_paused    = false;

// EGL state
static EGLDisplay g_egl_display = EGL_NO_DISPLAY;
static EGLSurface g_egl_surface = EGL_NO_SURFACE;
static EGLContext  g_egl_context = EGL_NO_CONTEXT;
static ANativeWindow *g_native_window = nullptr;  // Saved for EGL recreation

// GL state
static GLuint g_texture       = 0;
static GLuint g_program       = 0;
static GLuint g_vao           = 0;
static GLuint g_color_program = 0;  // For overlays
static GLuint g_color_vbo     = 0;  // Vertex buffer for overlay quads
static int    g_surface_width  = 0;
static int    g_surface_height = 0;

static FrameThrottle g_frame_throttle;

// Frame timing diagnostics
static int g_frame_count = 0;
static int g_late_frame_count = 0;

// ---------------------------------------------------------------------------
// OpenGL ES shaders
// ---------------------------------------------------------------------------

static const char *kVertexShader = R"(#version 300 es
out vec2 vTexCoord;
uniform vec2 uTexScale;
void main() {
    // Fullscreen quad from vertex ID (0-5)
    vec2 pos;
    vec2 uv;
    int id = gl_VertexID;
    if (id == 0)      { pos = vec2(-1, -1); uv = vec2(0, 1); }
    else if (id == 1) { pos = vec2( 1, -1); uv = vec2(1, 1); }
    else if (id == 2) { pos = vec2(-1,  1); uv = vec2(0, 0); }
    else if (id == 3) { pos = vec2(-1,  1); uv = vec2(0, 0); }
    else if (id == 4) { pos = vec2( 1, -1); uv = vec2(1, 1); }
    else              { pos = vec2( 1,  1); uv = vec2(1, 0); }
    gl_Position = vec4(pos, 0.0, 1.0);
    vTexCoord = uv * uTexScale;
}
)";

static const char *kFragmentShader = R"(#version 300 es
precision mediump float;
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uTexture;
void main() {
    fragColor = texture(uTexture, vTexCoord);
}
)";

// Simple 2D color shader for overlays (pause indicator, rewind bar)
static const char *kColorVertexShader = R"(#version 300 es
in vec2 aPosition;
uniform vec2 uResolution;
void main() {
    // Convert pixel coords to clip space (-1 to 1)
    vec2 clip = (aPosition / uResolution) * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
}
)";

static const char *kColorFragmentShader = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
out vec4 fragColor;
void main() {
    fragColor = uColor;
}
)";

// ---------------------------------------------------------------------------
// GL helpers
// ---------------------------------------------------------------------------

static GLuint CompileShader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(shader, sizeof(buf), nullptr, buf);
        LOGE("Shader compile error: %s", buf);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static bool InitGL()
{
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vs || !fs) return false;

    g_program = glCreateProgram();
    glAttachShader(g_program, vs);
    glAttachShader(g_program, fs);
    glLinkProgram(g_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(g_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(g_program, sizeof(buf), nullptr, buf);
        LOGE("Program link error: %s", buf);
        return false;
    }

    // Create color overlay program
    vs = CompileShader(GL_VERTEX_SHADER, kColorVertexShader);
    fs = CompileShader(GL_FRAGMENT_SHADER, kColorFragmentShader);
    if (!vs || !fs) return false;

    g_color_program = glCreateProgram();
    glAttachShader(g_color_program, vs);
    glAttachShader(g_color_program, fs);
    glLinkProgram(g_color_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGetProgramiv(g_color_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(g_color_program, sizeof(buf), nullptr, buf);
        LOGE("Color program link error: %s", buf);
        return false;
    }

    // Create VAO (required for ES 3.0, even with no vertex attribs)
    glGenVertexArrays(1, &g_vao);

    // Create VBO for overlay quads
    glGenBuffers(1, &g_color_vbo);

    // Create texture for SNES framebuffer (RGB565)
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, MAX_SNES_WIDTH, MAX_SNES_HEIGHT,
                 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, nullptr);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    LOGI("GL initialized: %s", glGetString(GL_RENDERER));
    return true;
}

static void TeardownGL()
{
    if (g_texture)       { glDeleteTextures(1, &g_texture); g_texture = 0; }
    if (g_vao)           { glDeleteVertexArrays(1, &g_vao); g_vao = 0; }
    if (g_color_vbo)     { glDeleteBuffers(1, &g_color_vbo); g_color_vbo = 0; }
    if (g_program)       { glDeleteProgram(g_program); g_program = 0; }
    if (g_color_program) { glDeleteProgram(g_color_program); g_color_program = 0; }
}

// ---------------------------------------------------------------------------
// EGL setup/teardown
// ---------------------------------------------------------------------------

static bool InitEGL(ANativeWindow *window)
{
    // If display already initialized, just recreate surface/context
    bool displayInitialized = (g_egl_display != EGL_NO_DISPLAY);

    if (!displayInitialized) {
        g_native_window = window;  // Save for potential recreation
        g_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(g_egl_display, nullptr, nullptr);
    }

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(g_egl_display, configAttribs, &config, 1, &numConfigs);
    if (numConfigs == 0) {
        LOGE("eglChooseConfig failed");
        return false;
    }

    // Match native window format to EGL config
    EGLint format;
    eglGetConfigAttrib(g_egl_display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(window, 0, 0, format);

    g_egl_surface = eglCreateWindowSurface(g_egl_display, config, window, nullptr);

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    g_egl_context = eglCreateContext(g_egl_display, config, EGL_NO_CONTEXT, contextAttribs);

    eglMakeCurrent(g_egl_display, g_egl_surface, g_egl_surface, g_egl_context);

    // Enable vsync (swap interval 1). This blocks eglSwapBuffers() at the display's
    // native refresh rate. We assume 60Hz for typical Android gaming handhelds
    // (Retroid Pocket, Anbernic). The small difference between 60.0Hz (display)
    // and 60.1Hz (SNES NTSC) is handled by DynamicRateControl audio drift correction.
    eglSwapInterval(g_egl_display, 1);

    eglQuerySurface(g_egl_display, g_egl_surface, EGL_WIDTH, &g_surface_width);
    eglQuerySurface(g_egl_display, g_egl_surface, EGL_HEIGHT, &g_surface_height);

    LOGI("EGL %s: %dx%d", displayInitialized ? "recreated" : "initialized",
         g_surface_width, g_surface_height);
    return true;
}

// Release EGL surface/context when app goes to background (saves GPU power)
// Display stays initialized for quick recreation via InitEGL()
static void ReleaseEGLForBackground()
{
    if (g_egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_egl_context != EGL_NO_CONTEXT)
            eglDestroyContext(g_egl_display, g_egl_context);
        if (g_egl_surface != EGL_NO_SURFACE)
            eglDestroySurface(g_egl_display, g_egl_surface);
    }
    g_egl_surface = EGL_NO_SURFACE;
    g_egl_context = EGL_NO_CONTEXT;
    LOGI("EGL released for background (GPU power saved)");
}

static void TeardownEGL()
{
    if (g_egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_egl_context != EGL_NO_CONTEXT)
            eglDestroyContext(g_egl_display, g_egl_context);
        if (g_egl_surface != EGL_NO_SURFACE)
            eglDestroySurface(g_egl_display, g_egl_surface);
        eglTerminate(g_egl_display);
    }
    g_egl_display = EGL_NO_DISPLAY;
    g_egl_surface = EGL_NO_SURFACE;
    g_egl_context = EGL_NO_CONTEXT;
    g_native_window = nullptr;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Overlay drawing
// ---------------------------------------------------------------------------

struct Vertex {
    float x, y;
};

// Draw a solid color quad (pixel coordinates)
static void DrawQuad(float x, float y, float w, float h, float r, float g, float b, float a)
{
    Vertex verts[6] = {
        { x,     y },
        { x + w, y },
        { x,     y + h },
        { x,     y + h },
        { x + w, y },
        { x + w, y + h }
    };

    glBindBuffer(GL_ARRAY_BUFFER, g_color_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

    glUseProgram(g_color_program);
    GLint posLoc = glGetAttribLocation(g_color_program, "aPosition");
    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glUniform2f(glGetUniformLocation(g_color_program, "uResolution"),
                (float)g_surface_width, (float)g_surface_height);
    glUniform4f(glGetUniformLocation(g_color_program, "uColor"), r, g, b, a);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Clean up: disable vertex attribute array
    glDisableVertexAttribArray(posLoc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Draw pause indicator (two vertical bars)
static void DrawPauseIndicator()
{
    float barWidth = 20.0f;
    float barHeight = 60.0f;
    float gap = 15.0f;
    float centerX = g_surface_width / 2.0f - barWidth - gap / 2.0f;
    float centerY = g_surface_height / 2.0f - barHeight / 2.0f;

    // Semi-transparent dark background
    DrawQuad(centerX - 10, centerY - 10, (barWidth * 2 + gap + 20), barHeight + 20,
             0.0f, 0.0f, 0.0f, 0.5f);

    // Two vertical bars (white)
    DrawQuad(centerX, centerY, barWidth, barHeight, 1.0f, 1.0f, 1.0f, 0.9f);
    DrawQuad(centerX + barWidth + gap, centerY, barWidth, barHeight, 1.0f, 1.0f, 1.0f, 0.9f);
}

// Draw rewind progress bar at bottom (<<<<< style)
static void DrawRewindBar()
{
    int depth = Emulator::GetRewindBufferDepth();
    int pos = Emulator::GetRewindPosition();
    if (depth <= 0 || pos < 0) return;

    float barHeight = 20.0f;
    float margin = 20.0f;
    float barY = g_surface_height - barHeight - margin;
    float barMaxWidth = g_surface_width * 0.8f;
    float barX = (g_surface_width - barMaxWidth) / 2.0f;

    // Calculate progress (0.0 = oldest, 1.0 = newest)
    float progress = (float)pos / (float)(depth - 1);
    float filledWidth = barMaxWidth * progress;

    // Background bar (dark gray)
    DrawQuad(barX, barY, barMaxWidth, barHeight, 0.2f, 0.2f, 0.2f, 0.8f);

    // Filled portion (cyan)
    if (filledWidth > 0) {
        DrawQuad(barX, barY, filledWidth, barHeight, 0.0f, 1.0f, 1.0f, 0.9f);
    }

    // Draw arrow indicators (<<<<<) getting shorter as we rewind
    int numArrows = 8;
    float arrowWidth = 10.0f;
    float arrowGap = barMaxWidth / (float)(numArrows + 1);

    for (int i = 0; i < numArrows; i++) {
        float arrowX = barX + arrowGap * (i + 1) - arrowWidth / 2.0f;
        // Make arrows fade out near the current position
        float arrowProgress = (float)i / (float)numArrows;
        float alpha = (arrowProgress < progress) ? 0.3f : 0.8f;

        // Draw left arrow shape as two triangles
        float ay = barY + barHeight / 2.0f - 3.0f;
        float ah = 6.0f;
        DrawQuad(arrowX, ay, arrowWidth * 0.6f, ah, 1.0f, 1.0f, 1.0f, alpha);
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

static void RenderFrame()
{
    if (g_egl_display == EGL_NO_DISPLAY) return;

    int w = Emulator::GetFrameWidth();
    int h = Emulator::GetFrameHeight();
    const uint16_t *fb = Emulator::GetFrameBuffer();

    if (fb && w > 0 && h > 0) {
        glBindTexture(GL_TEXTURE_2D, g_texture);
        // Framebuffer pitch is MAX_SNES_WIDTH, not the actual frame width
        glPixelStorei(GL_UNPACK_ROW_LENGTH, MAX_SNES_WIDTH);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, fb);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    // Calculate viewport for 4:3 aspect ratio
    float targetAspect = 4.0f / 3.0f;
    float viewAspect = (float)g_surface_width / (float)g_surface_height;

    int vpX = 0, vpY = 0, vpW = g_surface_width, vpH = g_surface_height;
    if (viewAspect > targetAspect) {
        vpW = (int)(g_surface_height * targetAspect);
        vpX = (g_surface_width - vpW) / 2;
    } else {
        vpH = (int)(g_surface_width / targetAspect);
        vpY = (g_surface_height - vpH) / 2;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(vpX, vpY, vpW, vpH);

    glUseProgram(g_program);
    glBindVertexArray(g_vao);

    // Set texture scale uniforms
    float scaleX = (w > 0) ? (float)w / (float)MAX_SNES_WIDTH  : 1.0f;
    float scaleY = (h > 0) ? (float)h / (float)MAX_SNES_HEIGHT : 1.0f;
    glUniform2f(glGetUniformLocation(g_program, "uTexScale"), scaleX, scaleY);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glUniform1i(glGetUniformLocation(g_program, "uTexture"), 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Draw overlays on top of full screen (reset viewport first)
    glViewport(0, 0, g_surface_width, g_surface_height);

    if (g_paused) {
        DrawPauseIndicator();
    } else if (Emulator::IsRewinding()) {
        DrawRewindBar();
    }

    // Reset GL state for next frame
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(g_vao);

    // Pace frame rate before swap. We target the SNES frame rate (~60.1Hz NTSC,
    // ~50.0Hz PAL) rather than the display refresh rate. On a 60Hz display (assumed
    // for typical Android handhelds), eglSwapBuffers below will block at 60Hz while
    // this throttle prevents busy-waiting. The small rate difference is handled by
    // DynamicRateControl audio drift correction.
    g_frame_throttle.set_frame_rate(Emulator::IsPAL()
        ? PAL_PROGRESSIVE_FRAME_RATE
        : NTSC_PROGRESSIVE_FRAME_RATE);
    g_frame_throttle.wait_for_frame_and_rebase_time();

    // Check if we're running behind (for diagnostics)
    g_frame_count++;
    double late = g_frame_throttle.get_late_frames();
    if (late >= 1.0) {
        g_late_frame_count++;
        // Log every ~5 seconds (at 60fps) if we're consistently late
        if (g_late_frame_count % 300 == 0) {
            LOGI("Frame timing: running %.2f frames behind target (%d late frames of %d total)",
                 late, g_late_frame_count, g_frame_count);
        }
    } else if (g_frame_count % 1800 == 0) {
        // Log status every ~30 seconds when running smoothly
        LOGI("Frame timing: on schedule (%d frames, %d late)", g_frame_count, g_late_frame_count);
    }

    eglSwapBuffers(g_egl_display, g_egl_surface);
}

// ---------------------------------------------------------------------------
// Audio (Oboe)
// ---------------------------------------------------------------------------

class AudioCallback : public oboe::AudioStreamDataCallback {
public:
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *stream,
            void *audioData,
            int32_t numFrames) override
    {
        (void)stream;
        // numFrames = stereo frame count; S9xMixSamples wants sample count (frames * 2)
        S9xMixSamples((uint8 *)audioData, numFrames * 2);
        return oboe::DataCallbackResult::Continue;
    }
};

static AudioCallback       g_audio_callback;
static oboe::AudioStream  *g_audio_stream = nullptr;

static void StartAudio()
{
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::I16)
           ->setChannelCount(oboe::ChannelCount::Stereo)
           ->setSampleRate(Settings.SoundPlaybackRate ? Settings.SoundPlaybackRate : 32040)
           ->setDataCallback(&g_audio_callback);

    oboe::Result result = builder.openStream(&g_audio_stream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open audio stream: %s", oboe::convertToText(result));
        return;
    }

    result = g_audio_stream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start audio stream: %s", oboe::convertToText(result));
    }

    LOGI("Audio started: %d Hz, %d ch",
         g_audio_stream->getSampleRate(),
         g_audio_stream->getChannelCount());
}

static void StopAudio()
{
    if (g_audio_stream) {
        g_audio_stream->requestStop();
        g_audio_stream->close();
        delete g_audio_stream;
        g_audio_stream = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

static uint16_t g_pad_buttons = 0;

// Touch gesture tracking for pause (two-finger tap) and rewind (two-finger swipe)
static struct TouchState {
    int activePointerCount = 0;
    float pointerX[2] = {0.0f, 0.0f};
    float pointerY[2] = {0.0f, 0.0f};
    float initialX[2] = {0.0f, 0.0f};  // Initial positions for tap detection
    float initialY[2] = {0.0f, 0.0f};
    float swipeStartX = 0.0f;
    bool twoFingerTapDetected = false;
    int64_t twoFingerTapTime = 0;
    bool rewinding = false;
} g_touch;

static void UpdateRewindState(bool rewindRequested)
{
    if (rewindRequested && !Emulator::IsRewinding()) {
        g_rewinding = true;
        Emulator::RewindStartContinuous();
    } else if (!rewindRequested && Emulator::IsRewinding()) {
        g_rewinding = false;
        Emulator::RewindStop();
    }
}

static int32_t HandleInputEvent(struct android_app *app, AInputEvent *event)
{
    (void)app;
    int32_t type = AInputEvent_getType(event);

    if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t keyCode = AKeyEvent_getKeyCode(event);
        int32_t action  = AKeyEvent_getAction(event);
        bool pressed = (action == AKEY_EVENT_ACTION_DOWN);

        uint16_t mask = 0;
        bool is_rewind = false;

        switch (keyCode) {
            case AKEYCODE_DPAD_UP:      mask = SNES_UP_MASK;     break;
            case AKEYCODE_DPAD_DOWN:    mask = SNES_DOWN_MASK;   break;
            case AKEYCODE_DPAD_LEFT:    mask = SNES_LEFT_MASK;   break;
            case AKEYCODE_DPAD_RIGHT:   mask = SNES_RIGHT_MASK;  break;
            case AKEYCODE_BUTTON_A:     mask = SNES_A_MASK;      break;
            case AKEYCODE_BUTTON_B:     mask = SNES_B_MASK;      break;
            case AKEYCODE_BUTTON_X:     mask = SNES_X_MASK;      break;
            case AKEYCODE_BUTTON_Y:     mask = SNES_Y_MASK;      break;
            case AKEYCODE_BUTTON_L1:    mask = SNES_TL_MASK;     break;
            case AKEYCODE_BUTTON_R1:    mask = SNES_TR_MASK;     break;
            case AKEYCODE_BUTTON_START: mask = SNES_START_MASK;  break;
            case AKEYCODE_BUTTON_SELECT: mask = SNES_SELECT_MASK; break;
            case AKEYCODE_BUTTON_L2:    is_rewind = true;        break;
            default: return 0;
        }

        if (is_rewind) {
            UpdateRewindState(pressed);
            return 1;
        }

        if (pressed)
            g_pad_buttons |= mask;
        else
            g_pad_buttons &= ~mask;

        Emulator::SetButtonState(0, g_pad_buttons);
        return 1;
    }

    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t source = AInputEvent_getSource(event);

        // Handle touchscreen events (two-finger tap to pause, two-finger swipe to rewind)
        if (source & AINPUT_SOURCE_TOUCHSCREEN) {
            int32_t action = AMotionEvent_getAction(event);
            int32_t actionCode = action & AMOTION_EVENT_ACTION_MASK;
            int32_t pointerIndex = (action >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT) & 0xff;
            int32_t pointerCount = AMotionEvent_getPointerCount(event);
            int64_t eventTime = AMotionEvent_getEventTime(event);

            // Constants for gesture detection
            static constexpr int64_t TWO_FINGER_TAP_TIMEOUT_NS = 300000000;  // 300 ms
            static constexpr float TWO_FINGER_TAP_DISTANCE = 50.0f; // max movement for tap (pixels)
            static constexpr float SWIPE_THRESHOLD = 100.0f; // min swipe distance to activate rewind (pixels)
            static constexpr float SWIPE_CANCEL_THRESHOLD = 30.0f; // min swipe distance to cancel rewind (pixels)

            switch (actionCode) {
                case AMOTION_EVENT_ACTION_DOWN:
                case AMOTION_EVENT_ACTION_POINTER_DOWN: {
                    g_touch.activePointerCount = pointerCount;
                    if (pointerCount <= 2) {
                        // Track current and initial pointer positions
                        for (int i = 0; i < pointerCount; i++) {
                            g_touch.pointerX[i] = AMotionEvent_getX(event, i);
                            g_touch.pointerY[i] = AMotionEvent_getY(event, i);
                            g_touch.initialX[i] = g_touch.pointerX[i];
                            g_touch.initialY[i] = g_touch.pointerY[i];
                        }
                        // Initialize gesture tracking when second finger goes down
                        if (pointerCount == 2) {
                            // Use average X position as swipe reference point
                            g_touch.swipeStartX = (g_touch.pointerX[0] + g_touch.pointerX[1]) / 2.0f;
                            g_touch.twoFingerTapDetected = true;
                            g_touch.twoFingerTapTime = eventTime;
                            g_touch.rewinding = false;
                        }
                    }
                    break;
                }

                case AMOTION_EVENT_ACTION_MOVE: {
                    if (g_touch.activePointerCount == 2) {
                        // Update pointer positions
                        for (int i = 0; i < 2; i++) {
                            g_touch.pointerX[i] = AMotionEvent_getX(event, i);
                            g_touch.pointerY[i] = AMotionEvent_getY(event, i);
                        }

                        // Check for swipe gesture (right to left)
                        float avgX = (g_touch.pointerX[0] + g_touch.pointerX[1]) / 2.0f;
                        float swipeDelta = g_touch.swipeStartX - avgX;

                        if (swipeDelta > SWIPE_THRESHOLD) {
                            // Swiping/holding left - activate/maintain rewind
                            if (!g_touch.rewinding) {
                                g_touch.rewinding = true;
                                UpdateRewindState(true);
                                LOGI("Rewind activated by two-finger swipe left (delta: %.1f)", swipeDelta);
                                g_touch.twoFingerTapDetected = false; // Cancel tap detection
                            }
                            // Keep rewinding as long as fingers stay to the left
                        } else if (swipeDelta < -SWIPE_CANCEL_THRESHOLD) {
                            // Swiped back right enough - stop rewind
                            if (g_touch.rewinding) {
                                g_touch.rewinding = false;
                                UpdateRewindState(false);
                                LOGI("Rewind stopped by two-finger swipe right");
                            }
                        }

                        // Check if fingers moved too much for tap detection
                        if (g_touch.twoFingerTapDetected) {
                            float maxMovement = 0.0f;
                            for (int i = 0; i < 2; i++) {
                                float dx = g_touch.pointerX[i] - g_touch.initialX[i];
                                float dy = g_touch.pointerY[i] - g_touch.initialY[i];
                                float movement = sqrtf(dx * dx + dy * dy);
                                if (movement > maxMovement) {
                                    maxMovement = movement;
                                }
                            }
                            // Cancel tap if fingers moved too much
                            if (maxMovement > TWO_FINGER_TAP_DISTANCE) {
                                g_touch.twoFingerTapDetected = false;
                            }
                        }
                    }
                    break;
                }

                case AMOTION_EVENT_ACTION_UP:
                case AMOTION_EVENT_ACTION_POINTER_UP: {
                    int newCount = pointerCount - 1;
                    g_touch.activePointerCount = newCount;

                    // Check for two-finger tap when lifting a finger from 2-finger gesture
                    if (pointerCount == 2 && g_touch.twoFingerTapDetected && !g_touch.rewinding) {
                        // Verify it was quick (within timeout)
                        int64_t elapsed = eventTime - g_touch.twoFingerTapTime;
                        if (elapsed < TWO_FINGER_TAP_TIMEOUT_NS) {
                            // Verify fingers didn't move much (already checked in MOVE, but double-check)
                            float maxMovement = 0.0f;
                            for (int i = 0; i < 2; i++) {
                                float dx = g_touch.pointerX[i] - g_touch.initialX[i];
                                float dy = g_touch.pointerY[i] - g_touch.initialY[i];
                                float movement = sqrtf(dx * dx + dy * dy);
                                if (movement > maxMovement) {
                                    maxMovement = movement;
                                }
                            }

                            if (maxMovement < TWO_FINGER_TAP_DISTANCE) {
                                // Two-finger tap detected!
                                g_paused = !g_paused;
                                LOGI("Pause %s by two-finger tap (movement: %.1fpx, time: %lldms)",
                                     g_paused ? "enabled" : "disabled",
                                     maxMovement, elapsed / 1000000LL);
                            }
                        }
                    }

                    // Stop rewind if we're down to 0 or 1 fingers
                    if (newCount < 2 && g_touch.rewinding) {
                        g_touch.rewinding = false;
                        UpdateRewindState(false);
                        LOGI("Rewind stopped (fingers lifted)");
                    }

                    g_touch.twoFingerTapDetected = false;
                    break;
                }

                case AMOTION_EVENT_ACTION_CANCEL:
                    // Cancel any ongoing gesture
                    if (g_touch.rewinding) {
                        UpdateRewindState(false);
                        g_touch.rewinding = false;
                    }
                    g_touch.twoFingerTapDetected = false;
                    g_touch.activePointerCount = 0;
                    break;

                default:
                    break;
            }

            return 1;
        }

        // Handle joystick/gamepad events
        if ((source & AINPUT_SOURCE_JOYSTICK) == 0)
            return 0;

        // Read D-pad from analog hat axes
        float hatX = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_X, 0);
        float hatY = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_Y, 0);

        // Clear directional bits and re-set from hat
        g_pad_buttons &= ~(SNES_UP_MASK | SNES_DOWN_MASK | SNES_LEFT_MASK | SNES_RIGHT_MASK);

        if (hatX < -0.5f) g_pad_buttons |= SNES_LEFT_MASK;
        if (hatX >  0.5f) g_pad_buttons |= SNES_RIGHT_MASK;
        if (hatY < -0.5f) g_pad_buttons |= SNES_UP_MASK;
        if (hatY >  0.5f) g_pad_buttons |= SNES_DOWN_MASK;

        // Read L2 trigger for rewind
        float l2 = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_LTRIGGER, 0);
        UpdateRewindState(l2 > 0.5f);

        Emulator::SetButtonState(0, g_pad_buttons);
        return 1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// ROM path extraction via JNI
// ---------------------------------------------------------------------------

static std::string GetRomPathFromIntent(struct android_app *app)
{
    JNIEnv *env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    // activity.getIntent()
    jclass activityClass = env->GetObjectClass(app->activity->clazz);
    jmethodID getIntent  = env->GetMethodID(activityClass, "getIntent", "()Landroid/content/Intent;");
    jobject intent       = env->CallObjectMethod(app->activity->clazz, getIntent);

    std::string romPath;

    if (intent) {
        jclass intentClass = env->GetObjectClass(intent);

        // First, try getDataString() for VIEW intents with file:// URIs
        jmethodID getDataString = env->GetMethodID(intentClass, "getDataString", "()Ljava/lang/String;");
        jstring uriString = (jstring)env->CallObjectMethod(intent, getDataString);

        if (uriString) {
            const char *uriStr = env->GetStringUTFChars(uriString, nullptr);
            std::string uri(uriStr);

            // Convert file:// URI to path
            if (uri.find("file://") == 0) {
                romPath = uri.substr(7); // Skip "file://"
                // URL decode the path (handle %20 for spaces, etc.)
                for (size_t i = 0; i < romPath.length(); ) {
                    if (romPath[i] == '%' && i + 2 < romPath.length()) {
                        char hex[3] = { romPath[i+1], romPath[i+2], 0 };
                        char c = (char)strtol(hex, nullptr, 16);
                        romPath.replace(i, 3, 1, c);
                        i++;
                    } else {
                        i++;
                    }
                }
            }

            env->ReleaseStringUTFChars(uriString, uriStr);
            env->DeleteLocalRef(uriString);
        }

        // Fallback: check for "rom_path" extra (for custom intents)
        if (romPath.empty()) {
            jmethodID getStringExtra = env->GetMethodID(intentClass, "getStringExtra",
                                                         "(Ljava/lang/String;)Ljava/lang/String;");
            jstring key   = env->NewStringUTF("rom_path");
            jstring value = (jstring)env->CallObjectMethod(intent, getStringExtra, key);

            if (value) {
                const char *str = env->GetStringUTFChars(value, nullptr);
                romPath = str;
                env->ReleaseStringUTFChars(value, str);
            }

            env->DeleteLocalRef(key);
        }
    }

    app->activity->vm->DetachCurrentThread();

    if (!romPath.empty()) {
        LOGI("ROM path from intent: %s", romPath.c_str());
    }

    return romPath;
}

// ---------------------------------------------------------------------------
// JNI functions for lifecycle control from Kotlin side
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_ezsnes9x_emulator_EmulatorActivity_nativeSuspend(JNIEnv *env, jobject thiz) {
    (void)env; (void)thiz;
    LOGI("Lifecycle: onPause - suspending emulation");
    if (g_running && !g_paused) {
        Emulator::Suspend();
        StopAudio();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_ezsnes9x_emulator_EmulatorActivity_nativeResume(JNIEnv *env, jobject thiz) {
    (void)env; (void)thiz;
    LOGI("Lifecycle: onResume - resuming emulation");
    if (g_running && !g_paused) {
        StartAudio();
        Emulator::Resume();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_ezsnes9x_emulator_EmulatorActivity_nativeStop(JNIEnv *env, jobject thiz) {
    (void)env; (void)thiz;
    LOGI("Lifecycle: onStop - releasing EGL to save GPU power");
    ReleaseEGLForBackground();
}

extern "C" JNIEXPORT void JNICALL
Java_com_ezsnes9x_emulator_EmulatorActivity_nativeStart(JNIEnv *env, jobject thiz) {
    (void)env; (void)thiz;
    LOGI("Lifecycle: onStart - recreating EGL");
    if (g_native_window) {
        InitEGL(g_native_window);
    }
}

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------

static void HandleAppCmd(struct android_app *app, int32_t cmd)
{
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) {
                if (InitEGL(app->window) && InitGL()) {
                    LOGI("Display initialized");
                } else {
                    LOGE("Display initialization failed");
                }
            }
            break;

        case APP_CMD_TERM_WINDOW:
            TeardownGL();
            TeardownEGL();
            break;

        case APP_CMD_GAINED_FOCUS:
            g_has_focus = true;
            if (g_running && !g_paused)
                Emulator::Resume();
            break;

        case APP_CMD_LOST_FOCUS:
            g_has_focus = false;
            if (g_running && !g_paused)
                Emulator::Suspend();
            break;

        case APP_CMD_DESTROY:
            if (g_running) {
                // Save suspend state before quitting (even if app didn't lose focus first)
                if (!g_paused)
                    Emulator::Suspend();
                StopAudio();
                Emulator::Shutdown();
                g_running = false;
            }
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// android_main — entry point (called from android_native_app_glue)
// ---------------------------------------------------------------------------

void android_main(struct android_app *app)
{
    g_app = app;
    app->onAppCmd     = HandleAppCmd;
    app->onInputEvent = HandleInputEvent;

    // Wait for window to be created
    int events;
    struct android_poll_source *source;
    while (!app->destroyRequested && !app->window) {
        ALooper_pollOnce(-1, nullptr, &events, (void **)&source);
        if (source)
            source->process(app, source);
    }

    if (app->destroyRequested) return;

    // Get ROM path
    std::string romPath = GetRomPathFromIntent(app);
    if (romPath.empty()) {
        // Fallback for development - use app's external storage
        romPath = "/storage/emulated/0/Android/data/com.ezsnes9x.emulator/files/rom.sfc";
        LOGI("No ROM in intent, using fallback: %s", romPath.c_str());
    }

    // Initialize emulator
    if (!Emulator::Init("")) {
        LOGE("Emulator::Init failed");
        return;
    }

    // Ensure un-paused when loading a new ROM (e.g., from launcher)
    g_paused = false;

    if (!Emulator::LoadROM(romPath.c_str())) {
        LOGE("Failed to load ROM: %s", romPath.c_str());
        Emulator::Shutdown();
        return;
    }

    LOGI("Loaded ROM: %s", Emulator::GetROMName());

    // Initialize frame throttle based on ROM region
    g_frame_throttle.set_frame_rate(Emulator::IsPAL()
        ? PAL_PROGRESSIVE_FRAME_RATE
        : NTSC_PROGRESSIVE_FRAME_RATE);
    g_frame_throttle.reset();

    // Start audio
    StartAudio();
    g_running = true;

    // Main loop
    while (!app->destroyRequested) {
        // Process all pending events
        while (ALooper_pollOnce(0, nullptr, &events, (void **)&source) >= 0) {
            if (source)
                source->process(app, source);
        }

        if (app->destroyRequested) break;

        if (g_running && g_has_focus && g_egl_display != EGL_NO_DISPLAY) {
            // Run one frame (skip if paused)
            if (!g_paused) {
                if (Emulator::IsRewinding()) {
                    Emulator::RewindTick();
                } else {
                    Emulator::RunFrame();
                }
            }

            RenderFrame();
        }
    }

    // Cleanup
    StopAudio();
    if (g_running) {
        Emulator::Shutdown();
        g_running = false;
    }
    TeardownGL();
    TeardownEGL();
}

