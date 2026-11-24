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
#include "skb_rasterizer.h"
#include "skb_image_atlas.h"


typedef struct culling_context_t {
	example_t base;

	skb_font_collection_t* font_collection;
	skb_temp_alloc_t* temp_alloc;
	render_context_t* rc;

	skb_layout_t* layout;

	view_t view;
	bool drag_view;
	bool drag_text;

	bool show_details;
	float atlas_scale;

} culling_context_t;


void culling_destroy(void* ctx_ptr);
void culling_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void culling_on_char(void* ctx_ptr, unsigned int codepoint);
void culling_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void culling_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void culling_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void culling_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void* culling_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	culling_context_t* ctx = skb_malloc(sizeof(culling_context_t));
	memset(ctx, 0, sizeof(culling_context_t));

	ctx->base.create = culling_create;
	ctx->base.destroy = culling_destroy;
	ctx->base.on_key = culling_on_key;
	ctx->base.on_char = culling_on_char;
	ctx->base.on_mouse_button = culling_on_mouse_button;
	ctx->base.on_mouse_move = culling_on_mouse_move;
	ctx->base.on_mouse_scroll = culling_on_mouse_scroll;
	ctx->base.on_update = culling_on_update;

	ctx->rc = rc;
	render_reset_atlas(rc, NULL);

	ctx->atlas_scale = 0.0f;
	ctx->show_details = true;

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


	// Render text
	const skb_attribute_t layout_attributes[] = {
		skb_attribute_make_text_wrap(SKB_WRAP_WORD_CHAR),
		skb_attribute_make_baseline_align(SKB_BASELINE_MIDDLE),
	};

	const skb_color_t ink_color = skb_rgba(32,32,32,255);
	skb_layout_params_t params = {
		.font_collection = ctx->font_collection,
		.layout_width = 400.f,
		.layout_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes),
	};

	const skb_attribute_t attributes[] = {
		skb_attribute_make_text_wrap(SKB_WRAP_WORD_CHAR),
		skb_attribute_make_baseline_align(SKB_BASELINE_MIDDLE),
		skb_attribute_make_font_size(24.f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
	};

	const char* text = "Hamburgerfontstiv 🤣🥰💀✌️🌴🐢🐐🍄⚽🍻👑📸 این یک تست است 😬👀🚨🏡🕊️🏆😻🌟私はその人を常に先生と 呼んでいた。";

	ctx->layout = skb_layout_create_utf8(ctx->temp_alloc, &params, text, -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(attributes));
	assert(ctx->layout);

	ctx->view = (view_t) { .cx = 400.f, .cy = 120.f, .scale = 1.f, .zoom_level = 0.f, };

	return ctx;

error:
	culling_destroy(ctx);
	return NULL;
}

void culling_destroy(void* ctx_ptr)
{
	culling_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_layout_destroy(ctx->layout);
	skb_font_collection_destroy(ctx->font_collection);
	skb_temp_alloc_destroy(ctx->temp_alloc);

	memset(ctx, 0, sizeof(culling_context_t));

	skb_free(ctx);
}

void culling_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	culling_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_F9) {
			ctx->show_details = !ctx->show_details;
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

void culling_on_char(void* ctx_ptr, unsigned int codepoint)
{
	culling_context_t* ctx = ctx_ptr;
	assert(ctx);
}

void culling_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	culling_context_t* ctx = ctx_ptr;
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

void culling_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	culling_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->drag_view) {
		view_drag_move(&ctx->view, mouse_x, mouse_y);
	}
}

void culling_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	culling_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

static void render_culling_text(culling_context_t* ctx, float x, float y, float font_size, skb_weight_t font_weight, skb_color_t color, const char* text)
{
}

void culling_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	culling_context_t* ctx = ctx_ptr;
	assert(ctx);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		debug_render_text(ctx->rc, (float)view_width - 20,20, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
		skb_temp_alloc_stats_t render_stats = skb_temp_alloc_stats(render_get_temp_alloc(ctx->rc));
		debug_render_text(ctx->rc, (float)view_width - 20,40, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Render Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)render_stats.used / 1024.f, (float)render_stats.allocated / 1024.f);
	}

	static const float view_inset_x = 400.f;
	static const float view_inset_y = 200.f;
	skb_rect2_t view = {
		.x = view_inset_x,
		.y = view_inset_y,
		.width = skb_maxf(0.f, (float)view_width - view_inset_x * 2.f),
		.height = skb_maxf(0.f, (float)view_height - view_inset_y * 2.f),
	};

	// Render viewport visualization
	{
		float x = view.x;
		x = debug_render_text(ctx->rc, x,view.y - 10, 13, RENDER_ALIGN_START, skb_rgba(255,64,64,220), "Viewport");
		debug_render_text(ctx->rc, x + 10,view.y - 10, 13, RENDER_ALIGN_START, skb_rgba(255,64,64,128), "(items outside will be culled)");
		debug_render_dashed_rect(ctx->rc, view.x, view.y, view.width, view.height, -10.f, skb_rgba(255, 64, 64, 220), -2.f);
	}

	render_push_transform(ctx->rc, ctx->view.cx, ctx->view.cy, ctx->view.scale);

	// Calculate screen space viewport in current local coordinates.
	const skb_rect2_t local_view = render_inv_transform_rect(ctx->rc, view);

	// Draw detailed culling bounds visualization
	if (ctx->show_details) {
		const skb_layout_line_t* lines = skb_layout_get_lines(ctx->layout);
		const int32_t lines_count = skb_layout_get_lines_count(ctx->layout);
		const skb_layout_run_t* layout_runs = skb_layout_get_layout_runs(ctx->layout);
		const skb_glyph_t* glyphs = skb_layout_get_glyphs(ctx->layout);

		// Draw glyphs
		for (int32_t li = 0; li <lines_count; li++) {
			const skb_layout_line_t* line = &lines[li];

			debug_render_stroked_rect(ctx->rc,
				line->culling_bounds.x, line->culling_bounds.y, line->culling_bounds.width, line->culling_bounds.height,
				skb_rgba(255,64,64,220), -2.f);

				for (int32_t ri = line->layout_run_range.start; ri < line->layout_run_range.end; ri++) {
					const skb_layout_run_t* run = &layout_runs[ri];

				if (run->type == SKB_CONTENT_RUN_OBJECT || run->type == SKB_CONTENT_RUN_ICON) {
					// Object or Icon
					debug_render_filled_rect(ctx->rc,
						run->bounds.x, run->bounds.y, run->bounds.width, run->bounds.height,
						skb_rgba(255,64,64,32));
				} else {
					// Text
					for (int32_t gi = run->glyph_range.start; gi < run->glyph_range.end; gi++) {
						const skb_glyph_t* glyph = &glyphs[gi];
						debug_render_filled_rect(ctx->rc,
							glyph->offset_x + line->common_glyph_bounds.x, glyph->offset_y + line->common_glyph_bounds.y, line->common_glyph_bounds.width, line->common_glyph_bounds.height,
							skb_rgba(255,64,64,32));
					}
				}
			}
		}
	}

	// Draw layout
	render_draw_layout(ctx->rc, &local_view, 0.f, 0.f, ctx->layout, SKB_RASTERIZE_ALPHA_SDF);

	render_pop_transform(ctx->rc);

	// Draw atlas
	render_update_atlas(ctx->rc);
	debug_render_atlas_overlay(ctx->rc, 20.f, 50.f, ctx->atlas_scale, 1);

	// Draw info
	debug_render_text(ctx->rc, (float)view_width - 20.f, (float)view_height - 15.f, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,255),
		"F9: Glyph details %s   F10: Atlas %.1f%%",
		ctx->show_details ? "ON" : "OFF", ctx->atlas_scale * 100.f);
}
