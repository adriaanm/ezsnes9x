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

#define LOG_TAG "snes9x"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

extern void EmulatorSetFrameSize(int width, int height);

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static struct android_app *g_app = nullptr;
static bool g_running   = false;
static bool g_has_focus = false;
static bool g_rewinding = false;

// EGL state
static EGLDisplay g_egl_display = EGL_NO_DISPLAY;
static EGLSurface g_egl_surface = EGL_NO_SURFACE;
static EGLContext  g_egl_context = EGL_NO_CONTEXT;

// GL state
static GLuint g_texture  = 0;
static GLuint g_program  = 0;
static GLuint g_vao      = 0;
static int    g_surface_width  = 0;
static int    g_surface_height = 0;

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

    // Create VAO (required for ES 3.0, even with no vertex attribs)
    glGenVertexArrays(1, &g_vao);

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
    if (g_texture) { glDeleteTextures(1, &g_texture); g_texture = 0; }
    if (g_vao)     { glDeleteVertexArrays(1, &g_vao); g_vao = 0; }
    if (g_program) { glDeleteProgram(g_program); g_program = 0; }
}

// ---------------------------------------------------------------------------
// EGL setup/teardown
// ---------------------------------------------------------------------------

static bool InitEGL(ANativeWindow *window)
{
    g_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(g_egl_display, nullptr, nullptr);

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

    // Enable vsync (swap interval 1)
    eglSwapInterval(g_egl_display, 1);

    eglQuerySurface(g_egl_display, g_egl_surface, EGL_WIDTH, &g_surface_width);
    eglQuerySurface(g_egl_display, g_egl_surface, EGL_HEIGHT, &g_surface_height);

    LOGI("EGL surface: %dx%d", g_surface_width, g_surface_height);
    return true;
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

    // activity.getIntent().getStringExtra("rom_path")
    jclass activityClass = env->GetObjectClass(app->activity->clazz);
    jmethodID getIntent  = env->GetMethodID(activityClass, "getIntent", "()Landroid/content/Intent;");
    jobject intent       = env->CallObjectMethod(app->activity->clazz, getIntent);

    std::string romPath;

    if (intent) {
        jclass intentClass     = env->GetObjectClass(intent);
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

    app->activity->vm->DetachCurrentThread();
    return romPath;
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
            if (g_running)
                Emulator::Resume();
            break;

        case APP_CMD_LOST_FOCUS:
            g_has_focus = false;
            if (g_running)
                Emulator::Suspend();
            break;

        case APP_CMD_DESTROY:
            if (g_running) {
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
        // Fallback for development
        romPath = "/sdcard/rom.sfc";
        LOGI("No ROM in intent, using fallback: %s", romPath.c_str());
    }

    // Initialize emulator
    if (!Emulator::Init("")) {
        LOGE("Emulator::Init failed");
        return;
    }

    if (!Emulator::LoadROM(romPath.c_str())) {
        LOGE("Failed to load ROM: %s", romPath.c_str());
        Emulator::Shutdown();
        return;
    }

    LOGI("Loaded ROM: %s", Emulator::GetROMName());

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
            // Run one frame
            if (Emulator::IsRewinding()) {
                Emulator::RewindTick();
            } else {
                Emulator::RunFrame();
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

// ---------------------------------------------------------------------------
// Port interface stubs — platform-specific (Android)
// ---------------------------------------------------------------------------

bool8 S9xInitUpdate()
{
    return true;
}

bool8 S9xDeinitUpdate(int width, int height)
{
    EmulatorSetFrameSize(width, height);
    return true;
}

bool8 S9xContinueUpdate(int width, int height)
{
    EmulatorSetFrameSize(width, height);
    return true;
}

void S9xSyncSpeed()
{
    // EGL swap interval handles timing
}

bool8 S9xOpenSoundDevice()
{
    return true;
}
