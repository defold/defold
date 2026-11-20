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

#ifndef DM_WINDOW_WAYLAND_H
#define DM_WINDOW_WAYLAND_H

#include "wayland_client.h"
#include "window_linux.h"
#include <dlib/array.h>

struct LinuxOutput
{
    HWindow    m_Window;
    wl_output* m_Output;
    uint32_t   m_Name;
    int        m_Scale, m_Refresh, m_WidthMM, m_Width;
    bool       m_Entered;
};

struct LinuxWayland
{
    wl_registry*                     m_Registry;
    wl_compositor*                   m_Compositor;
    wl_shm*                          m_Shm;
    xdg_wm_base*                     m_Shell;
    zxdg_decoration_manager_v1*      m_DecorManager;
    wp_viewporter*                   m_Viewporter;
    wp_fractional_scale_manager_v1*  m_ScaleManager;
    zwp_relative_pointer_manager_v1* m_RelativeManager;
    zwp_pointer_constraints_v1*      m_Constraints;
    zwp_idle_inhibit_manager_v1*     m_IdleManager;
    xdg_activation_v1*               m_Activation;
    wl_data_device_manager*          m_DataManager;
    wl_seat*                         m_Seat;
    uint32_t                         m_SeatName;
    dmArray<LinuxOutput*>            m_Outputs;

    xdg_surface*                 m_XdgSurface;
    xdg_toplevel*                m_Toplevel;
    zxdg_toplevel_decoration_v1* m_Decoration;
    wp_viewport*                 m_Viewport;
    wp_fractional_scale_v1*      m_Fractional;
    zwp_idle_inhibitor_v1*       m_Inhibitor;
    xdg_activation_token_v1*     m_Token;
    struct libdecor*             m_Decor;
    struct libdecor_frame*       m_Frame;
    int                          m_PendingWidth, m_PendingHeight;
    uint32_t                     m_PreferredScale;
    bool                         m_Configured, m_Unmapped, m_Lost;

    wl_keyboard*               m_Keyboard;
    xkb_context*               m_Xkb;
    xkb_keymap*                m_Keymap;
    xkb_state*                 m_KeyState;
    xkb_compose_state*         m_Compose;
    int32_t                    m_RepeatDelay, m_RepeatRate;
    uint32_t                   m_RepeatKey, m_InputSerial;
    uint64_t                   m_NextRepeat;
    wl_pointer*                m_Pointer;
    wl_cursor_theme*           m_CursorTheme;
    wl_surface*                m_CursorSurface;
    uint32_t                   m_EnterSerial;
    double                     m_Axis;
    int                        m_Discrete;
    zwp_relative_pointer_v1*   m_Relative;
    zwp_locked_pointer_v1*     m_Lock;
    wl_data_device*            m_DataDevice;
    wl_data_source*            m_DataSource;
    dmArray<char>              m_Clipboard;
    dmArray<char>              m_Title;
};

namespace dmPlatform
{
    void WaylandBindSeat(HWindow window, uint32_t name, uint32_t version);
    void WaylandReleaseSeat(HWindow window);
    void WaylandInitDataDevice(HWindow window);
    void WaylandSetClipboard(HWindow window, const char* text, unsigned length);
    void WaylandSetCursor(HWindow window);
    void WaylandReleaseCursor(HWindow window);
    void WaylandRepeat(HWindow window);
}
#endif
