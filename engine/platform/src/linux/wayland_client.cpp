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

#include "wayland_client.h"
#include <dlfcn.h>

WaylandClient g_WaylandClient;

static bool LoadBindings()
{
    void* client = dlopen("libwayland-client.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!client)
    {
        return false;
    }

    void* xkb = dlopen("libxkbcommon.so.0", RTLD_NOW | RTLD_LOCAL);
    void* cursor = dlopen("libwayland-cursor.so.0", RTLD_NOW | RTLD_LOCAL);
    bool ok = xkb != 0;
#define LOOKUP(f, lib) g_WaylandClient.f##_ = (__typeof__(g_WaylandClient.f##_))dlsym(lib, #f);
#define CLIENT(f) LOOKUP(f, client) ok &= g_WaylandClient.f##_ != 0;
#define XKB(f) LOOKUP(f, xkb) ok &= g_WaylandClient.f##_ != 0;
#define CURSOR(f) LOOKUP(f, cursor)
    DM_WL_CLIENT(CLIENT)
    if (xkb)
    {
        DM_WL_XKB(XKB)
    }

    if (cursor)
    {
        DM_WL_CURSOR(CURSOR)
    }

#undef CLIENT
#undef XKB
#undef CURSOR
#undef LOOKUP
    if (!ok)
    {
        if (cursor)
        {
            dlclose(cursor);
        }

        if (xkb)
        {
            dlclose(xkb);
        }

        dlclose(client);
    }

    return ok;
}

bool LoadWaylandClient()
{
    static const bool available = LoadBindings();
    return available;
}
