// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "debug_render.h"
#include "render.h"
#include "utils.h"

#include "skb_common.h"
#include "skb_icon_collection.h"


typedef struct icons_context_t {
	example_t base;

	skb_icon_collection_t* icon_collection;
	skb_temp_alloc_t* temp_alloc;
	render_context_t* rc;

	view_t view;
	bool drag_view;

	bool show_icon_bounds;
	float atlas_scale;

} icons_context_t;


void icons_destroy(void* ctx_ptr);
void icons_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void icons_on_char(void* ctx_ptr, unsigned int codepoint);
void icons_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void icons_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void icons_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void icons_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void* icons_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	icons_context_t* ctx = skb_malloc(sizeof(icons_context_t));
	memset(ctx, 0, sizeof(icons_context_t));

	ctx->base.create = icons_create;
	ctx->base.destroy = icons_destroy;
	ctx->base.on_key = icons_on_key;
	ctx->base.on_char = icons_on_char;
	ctx->base.on_mouse_button = icons_on_mouse_button;
	ctx->base.on_mouse_move = icons_on_mouse_move;
	ctx->base.on_mouse_scroll = icons_on_mouse_scroll;
	ctx->base.on_update = icons_on_update;

	ctx->rc = rc;
	render_reset_atlas(rc, NULL);

	ctx->atlas_scale = 0.0f;
	ctx->show_icon_bounds = true;

	ctx->icon_collection = skb_icon_collection_create();
	assert(ctx->icon_collection);

	skb_icon_handle_t icon1 = skb_icon_collection_add_picosvg_icon(ctx->icon_collection, "icon", "data/grad_pico.svg");
	if (!icon1) {
		skb_debug_log("Failed to load icon1\n");
		goto error;
	}
	skb_icon_handle_t icon2 = skb_icon_collection_add_picosvg_icon(ctx->icon_collection, "astro", "data/astronaut_pico.svg");
	if (!icon2) {
		skb_debug_log("Failed to load icon2\n");
		goto error;
	}

	skb_icon_handle_t icon3 = skb_icon_collection_add_picosvg_icon(ctx->icon_collection, "pen", "data/pen_pico.svg");
	if (!icon3) {
		skb_debug_log("Failed to load icon3\n");
		goto error;
	}
	skb_icon_collection_set_is_color(ctx->icon_collection, icon3, false); // render as alpha.

	// Procedural icon
	{
		skb_icon_handle_t icon4 = skb_icon_collection_add_icon(ctx->icon_collection, "arrow", 20,20);
		if (!icon4) {
			skb_debug_log("Failed to make icon3\n");
			goto error;
		}

		skb_icon_builder_t builder = skb_icon_builder_make(ctx->icon_collection, icon4);

		skb_icon_builder_begin_shape(&builder);

		skb_icon_builder_move_to(&builder, (skb_vec2_t){18,10});
		skb_icon_builder_line_to(&builder, (skb_vec2_t){4,16});
		skb_icon_builder_quad_to(&builder, (skb_vec2_t){8,10}, (skb_vec2_t){4,4});
		skb_icon_builder_close_path(&builder);
		skb_color_stop_t stops[] = {
			{0.1f, skb_rgba(255,198,176,255) },
			{0.6f, skb_rgba(255,102,0,255) },
			{1.f, skb_rgba(163,53,53,255) },
		};
		skb_icon_builder_fill_linear_gradient(&builder, (skb_vec2_t){8,4}, (skb_vec2_t){12,16}, skb_mat2_make_identity(), SKB_SPREAD_PAD, stops, SKB_COUNTOF(stops));

		skb_icon_builder_end_shape(&builder);
	}

	// Make simiar icons with different spread modes.
	for (int32_t i = 0; i < 3; i++) {
		char name[32];
		snprintf(name, 32, "grad_%d", i);
		skb_icon_handle_t icon = skb_icon_collection_add_icon(ctx->icon_collection, name, 20,100);
		if (!icon) {
			skb_debug_log("Failed to make %s\n", name);
			goto error;
		}

		skb_gradient_spread_t spread = SKB_SPREAD_PAD;
		if (i == 1) spread = SKB_SPREAD_REPEAT;
		if (i == 2) spread = SKB_SPREAD_REFLECT;

		skb_icon_builder_t builder = skb_icon_builder_make(ctx->icon_collection, icon);

		skb_icon_builder_begin_shape(&builder);

		skb_icon_builder_move_to(&builder, (skb_vec2_t){2,2});
		skb_icon_builder_line_to(&builder, (skb_vec2_t){18,2});
		skb_icon_builder_line_to(&builder, (skb_vec2_t){18,98});
		skb_icon_builder_line_to(&builder, (skb_vec2_t){2,98});
		skb_icon_builder_close_path(&builder);
		skb_color_stop_t stops[] = {
			{0.0f, skb_rgba(255,102,0,255) },
			{0.5f, skb_rgba(238,242,33,255) },
			{1.f, skb_rgba(49,109,237,255) },
		};
		skb_icon_builder_fill_linear_gradient(&builder, (skb_vec2_t){2,25}, (skb_vec2_t){2,50}, skb_mat2_make_identity(), spread, stops, SKB_COUNTOF(stops));

		skb_icon_builder_end_shape(&builder);
	}

	ctx->temp_alloc = skb_temp_alloc_create(512*1024);
	assert(ctx->temp_alloc);

	ctx->view = (view_t) { .cx = 400.f, .cy = 120.f, .scale = 1.f, .zoom_level = 0.f, };

	return ctx;

error:
	icons_destroy(ctx);
	return NULL;
}

void icons_destroy(void* ctx_ptr)
{
	icons_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_icon_collection_destroy(ctx->icon_collection);
	skb_temp_alloc_destroy(ctx->temp_alloc);

	memset(ctx, 0, sizeof(icons_context_t));

	skb_free(ctx);
}

void icons_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	icons_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_F9) {
			ctx->show_icon_bounds = !ctx->show_icon_bounds;
		}
		if (key == GLFW_KEY_F10) {
			ctx->atlas_scale += 0.25f;
			if (ctx->atlas_scale > 1.01f)
				ctx->atlas_scale = 0.0f;
		}
	}

	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_ESCAPE) {
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
	}
}

void icons_on_char(void* ctx_ptr, unsigned int codepoint)
{
	icons_context_t* ctx = ctx_ptr;
	assert(ctx);
}

void icons_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	icons_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS) {
			if (!ctx->drag_view) {
				view_drag_start(&ctx->view, mouse_x, mouse_y);
				ctx->drag_view = true;
			}
		}
		if (action == GLFW_RELEASE) {
			if (ctx->drag_view) {
				ctx->drag_view = false;
			}
		}
	}
}

void icons_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	icons_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->drag_view) {
		view_drag_move(&ctx->view, mouse_x, mouse_y);
	}
}

void icons_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	icons_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

static float draw_icon(icons_context_t* ctx, skb_icon_handle_t icon_handle, float ox, float oy, float icon_height, skb_rasterize_alpha_mode_t alpha_mode)
{
	if (!icon_handle) return 0.f;

	skb_vec2_t icon_size = skb_icon_collection_calc_proportional_size(ctx->icon_collection, icon_handle, SKB_SIZE_AUTO, (float)icon_height);
	if (ctx->show_icon_bounds)
		debug_render_stroked_rect(ctx->rc, ox, oy, icon_size.x, icon_size.y, skb_rgba(0,0,0,64), -1.f);

	render_draw_icon(ctx->rc, ox, oy, ctx->icon_collection, icon_handle, SKB_SIZE_AUTO, icon_height, skb_rgba(255,255,255,255), alpha_mode);
	return icon_size.x + 10.f;
}


void icons_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	icons_context_t* ctx = ctx_ptr;
	assert(ctx);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		debug_render_text(ctx->rc, (float)view_width - 20,20, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
		skb_temp_alloc_stats_t render_stats = skb_temp_alloc_stats(render_get_temp_alloc(ctx->rc));
		debug_render_text(ctx->rc, (float)view_width - 20,40, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Render Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)render_stats.used / 1024.f, (float)render_stats.allocated / 1024.f);
	}

	render_push_transform(ctx->rc, ctx->view.cx, ctx->view.cy, ctx->view.scale);

	float ox = 20.f;
	float oy = 20.f;

	{

		ox = 20.f;
		debug_render_text(ctx->rc, ox, oy - 10.f, 12, RENDER_ALIGN_START, skb_rgba(0,0,0,255), "SDF");
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "icon"), ox, oy, 128.f, SKB_RASTERIZE_ALPHA_SDF);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "astro"), ox, oy, 128.f, SKB_RASTERIZE_ALPHA_SDF);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "pen"), ox, oy, 40.f, SKB_RASTERIZE_ALPHA_SDF);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "arrow"), ox, oy, 40.f, SKB_RASTERIZE_ALPHA_SDF);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "grad_0"), ox, oy, 100.f, SKB_RASTERIZE_ALPHA_SDF);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "grad_1"), ox, oy, 100.f, SKB_RASTERIZE_ALPHA_SDF);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "grad_2"), ox, oy, 100.f, SKB_RASTERIZE_ALPHA_SDF);

	}
	oy += 180.f;

	{
		ox = 20.f;
		debug_render_text(ctx->rc, ox, oy - 10.f, 13, RENDER_ALIGN_START, skb_rgba(0,0,0,255), "Alpha");
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "icon"), ox, oy, 128.f, SKB_RASTERIZE_ALPHA_MASK);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "astro"), ox, oy, 128.f, SKB_RASTERIZE_ALPHA_MASK);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "pen"), ox, oy, 40.f, SKB_RASTERIZE_ALPHA_MASK);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "arrow"), ox, oy, 40.f, SKB_RASTERIZE_ALPHA_MASK);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "grad_0"), ox, oy, 100.f, SKB_RASTERIZE_ALPHA_MASK);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "grad_1"), ox, oy, 100.f, SKB_RASTERIZE_ALPHA_MASK);
		ox += draw_icon(ctx, skb_icon_collection_find_icon(ctx->icon_collection, "grad_2"), ox, oy, 100.f, SKB_RASTERIZE_ALPHA_MASK);
	}

	render_pop_transform(ctx->rc);

	// Draw atlas
	render_update_atlas(ctx->rc);
	debug_render_atlas_overlay(ctx->rc, 20.f, 50.f, ctx->atlas_scale, 1);

	// Draw info
	debug_render_text(ctx->rc, (float)view_width - 20.f, (float)view_height - 15.f, 13.f, RENDER_ALIGN_END, skb_rgba(0,0,0,255),
		"F9: Icon details %s   F10: Atlas %.1f%%",
		ctx->show_icon_bounds ? "ON" : "OFF", ctx->atlas_scale * 100.f);
}
