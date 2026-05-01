// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include "skb_common.h"
#include "skb_layout.h"
#include "skb_rasterizer.h"

typedef struct view_t {
	float cx;
	float cy;
	float scale;
	float zoom_level;
	float viewport_width;
	float viewport_height;

	// Drag state
	float drag_start_mx;
	float drag_start_my;
	float drag_start_vx;
	float drag_start_vy;
} view_t;

static inline skb_vec2_t view_transform_pt(view_t* view, skb_vec2_t pt)
{
	return (skb_vec2_t) {
		.x = pt.x * view->scale + view->cx,
		.y = pt.y * view->scale + view->cy,
	};
}

static inline skb_rect2_t view_transform_rect(view_t* view, skb_rect2_t r)
{
	return (skb_rect2_t) {
		.x = r.x * view->scale + view->cx,
		.y = r.y * view->scale + view->cy,
		.width = r.width * view->scale,
		.height = r.height * view->scale,
	};
}

static inline float view_transform_x(view_t* view, float x)
{
	return x * view->scale + view->cx;
}

static inline float view_transform_y(view_t* view, float y)
{
	return y * view->scale + view->cy;
}

static inline void view_drag_start(view_t* view, float mouse_x, float mouse_y)
{
	view->drag_start_mx = mouse_x;
	view->drag_start_my = mouse_y;
	view->drag_start_vx = view->cx;
	view->drag_start_vy = view->cy;
}

static inline void view_drag_move(view_t* view, float mouse_x, float mouse_y)
{
	const float dx = mouse_x - view->drag_start_mx;
	const float dy = mouse_y - view->drag_start_my;
	view->cx = view->drag_start_vx + dx; // / view->scale;
	view->cy = view->drag_start_vy + dy; // / view->scale;
}

static inline void view_scroll_zoom(view_t* view, float mouse_x, float mouse_y, float delta_scroll)
{
	// Calculate view origin relation to mouse pos in unscale space.
	float old_rel_view_x = (view->cx - mouse_x) / view->scale;
	float old_rel_view_y = (view->cy - mouse_y) / view->scale;

	view->zoom_level = skb_clampf(view->zoom_level + delta_scroll, -8.f, 8.f);
	view->scale = powf(1.5f, view->zoom_level);

	// Calculate view origin from mouse pos, relative view pos, and new scale.
	// This centers the zoom around the mouse.
	view->cx = mouse_x + old_rel_view_x * view->scale;
	view->cy = mouse_y + old_rel_view_y * view->scale;
}

static inline skb_rect2_t calc_decoration_rect(const skb_decoration_t* decoration, const skb_attribute_decoration_t attr_decoration)
{
	float offset_x = decoration->offset_x;
	float offset_y = decoration->offset_y;
	skb_vec2_t size = skb_rasterizer_get_decoration_pattern_size(attr_decoration.style, decoration->thickness);
	if (attr_decoration.position == SKB_DECORATION_OVERLINE)
		offset_y -= size.y; // Above the position.
	else if (attr_decoration.position == SKB_DECORATION_THROUGHLINE)
		offset_y -= size.y * 0.5f; // Center.
	skb_rect2_t rect = {0};
	rect.x = offset_x;
	rect.y = offset_y;
	rect.width = decoration->length;
	rect.height = decoration->thickness;
	return rect;
}

//
// Atlas draw helpers
//
typedef struct skb_image_atlas_t skb_image_atlas_t;
void debug_draw_atlas(skb_image_atlas_t* atlas, float sx, float sy, float scale, int32_t columns);


typedef struct GLFWwindow GLFWwindow;

typedef void* example_create_t(void);
typedef void example_destroy_t(void* ctx_ptr);
typedef void example_on_key_t(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
typedef void example_on_char_t(void* ctx_ptr, unsigned int codepoint);
typedef void example_on_mouse_button_t(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
typedef void example_on_mouse_move_t(void* ctx_ptr, float mouse_x, float mouse_y);
typedef void example_on_mouse_scroll_t(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
typedef void example_on_update_t(void* ctx_ptr, int32_t view_width, int32_t view_height);

typedef struct example_t {
	example_create_t* create;
	example_destroy_t* destroy;
	example_on_key_t* on_key;
	example_on_char_t* on_char;
	example_on_mouse_button_t* on_mouse_button;
	example_on_mouse_move_t* on_mouse_move;
	example_on_mouse_scroll_t* on_mouse_scroll;
	example_on_update_t* on_update;
} example_t;

#endif // UTILS_H
