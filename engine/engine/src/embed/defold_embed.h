// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ANativeWindow;

/**
 * Host-driven embed API (Android + iOS).
 *
 * Host owns the surface (ANativeWindow / UIView) and calls Create/Update/Destroy
 * on the host render thread (often the UI thread). NativeActivity /
 * UIApplicationMain are not required.
 *
 * Lifecycle:
 *   Create → Update* → (Pause/Resume)* → Destroy
 * Destroy tears down the engine instance and glfw. Process-level engine globals
 * (extensions, sockets, log) stay initialized so Create may be called again in
 * the same process (e.g. leave/re-enter a Compose / SwiftUI screen).
 *
 * Android optional Activity methods — see historical note in embed docs.
 * iOS: pass UIView* via config.view; default graphics_adapter is "metal".
 */

typedef struct DefoldEmbedConfig {
    /* Android */
    void*                 java_vm;          // JavaVM*
    void*                 activity;         // jobject Activity (local or global)
    void*                 asset_manager;    // jobject AssetManager (optional)
    struct ANativeWindow* native_window;    // required at Create (Android)

    /* iOS */
    void*                 view;             // UIView* (required at Create on iOS)

    /* Common */
    int                   argc;
    char**                argv;             // optional engine args
    /**
     * Graphics adapter name: "opengles", "vulkan", "metal", "opengl", or NULL.
     * NULL defaults to "opengles" on Android and "metal" on iOS.
     */
    const char*           graphics_adapter;
} DefoldEmbedConfig;

typedef void* DefoldEmbedHandle;

/**
 * Engine → host JSON callback. Invoked on the thread that calls Defold_EmbedUpdate
 * (host frame thread). JSON is UTF-8; valid only for the duration of the call.
 */
typedef void (*DefoldEmbedMessageCallback)(void* user_data, const char* json_utf8);

__attribute__((visibility("default")))
DefoldEmbedHandle Defold_EmbedCreate(const DefoldEmbedConfig* config);

/** @return 0 ok, 1 reboot requested, -1 exit/error */
__attribute__((visibility("default")))
int Defold_EmbedUpdate(DefoldEmbedHandle handle);

__attribute__((visibility("default")))
void Defold_EmbedDestroy(DefoldEmbedHandle handle);

__attribute__((visibility("default")))
void Defold_EmbedPause(DefoldEmbedHandle handle);

__attribute__((visibility("default")))
void Defold_EmbedResume(DefoldEmbedHandle handle);

__attribute__((visibility("default")))
void Defold_EmbedAttachWindow(DefoldEmbedHandle handle, struct ANativeWindow* window);

/** iOS: re-bind host UIView after resize / reattach. */
__attribute__((visibility("default")))
void Defold_EmbedAttachView(DefoldEmbedHandle handle, void* view /* UIView* */);

/**
 * Host → engine: enqueue UTF-8 JSON envelope
 * `{ "v":1, "type":"...", "payload":{ } }`. Delivered to Lua via host.poll()
 * during a subsequent Update.
 */
__attribute__((visibility("default")))
void Defold_EmbedSendMessage(DefoldEmbedHandle handle, const char* json_utf8);

/**
 * Engine → host: register callback for outbound JSON (elapsed/log/…).
 * Replaces any previous callback. Pass cb=NULL to clear.
 * Must be re-registered after Destroy→Create.
 */
__attribute__((visibility("default")))
void Defold_EmbedSetMessageCallback(DefoldEmbedHandle handle,
                                    DefoldEmbedMessageCallback cb,
                                    void* user_data);

/** Keep-alive for linker; do not call. */
void Defold_EmbedKeepAlive(void);

#ifdef __cplusplus
}
#endif
