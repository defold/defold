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
#include "skb_rasterizer.h"
#include "skb_layout.h"
#include "skb_image_atlas.h"


typedef struct inlineobj_context_t {
	example_t base;

	skb_font_collection_t* font_collection;
	skb_icon_collection_t* icon_collection;
	skb_temp_alloc_t* temp_alloc;
	render_context_t* rc;

	skb_layout_t* layout;

	view_t view;
	bool drag_view;
	bool drag_text;

	bool show_details;
	float atlas_scale;

} inlineobj_context_t;


void inlineobj_destroy(void* ctx_ptr);
void inlineobj_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void inlineobj_on_char(void* ctx_ptr, unsigned int codepoint);
void inlineobj_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void inlineobj_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void inlineobj_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void inlineobj_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void* inlineobj_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	inlineobj_context_t* ctx = skb_malloc(sizeof(inlineobj_context_t));
	memset(ctx, 0, sizeof(inlineobj_context_t));

	ctx->base.create = inlineobj_create;
	ctx->base.destroy = inlineobj_destroy;
	ctx->base.on_key = inlineobj_on_key;
	ctx->base.on_char = inlineobj_on_char;
	ctx->base.on_mouse_button = inlineobj_on_mouse_button;
	ctx->base.on_mouse_move = inlineobj_on_mouse_move;
	ctx->base.on_mouse_scroll = inlineobj_on_mouse_scroll;
	ctx->base.on_update = inlineobj_on_update;

	ctx->rc = rc;
	render_reset_atlas(rc, NULL);

	ctx->show_details = true;
	ctx->atlas_scale = 0.0f;

	ctx->font_collection = skb_font_collection_create();
	assert(ctx->font_collection);

	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSansCondensed-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
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

	ctx->icon_collection = skb_icon_collection_create();
	assert(ctx->icon_collection);

	skb_icon_handle_t icon_astro = skb_icon_collection_add_picosvg_icon(ctx->icon_collection, "astro", "data/astronaut_pico.svg");
	if (!icon_astro) {
		skb_debug_log("Failed to load icon_astro\n");
		goto error;
	}
	skb_icon_handle_t icon_pen = skb_icon_collection_add_picosvg_icon(ctx->icon_collection, "pen", "data/pen_pico.svg");
	if (!icon_pen) {
		skb_debug_log("Failed to load icon3\n");
		goto error;
	}
	skb_icon_collection_set_is_color(ctx->icon_collection, icon_pen, false); // render as alpha.

	ctx->temp_alloc = skb_temp_alloc_create(512*1024);
	assert(ctx->temp_alloc);

	skb_color_t ink_color = skb_rgba(64,64,64,255);

	const skb_attribute_t layout_attributes[] = {
		skb_attribute_make_text_wrap(SKB_WRAP_WORD_CHAR),
	};

	skb_layout_params_t params = {
		.font_collection = ctx->font_collection,
		.icon_collection = ctx->icon_collection,
		.layout_width = 600.f,
		.layout_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes),
	};

	const skb_attribute_t text_attributes[] = {
		skb_attribute_make_font_size(25.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
	};

	const skb_attribute_t text2_attributes[] = {
		skb_attribute_make_font_size(50.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
	};

	static const float object_size = 50.f;
	const skb_attribute_t object_attributes[] = {
		skb_attribute_make_object_align(0.5f, SKB_OBJECT_ALIGN_TEXT_BEFORE, SKB_BASELINE_CENTRAL),
		skb_attribute_make_inline_padding(10,10,0,0),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(255,128,128,255)),
	};

	const skb_attribute_t object2_attributes[] = {
		skb_attribute_make_object_align(0.5f, SKB_OBJECT_ALIGN_TEXT_AFTER, SKB_BASELINE_CENTRAL),
		skb_attribute_make_inline_padding(10,10,0,0),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(128,220,128,255)),
	};

	const skb_attribute_t object3_attributes[] = {
		skb_attribute_make_object_align(0.65f, SKB_OBJECT_ALIGN_SELF, SKB_BASELINE_ALPHABETIC),
		skb_attribute_make_inline_padding(10,10,0,0),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(128,128,255,255)),
	};

	const skb_attribute_t icon_attributes[] = {
		skb_attribute_make_object_align(0.5f, SKB_OBJECT_ALIGN_TEXT_AFTER_OR_BEFORE, SKB_BASELINE_CENTRAL),
		skb_attribute_make_inline_padding(5,5,5,5),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(32,32,220,255)),
	};

	skb_content_run_t runs[] = {
		skb_content_run_make_utf8("Djúpur", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(text_attributes), 0),
		skb_content_run_make_object(1, object_size, object_size, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(object_attributes), 0),
//		skb_text_run_make_utf8(" این یک.\n", -1, attributes_text2, SKB_COUNTOF(attributes_text2)),
//		skb_text_run_make_utf8(" 呼んでいた.\n", -1, attributes_text2, SKB_COUNTOF(attributes_text2)),
		skb_content_run_make_utf8("Fjörður.\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(text2_attributes), 0),

		skb_content_run_make_utf8("Djúpur", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(text_attributes), 0),
		skb_content_run_make_object(2, object_size, object_size, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(object2_attributes), 0),
		skb_content_run_make_utf8("Fjörður.\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(text2_attributes), 0),

		skb_content_run_make_utf8("Djúpur", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(text_attributes), 0),
		skb_content_run_make_object(3, object_size, object_size, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(object3_attributes), 0),
		skb_content_run_make_utf8("Fjörður.\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(text2_attributes), 0),

		skb_content_run_make_icon(skb_icon_collection_find_icon(ctx->icon_collection, "astro"), SKB_SIZE_AUTO, object_size, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(icon_attributes), 0),
		skb_content_run_make_utf8("Icon and two", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(text_attributes), 0),
		skb_content_run_make_icon(skb_icon_collection_find_icon(ctx->icon_collection, "pen"), SKB_SIZE_AUTO, object_size * 0.75f, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(icon_attributes), 0),
	};

	ctx->layout = skb_layout_create_from_runs(ctx->temp_alloc, &params, runs, SKB_COUNTOF(runs));
	assert(ctx->layout);

	ctx->view = (view_t) { .cx = 400.f, .cy = 120.f, .scale = 1.f, .zoom_level = 0.f, };

	return ctx;

error:
	inlineobj_destroy(ctx);
	return NULL;
}

void inlineobj_destroy(void* ctx_ptr)
{
	inlineobj_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_layout_destroy(ctx->layout);
	skb_font_collection_destroy(ctx->font_collection);
	skb_icon_collection_destroy(ctx->icon_collection);
	skb_temp_alloc_destroy(ctx->temp_alloc);

	memset(ctx, 0, sizeof(inlineobj_context_t));

	skb_free(ctx);
}

void inlineobj_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	inlineobj_context_t* ctx = ctx_ptr;
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

void inlineobj_on_char(void* ctx_ptr, unsigned int codepoint)
{
	inlineobj_context_t* ctx = ctx_ptr;
	assert(ctx);
}

void inlineobj_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	inlineobj_context_t* ctx = ctx_ptr;
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

void inlineobj_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	inlineobj_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->drag_view) {
		view_drag_move(&ctx->view, mouse_x, mouse_y);
	}
}

void inlineobj_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	inlineobj_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

void inlineobj_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	inlineobj_context_t* ctx = ctx_ptr;
	assert(ctx);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		debug_render_text(ctx->rc, (float)view_width - 20,20, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
		skb_temp_alloc_stats_t render_stats = skb_temp_alloc_stats(render_get_temp_alloc(ctx->rc));
		debug_render_text(ctx->rc, (float)view_width - 20,40, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Render Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)render_stats.used / 1024.f, (float)render_stats.allocated / 1024.f);
	}

	render_push_transform(ctx->rc, ctx->view.cx, ctx->view.cy, ctx->view.scale);

	// Draw visual result
	render_draw_layout(ctx->rc, NULL, 0, 0, ctx->layout, SKB_RASTERIZE_ALPHA_SDF);

	// Draw objects
	{
		const skb_layout_run_t* layout_runs = skb_layout_get_layout_runs(ctx->layout);
		const int32_t layout_runs_count = skb_layout_get_layout_runs_count(ctx->layout);
		const skb_layout_params_t* params = skb_layout_get_params(ctx->layout);

		for (int32_t ri = 0; ri < layout_runs_count; ri++) {
			const skb_layout_run_t* run = &layout_runs[ri];
			const skb_attribute_set_t run_attributes = skb_layout_get_layout_run_attributes(ctx->layout, run);

			skb_attribute_paint_t paint = skb_attributes_get_paint(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, run_attributes, params->attribute_collection);

			if (run->type == SKB_CONTENT_RUN_OBJECT) {
				skb_rect2_t object_rect = skb_layout_get_layout_run_content_bounds(ctx->layout, run);

				// Draw object
				const skb_attribute_object_align_t attr_object_align = skb_attributes_get_object_align(run_attributes, params->attribute_collection);
				debug_render_filled_rect(ctx->rc, object_rect.x, object_rect.y, object_rect.width, object_rect.height, paint.color);

				// Draw baseline
				const float baseline = object_rect.height * attr_object_align.baseline_ratio;
				const float y = object_rect.y + baseline;
				debug_render_line(ctx->rc, object_rect.x, y, object_rect.x + object_rect.width, y, skb_rgba(255,255,255,255), 2.f);
			}
		}
	}

	if (ctx->show_details) {
		// Draw layout details
		const skb_layout_line_t* lines = skb_layout_get_lines(ctx->layout);
		const int32_t lines_count = skb_layout_get_lines_count(ctx->layout);
		const skb_layout_run_t* layout_runs = skb_layout_get_layout_runs(ctx->layout);
		const int32_t layout_runs_count = skb_layout_get_layout_runs_count(ctx->layout);
		const skb_layout_params_t* params = skb_layout_get_params(ctx->layout);

		// Draw line baselines
		for (int32_t li = 0; li < lines_count; li++) {
			const skb_layout_line_t* line = &lines[li];
			float min_x = line->bounds.x;
			float max_x = line->bounds.x + line->bounds.width;
			debug_render_line(ctx->rc, min_x, line->baseline, max_x, line->baseline, skb_rgba(0,0,0,128), 1.f);

			debug_render_stroked_rect(ctx->rc, line->bounds.x, line->bounds.y, line->bounds.width, line->bounds.height, skb_rgba(0,0,255,255), -1.f);
		}

		for (int32_t ri = 0; ri < layout_runs_count; ri++) {
			const skb_layout_run_t* run = &layout_runs[ri];
			const skb_attribute_set_t run_attributes = skb_layout_get_layout_run_attributes(ctx->layout, run);

			debug_render_stroked_rect(ctx->rc, run->bounds.x, run->bounds.y, run->bounds.width, run->bounds.height, skb_rgba(0,128,220,255), -1.f);

			if (run->type == SKB_CONTENT_RUN_ICON) {

				skb_rect2_t icon_rect = skb_layout_get_layout_run_content_bounds(ctx->layout, run);
				debug_render_stroked_rect(ctx->rc, icon_rect.x, icon_rect.y, icon_rect.width, icon_rect.height, skb_rgba(0,0,0,128), -1.f);
			}
		}
	}

	render_pop_transform(ctx->rc);

	// Draw atlas
	render_update_atlas(ctx->rc);
	debug_render_atlas_overlay(ctx->rc, 20.f, 50.f, ctx->atlas_scale, 1);

	// Draw info
	debug_render_text(ctx->rc, (float)view_width - 20.f, (float)view_height - 15.f, 13.f, RENDER_ALIGN_END, skb_rgba(0,0,0,255),
		"F9: Glyph details %s   F10: Atlas %.1f%%",
		ctx->show_details ? "ON" : "OFF",
		ctx->atlas_scale * 100.f);
}
