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
#include <dlib/dstrings.h>
#include <dlib/path.h>
#include <dlib/utf8.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace dmPlatform
{
    // A get_session message has a header, object ID, reason and string length before the ID.
    static const size_t SESSION_ID_BUFFER_SIZE = 4096 - 20;
    static const char SESSION_SUFFIX[] = ".session";
    typedef char SessionPath[DMPATH_MAX_PATH + sizeof(SESSION_SUFFIX)];

    static void GetSessionPath(HWindow window, SessionPath& path)
    {
        dmSnPrintf(path, sizeof(path), "%s%s", GetWindowPlacementPath(window), SESSION_SUFFIX);
    }

    static bool ReadSessionId(const char* path, char (& id)[SESSION_ID_BUFFER_SIZE])
    {
        FILE* file = fopen(path, "rb");
        if (!file)
        {
            return false;
        }

        const size_t length = fread(id, 1, sizeof(id), file);
        const bool valid = !ferror(file) && length > 0 && length < sizeof(id) &&
                           !memchr(id, 0, length) && dmUtf8::Validate(id, length) == length;
        fclose(file);
        if (valid)
        {
            id[length] = 0;
        }

        return valid;
    }

    static void SessionCreated(void* data, xdg_session_v1*, const char* id)
    {
        // An opaque compositor ID lives alongside the player's size and reset token.
        SessionPath path;
        GetSessionPath((HWindow)data, path);
        char temporary[sizeof(path) + sizeof(".XXXXXX")];
        dmSnPrintf(temporary, sizeof(temporary), "%s.XXXXXX", path);
        int fd = mkstemp(temporary);
        if (fd < 0)
        {
            return;
        }

        FILE* file = fdopen(fd, "wb");
        if (!file)
        {
            close(fd);
            unlink(temporary);
            return;
        }

        bool written = fputs(id, file) >= 0;
        if (fclose(file) != 0 || !written || rename(temporary, path) != 0)
        {
            unlink(temporary);
        }
    }

    static void SessionRestored(void*, xdg_session_v1*)
    {
    }

    static void SessionReplaced(void* data, xdg_session_v1*)
    {
        LinuxWayland* wl = ((HWindow)data)->m_Wayland;
        // The new player owns this slot. An older player must not reclaim it when shown again.
        wl->m_SessionReplaced = true;
        WaylandReleaseSession(wl);
    }

    static void ToplevelRestored(void* data, xdg_toplevel_session_v1*)
    {
        ((HWindow)data)->m_Wayland->m_SessionRestored = true;
    }

    static const xdg_session_v1_listener g_Session = { SessionCreated, SessionRestored, SessionReplaced };
    static const xdg_toplevel_session_v1_listener g_ToplevelSession = { ToplevelRestored };

    bool WaylandSessionEnabled(HWindow window)
    {
        const LinuxWayland* wl = window->m_Wayland;
        return wl->m_SessionManager && !wl->m_SessionReplaced && !window->m_Params.m_Fullscreen && window->m_Placement;
    }

    void WaylandRestoreSession(HWindow window, xdg_toplevel* toplevel)
    {
        LinuxWayland* wl = window->m_Wayland;
        SessionPath path;
        GetSessionPath(window, path);
        char id[SESSION_ID_BUFFER_SIZE];
        wl->m_Session = xdg_session_manager_v1_get_session(wl->m_SessionManager, XDG_SESSION_MANAGER_V1_REASON_LAUNCH,
                                                           ReadSessionId(path, id) ? id : 0);
        if (wl->m_Session)
        {
            xdg_session_v1_add_listener(wl->m_Session, &g_Session, window);
            // This also registers a new toplevel if the compositor has no saved state for the name.
            wl->m_ToplevelSession = xdg_session_v1_restore_toplevel(wl->m_Session, toplevel, "player");
            if (wl->m_ToplevelSession)
            {
                xdg_toplevel_session_v1_add_listener(wl->m_ToplevelSession, &g_ToplevelSession, window);
            }
        }
    }

    void WaylandReleaseSession(LinuxWayland* wl)
    {
        if (wl->m_ToplevelSession)
        {
            xdg_toplevel_session_v1_destroy(wl->m_ToplevelSession);
            wl->m_ToplevelSession = 0;
        }

        if (wl->m_Session)
        {
            xdg_session_v1_destroy(wl->m_Session);
            wl->m_Session = 0;
        }

        wl->m_SessionRestored = false;
    }

}
