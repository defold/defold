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
#include "skb_font_collection.h"
#include "skb_layout.h"
#include "skb_layout_cache.h"
#include "skb_rasterizer.h"
#include "skb_image_atlas.h"


typedef struct cached_context_t {
	example_t base;

	skb_font_collection_t* font_collection;
	skb_temp_alloc_t* temp_alloc;
	render_context_t* rc;

	skb_layout_cache_t* layout_cache;

	view_t view;
	bool drag_view;
	bool drag_text;

	bool show_glyph_bounds;
	float atlas_scale;

} cached_context_t;


void cached_destroy(void* ctx_ptr);
void cached_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void cached_on_char(void* ctx_ptr, unsigned int codepoint);
void cached_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void cached_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void cached_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void cached_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void* cached_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	cached_context_t* ctx = skb_malloc(sizeof(cached_context_t));
	memset(ctx, 0, sizeof(cached_context_t));

	ctx->base.create = cached_create;
	ctx->base.destroy = cached_destroy;
	ctx->base.on_key = cached_on_key;
	ctx->base.on_char = cached_on_char;
	ctx->base.on_mouse_button = cached_on_mouse_button;
	ctx->base.on_mouse_move = cached_on_mouse_move;
	ctx->base.on_mouse_scroll = cached_on_mouse_scroll;
	ctx->base.on_update = cached_on_update;

	ctx->rc = rc;
	render_reset_atlas(rc, NULL);

	ctx->atlas_scale = 0.0f;

	ctx->font_collection = skb_font_collection_create();
	assert(ctx->font_collection);

	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Italic.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Bold.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSansArabic-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSansJP-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSansKR-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSansDevanagari-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoSansBrahmi-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoSerifBalinese-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoSansTamil-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoSansBengali-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoSansThai-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoColorEmoji-Regular.ttf", SKB_FONT_FAMILY_EMOJI);

	ctx->temp_alloc = skb_temp_alloc_create(512*1024);
	assert(ctx->temp_alloc);

	ctx->layout_cache = skb_layout_cache_create();
	assert(ctx->layout_cache);

	ctx->view = (view_t) { .cx = 400.f, .cy = 120.f, .scale = 1.f, .zoom_level = 0.f, };

	return ctx;

error:
	cached_destroy(ctx);
	return NULL;
}

void cached_destroy(void* ctx_ptr)
{
	cached_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_layout_cache_destroy(ctx->layout_cache);
	skb_font_collection_destroy(ctx->font_collection);
	skb_temp_alloc_destroy(ctx->temp_alloc);

	memset(ctx, 0, sizeof(cached_context_t));

	skb_free(ctx);
}

void cached_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	cached_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_F9) {
			ctx->show_glyph_bounds = !ctx->show_glyph_bounds;
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

void cached_on_char(void* ctx_ptr, unsigned int codepoint)
{
	cached_context_t* ctx = ctx_ptr;
	assert(ctx);
}

void cached_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	cached_context_t* ctx = ctx_ptr;
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

void cached_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	cached_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->drag_view) {
		view_drag_move(&ctx->view, mouse_x, mouse_y);
	}
}

void cached_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	cached_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

static void render_cached_text(cached_context_t* ctx, float x, float y, float font_size, skb_weight_t font_weight, skb_color_t color, const char* text)
{
	const skb_attribute_t layout_attributes[] = {
		skb_attribute_make_baseline_align(SKB_BASELINE_MIDDLE),
	};

	skb_layout_params_t params = {
		.font_collection = ctx->font_collection,
		.layout_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes),
	};

	const skb_attribute_t attributes[] = {
		skb_attribute_make_font_size(font_size),
		skb_attribute_make_font_weight(font_weight),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, color),
	};

	const skb_layout_t* layout = skb_layout_cache_get_utf8(ctx->layout_cache, ctx->temp_alloc, &params, text, -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(attributes));
	assert(layout);

	// Draw layout
	render_draw_layout(ctx->rc, NULL, x, y, layout, SKB_RASTERIZE_ALPHA_SDF);
}

void cached_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	cached_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_layout_cache_compact(ctx->layout_cache);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		debug_render_text(ctx->rc, (float)view_width - 20,20, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
		skb_temp_alloc_stats_t render_stats = skb_temp_alloc_stats(render_get_temp_alloc(ctx->rc));
		debug_render_text(ctx->rc, (float)view_width - 20,40, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Render Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)render_stats.used / 1024.f, (float)render_stats.allocated / 1024.f);
	}

	render_push_transform(ctx->rc, ctx->view.cx, ctx->view.cy, ctx->view.scale);

	// Draw visual result
	const skb_color_t ink_color_trans = skb_rgba(32,32,32,128);

	render_cached_text(ctx, 0.f, 0.f, 15.f, SKB_WEIGHT_NORMAL, ink_color_trans, "Moikka");
	render_cached_text(ctx, 0.f, 20.f, 35.f, SKB_WEIGHT_BOLD, skb_rgba(255,0,0,255), "Tsuiba! 123");
	render_cached_text(ctx, 0.f, 70.f, 15.f, SKB_WEIGHT_NORMAL, skb_rgba(255,0,0,255), "😬👀🚨");

	render_pop_transform(ctx->rc);

	// Draw atlas
	render_update_atlas(ctx->rc);
	debug_render_atlas_overlay(ctx->rc, 20.f, 50.f, ctx->atlas_scale, 1);

	// Draw info
	debug_render_text(ctx->rc, (float)view_width - 20.f, (float)view_height - 15.f, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,255),
		"F9: Glyph details %s   F10: Atlas %.1f%%",
		ctx->show_glyph_bounds ? "ON" : "OFF", ctx->atlas_scale * 100.f);
}
