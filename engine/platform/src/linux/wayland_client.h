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

#ifndef DM_WAYLAND_CLIENT_H
#define DM_WAYLAND_CLIENT_H

#include <wayland-client-core.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>

#define DM_WL_CLIENT(F) \
    F(wl_display_connect) \
    F(wl_display_disconnect) \
    F(wl_display_roundtrip) \
    F(wl_display_dispatch_pending) \
    F(wl_display_flush) \
    F(wl_display_prepare_read) \
    F(wl_display_cancel_read) \
    F(wl_display_read_events) \
    F(wl_display_get_fd) \
    F(wl_proxy_marshal_flags) \
    F(wl_proxy_get_version) \
    F(wl_proxy_destroy) \
    F(wl_proxy_add_listener) \
    F(wl_proxy_get_user_data) \
    F(wl_proxy_set_user_data)
#define DM_WL_CURSOR(F) \
    F(wl_cursor_theme_load) \
    F(wl_cursor_theme_destroy) \
    F(wl_cursor_theme_get_cursor) \
    F(wl_cursor_image_get_buffer)
#define DM_WL_XKB(F) \
    F(xkb_context_new) \
    F(xkb_context_unref) \
    F(xkb_keymap_new_from_string) \
    F(xkb_keymap_unref) \
    F(xkb_keymap_key_repeats) \
    F(xkb_state_new) \
    F(xkb_state_unref) \
    F(xkb_state_update_mask) \
    F(xkb_state_key_get_one_sym) \
    F(xkb_state_key_get_utf8) \
    F(xkb_compose_table_new_from_locale) \
    F(xkb_compose_table_unref) \
    F(xkb_compose_state_new) \
    F(xkb_compose_state_unref) \
    F(xkb_compose_state_feed) \
    F(xkb_compose_state_get_status) \
    F(xkb_compose_state_get_utf8) \
    F(xkb_compose_state_reset)
struct WaylandClient
{
#define DM_WL_FIELD(f) __typeof__(&f) f##_;
    DM_WL_CLIENT(DM_WL_FIELD)
    DM_WL_CURSOR(DM_WL_FIELD)
    DM_WL_XKB(DM_WL_FIELD)
#undef DM_WL_FIELD
};
extern WaylandClient g_WaylandClient;
bool LoadWaylandClient();

// Bind scanner-generated inline requests to the optional client library.
#define wl_proxy_marshal_flags(...) g_WaylandClient.wl_proxy_marshal_flags_(__VA_ARGS__)
#define wl_proxy_get_version(...) g_WaylandClient.wl_proxy_get_version_(__VA_ARGS__)
#define wl_proxy_destroy(...) g_WaylandClient.wl_proxy_destroy_(__VA_ARGS__)
#define wl_proxy_add_listener(...) g_WaylandClient.wl_proxy_add_listener_(__VA_ARGS__)
#define wl_proxy_get_user_data(...) g_WaylandClient.wl_proxy_get_user_data_(__VA_ARGS__)
#define wl_proxy_set_user_data(...) g_WaylandClient.wl_proxy_set_user_data_(__VA_ARGS__)
#include "wayland-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "xdg-activation-v1-client-protocol.h"
#endif
