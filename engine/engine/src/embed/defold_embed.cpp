// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0
//
// Host-driven embed entry (Android). Host owns Activity + ANativeWindow;
// NativeActivity is not required.

#if !defined(ANDROID)
#error "defold_embed.cpp is Android-only for now"
#endif

#include "defold_embed.h"
#include "defold_embed_bus.h"

#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <jni.h>

#include <stdlib.h>
#include <string.h>

#include <dlib/dstrings.h>
#include <dmsdk/dlib/android.h>
#include <glfw/glfw.h>

#include "../engine.h"

extern "C" {
extern struct android_app* g_AndroidApp;
void _glfwAndroidHandleCommand(struct android_app* app, int32_t cmd);
void _glfwAndroidPlatformOnTermWindow(void);
void _glfwAndroidPlatformOnInitWindow(void);
void _glfwAndroidSetEmbedUserIconified(int iconified);
void dmExportedSymbols();
}

#define EMBED_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "DefoldEmbed", __VA_ARGS__)
#define EMBED_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "DefoldEmbed", __VA_ARGS__)

struct DefoldEmbedState {
    ANativeActivity activity;
    android_app app;
    jobject activity_global;
    jobject asset_manager_global;
    JavaVM* vm;
    dmEngine::HEngine engine;
    int paused;
};

static DefoldEmbedState* g_Embed = 0;
static char g_FallbackArg0[] = "libdmengine.so";

// Process-lifetime globals (extensions, sockets, log, DDF types). Destroy tears
// down the engine instance and glfw only — Initialize stays until process exit.
// Re-running dmExportedSymbols/dmEngineInitialize after Destroy is not supported.
static int g_EmbedEngineGlobalsReady = 0;

void Defold_EmbedKeepAlive(void)
{
    // Referenced from CMake exported-symbols list so Create/Update/Destroy are
    // retained under -fvisibility=hidden / --gc-sections.
    volatile void* keep[] = {
        (void*)&Defold_EmbedCreate,
        (void*)&Defold_EmbedUpdate,
        (void*)&Defold_EmbedDestroy,
        (void*)&Defold_EmbedPause,
        (void*)&Defold_EmbedResume,
        (void*)&Defold_EmbedAttachWindow,
        (void*)&Defold_EmbedAttachView,
        (void*)&Defold_EmbedSendMessage,
        (void*)&Defold_EmbedSetMessageCallback,
    };
    (void)keep;
}

static int AttachEnv(JavaVM* vm, JNIEnv** env_out)
{
    JNIEnv* env = 0;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
    {
        if (vm->AttachCurrentThread(&env, 0) != 0)
            return 0;
    }
    *env_out = env;
    return 1;
}

static void ReleasePartialCreate(DefoldEmbedState* state, JNIEnv* env, ANativeWindow* window, int release_window)
{
    glfwAndroidSetExternalWindow(0);
    if (release_window && window)
        ANativeWindow_release(window);
    dmAndroid::SetAndroidApp(0);
    g_AndroidApp = 0;
    if (env)
    {
        if (state->asset_manager_global)
            env->DeleteGlobalRef(state->asset_manager_global);
        if (state->activity_global)
            env->DeleteGlobalRef(state->activity_global);
    }
    free(state);
}

DefoldEmbedHandle Defold_EmbedCreate(const DefoldEmbedConfig* config)
{
    if (!config || !config->java_vm || !config->activity || !config->native_window)
    {
        EMBED_LOGE("Defold_EmbedCreate: missing required config fields");
        return 0;
    }
    if (g_Embed)
    {
        EMBED_LOGE("Defold_EmbedCreate: already created (single instance)");
        return 0;
    }

    JavaVM* vm = (JavaVM*)config->java_vm;
    JNIEnv* env = 0;
    if (!AttachEnv(vm, &env))
    {
        EMBED_LOGE("Defold_EmbedCreate: AttachCurrentThread failed");
        return 0;
    }

    DefoldEmbedState* state = (DefoldEmbedState*)calloc(1, sizeof(DefoldEmbedState));
    if (!state)
        return 0;

    state->vm = vm;
    state->activity_global = env->NewGlobalRef((jobject)config->activity);
    if (!state->activity_global)
    {
        free(state);
        return 0;
    }

    jobject am_obj = (jobject)config->asset_manager;
    if (!am_obj)
    {
        jclass activity_cls = env->GetObjectClass(state->activity_global);
        jmethodID get_assets = env->GetMethodID(activity_cls, "getAssets", "()Landroid/content/res/AssetManager;");
        am_obj = get_assets ? env->CallObjectMethod(state->activity_global, get_assets) : 0;
        env->DeleteLocalRef(activity_cls);
    }
    if (!am_obj)
    {
        EMBED_LOGE("Defold_EmbedCreate: AssetManager missing");
        env->DeleteGlobalRef(state->activity_global);
        free(state);
        return 0;
    }
    state->asset_manager_global = env->NewGlobalRef(am_obj);

    AAssetManager* asset_manager = AAssetManager_fromJava(env, state->asset_manager_global);
    if (!asset_manager)
    {
        EMBED_LOGE("Defold_EmbedCreate: AAssetManager_fromJava failed");
        env->DeleteGlobalRef(state->asset_manager_global);
        env->DeleteGlobalRef(state->activity_global);
        free(state);
        return 0;
    }

    // Minimal ANativeActivity / android_app for existing glfw/dlib call sites.
    memset(&state->activity, 0, sizeof(state->activity));
    state->activity.vm = vm;
    state->activity.env = env;
    state->activity.clazz = state->activity_global;
    state->activity.assetManager = asset_manager;

    memset(&state->app, 0, sizeof(state->app));
    state->app.activity = &state->activity;
    state->app.window = config->native_window;
    state->app.looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    if (!state->app.looper)
    {
        EMBED_LOGE("Defold_EmbedCreate: ALooper_prepare failed");
        env->DeleteGlobalRef(state->asset_manager_global);
        env->DeleteGlobalRef(state->activity_global);
        free(state);
        return 0;
    }

    dmAndroid::SetAndroidApp(&state->app);
    g_AndroidApp = &state->app;

    ANativeWindow_acquire(config->native_window);
    glfwAndroidSetExternalWindow(config->native_window);

    // Same order as engine_main Android: INIT_WINDOW before glfwInit.
    // glfwInit clears _glfwWin (including opened); delivering INIT_WINDOW after
    // glfwInit would set opened=1 and make glfwOpenWindow fail.
    _glfwAndroidHandleCommand(&state->app, APP_CMD_INIT_WINDOW);
    _glfwAndroidHandleCommand(&state->app, APP_CMD_START);
    _glfwAndroidHandleCommand(&state->app, APP_CMD_RESUME);
    _glfwAndroidHandleCommand(&state->app, APP_CMD_GAINED_FOCUS);

    if (!glfwInit())
    {
        EMBED_LOGE("Defold_EmbedCreate: glfwInit failed");
        ReleasePartialCreate(state, env, config->native_window, 1);
        return 0;
    }

    if (!g_EmbedEngineGlobalsReady)
    {
        dmExportedSymbols();
        dmEngineInitialize();
        g_EmbedEngineGlobalsReady = 1;
    }

    // Default graphics adapter is OpenGLES (host Surface + dmglfw EGL).
    static char g_GraphicsAdapterArg[64];
    const char* adapter = config->graphics_adapter;
    if (!adapter || !adapter[0])
        adapter = "opengles";
    dmSnPrintf(g_GraphicsAdapterArg, sizeof(g_GraphicsAdapterArg), "--graphics-adapter=%s", adapter);

    char* owned_argv_storage[16];
    char** argv = config->argv;
    int argc = config->argc;
    char* fallback_argv[3] = { g_FallbackArg0, g_GraphicsAdapterArg, 0 };

    if (argc <= 0 || !argv)
    {
        argc = 2;
        argv = fallback_argv;
    }
    else
    {
        int has_adapter = 0;
        for (int i = 0; i < argc; ++i)
        {
            if (argv[i] && strstr(argv[i], "--graphics-adapter=") == argv[i])
            {
                has_adapter = 1;
                break;
            }
        }
        if (!has_adapter && argc + 1 < (int)(sizeof(owned_argv_storage) / sizeof(owned_argv_storage[0])))
        {
            for (int i = 0; i < argc; ++i)
                owned_argv_storage[i] = argv[i];
            owned_argv_storage[argc++] = g_GraphicsAdapterArg;
            owned_argv_storage[argc] = 0;
            argv = owned_argv_storage;
        }
    }

    state->engine = dmEngineCreate(argc, argv);
    if (!state->engine)
    {
        EMBED_LOGE("Defold_EmbedCreate: dmEngineCreate failed");
        glfwTerminate();
        ReleasePartialCreate(state, env, config->native_window, 1);
        return 0;
    }

    g_Embed = state;
    EMBED_LOGI("Defold_EmbedCreate OK engine=%p window=%p", (void*)state->engine, (void*)config->native_window);
    return state;
}

int Defold_EmbedUpdate(DefoldEmbedHandle handle)
{
    DefoldEmbedState* state = (DefoldEmbedState*)handle;
    if (!state || !state->engine)
        return -1;
    if (state->paused)
        return 0;

    dmEngine::UpdateResult r = dmEngineUpdate(state->engine);
    DefoldEmbedBus_FlushOutbound();
    if (r == dmEngine::RESULT_OK)
        return 0;
    if (r == dmEngine::RESULT_REBOOT)
        return 1;
    return -1;
}

void Defold_EmbedSendMessage(DefoldEmbedHandle handle, const char* json_utf8)
{
    (void)handle;
    if (!json_utf8)
        return;
    DefoldEmbedBus_EnqueueInbound(json_utf8);
}

void Defold_EmbedSetMessageCallback(DefoldEmbedHandle handle,
                                    DefoldEmbedMessageCallback cb,
                                    void* user_data)
{
    (void)handle;
    DefoldEmbedBus_SetCallback(cb, user_data);
}

void Defold_EmbedDestroy(DefoldEmbedHandle handle)
{
    DefoldEmbedState* state = (DefoldEmbedState*)handle;
    if (!state)
        return;

    DefoldEmbedBus_Reset();

    // Clear host window first — Surface may already be gone on screen dispose.
    state->paused = 1;
    glfwAndroidSetExternalWindow(0);
    if (state->app.window)
    {
        ANativeWindow_release(state->app.window);
        state->app.window = 0;
    }

    if (state->engine)
    {
        dmEngineDestroy(state->engine);
        state->engine = 0;
    }
    // Intentionally skip dmEngineFinalize(): process-level globals must remain
    // so a later Create in the same process can succeed.
    glfwTerminate();

    dmAndroid::SetAndroidApp(0);
    g_AndroidApp = 0;

    JNIEnv* env = 0;
    if (state->vm && AttachEnv(state->vm, &env))
    {
        if (state->asset_manager_global)
            env->DeleteGlobalRef(state->asset_manager_global);
        if (state->activity_global)
            env->DeleteGlobalRef(state->activity_global);
    }

    if (g_Embed == state)
        g_Embed = 0;
    free(state);
    EMBED_LOGI("Defold_EmbedDestroy done");
}

void Defold_EmbedPause(DefoldEmbedHandle handle)
{
    DefoldEmbedState* state = (DefoldEmbedState*)handle;
    if (!state)
        return;
    state->paused = 1;
    _glfwAndroidHandleCommand(&state->app, APP_CMD_PAUSE);
}

void Defold_EmbedResume(DefoldEmbedHandle handle)
{
    DefoldEmbedState* state = (DefoldEmbedState*)handle;
    if (!state)
        return;
    state->paused = 0;
    // Clear sticky hide_app iconify so computeIconifiedState can un-iconify.
    _glfwAndroidSetEmbedUserIconified(0);
    _glfwAndroidHandleCommand(&state->app, APP_CMD_RESUME);
}

void Defold_EmbedAttachWindow(DefoldEmbedHandle handle, struct ANativeWindow* window)
{
    DefoldEmbedState* state = (DefoldEmbedState*)handle;
    if (!state || !window)
        return;

    // Same window re-attach (common from SurfaceView callbacks): do not
    // ANativeWindow_acquire again — glfwAndroidSetExternalWindow early-returns
    // without a matching release, which would leak a ref each call.
    if (state->app.window == window)
    {
        glfwAndroidSetExternalWindow(window);
        return;
    }

    // Different window: tear down GLES surface bound to the old native window,
    // then rebind. (_glfwAndroidHandleCommand(INIT_WINDOW) only sets opened=1;
    // real recreate lives in OnTerm/OnInit.)
    if (state->app.window)
    {
        _glfwAndroidPlatformOnTermWindow();
        ANativeWindow_release(state->app.window);
        state->app.window = 0;
        glfwAndroidSetExternalWindow(0);
    }

    ANativeWindow_acquire(window);
    state->app.window = window;
    glfwAndroidSetExternalWindow(window);
    _glfwAndroidPlatformOnInitWindow();
}

void Defold_EmbedAttachView(DefoldEmbedHandle handle, void* view)
{
    (void)handle;
    (void)view;
}
