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

#include <dlib/time.h>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dlib/math.h>

namespace dmPlatform
{
    static void ReleaseKeymap(LinuxWayland* wl)
    {
        if (wl->m_KeyState)
        {
            g_WaylandClient.xkb_state_unref_(wl->m_KeyState);
        }

        if (wl->m_Keymap)
        {
            g_WaylandClient.xkb_keymap_unref_(wl->m_Keymap);
        }

        wl->m_KeyState = 0;
        wl->m_Keymap = 0;
        wl->m_NextRepeat = 0;
    }

    static void Keymap(void* data, wl_keyboard*, uint32_t format, int fd, uint32_t size)
    {
        LinuxWayland* wl = ((HWindow)data)->m_Wayland;
        ReleaseKeymap(wl);
        if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size && wl->m_Xkb)
        {
            char* map = (char*)mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (map != MAP_FAILED)
            {
                if (map[size - 1] == 0)
                {
                    wl->m_Keymap = g_WaylandClient.xkb_keymap_new_from_string_(wl->m_Xkb, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
                }

                munmap(map, size);
                if (wl->m_Keymap)
                {
                    wl->m_KeyState = g_WaylandClient.xkb_state_new_(wl->m_Keymap);
                }
            }
        }

        close(fd);
    }

    static void PhysicalKey(HWindow w, uint32_t key, bool down)
    {
        int code = LinuxKeyCode(key);
        if (code >= 0 && code < (int)sizeof(w->m_Keys))
        {
            w->m_Keys[code] = down;
        }
    }

    static void KeyEnter(void* data, wl_keyboard*, uint32_t serial, wl_surface* surface, wl_array* keys)
    {
        HWindow w = (HWindow)data;
        if (!surface || surface != w->m_Surface)
        {
            return;
        }

        // The focus callback can claim the clipboard before the first key or button press.
        w->m_Wayland->m_InputSerial = serial;
        const uint32_t* key = (uint32_t*)keys->data;
        for (size_t i = 0; i < keys->size / sizeof(*key); ++i)
        {
            PhysicalKey(w, key[i], true);
        }

        LinuxFocus(w, true);
    }

    void WaylandResetKeyboard(LinuxWayland* wl)
    {
        wl->m_NextRepeat = 0;
        if (wl->m_Compose)
        {
            g_WaylandClient.xkb_compose_state_reset_(wl->m_Compose);
        }
    }

    static void KeyLeave(void* data, wl_keyboard*, uint32_t, wl_surface* surface)
    {
        HWindow w = (HWindow)data;
        if (surface == w->m_Surface)
        {
            WaylandResetKeyboard(w->m_Wayland);
            LinuxFocus(w, false);
        }
    }

    static void KeyText(HWindow w, uint32_t key)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (!wl->m_KeyState ||
            g_WaylandClient.xkb_state_mod_name_is_active_(wl->m_KeyState, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0 ||
            g_WaylandClient.xkb_state_mod_name_is_active_(wl->m_KeyState, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0)
        {
            return;
        }

        char text[128];
        int length = 0;
        if (wl->m_Compose)
        {
            xkb_keysym_t sym = g_WaylandClient.xkb_state_key_get_one_sym_(wl->m_KeyState, key + 8);
            g_WaylandClient.xkb_compose_state_feed_(wl->m_Compose, sym);
            switch (g_WaylandClient.xkb_compose_state_get_status_(wl->m_Compose))
            {
                case XKB_COMPOSE_COMPOSING:
                {
                    return;
                }
                case XKB_COMPOSE_CANCELLED:
                {
                    g_WaylandClient.xkb_compose_state_reset_(wl->m_Compose);
                    return;
                }
                case XKB_COMPOSE_COMPOSED:
                {
                    length = g_WaylandClient.xkb_compose_state_get_utf8_(wl->m_Compose, text, sizeof(text));
                    g_WaylandClient.xkb_compose_state_reset_(wl->m_Compose);
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        if (!length)
        {
            length = g_WaylandClient.xkb_state_key_get_utf8_(wl->m_KeyState, key + 8, text, sizeof(text));
        }

        if (length > 0 && length < (int)sizeof(text))
        {
            LinuxText(w, text, length);
        }
    }

    static void Key(void* data, wl_keyboard*, uint32_t serial, uint32_t, uint32_t key, uint32_t state)
    {
        HWindow w = (HWindow)data;
        // Key events carry no surface. Queued presses belong to the old focus until the replacement gets an enter.
        if (!w->m_Focused || !w->m_Surface || w->m_DeletePending)
        {
            return;
        }

        LinuxWayland* wl = w->m_Wayland;
        wl->m_InputSerial = serial;
        bool down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
        PhysicalKey(w, key, down);
        if (down)
        {
            if (wl->m_RepeatRate > 0 && wl->m_Keymap && g_WaylandClient.xkb_keymap_key_repeats_(wl->m_Keymap, key + 8))
            {
                wl->m_RepeatKey = key;
                wl->m_NextRepeat = dmTime::GetMonotonicTime() + 1000ULL * wl->m_RepeatDelay;
            }

            // Closing the window from the character callback cancels the repeat set up for this press.
            KeyText(w, key);
        }
        else if (key == wl->m_RepeatKey)
        {
            wl->m_NextRepeat = 0;
        }
    }

    void WaylandRepeat(HWindow w, uint64_t now)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (wl->m_RepeatRate <= 0)
        {
            return;
        }

        uint64_t interval = dmMath::Max(1, 1000000 / wl->m_RepeatRate);
        while (wl->m_NextRepeat && wl->m_NextRepeat <= now)
        {
            wl->m_NextRepeat += interval;
            KeyText(w, wl->m_RepeatKey);
        }
    }

    static void Modifiers(void* data, wl_keyboard*, uint32_t, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
    {
        xkb_state* state = ((HWindow)data)->m_Wayland->m_KeyState;
        if (state)
        {
            g_WaylandClient.xkb_state_update_mask_(state, depressed, latched, locked, 0, 0, group);
        }
    }

    static void RepeatInfo(void* data, wl_keyboard*, int32_t rate, int32_t delay)
    {
        LinuxWayland* wl = ((HWindow)data)->m_Wayland;
        wl->m_RepeatRate = rate;
        wl->m_RepeatDelay = dmMath::Max(delay, 0);
        if (rate <= 0)
        {
            wl->m_NextRepeat = 0;
        }
    }

    const wl_keyboard_listener g_WaylandKeyboardListener = { Keymap, KeyEnter, KeyLeave, Key, Modifiers, RepeatInfo };

    static void Relative(void* data, zwp_relative_pointer_v1*, uint32_t, uint32_t, wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t, wl_fixed_t)
    {
        HWindow w = (HWindow)data;
        if (w->m_Wayland->m_PointerLocked)
        {
            w->m_MouseX += wl_fixed_to_double(dx);
            w->m_MouseY += wl_fixed_to_double(dy);
        }
    }

    const zwp_relative_pointer_v1_listener g_WaylandRelativePointerListener = { Relative };
    static void Locked(void* data, zwp_locked_pointer_v1*)
    {
        HWindow w = (HWindow)data;
        w->m_Wayland->m_UnlockedX = w->m_MouseX;
        w->m_Wayland->m_UnlockedY = w->m_MouseY;
        w->m_Wayland->m_PointerLocked = true;
    }

    static void RestorePointerPosition(HWindow w)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (wl->m_PointerLocked)
        {
            w->m_MouseX = wl->m_UnlockedX;
            w->m_MouseY = wl->m_UnlockedY;
            wl->m_PointerLocked = false;
        }
    }

    static void Unlocked(void* data, zwp_locked_pointer_v1*)
    {
        RestorePointerPosition((HWindow)data);
    }

    const zwp_locked_pointer_v1_listener g_WaylandLockedPointerListener = { Locked, Unlocked };
    void WaylandReleaseCursor(HWindow w)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (wl->m_Lock)
        {
            zwp_locked_pointer_v1_destroy(wl->m_Lock);
        }

        if (wl->m_Relative)
        {
            zwp_relative_pointer_v1_destroy(wl->m_Relative);
        }

        wl->m_Lock = 0;
        RestorePointerPosition(w);
        wl->m_Relative = 0;
    }

    static wl_cursor_theme* LoadCursorTheme(wl_shm* shm, uint32_t scale)
    {
        int size = 24;
        const char* setting = getenv("XCURSOR_SIZE");
        if (setting)
        {
            char* end;
            long pixels = strtol(setting, &end, 10);
            if (*end == '\0' && pixels > 0 && pixels <= INT_MAX / 4 / pixels)
            {
                size = (int)pixels;
            }
        }

        // The initial ARGB cursor buffer must fit in a Wayland shared memory pool.
        int64_t pixels = (int64_t)size * scale;
        return pixels <= INT_MAX / 4 / pixels ? g_WaylandClient.wl_cursor_theme_load_(getenv("XCURSOR_THEME"), (int)pixels, shm) : 0;
    }

    static wl_cursor_image* CursorImage(wl_cursor_theme* theme, const char* name)
    {
        wl_cursor* cursor = theme ? g_WaylandClient.wl_cursor_theme_get_cursor_(theme, name) : 0;
        return cursor && cursor->image_count ? cursor->images[0] : 0;
    }

    void WaylandSetCursor(HWindow w)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (!w->m_Locked || !w->m_Focused)
        {
            WaylandReleaseCursor(w);
        }

        if (!wl->m_Pointer || !w->m_Surface || !wl->m_EnterSerial)
        {
            return;
        }

        const char* frame_cursor = WaylandFrameCursor(wl);
        if (wl->m_PointerSurface != w->m_Surface && !frame_cursor)
        {
            return;
        }

        if (!frame_cursor && w->m_Locked && w->m_Focused)
        {
            if (!wl->m_Lock && wl->m_Constraints && wl->m_RelativeManager)
            {
                wl->m_Lock = zwp_pointer_constraints_v1_lock_pointer(wl->m_Constraints, w->m_Surface, wl->m_Pointer, 0, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
                zwp_locked_pointer_v1_add_listener(wl->m_Lock, &g_WaylandLockedPointerListener, w);
                wl->m_Relative = zwp_relative_pointer_manager_v1_get_relative_pointer(wl->m_RelativeManager, wl->m_Pointer);
                zwp_relative_pointer_v1_add_listener(wl->m_Relative, &g_WaylandRelativePointerListener, w);
            }

            wl_pointer_set_cursor(wl->m_Pointer, wl->m_EnterSerial, 0, 0, 0);
            return;
        }

        if (!wl->m_Shm || !g_WaylandClient.wl_cursor_theme_load_)
        {
            return;
        }

        const char* name = frame_cursor ? frame_cursor : (w->m_LinkCursor ? "hand2" : "left_ptr");
        uint32_t scale = (uint32_t)ceilf(dmMath::Max(w->m_Scale, 1.0f));
        wl_cursor_image* img = 0;
        if (scale > 1 && wl_surface_get_version(w->m_Surface) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
        {
            if (wl->m_CursorThemeScale != scale)
            {
                if (wl->m_ScaledCursorTheme)
                {
                    g_WaylandClient.wl_cursor_theme_destroy_(wl->m_ScaledCursorTheme);
                }

                wl->m_ScaledCursorTheme = LoadCursorTheme(wl->m_Shm, scale);
                wl->m_CursorThemeScale = scale;
            }

            img = CursorImage(wl->m_ScaledCursorTheme, name);
        }

        // A theme may return the nearest available size. Wayland requires both
        // buffer dimensions to be exact multiples of the surface's integer scale.
        if (!img || img->width % scale || img->height % scale)
        {
            scale = 1;
            if (!wl->m_CursorTheme)
            {
                wl->m_CursorTheme = LoadCursorTheme(wl->m_Shm, scale);
            }

            img = CursorImage(wl->m_CursorTheme, name);
            if (!img)
            {
                return;
            }
        }

        if (!wl->m_CursorSurface)
        {
            wl->m_CursorSurface = wl_compositor_create_surface(wl->m_Compositor);
        }

        if (wl_surface_get_version(wl->m_CursorSurface) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
        {
            wl_surface_set_buffer_scale(wl->m_CursorSurface, scale);
        }

        wl_pointer_set_cursor(wl->m_Pointer, wl->m_EnterSerial, wl->m_CursorSurface, img->hotspot_x / scale, img->hotspot_y / scale);
        wl_surface_attach(wl->m_CursorSurface, g_WaylandClient.wl_cursor_image_get_buffer_(img), 0, 0);
        wl_surface_damage(wl->m_CursorSurface, 0, 0, img->width / scale, img->height / scale);
        wl_surface_commit(wl->m_CursorSurface);
    }

    static void Enter(void* data, wl_pointer*, uint32_t serial, wl_surface* surface, wl_fixed_t x, wl_fixed_t y)
    {
        HWindow w = (HWindow)data;
        w->m_Wayland->m_PointerSurface = surface;
        if (surface != w->m_Surface)
        {
            LinuxWayland* wl = w->m_Wayland;
            wl->m_FramePointerX = wl_fixed_to_int(x);
            wl->m_FramePointerY = wl_fixed_to_int(y);
            if (WaylandFrameCursor(wl))
            {
                wl->m_EnterSerial = serial;
                WaylandSetCursor(w);
            }

            return;
        }

        w->m_Wayland->m_EnterSerial = serial;
        w->m_MouseX = wl_fixed_to_double(x);
        w->m_MouseY = wl_fixed_to_double(y);
        WaylandSetCursor(w);
    }

    static void Leave(void* data, wl_pointer*, uint32_t, wl_surface*)
    {
        HWindow w = (HWindow)data;
        w->m_Wayland->m_PointerSurface = 0;
        w->m_Wayland->m_EnterSerial = 0;
        memset(w->m_Buttons, 0, sizeof(w->m_Buttons));
    }

    static bool PointerOverContent(HWindow w)
    {
        return w->m_Surface && w->m_Wayland->m_PointerSurface == w->m_Surface;
    }

    static void Motion(void* data, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y)
    {
        HWindow w = (HWindow)data;
        if (PointerOverContent(w) && !w->m_Wayland->m_PointerLocked)
        {
            w->m_MouseX = wl_fixed_to_double(x);
            w->m_MouseY = wl_fixed_to_double(y);
        }
        else
        {
            LinuxWayland* wl = w->m_Wayland;
            const char* cursor = WaylandFrameCursor(wl);
            if (cursor)
            {
                wl->m_FramePointerX = wl_fixed_to_int(x);
                wl->m_FramePointerY = wl_fixed_to_int(y);
                if (strcmp(cursor, WaylandFrameCursor(wl)))
                {
                    WaylandSetCursor(w);
                }
            }
        }
    }

    static void Button(void* data, wl_pointer*, uint32_t serial, uint32_t, uint32_t button, uint32_t state)
    {
        HWindow w = (HWindow)data;
        if (!PointerOverContent(w))
        {
            if (state == WL_POINTER_BUTTON_STATE_PRESSED)
            {
                WaylandFrameButton(w, serial, button);
            }

            return;
        }

        w->m_Wayland->m_InputSerial = serial;
        if (button >= BTN_LEFT && button < BTN_LEFT + sizeof(w->m_Buttons))
        {
            w->m_Buttons[button - BTN_LEFT] = state == WL_POINTER_BUTTON_STATE_PRESSED;
        }
    }

    static void Frame(void* data, wl_pointer*)
    {
        HWindow w = (HWindow)data;
        LinuxWayland* wl = w->m_Wayland;
        w->m_Wheel -= wl->m_Discrete ? wl->m_Discrete : wl->m_Axis / 10.0;
        wl->m_Discrete = 0;
        wl->m_Axis = 0;
    }

    static void Axis(void* data, wl_pointer* pointer, uint32_t, uint32_t axis, wl_fixed_t value)
    {
        HWindow w = (HWindow)data;
        if (!PointerOverContent(w) || axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
        {
            return;
        }

        w->m_Wayland->m_Axis += wl_fixed_to_double(value);
        if (wl_pointer_get_version(pointer) < WL_POINTER_FRAME_SINCE_VERSION)
        {
            Frame(data, pointer);
        }
    }

    static void AxisSource(void*, wl_pointer*, uint32_t)
    {
    }

    static void AxisStop(void*, wl_pointer*, uint32_t, uint32_t)
    {
    }

    static void Discrete(void* data, wl_pointer*, uint32_t axis, int32_t ticks)
    {
        HWindow w = (HWindow)data;
        if (PointerOverContent(w) && axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
        {
            w->m_Wayland->m_Discrete += ticks;
        }
    }

    const wl_pointer_listener g_WaylandPointerListener = { Enter, Leave, Motion, Button, Axis, Frame, AxisSource, AxisStop, Discrete };
    static void Capabilities(void* data, wl_seat* seat, uint32_t caps)
    {
        HWindow w = (HWindow)data;
        LinuxWayland* wl = w->m_Wayland;
        if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wl->m_Keyboard)
        {
            wl->m_Keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(wl->m_Keyboard, &g_WaylandKeyboardListener, w);
        }
        else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl->m_Keyboard)
        {
            KeyLeave(w, wl->m_Keyboard, 0, w->m_Surface);
            wl_keyboard_release(wl->m_Keyboard);
            wl->m_Keyboard = 0;
            ReleaseKeymap(wl);
        }

        if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wl->m_Pointer)
        {
            wl->m_Pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(wl->m_Pointer, &g_WaylandPointerListener, w);
        }
        else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && wl->m_Pointer)
        {
            WaylandReleaseCursor(w);
            wl_pointer_release(wl->m_Pointer);
            wl->m_Pointer = 0;
            Leave(w, 0, 0, wl->m_PointerSurface);
        }
    }

    static void SeatName(void*, wl_seat*, const char*)
    {
    }

    static const wl_seat_listener g_Seat = { Capabilities, SeatName };
    void WaylandBindSeat(HWindow w, uint32_t name, uint32_t version)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (wl->m_Seat || version < 3)
        {
            return;
        }

        wl->m_SeatName = name;
        wl->m_Seat = (wl_seat*)wl_registry_bind(wl->m_Registry, name, &wl_seat_interface, dmMath::Min(version, 5u));
        wl_seat_add_listener(wl->m_Seat, &g_Seat, w);
        WaylandInitDataDevice(w);
    }

    void WaylandReleaseSeat(HWindow w)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (!wl->m_Seat)
        {
            return;
        }

        Capabilities(w, wl->m_Seat, 0);
        if (wl->m_DataSource)
        {
            wl_data_source_destroy(wl->m_DataSource);
        }

        if (wl->m_DataDevice)
        {
            wl_data_device_release(wl->m_DataDevice);
        }

        wl->m_DataSource = 0;
        wl->m_DataDevice = 0;
        wl->m_InputSerial = 0;
        if (wl_seat_get_version(wl->m_Seat) >= WL_SEAT_RELEASE_SINCE_VERSION)
        {
            wl_seat_release(wl->m_Seat);
        }
        else
        {
            wl_seat_destroy(wl->m_Seat);
        }

        wl->m_Seat = 0;
    }

    static void Offer(void*, wl_data_device*, wl_data_offer* offer)
    {
        wl_data_offer_destroy(offer);
    }

    static void DragEnter(void*, wl_data_device*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t, wl_data_offer*)
    {
    }

    static void DragEvent(void*, wl_data_device*)
    {
    }

    static void DragMotion(void*, wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t)
    {
    }

    static void Selection(void*, wl_data_device*, wl_data_offer*)
    {
    }

    static const wl_data_device_listener g_DataDevice = { Offer, DragEnter, DragEvent, DragMotion, DragEvent, Selection };
    void WaylandInitDataDevice(HWindow w)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (wl->m_DataManager && wl->m_Seat && !wl->m_DataDevice)
        {
            wl->m_DataDevice = wl_data_device_manager_get_data_device(wl->m_DataManager, wl->m_Seat);
            wl_data_device_add_listener(wl->m_DataDevice, &g_DataDevice, w);
        }
    }

    static void Target(void*, wl_data_source*, const char*)
    {
    }

    struct ClipboardWrite
    {
        int      m_Fd;
        unsigned m_Length;
        char     m_Text[1];
    };

    // Do not block the event queue when a clipboard consumer stalls or closes its pipe.
    static void* WriteClipboard(void* data)
    {
        ClipboardWrite* request = (ClipboardWrite*)data;
        sigset_t signals;
        sigemptyset(&signals);
        sigaddset(&signals, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &signals, 0);
        pollfd descriptor = { request->m_Fd, POLLOUT, 0 };
        unsigned offset = 0;
        while (offset < request->m_Length)
        {
            if (poll(&descriptor, 1, -1) < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                break;
            }

            ssize_t sent = write(request->m_Fd, request->m_Text + offset, request->m_Length - offset);
            if (sent < 0 && (errno == EINTR || errno == EAGAIN))
            {
                continue;
            }

            if (sent <= 0)
            {
                break;
            }

            offset += sent;
        }

        close(request->m_Fd);
        free(request);
        return 0;
    }

    static void Send(void* data, wl_data_source*, const char*, int fd)
    {
        const dmArray<char>& text = ((HWindow)data)->m_Wayland->m_Clipboard;
        ClipboardWrite* request = (ClipboardWrite*)malloc(sizeof(ClipboardWrite) + text.Size());
        if (request)
        {
            request->m_Fd = fd;
            request->m_Length = text.Size();
            if (text.Size())
            {
                memcpy(request->m_Text, text.Begin(), text.Size());
            }

            pthread_t thread;
            if (pthread_create(&thread, 0, WriteClipboard, request) == 0)
            {
                pthread_detach(thread);
                return;
            }

            free(request);
        }

        close(fd);
    }

    static void Cancelled(void* data, wl_data_source* source)
    {
        LinuxWayland* wl = ((HWindow)data)->m_Wayland;
        if (wl->m_DataSource == source)
        {
            wl->m_DataSource = 0;
        }

        wl_data_source_destroy(source);
    }

    const wl_data_source_listener g_WaylandDataSourceListener = { Target, Send, Cancelled };
    void WaylandSetClipboard(HWindow w, const char* text, unsigned length)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (!wl->m_DataDevice || !wl->m_InputSerial)
        {
            return;
        }

        if (wl->m_DataSource)
        {
            wl_data_source_destroy(wl->m_DataSource);
        }

        wl->m_Clipboard.EnsureSize(length);
        if (length)
        {
            memcpy(wl->m_Clipboard.Begin(), text, length);
        }

        wl->m_DataSource = wl_data_device_manager_create_data_source(wl->m_DataManager);
        wl_data_source_add_listener(wl->m_DataSource, &g_WaylandDataSourceListener, w);
        wl_data_source_offer(wl->m_DataSource, "text/plain;charset=utf-8");
        wl_data_source_offer(wl->m_DataSource, "text/plain");
        wl_data_device_set_selection(wl->m_DataDevice, wl->m_DataSource, wl->m_InputSerial);
    }

}
