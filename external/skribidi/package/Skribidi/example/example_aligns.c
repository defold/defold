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


typedef struct aligns_context_t {
	example_t base;

	skb_font_collection_t* font_collection;
	skb_temp_alloc_t* temp_alloc;
	render_context_t* rc;

	skb_layout_cache_t* layout_cache;

	view_t view;
	bool drag_view;
	bool drag_text;

	uint8_t wrap;
	uint8_t overflow;
	uint8_t vert_trim;
	uint8_t layout_size_idx;
	uint8_t example_text_idx;

	bool show_run_details;
	float atlas_scale;

} aligns_context_t;


void aligns_destroy(void* ctx_ptr);
void aligns_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void aligns_on_char(void* ctx_ptr, unsigned int codepoint);
void aligns_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void aligns_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void aligns_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void aligns_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void* aligns_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	aligns_context_t* ctx = skb_malloc(sizeof(aligns_context_t));
	memset(ctx, 0, sizeof(aligns_context_t));

	ctx->base.create = aligns_create;
	ctx->base.destroy = aligns_destroy;
	ctx->base.on_key = aligns_on_key;
	ctx->base.on_char = aligns_on_char;
	ctx->base.on_mouse_button = aligns_on_mouse_button;
	ctx->base.on_mouse_move = aligns_on_mouse_move;
	ctx->base.on_mouse_scroll = aligns_on_mouse_scroll;
	ctx->base.on_update = aligns_on_update;

	ctx->rc = rc;
	render_reset_atlas(rc, NULL);

	ctx->wrap = SKB_WRAP_WORD;
	ctx->overflow = SKB_OVERFLOW_ELLIPSIS;
	ctx->vert_trim = SKB_VERTICAL_TRIM_DEFAULT;
	ctx->example_text_idx = 1;
	ctx->layout_size_idx = 1;

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
	aligns_destroy(ctx);
	return NULL;
}

void aligns_destroy(void* ctx_ptr)
{
	aligns_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_layout_cache_destroy(ctx->layout_cache);
	skb_font_collection_destroy(ctx->font_collection);
	skb_temp_alloc_destroy(ctx->temp_alloc);

	memset(ctx, 0, sizeof(aligns_context_t));

	skb_free(ctx);
}

static uint8_t inc_wrap(uint8_t n, uint8_t max)
{
	if ((n+1) >= max) return 0;
	return n + 1;
}

void aligns_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	aligns_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (action == GLFW_PRESS) {

		if (key == GLFW_KEY_F4)
			ctx->layout_size_idx = inc_wrap(ctx->layout_size_idx, 4);
		if (key == GLFW_KEY_F5)
			ctx->example_text_idx = inc_wrap(ctx->example_text_idx, 3);
		if (key == GLFW_KEY_F6)
			ctx->wrap = inc_wrap(ctx->wrap, 3);
		if (key == GLFW_KEY_F7)
			ctx->overflow = inc_wrap(ctx->overflow, 3);
		if (key == GLFW_KEY_F8)
			ctx->vert_trim = inc_wrap(ctx->vert_trim, 2);

		if (key == GLFW_KEY_F9) {
			ctx->show_run_details = !ctx->show_run_details;
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

void aligns_on_char(void* ctx_ptr, unsigned int codepoint)
{
	aligns_context_t* ctx = ctx_ptr;
	assert(ctx);
}

void aligns_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	aligns_context_t* ctx = ctx_ptr;
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

void aligns_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	aligns_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->drag_view) {
		view_drag_move(&ctx->view, mouse_x, mouse_y);
	}
}

void aligns_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	aligns_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

void aligns_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	aligns_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_layout_cache_compact(ctx->layout_cache);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		debug_render_text(ctx->rc, (float)view_width - 20,20, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
		skb_temp_alloc_stats_t render_stats = skb_temp_alloc_stats(render_get_temp_alloc(ctx->rc));
		debug_render_text(ctx->rc, (float)view_width - 20,40, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Render Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)render_stats.used / 1024.f, (float)render_stats.allocated / 1024.f);
	}

	const char* align_labels[] = { "Start", "Center", "End", "left", "Right", "Top", "Bottom" };
	const char* wrap_labels[] = { "None", "Word", "Word & Char" };
	const char* overflow_labels[] = { "None", "Clip", "Ellipsis" };
	const char* vert_trim_labels[] = { "Ascender to Descender", "Cap Height to Baseline" };

	// Draw visual result
	render_push_transform(ctx->rc, ctx->view.cx, ctx->view.cy, ctx->view.scale);

	const float layout_sizes[] = {
		200.f, 40.f,
		100.f, 100.f,
		300.f, 200.f,
		300.f, 80.f,
	};

	float x = 0.f;
	float y = 0.f;

	float layout_width = layout_sizes[ctx->layout_size_idx * 2 + 0];
	float layout_height = layout_sizes[ctx->layout_size_idx * 2 + 1];

	const char* example_text[] = { "Halló fjörður!", "Quick brown hamburgerfontstiv with aïoli.", "أَفَإِستَسقَينَاكُمُوها این یک" };

	for (uint8_t v = 0; v < 3; v++) {

		static const uint8_t valiang_opts[3] = { SKB_ALIGN_TOP, SKB_ALIGN_CENTER, SKB_ALIGN_BOTTOM };
		const uint8_t valign = valiang_opts[v];

		debug_render_text(ctx->rc, x - 10.f, y + layout_height * 0.5f + 6, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,128), align_labels[valign]);

		for (uint8_t h = 0; h < 5; h++) {

			static const uint8_t haliang_opts[5] = { SKB_ALIGN_START, SKB_ALIGN_CENTER, SKB_ALIGN_END, SKB_ALIGN_LEFT, SKB_ALIGN_RIGHT };
			const uint8_t halign = haliang_opts[h];

			const float tx = x + (float)halign * (layout_width + 120.f);
			const float ty = y;

			debug_render_text(ctx->rc, tx + layout_width * 0.5f, ty - 10.f, 13, RENDER_ALIGN_CENTER, skb_rgba(0,0,0,128), align_labels[halign]);
			debug_render_stroked_rect(ctx->rc, tx, ty, layout_width, layout_height, skb_rgba(255,192,0,255), -1.f);

			const skb_attribute_t layout_attributes[] = {
				skb_attribute_make_horizontal_align(halign),
				skb_attribute_make_vertical_align(valign),
				skb_attribute_make_text_wrap(ctx->wrap),
				skb_attribute_make_text_overflow(ctx->overflow),
				skb_attribute_make_vertical_trim(ctx->vert_trim),
				skb_attribute_make_list_marker(SKB_LIST_MARKER_CODEPOINT, 32, 5, 0x2022), // bullet
			};

			skb_layout_params_t params = {
				.font_collection = ctx->font_collection,
				.layout_width = layout_width,
				.layout_height = layout_height,
				.layout_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes),
			};

			const skb_attribute_t attributes[] = {
				skb_attribute_make_font_size(24.f),
				skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(0,0,0,255)),
			};

			const skb_layout_t* layout = skb_layout_cache_get_utf8(ctx->layout_cache, ctx->temp_alloc, &params, example_text[ctx->example_text_idx], -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(attributes));
			assert(layout);

			skb_rect2_t bounds = skb_layout_get_bounds(layout);
			debug_render_stroked_rect(ctx->rc, tx + bounds.x, ty + bounds.y, bounds.width, bounds.height, skb_rgba(0,0,0,64), -1.f);

			// Draw layout
			render_draw_layout(ctx->rc, NULL, tx, ty, layout, SKB_RASTERIZE_ALPHA_SDF);

			if (ctx->show_run_details) {
				debug_render_layout_lines(ctx->rc, tx, ty, layout);
				debug_render_layout_runs(ctx->rc, tx, ty, layout);
			}
		}
		y += layout_height + 120.f;
	}

	render_pop_transform(ctx->rc);

	// Draw atlas
	render_update_atlas(ctx->rc);
	debug_render_atlas_overlay(ctx->rc, 20.f, 50.f, ctx->atlas_scale, 1);

	// Draw info
	debug_render_text(ctx->rc, (float)view_width - 20.f, (float)view_height - 15.f, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,255),
		"F4: Change layout size   F5: Change example text   Wrap (F6): %s   Overflow (F7): %s   Vert trim (F8): %s   F9: Run details %s   F10: Atlas %.1f%%",
		wrap_labels[ctx->wrap], overflow_labels[ctx->overflow], vert_trim_labels[ctx->vert_trim], ctx->show_run_details ? "ON" : "OFF", ctx->atlas_scale * 100.f);
}
