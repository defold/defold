// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "window_wayland.h"
#include "xdg-session-management-v1-client-protocol.h"
#include <libdecor-0/libdecor.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlib/math.h>

namespace dmPlatform
{
    // A compositor can supply its own frames. On desktops using client-side
    // decorations, libdecor integrates the shell's title bar and resize handles.
#define DECOR_FUNCTIONS(F) \
    F(libdecor_new) \
    F(libdecor_unref) \
    F(libdecor_dispatch) \
    F(libdecor_decorate) \
    F(libdecor_frame_unref) \
    F(libdecor_frame_get_xdg_toplevel) \
    F(libdecor_frame_map) \
    F(libdecor_frame_commit) \
    F(libdecor_state_new) \
    F(libdecor_state_free) \
    F(libdecor_frame_set_title) \
    F(libdecor_frame_set_app_id) \
    F(libdecor_configuration_get_content_size) \
    F(libdecor_configuration_get_window_state) \
    F(libdecor_frame_set_minimized) \
    F(libdecor_frame_set_fullscreen)
    struct Decor
    {
#define FIELD(f) __typeof__(&f) f##_;
        DECOR_FUNCTIONS(FIELD)
#undef FIELD
    };
    static Decor g_Decor;
    static bool LoadDecorBindings()
    {
        void* lib = dlopen("libdecor-0.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!lib)
        {
            return false;
        }

#define LOAD(f) if (!(g_Decor.f##_ = (__typeof__(&f))dlsym(lib, #f))) { dlclose(lib); return false; }
        DECOR_FUNCTIONS(LOAD)
#undef LOAD
        return true;
    }

    float WaylandGetDisplayDensity(const LinuxWayland* wl)
    {
        if (wl->m_Outputs.Empty())
        {
            return 96.0f;
        }

        // An unmapped surface has no entered output. Use the same first output as fullscreen startup.
        const LinuxOutput* output = wl->m_Outputs[0];
        for (uint32_t i = 1; i < wl->m_Outputs.Size(); ++i)
        {
            const LinuxOutput* candidate = wl->m_Outputs[i];
            if (candidate->m_Entered && (!output->m_Entered || candidate->m_WidthMM > 0))
            {
                output = candidate;
            }
        }

        return CalculateDisplayDensity(output->m_WidthMM, output->m_Width);
    }

    static void Resize(HWindow w, int width, int height)
    {
        // A preferred scale can arrive before the initial fullscreen size.
        if (width <= 0 || height <= 0)
        {
            return;
        }

        LinuxWayland* wl = w->m_Wayland;
        int integer_scale = 0;
        for (uint32_t i = 0; i < wl->m_Outputs.Size(); ++i)
        {
            LinuxOutput* out = wl->m_Outputs[i];
            if (!out->m_Entered)
            {
                continue;
            }

            integer_scale = dmMath::Max(integer_scale, out->m_Scale);
            if (out->m_Refresh)
            {
                w->m_RefreshRate = out->m_Refresh;
            }
        }

        // Wayland window geometry is logical. Back the surface at the output's
        // preferred scale, including when a game uses the default display settings.
        // An unmapped surface has no outputs. Keep its backing size until the
        // compositor identifies the next output, including across hide/show.
        float scale = integer_scale ? (float)integer_scale : w->m_Scale;
        if (wl->m_Viewport && wl->m_PreferredScale)
        {
            scale = wl->m_PreferredScale / 120.0f;
        }

        if (w->m_Surface)
        {
            if (wl_surface_get_version(w->m_Surface) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
            {
                wl_surface_set_buffer_scale(w->m_Surface, wl->m_Viewport ? 1 : (int)scale);
            }

            if (wl->m_Viewport)
            {
                wp_viewport_set_destination(wl->m_Viewport, width, height);
            }
        }

        if (wl->m_XdgSurface && wl->m_Configured)
        {
            WaylandResizeFrame(w, width, height);
        }
        LinuxUpdateSize(w, width, height, scale);
    }

    static void Fractional(void* data, wp_fractional_scale_v1*, uint32_t scale)
    {
        HWindow w = (HWindow)data;
        w->m_Wayland->m_PreferredScale = scale;
        Resize(w, w->m_Width, w->m_Height);
    }

    static const wp_fractional_scale_v1_listener g_Fractional = { Fractional };
    static void Enter(void* data, wl_surface*, wl_output* output)
    {
        HWindow w = (HWindow)data;
        w->m_Wayland->m_SurfaceEntered = true;
        for (uint32_t i = 0; i < w->m_Wayland->m_Outputs.Size(); ++i)
        {
            LinuxOutput* out = w->m_Wayland->m_Outputs[i];
            if (out->m_Output == output)
            {
                out->m_Entered = true;
            }
        }

        Resize(w, w->m_Width, w->m_Height);
    }

    static void Leave(void* data, wl_surface*, wl_output* output)
    {
        HWindow w = (HWindow)data;
        for (uint32_t i = 0; i < w->m_Wayland->m_Outputs.Size(); ++i)
        {
            LinuxOutput* out = w->m_Wayland->m_Outputs[i];
            if (out->m_Output == output)
            {
                out->m_Entered = false;
            }
        }

        Resize(w, w->m_Width, w->m_Height);
    }

    static const wl_surface_listener g_Surface = { Enter, Leave };
    static void Geometry(void* data, wl_output*, int32_t, int32_t, int32_t width_mm, int32_t, int32_t, const char*, const char*, int32_t transform)
    {
        LinuxOutput* out = (LinuxOutput*)data;
        out->m_WidthMM = width_mm;
        out->m_SwapAxes = transform == WL_OUTPUT_TRANSFORM_90 || transform == WL_OUTPUT_TRANSFORM_270 ||
                          transform == WL_OUTPUT_TRANSFORM_FLIPPED_90 || transform == WL_OUTPUT_TRANSFORM_FLIPPED_270;
    }

    static void Mode(void* data, wl_output*, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
    {
        if (flags & WL_OUTPUT_MODE_CURRENT)
        {
            LinuxOutput* out = (LinuxOutput*)data;
            out->m_Refresh = (refresh + 500) / 1000;
            out->m_Width = width;
            out->m_Height = height;
        }
    }

    static void OutputDone(void* data, wl_output*)
    {
        LinuxOutput* out = (LinuxOutput*)data;
        if (out->m_Entered)
        {
            Resize(out->m_Window, out->m_Window->m_Width, out->m_Window->m_Height);
        }
    }

    static void Scale(void* data, wl_output*, int32_t scale)
    {
        ((LinuxOutput*)data)->m_Scale = scale;
    }

    static const wl_output_listener g_Output = { Geometry, Mode, OutputDone, Scale };
    static void Ping(void*, xdg_wm_base* shell, uint32_t serial)
    {
        xdg_wm_base_pong(shell, serial);
    }

    static const xdg_wm_base_listener g_Shell = { Ping };
    static void Global(void* data, wl_registry* registry, uint32_t name, const char* type, uint32_t version)
    {
        HWindow w = (HWindow)data;
        LinuxWayland* wl = w->m_Wayland;
#define BIND(protocol, field, max_version) \
        if (!strcmp(type, #protocol)) \
        { \
            if (!wl->field) wl->field = (protocol*)wl_registry_bind(registry, name, &protocol##_interface, dmMath::Min(version, max_version)); \
            return; \
        }
        BIND(wl_compositor, m_Compositor, 4u)
        BIND(wl_subcompositor, m_Subcompositor, 1u)
        BIND(wl_shm, m_Shm, 1u)
        BIND(xdg_wm_base, m_Shell, 2u)
        BIND(zxdg_decoration_manager_v1, m_DecorManager, 1u)
        BIND(wp_viewporter, m_Viewporter, 1u)
        BIND(wp_fractional_scale_manager_v1, m_ScaleManager, 1u)
        BIND(zwp_relative_pointer_manager_v1, m_RelativeManager, 1u)
        BIND(zwp_pointer_constraints_v1, m_Constraints, 1u)
        BIND(zwp_idle_inhibit_manager_v1, m_IdleManager, 1u)
        BIND(xdg_activation_v1, m_Activation, 1u)
        BIND(xdg_session_manager_v1, m_SessionManager, 1u)
#undef BIND
        if (!strcmp(type, "wl_data_device_manager") && version >= 2 && !wl->m_DataManager)
        {
            wl->m_DataManager = (wl_data_device_manager*)wl_registry_bind(registry, name, &wl_data_device_manager_interface, 2);
            WaylandInitDataDevice(w);
        }
        else if (!strcmp(type, "wl_seat"))
        {
            WaylandBindSeat(w, name, version);
        }
        else if (!strcmp(type, "wl_output") && version >= 2)
        {
            LinuxOutput* out = new LinuxOutput();
            out->m_Window = w;
            out->m_Name = name;
            out->m_Scale = 1;
            out->m_Output = (wl_output*)wl_registry_bind(registry, name, &wl_output_interface, 2);
            wl_output_add_listener(out->m_Output, &g_Output, out);
            if (wl->m_Outputs.Full())
            {
                wl->m_Outputs.OffsetCapacity(4);
            }

            wl->m_Outputs.Push(out);
        }
    }

    static void Remove(void* data, wl_registry*, uint32_t name)
    {
        HWindow w = (HWindow)data;
        LinuxWayland* wl = w->m_Wayland;
        if (wl->m_SeatName == name)
        {
            WaylandReleaseSeat(w);
        }

        for (uint32_t i = 0; i < wl->m_Outputs.Size(); ++i)
        {
            LinuxOutput* out = wl->m_Outputs[i];
            if (out->m_Name != name)
            {
                continue;
            }

            wl_output_destroy(out->m_Output);
            delete out;
            // Keep registry order so fullscreen still prefers the first remaining output after a disconnect.
            for (uint32_t j = i + 1; j < wl->m_Outputs.Size(); ++j)
            {
                wl->m_Outputs[j - 1] = wl->m_Outputs[j];
            }

            wl->m_Outputs.Pop();
            if (w->m_Surface)
            {
                Resize(w, w->m_Width, w->m_Height);
            }

            return;
        }
    }

    static const wl_registry_listener g_Registry = { Global, Remove };

    static bool UsePlayerSizeForRestoredWindow(HWindow w)
    {
        const LinuxWayland* wl = w->m_Wayland;
        // A restored floating window gets its size from the existing Defold placement
        // code, which also handles changes to game.project and Reset Resolution.
        return !wl->m_Configured && wl->m_SessionRestored && NormalPlacementWindow(w);
    }

    static void Configure(void* data, xdg_surface* surface, uint32_t serial)
    {
        HWindow w = (HWindow)data;
        LinuxWayland* wl = w->m_Wayland;
        xdg_surface_ack_configure(surface, serial);
        int width = wl->m_PendingWidth, height = wl->m_PendingHeight;
        if (UsePlayerSizeForRestoredWindow(w))
        {
            width = w->m_Width;
            height = w->m_Height;
        }
        else
        {
            WaylandFrameContentSize(w, &width, &height);
        }

        wl->m_Configured = true;
        Resize(w, width > 0 ? width : w->m_Width, height > 0 ? height : w->m_Height);
    }

    static const xdg_surface_listener g_Configure = { Configure };
    static void Toplevel(void* data, xdg_toplevel*, int32_t width, int32_t height, wl_array* states)
    {
        HWindow w = (HWindow)data;
        w->m_Wayland->m_PendingWidth = width;
        w->m_Wayland->m_PendingHeight = height;
        w->m_Maximized = false;
        w->m_Wayland->m_ShellFullscreen = w->m_Wayland->m_ShellTiled = false;
        uint32_t* values = (uint32_t*)states->data;
        for (size_t i = 0; i < states->size / sizeof(*values); ++i)
        {
            if (values[i] == XDG_TOPLEVEL_STATE_MAXIMIZED)
            {
                w->m_Maximized = true;
            }

            if (values[i] == XDG_TOPLEVEL_STATE_FULLSCREEN)
            {
                w->m_Wayland->m_ShellFullscreen = true;
            }

            if (values[i] >= XDG_TOPLEVEL_STATE_TILED_LEFT && values[i] <= XDG_TOPLEVEL_STATE_TILED_BOTTOM)
            {
                w->m_Wayland->m_ShellTiled = true;
            }
        }
    }

    static void RequestClose(void* data, xdg_toplevel*)
    {
        TriggerCloseCallback((HWindow)data);
    }

    const xdg_toplevel_listener g_WaylandToplevelListener = { Toplevel, RequestClose };
    static void DecorError(libdecor*, libdecor_error, const char* msg)
    {
        dmLogWarning("Wayland decoration: %s", msg);
    }

    static libdecor_interface g_DecorInterface = { DecorError };
    static void DecorSize(HWindow w, int width, int height, libdecor_configuration* cfg)
    {
        libdecor_state* state = g_Decor.libdecor_state_new_(width, height);
        g_Decor.libdecor_frame_commit_(w->m_Wayland->m_Frame, state, cfg);
        g_Decor.libdecor_state_free_(state);
        Resize(w, width, height);
    }

    static void DecorConfigure(libdecor_frame* frame, libdecor_configuration* cfg, void* data)
    {
        HWindow w = (HWindow)data;
        int width = w->m_Width, height = w->m_Height;
        libdecor_window_state state;
        if (g_Decor.libdecor_configuration_get_window_state_(cfg, &state))
        {
            w->m_Maximized = state & LIBDECOR_WINDOW_STATE_MAXIMIZED;
            w->m_Wayland->m_ShellFullscreen = state & LIBDECOR_WINDOW_STATE_FULLSCREEN;
            w->m_Wayland->m_ShellTiled = state & (LIBDECOR_WINDOW_STATE_TILED_LEFT | LIBDECOR_WINDOW_STATE_TILED_RIGHT |
                                                  LIBDECOR_WINDOW_STATE_TILED_TOP | LIBDECOR_WINDOW_STATE_TILED_BOTTOM);
        }

        if (!UsePlayerSizeForRestoredWindow(w))
        {
            g_Decor.libdecor_configuration_get_content_size_(cfg, frame, &width, &height);
        }

        DecorSize(w, width > 0 ? width : w->m_Width, height > 0 ? height : w->m_Height, cfg);
        w->m_Wayland->m_Configured = true;
    }

    static void DecorClose(libdecor_frame*, void* data)
    {
        TriggerCloseCallback((HWindow)data);
    }

    static void DecorCommit(libdecor_frame*, void* data)
    {
        wl_surface_commit(((HWindow)data)->m_Surface);
    }

    static void Dismiss(libdecor_frame*, const char*, void*)
    {
    }

    static libdecor_frame_interface g_Frame = { DecorConfigure, DecorClose, DecorCommit, Dismiss };

    static bool Dispatch(HWindow w)
    {
        WaylandClient& client = g_WaylandClient;
        wl_display* display = w->m_WlDisplay;
        if (w->m_Wayland->m_Decor && g_Decor.libdecor_dispatch_(w->m_Wayland->m_Decor, 0) < 0)
        {
            return false;
        }

        while (client.wl_display_prepare_read_(display) != 0)
        {
            if (client.wl_display_dispatch_pending_(display) < 0)
            {
                return false;
            }
        }

        bool flushed = client.wl_display_flush_(display) >= 0 || errno == EAGAIN;
        pollfd fd = { client.wl_display_get_fd_(display), POLLIN, 0 };
        int result = poll(&fd, 1, 0);
        if (result > 0 && (fd.revents & POLLIN))
        {
            if (client.wl_display_read_events_(display) < 0)
            {
                return false;
            }
        }
        else
        {
            client.wl_display_cancel_read_(display);
        }

        if (fd.revents & (POLLHUP | POLLERR | POLLNVAL))
        {
            return false;
        }

        if (!flushed)
        {
            return false;
        }

        return client.wl_display_dispatch_pending_(display) >= 0;
    }

    static void Poll(HWindow w)
    {
        if (w->m_Wayland->m_Lost)
        {
            return;
        }

        if (!Dispatch(w))
        {
            w->m_Wayland->m_Lost = true;
            if (w->m_Open)
            {
                TriggerCloseCallback(w);
            }

            return;
        }

        WaylandRepeat(w, dmTime::GetMonotonicTime());
    }

    static void ApplyTitle(LinuxWayland* wl)
    {
        const char* title = wl->m_Title.Begin();
        if (wl->m_Frame)
        {
            g_Decor.libdecor_frame_set_title_(wl->m_Frame, title);
        }
        else if (wl->m_Toplevel)
        {
            xdg_toplevel_set_title(wl->m_Toplevel, title);
        }
    }

    static void Title(HWindow w, const char* title)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (!title)
        {
            title = "";
        }

        size_t length = strlen(title) + 1;
        wl->m_Title.EnsureSize(length);
        memcpy(wl->m_Title.Begin(), title, length);
        ApplyTitle(wl);
    }

    static void Iconify(HWindow w)
    {
        // Wayland can ignore this request. Repaint suspension also occurs when another window covers the game.
        LinuxWayland* wl = w->m_Wayland;
        if (wl->m_Frame)
        {
            g_Decor.libdecor_frame_set_minimized_(wl->m_Frame);
        }
        else if (wl->m_Toplevel)
        {
            xdg_toplevel_set_minimized(wl->m_Toplevel);
        }
    }

#define DROP(field, fn) if (wl->field) { fn(wl->field); wl->field = 0; }

    static void DecorationMode(void* data, zxdg_toplevel_decoration_v1*, uint32_t mode)
    {
        ((HWindow)data)->m_Wayland->m_ClientDecorated = mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    }

    static const zxdg_toplevel_decoration_v1_listener g_Decoration = { DecorationMode };

    static void ReleaseRole(LinuxWayland* wl)
    {
        if (!wl->m_SurfaceEntered || !wl->m_Configured)
        {
            // A startup that fails before its first frame must not replace saved placement.
            WaylandReleaseSession(wl);
        }

        WaylandDestroyFrame(wl);
        DROP(m_Frame, g_Decor.libdecor_frame_unref_)
        DROP(m_Decoration, zxdg_toplevel_decoration_v1_destroy)
        DROP(m_Toplevel, xdg_toplevel_destroy)
        DROP(m_XdgSurface, xdg_surface_destroy)
        // Keep the session active until the compositor unmanages the toplevel and records its state.
        WaylandReleaseSession(wl);
        wl->m_Configured = false;
        wl->m_ClientDecorated = wl->m_ShellFullscreen = wl->m_ShellTiled = false;
        wl->m_PendingWidth = wl->m_PendingHeight = 0;
    }

    static wl_output* FullscreenOutput(const LinuxWayland* wl)
    {
        // Wayland has no primary-output flag. GLFW used the first reported output as its preference.
        return wl->m_Outputs.Size() ? wl->m_Outputs[0]->m_Output : 0;
    }

    static void ApplyAppId(LinuxWayland* wl)
    {
        const char* file = getenv("GIO_LAUNCHED_DESKTOP_FILE");
        const char* pid = getenv("GIO_LAUNCHED_DESKTOP_FILE_PID");
        char* end;
        // A game started by the editor can inherit the editor's desktop entry.
        if (!file || !pid || strtol(pid, &end, 10) != getpid() || *end)
        {
            return;
        }

        const char* slash = strrchr(file, '/');
        const char* name = slash ? slash + 1 : file;
        const char* ext = strrchr(name, '.');
        char id[NAME_MAX + 1];
        if (!ext || ext == name || strcmp(ext, ".desktop") || (size_t)(ext - name) >= sizeof(id))
        {
            return;
        }

        memcpy(id, name, ext - name);
        id[ext - name] = '\0';
        if (wl->m_Frame)
        {
            g_Decor.libdecor_frame_set_app_id_(wl->m_Frame, id);
        }
        else
        {
            xdg_toplevel_set_app_id(wl->m_Toplevel, id);
        }
    }

    static bool Map(HWindow w)
    {
        LinuxWayland* wl = w->m_Wayland;
        wl->m_Configured = false;
        if (wl->m_Decor)
        {
            wl->m_Frame = g_Decor.libdecor_decorate_(wl->m_Decor, w->m_Surface, &g_Frame, w);
        }

        if (!wl->m_Frame)
        {
            wl->m_XdgSurface = xdg_wm_base_get_xdg_surface(wl->m_Shell, w->m_Surface);
            xdg_surface_add_listener(wl->m_XdgSurface, &g_Configure, w);
            wl->m_Toplevel = xdg_surface_get_toplevel(wl->m_XdgSurface);
            xdg_toplevel_add_listener(wl->m_Toplevel, &g_WaylandToplevelListener, w);
            wl->m_ClientDecorated = !wl->m_DecorManager;
            if (wl->m_DecorManager)
            {
                wl->m_Decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(wl->m_DecorManager, wl->m_Toplevel);
                zxdg_toplevel_decoration_v1_add_listener(wl->m_Decoration, &g_Decoration, w);
                zxdg_toplevel_decoration_v1_set_mode(wl->m_Decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
            }
        }

        ApplyAppId(wl);
        ApplyTitle(wl);
        bool map_frame = wl->m_Frame != 0;
        const bool restore_session = WaylandSessionEnabled(w);
        if (!map_frame)
        {
            if (w->m_Params.m_Fullscreen)
            {
                xdg_toplevel_set_fullscreen(wl->m_Toplevel, FullscreenOutput(wl));
            }
            else if (restore_session)
            {
                WaylandRestoreSession(w, wl->m_Toplevel);
            }

            wl_surface_commit(w->m_Surface);
        }

        const uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!wl->m_Configured && Dispatch(w))
        {
            // libdecor creates the toplevel asynchronously. Fullscreen and session requests
            // need the toplevel before the first surface commit.
            xdg_toplevel* toplevel = map_frame ? g_Decor.libdecor_frame_get_xdg_toplevel_(wl->m_Frame) : 0;
            if (map_frame && (toplevel || (!w->m_Params.m_Fullscreen && !restore_session)))
            {
                if (w->m_Params.m_Fullscreen)
                {
                    g_Decor.libdecor_frame_set_fullscreen_(wl->m_Frame, FullscreenOutput(wl));
                }
                else if (restore_session)
                {
                    WaylandRestoreSession(w, toplevel);
                }

                g_Decor.libdecor_frame_map_(wl->m_Frame);
                map_frame = false;
                continue;
            }

            if (wl->m_Configured)
            {
                break;
            }

            uint64_t now = dmTime::GetMonotonicTime();
            if (now >= deadline)
            {
                break;
            }

            pollfd fd = { g_WaylandClient.wl_display_get_fd_(w->m_WlDisplay), POLLIN, 0 };
            poll(&fd, 1, dmMath::Min(50, (int)((deadline - now) / 1000) + 1));
        }

        wl->m_Unmapped = !wl->m_Configured;
        if (wl->m_Unmapped)
        {
            ReleaseRole(wl);
        }

        return wl->m_Configured;
    }

    static bool Open(HWindow w)
    {
        LinuxWayland* wl = w->m_Wayland;
        w->m_Surface = wl_compositor_create_surface(wl->m_Compositor);
        wl_surface_add_listener(w->m_Surface, &g_Surface, w);
        // The compositor clips this region to the surface, including after a resize.
        wl_region* opaque = wl_compositor_create_region(wl->m_Compositor);
        if (opaque)
        {
            wl_region_add(opaque, 0, 0, INT32_MAX, INT32_MAX);
            wl_surface_set_opaque_region(w->m_Surface, opaque);
            wl_region_destroy(opaque);
        }
        if (wl->m_ScaleManager && wl->m_Viewporter)
        {
            wl->m_Viewport = wp_viewporter_get_viewport(wl->m_Viewporter, w->m_Surface);
            wl->m_Fractional = wp_fractional_scale_manager_v1_get_fractional_scale(wl->m_ScaleManager, w->m_Surface);
            wp_fractional_scale_v1_add_listener(wl->m_Fractional, &g_Fractional, w);
        }

        if (!wl->m_DecorManager && wl->m_Seat && !wl->m_Decor)
        {
            static const bool available = LoadDecorBindings();
            if (available)
            {
                wl->m_Decor = g_Decor.libdecor_new_(w->m_WlDisplay, &g_DecorInterface);
            }
        }

        Title(w, w->m_Params.m_Title ? w->m_Params.m_Title : "");
        if (w->m_Params.m_Fullscreen && wl->m_IdleManager)
        {
            wl->m_Inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(wl->m_IdleManager, w->m_Surface);
        }

        wl->m_Unmapped = w->m_Params.m_Hidden;
        if (wl->m_Unmapped || !w->m_Width || !w->m_Height)
        {
            // Output mode and geometry arrive after the registry binds. Map normally dispatches them.
            if (g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay) < 0)
            {
                return false;
            }

            if (w->m_Params.m_Fullscreen)
            {
                // EGL needs a drawable even before the first xdg_toplevel configure.
                const LinuxOutput* output = wl->m_Outputs.Empty() ? 0 : wl->m_Outputs[0];
                int width = w->m_Width, height = w->m_Height;
                float scale = w->m_Scale;
                if (output && output->m_Width > 0 && output->m_Height > 0)
                {
                    scale = wl->m_PreferredScale ? wl->m_PreferredScale / 120.0f : (float)output->m_Scale;
                    width = (int)lround((output->m_SwapAxes ? output->m_Height : output->m_Width) / scale);
                    height = (int)lround((output->m_SwapAxes ? output->m_Width : output->m_Height) / scale);
                }

                LinuxUpdateSize(w, dmMath::Max(1, width), dmMath::Max(1, height), scale);
            }
        }

        if (!wl->m_Unmapped && !Map(w))
        {
            return false;
        }

        VisualID unused;
        return LinuxPrepareGL(w, &unused);
    }

    static void Activate(void* data, xdg_activation_token_v1* token, const char* value)
    {
        HWindow w = (HWindow)data;
        xdg_activation_v1_activate(w->m_Wayland->m_Activation, value, w->m_Surface);
        xdg_activation_token_v1_destroy(token);
        w->m_Wayland->m_Token = 0;
    }

    static const xdg_activation_token_v1_listener g_Token = { Activate };
    static void Show(HWindow w, bool show)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (!show)
        {
            if (!wl->m_Unmapped)
            {
                ReleaseRole(wl);
                wl_surface_attach(w->m_Surface, 0, 0, 0);
                wl_surface_commit(w->m_Surface);
                wl->m_Unmapped = true;
            }
        }
        else
        {
            if (wl->m_Unmapped)
            {
                // A Vulkan swapchain can present while hidden. The new shell
                // role requires an initial commit with no attached buffer.
                wl_surface_attach(w->m_Surface, 0, 0, 0);
                wl_surface_commit(w->m_Surface);
                if (!Map(w))
                {
                    w->m_Shown = false;
                    return;
                }
            }

            if (w->m_Params.m_FocusOnShow && wl->m_Activation && !wl->m_Token)
            {
                wl->m_Token = xdg_activation_v1_get_activation_token(wl->m_Activation);
                xdg_activation_token_v1_add_listener(wl->m_Token, &g_Token, w);
                xdg_activation_token_v1_set_surface(wl->m_Token, w->m_Surface);
                if (wl->m_Seat && wl->m_InputSerial)
                {
                    xdg_activation_token_v1_set_serial(wl->m_Token, wl->m_InputSerial, wl->m_Seat);
                }

                xdg_activation_token_v1_commit(wl->m_Token);
            }
        }

        g_WaylandClient.wl_display_flush_(w->m_WlDisplay);
    }

    static void Size(HWindow w, int width, int height)
    {
        if (w->m_Wayland->m_Frame)
        {
            DecorSize(w, width, height, 0);
        }
        else
        {
            Resize(w, width, height);
        }
    }

    static void Close(HWindow w)
    {
        LinuxWayland* wl = w->m_Wayland;
        WaylandReleaseCursor(w);
        DROP(m_Token, xdg_activation_token_v1_destroy)
        DROP(m_Inhibitor, zwp_idle_inhibitor_v1_destroy)
        DROP(m_Fractional, wp_fractional_scale_v1_destroy)
        DROP(m_Viewport, wp_viewport_destroy)
        ReleaseRole(wl);
        if (w->m_Surface)
        {
            wl_surface_destroy(w->m_Surface);
        }

        w->m_Surface = 0;
        wl->m_SurfaceEntered = false;
        for (uint32_t i = 0; i < wl->m_Outputs.Size(); ++i)
        {
            LinuxOutput* out = wl->m_Outputs[i];
            out->m_Entered = false;
        }

        wl->m_PreferredScale = wl->m_EnterSerial = wl->m_NextRepeat = 0;
        wl->m_PointerSurface = 0;
        wl->m_PendingWidth = wl->m_PendingHeight = 0;
        wl->m_SessionReplaced = false;
    }

    static void Disconnect(HWindow w)
    {
        LinuxWayland* wl = w->m_Wayland;
        WaylandReleaseSeat(w);
        DROP(m_CursorSurface, wl_surface_destroy)
        DROP(m_ScaledCursorTheme, g_WaylandClient.wl_cursor_theme_destroy_)
        DROP(m_CursorTheme, g_WaylandClient.wl_cursor_theme_destroy_)
        DROP(m_Decor, g_Decor.libdecor_unref_)
        for (uint32_t i = 0; i < wl->m_FrameBuffers.Size(); ++i)
        {
            wl_buffer_destroy(wl->m_FrameBuffers[i]);
        }
        DROP(m_Compose, g_WaylandClient.xkb_compose_state_unref_)
        DROP(m_Xkb, g_WaylandClient.xkb_context_unref_)
        for (uint32_t i = 0; i < wl->m_Outputs.Size(); ++i)
        {
            LinuxOutput* out = wl->m_Outputs[i];
            wl_output_destroy(out->m_Output);
            delete out;
        }

        DROP(m_DataManager, wl_data_device_manager_destroy)
        DROP(m_ScaleManager, wp_fractional_scale_manager_v1_destroy)
        DROP(m_Viewporter, wp_viewporter_destroy)
        DROP(m_IdleManager, zwp_idle_inhibit_manager_v1_destroy)
        DROP(m_Activation, xdg_activation_v1_destroy)
        DROP(m_SessionManager, xdg_session_manager_v1_destroy)
        DROP(m_RelativeManager, zwp_relative_pointer_manager_v1_destroy)
        DROP(m_Constraints, zwp_pointer_constraints_v1_destroy)
        DROP(m_DecorManager, zxdg_decoration_manager_v1_destroy)
        DROP(m_Shell, xdg_wm_base_destroy)
        DROP(m_Subcompositor, wl_subcompositor_destroy)
        DROP(m_Compositor, wl_compositor_destroy)
        DROP(m_Shm, wl_shm_destroy)
        DROP(m_Registry, wl_registry_destroy)
#undef DROP
        g_WaylandClient.wl_display_flush_(w->m_WlDisplay);
        g_WaylandClient.wl_display_disconnect_(w->m_WlDisplay);
        delete wl;
        w->m_Wayland = 0;
        w->m_WlDisplay = 0;
    }

    bool LinuxConnectWayland(HWindow w)
    {
        if (!LoadWaylandClient())
        {
            return false;
        }

        w->m_WlDisplay = g_WaylandClient.wl_display_connect_(0);
        if (!w->m_WlDisplay)
        {
            return false;
        }

        static const LinuxWindowOps ops = { "wayland", Open, Close, Disconnect, Poll, Show, Iconify, Title, Size, WaylandSetCursor, WaylandSetClipboard };
        w->m_Ops = &ops;
        LinuxWayland* wl = w->m_Wayland = new LinuxWayland();
        wl->m_Xkb = g_WaylandClient.xkb_context_new_(XKB_CONTEXT_NO_FLAGS);
        const char* locale = getenv("LC_ALL");
        if (!locale || !*locale)
        {
            locale = getenv("LC_CTYPE");
        }

        if (!locale || !*locale)
        {
            locale = getenv("LANG");
        }

        if (wl->m_Xkb)
        {
            xkb_compose_table* table = g_WaylandClient.xkb_compose_table_new_from_locale_(wl->m_Xkb, locale && *locale ? locale : "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
            if (table)
            {
                wl->m_Compose = g_WaylandClient.xkb_compose_state_new_(table, XKB_COMPOSE_STATE_NO_FLAGS);
                g_WaylandClient.xkb_compose_table_unref_(table);
            }
        }

        wl->m_Registry = wl_display_get_registry(w->m_WlDisplay);
        wl_registry_add_listener(wl->m_Registry, &g_Registry, w);
        if (g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay) < 0 || !wl->m_Shell || !wl->m_Compositor)
        {
            Disconnect(w);
            return false;
        }

        xdg_wm_base_add_listener(wl->m_Shell, &g_Shell, w);
        return true;
    }

}
