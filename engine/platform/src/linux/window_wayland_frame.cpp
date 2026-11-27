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
#include <dlib/log.h>
#include <dlib/math.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

namespace dmPlatform
{
    enum
    {
        TOP, RIGHT, BOTTOM, LEFT, PART_COUNT
    };
    const int BORDER = 4;
    const int TITLE_HEIGHT = 26;

    struct FrameRect
    {
        int x, y, width, height;
    };

    static void FrameRects(int width, int height, FrameRect* rects)
    {
        rects[TOP] = { -BORDER, -TITLE_HEIGHT, width + 2 * BORDER, TITLE_HEIGHT };
        rects[RIGHT] = { width, 0, BORDER, height };
        rects[BOTTOM] = { -BORDER, height, width + 2 * BORDER, BORDER };
        rects[LEFT] = { -BORDER, 0, BORDER, height };
    }

    static bool NeedsFrame(HWindow w)
    {
        LinuxWayland* wl = w->m_Wayland;
        return wl->m_ClientDecorated && wl->m_Shm && wl->m_Subcompositor && !wl->m_ShellFullscreen && !w->m_Params.m_Fullscreen;
    }

    void WaylandFrameContentSize(HWindow w, int* width, int* height)
    {
        if (NeedsFrame(w))
        {
            if (*width > 0)
            {
                *width = dmMath::Max(1, *width - 2 * BORDER);
            }

            if (*height > 0)
            {
                *height = dmMath::Max(1, *height - TITLE_HEIGHT - BORDER);
            }
        }
    }

    void WaylandDestroyFrame(LinuxWayland* wl)
    {
        for (LinuxWayland::FramePart& part : wl->m_FrameParts)
        {
            if (part.m_Surface)
            {
                if (wl->m_PointerSurface == part.m_Surface)
                {
                    wl->m_PointerSurface = 0;
                    wl->m_EnterSerial = 0;
                }

                wl_subsurface_destroy(part.m_Subsurface);
                wl_surface_destroy(part.m_Surface);
                part = {};
            }
        }

        wl->m_FrameWidth = wl->m_FrameHeight = 0;
    }

    static void BufferReleased(void* data, wl_buffer* buffer)
    {
        dmArray<wl_buffer*>& buffers = ((LinuxWayland*)data)->m_FrameBuffers;
        for (uint32_t i = 0; i < buffers.Size(); ++i)
        {
            if (buffers[i] == buffer)
            {
                buffers.EraseSwap(i);
                break;
            }
        }

        wl_buffer_destroy(buffer);
    }

    static const wl_buffer_listener g_BufferListener = { BufferReleased };

    static wl_shm_pool* FramePixels(wl_shm* shm, int size, int title_width)
    {
        FILE* file = tmpfile();
        if (!file)
        {
            return 0;
        }

        int fd = fileno(file);
        wl_shm_pool* pool = 0;
        if (ftruncate(fd, size) == 0)
        {
            void* pixels = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (pixels != MAP_FAILED)
            {
                memset(pixels, 0x44, size);
                // Draw a close control in the right-hand end of the title bar.
                if (title_width >= TITLE_HEIGHT)
                {
                    uint32_t* row = (uint32_t*)pixels + 8 * title_width + title_width - 18;
                    for (int i = 0; i < 10; ++i, row += title_width)
                    {
                        row[i] = row[9 - i] = 0xffffffff;
                    }
                }

                pool = wl_shm_create_pool(shm, fd, size);
                munmap(pixels, size);
            }
        }

        fclose(file);
        return pool;
    }

    void WaylandResizeFrame(HWindow w, int width, int height)
    {
        LinuxWayland* wl = w->m_Wayland;
        if (!NeedsFrame(w))
        {
            WaylandDestroyFrame(wl);
            xdg_surface_set_window_geometry(wl->m_XdgSurface, 0, 0, width, height);
            return;
        }

        if (width == wl->m_FrameWidth && height == wl->m_FrameHeight)
        {
            return;
        }

        // Only the perimeter needs storage, including on compositors without a viewporter.
        int64_t size = 4 * (((int64_t)width + 2 * BORDER) * (TITLE_HEIGHT + BORDER) + (int64_t)height * 2 * BORDER);
        if (size <= 0 || size > INT32_MAX)
        {
            return;
        }

        FrameRect rects[PART_COUNT];
        FrameRects(width, height, rects);
        wl_shm_pool* pool = FramePixels(wl->m_Shm, (int)size, rects[TOP].width);
        if (!pool)
        {
            dmLogWarning("Could not allocate Wayland window decorations.");
            return;
        }

        int offset = 0;
        for (unsigned i = 0; i < PART_COUNT; ++i)
        {
            LinuxWayland::FramePart& part = wl->m_FrameParts[i];
            const FrameRect& rect = rects[i];
            if (!part.m_Surface)
            {
                part.m_Surface = wl_compositor_create_surface(wl->m_Compositor);
                part.m_Subsurface = wl_subcompositor_get_subsurface(wl->m_Subcompositor, part.m_Surface, w->m_Surface);
            }

            wl_subsurface_set_position(part.m_Subsurface, rect.x, rect.y);
            wl_buffer* buffer = wl_shm_pool_create_buffer(pool, offset, rect.width, rect.height, rect.width * 4, WL_SHM_FORMAT_XRGB8888);
            // Synchronized subsurfaces can outlive this resize until their parent commits.
            if (wl->m_FrameBuffers.Full())
            {
                wl->m_FrameBuffers.OffsetCapacity(PART_COUNT);
            }

            wl->m_FrameBuffers.Push(buffer);
            wl_buffer_add_listener(buffer, &g_BufferListener, wl);
            wl_surface_attach(part.m_Surface, buffer, 0, 0);
            wl_surface_damage(part.m_Surface, 0, 0, rect.width, rect.height);
            wl_surface_commit(part.m_Surface);
            offset += rect.width * rect.height * 4;
        }

        wl_shm_pool_destroy(pool);
        wl->m_FrameWidth = width;
        wl->m_FrameHeight = height;
        xdg_surface_set_window_geometry(wl->m_XdgSurface, -BORDER, -TITLE_HEIGHT, width + 2 * BORDER, height + TITLE_HEIGHT + BORDER);
    }

    static int PointerPart(LinuxWayland* wl)
    {
        if (wl->m_PointerSurface)
        {
            for (int i = 0; i < PART_COUNT; ++i)
            {
                if (wl->m_PointerSurface == wl->m_FrameParts[i].m_Surface)
                {
                    return i;
                }
            }
        }

        return -1;
    }

    static xdg_toplevel_resize_edge ResizeEdge(LinuxWayland* wl, int part)
    {
        if (part == LEFT || part == RIGHT)
        {
            return part == LEFT ? XDG_TOPLEVEL_RESIZE_EDGE_LEFT : XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
        }

        if (part == TOP && wl->m_FramePointerY >= BORDER)
        {
            return XDG_TOPLEVEL_RESIZE_EDGE_NONE;
        }

        if (wl->m_FramePointerX < 2 * BORDER)
        {
            return part == TOP ? XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT : XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
        }

        if (wl->m_FramePointerX >= wl->m_FrameWidth)
        {
            return part == TOP ? XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT : XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
        }

        return part == TOP ? XDG_TOPLEVEL_RESIZE_EDGE_TOP : XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
    }

    const char* WaylandFrameCursor(LinuxWayland* wl)
    {
        int part = PointerPart(wl);
        if (part < 0)
        {
            return 0;
        }

        switch (ResizeEdge(wl, part))
        {
            case XDG_TOPLEVEL_RESIZE_EDGE_TOP:
            {
                return "top_side";
            }
            case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM:
            {
                return "bottom_side";
            }
            case XDG_TOPLEVEL_RESIZE_EDGE_LEFT:
            {
                return "left_side";
            }
            case XDG_TOPLEVEL_RESIZE_EDGE_RIGHT:
            {
                return "right_side";
            }
            case XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT:
            {
                return "top_left_corner";
            }
            case XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT:
            {
                return "top_right_corner";
            }
            case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT:
            {
                return "bottom_left_corner";
            }
            case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT:
            {
                return "bottom_right_corner";
            }
            default:
            {
                return "left_ptr";
            }
        }
    }

    void WaylandFrameButton(HWindow w, uint32_t serial, uint32_t button)
    {
        LinuxWayland* wl = w->m_Wayland;
        int part = PointerPart(wl);
        if (part < 0)
        {
            return;
        }

        xdg_toplevel_resize_edge edge = ResizeEdge(wl, part);
        if (button == BTN_LEFT && edge == XDG_TOPLEVEL_RESIZE_EDGE_NONE &&
            wl->m_FramePointerX >= wl->m_FrameWidth + 2 * BORDER - TITLE_HEIGHT)
        {
            WaylandRequestClose(w);
        }
        else if (wl->m_Seat && button == BTN_LEFT)
        {
            if (edge == XDG_TOPLEVEL_RESIZE_EDGE_NONE)
            {
                xdg_toplevel_move(wl->m_Toplevel, wl->m_Seat, serial);
            }
            else if (!w->m_Maximized)
            {
                xdg_toplevel_resize(wl->m_Toplevel, wl->m_Seat, serial, edge);
            }
        }
        else if (wl->m_Seat && button == BTN_RIGHT)
        {
            FrameRect rects[PART_COUNT];
            FrameRects(wl->m_FrameWidth, wl->m_FrameHeight, rects);
            xdg_toplevel_show_window_menu(wl->m_Toplevel, wl->m_Seat, serial,
                                          rects[part].x + BORDER + wl->m_FramePointerX,
                                          rects[part].y + TITLE_HEIGHT + wl->m_FramePointerY);
        }
    }

}
