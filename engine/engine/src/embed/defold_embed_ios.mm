// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0
//
// Host-driven embed entry (iOS). Host owns UIView + CADisplayLink;
// UIApplicationMain / AppDelegate loop is not required.

#if !defined(DM_PLATFORM_IOS)
#error "defold_embed_ios.mm is iOS-only"
#endif

#include "defold_embed.h"
#include "defold_embed_bus.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <stdlib.h>
#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <glfw/glfw.h>

#include "../engine.h"

extern "C" {
void dmExportedSymbols();
}

struct DefoldEmbedState {
    dmEngine::HEngine engine;
    int paused;
    UIView* view; // retained
};

static DefoldEmbedState* g_Embed = 0;
static char g_FallbackArg0[] = "libdmengine_embed";

// Process-lifetime globals (extensions, sockets, log, DDF types). Destroy tears
// down the engine instance and glfw only — Initialize stays until process exit.
static int g_EmbedEngineGlobalsReady = 0;

void Defold_EmbedKeepAlive(void)
{
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

static void ReleasePartialCreate(DefoldEmbedState* state)
{
    glfwIosSetExternalView(0);
    if (state->view)
    {
        [state->view release];
        state->view = nil;
    }
    free(state);
}

DefoldEmbedHandle Defold_EmbedCreate(const DefoldEmbedConfig* config)
{
    if (!config || !config->view)
    {
        dmLogError("Defold_EmbedCreate: missing required UIView");
        return 0;
    }
    if (g_Embed)
    {
        dmLogError("Defold_EmbedCreate: already created (single instance)");
        return 0;
    }

    DefoldEmbedState* state = (DefoldEmbedState*)calloc(1, sizeof(DefoldEmbedState));
    if (!state)
        return 0;

    UIView* view = (UIView*)config->view;
    state->view = [view retain];
    glfwIosSetExternalView(view);

    if (!glfwInit())
    {
        dmLogError("Defold_EmbedCreate: glfwInit failed");
        ReleasePartialCreate(state);
        return 0;
    }

    if (!g_EmbedEngineGlobalsReady)
    {
        dmExportedSymbols();
        dmEngineInitialize();
        g_EmbedEngineGlobalsReady = 1;
    }

    static char g_GraphicsAdapterArg[64];
    const char* adapter = config->graphics_adapter;
    if (!adapter || !adapter[0])
        adapter = "metal";
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
        dmLogError("Defold_EmbedCreate: dmEngineCreate failed");
        glfwTerminate();
        ReleasePartialCreate(state);
        return 0;
    }

    g_Embed = state;
    dmLogInfo("Defold_EmbedCreate OK (metal) engine=%p view=%p", (void*)state->engine, (void*)view);
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

    state->paused = 1;
    glfwIosSetExternalView(0);

    if (state->engine)
    {
        dmEngineDestroy(state->engine);
        state->engine = 0;
    }
    // Intentionally skip dmEngineFinalize(): process-level globals must remain.
    glfwTerminate();

    if (state->view)
    {
        [state->view release];
        state->view = nil;
    }

    if (g_Embed == state)
        g_Embed = 0;
    free(state);
    dmLogInfo("Defold_EmbedDestroy done");
}

void Defold_EmbedPause(DefoldEmbedHandle handle)
{
    DefoldEmbedState* state = (DefoldEmbedState*)handle;
    if (!state)
        return;
    state->paused = 1;
}

void Defold_EmbedResume(DefoldEmbedHandle handle)
{
    DefoldEmbedState* state = (DefoldEmbedState*)handle;
    if (!state)
        return;
    state->paused = 0;
    // Clears sticky hide_app iconify from _glfwPlatformIconifyWindow (embed).
    glfwRestoreWindow();
}

void Defold_EmbedAttachWindow(DefoldEmbedHandle handle, struct ANativeWindow* window)
{
    (void)handle;
    (void)window;
}

void Defold_EmbedAttachView(DefoldEmbedHandle handle, void* view)
{
    DefoldEmbedState* state = (DefoldEmbedState*)handle;
    if (!state)
        return;

    // view == NULL → detach host surface (leave Main without Destroy).
    if (!view)
    {
        glfwIosSetExternalView(0);
        if (state->view)
        {
            [state->view release];
            state->view = nil;
        }
        return;
    }

    UIView* next = (UIView*)view;
    if (state->view != next)
    {
        [next retain];
        if (state->view)
            [state->view release];
        state->view = next;
    }

    glfwIosSetExternalView(next);

    // Refresh glfw window size + keep Metal sublayer on this view.
    CGRect bounds = next.bounds;
    CGFloat scale = next.contentScaleFactor > 0 ? next.contentScaleFactor : [UIScreen mainScreen].scale;
    int w = (int)(bounds.size.width * scale + 0.5f);
    int h = (int)(bounds.size.height * scale + 0.5f);
    if (w > 0 && h > 0)
        glfwSetWindowSize(w, h);
}
