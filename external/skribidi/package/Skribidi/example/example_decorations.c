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


typedef struct decorations_context_t {
	example_t base;

	skb_font_collection_t* font_collection;
	skb_temp_alloc_t* temp_alloc;
	render_context_t* rc;

	skb_layout_t* layout;

	view_t view;
	bool drag_view;
	bool drag_text;

	float atlas_scale;

} decorations_context_t;


void decorations_destroy(void* ctx_ptr);
void decorations_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void decorations_on_char(void* ctx_ptr, unsigned int codepoint);
void decorations_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void decorations_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void decorations_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void decorations_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void* decorations_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	decorations_context_t* ctx = skb_malloc(sizeof(decorations_context_t));
	memset(ctx, 0, sizeof(decorations_context_t));

	ctx->base.create = decorations_create;
	ctx->base.destroy = decorations_destroy;
	ctx->base.on_key = decorations_on_key;
	ctx->base.on_char = decorations_on_char;
	ctx->base.on_mouse_button = decorations_on_mouse_button;
	ctx->base.on_mouse_move = decorations_on_mouse_move;
	ctx->base.on_mouse_scroll = decorations_on_mouse_scroll;
	ctx->base.on_update = decorations_on_update;

	ctx->rc = rc;
	render_reset_atlas(rc, NULL);

	ctx->atlas_scale = 0.25f;

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

	ctx->temp_alloc = skb_temp_alloc_create(512*1024);
	assert(ctx->temp_alloc);

	skb_color_t ink_color = skb_rgba(64,64,64,255);

	const skb_attribute_t layout_attributes[] = {
		skb_attribute_make_text_wrap(SKB_WRAP_WORD_CHAR),
		skb_attribute_make_baseline_align(SKB_BASELINE_MIDDLE),
	};

	skb_layout_params_t params = {
		.font_collection = ctx->font_collection,
		.layout_width = 600.f,
		.layout_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes),
	};

	const skb_attribute_t deco_solid_attributes[] = {
		skb_attribute_make_font_size(25.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
		skb_attribute_make_paint_color(SKB_PAINT_DECORATION_UNDERLINE, SKB_PAINT_STATE_DEFAULT, skb_rgba(255,64,0,255)),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_UNDER, SKB_DECORATION_STYLE_SOLID, 2.f, 0.f, SKB_PAINT_DECORATION_UNDERLINE),
	};

	const skb_attribute_t deco_double_attributes[] = {
		skb_attribute_make_font_size(25.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
		skb_attribute_make_paint_color(SKB_PAINT_DECORATION_UNDERLINE, SKB_PAINT_STATE_DEFAULT, skb_rgba(255,64,0,255)),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_UNDER, SKB_DECORATION_STYLE_DOUBLE, 2.f, 0.f, SKB_PAINT_DECORATION_UNDERLINE),
	};

	const skb_attribute_t deco_dotted_attributes[] = {
		skb_attribute_make_font_size(25.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
		skb_attribute_make_paint_color(SKB_PAINT_DECORATION_UNDERLINE, SKB_PAINT_STATE_DEFAULT, skb_rgba(255,64,0,255)),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_UNDER, SKB_DECORATION_STYLE_DOTTED, 2.f, 0.f, SKB_PAINT_DECORATION_UNDERLINE),
	};

	const skb_attribute_t deco_dashed_attributes[] = {
		skb_attribute_make_font_size(25.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
		skb_attribute_make_paint_color(SKB_PAINT_DECORATION_UNDERLINE, SKB_PAINT_STATE_DEFAULT, skb_rgba(255,64,0,255)),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_UNDER, SKB_DECORATION_STYLE_DASHED, 2.f, 0.f, SKB_PAINT_DECORATION_UNDERLINE),
	};

	const skb_attribute_t deco_wavy_attributes[] = {
		skb_attribute_make_font_size(25.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
		skb_attribute_make_paint_color(SKB_PAINT_DECORATION_UNDERLINE, SKB_PAINT_STATE_DEFAULT, skb_rgba(255,64,0,255)),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_UNDER, SKB_DECORATION_STYLE_WAVY, 2.f, 0.f, SKB_PAINT_DECORATION_UNDERLINE),
	};


	skb_content_run_t runs[] = {
		skb_content_run_make_utf8("Quick fox jumps over lazy dog.\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(deco_solid_attributes), 0),
		skb_content_run_make_utf8("Quick fox jumps over lazy dog.\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(deco_double_attributes), 0),
		skb_content_run_make_utf8("Quick fox jumps over lazy dog.\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(deco_dotted_attributes), 0),
		skb_content_run_make_utf8("Quick fox jumps over lazy dog.\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(deco_dashed_attributes), 0),
		skb_content_run_make_utf8("Quick fox jumps over lazy dog.\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(deco_wavy_attributes), 0),
	};

	ctx->layout = skb_layout_create_from_runs(ctx->temp_alloc, &params, runs, SKB_COUNTOF(runs));
	assert(ctx->layout);

	ctx->view = (view_t) { .cx = 400.f, .cy = 120.f, .scale = 1.f, .zoom_level = 0.f, };

	return ctx;

error:
	decorations_destroy(ctx);
	return NULL;
}

void decorations_destroy(void* ctx_ptr)
{
	decorations_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_layout_destroy(ctx->layout);
	skb_font_collection_destroy(ctx->font_collection);
	skb_temp_alloc_destroy(ctx->temp_alloc);

	memset(ctx, 0, sizeof(decorations_context_t));

	skb_free(ctx);
}

void decorations_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	decorations_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (action == GLFW_PRESS) {
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

void decorations_on_char(void* ctx_ptr, unsigned int codepoint)
{
	decorations_context_t* ctx = ctx_ptr;
	assert(ctx);
}

void decorations_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	decorations_context_t* ctx = ctx_ptr;
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

void decorations_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	decorations_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->drag_view) {
		view_drag_move(&ctx->view, mouse_x, mouse_y);
	}
}

void decorations_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	decorations_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

void decorations_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	decorations_context_t* ctx = ctx_ptr;
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

	// Draw examples of the decoration patterns.
	{
		skb_image_atlas_t* atlas = render_get_atlas(ctx->rc);
		for (int32_t style = 0; style < 5; style++) {
			float ax = 500.f;
			float ay = style * 50.f;

			const float pattern_thickness = 5.f;
			const float pattern_offset = -ctx->view.cx / ctx->view.scale; // Offset based on view center to test offsetting
			const float pattern_length = 250.f;

			// Visual pattern size.
			{
				skb_vec2_t size = skb_rasterizer_get_decoration_pattern_size(style, pattern_thickness);
				skb_rect2_t pat_bounds = { .x = ax, .y = ay, .width = size.x, .height = size.y };
				debug_render_stroked_rect(ctx->rc, pat_bounds.x, pat_bounds.y, pat_bounds.width, pat_bounds.height, skb_rgba(255,128,64,255), -1.f);
			}

			// Pattern quad size
			{
				skb_quad_t quad = skb_image_atlas_get_decoration_quad(
					atlas, ax, ay, ctx->view.scale,
					SKB_DECORATION_LINE_UNDER, style, pattern_length, pattern_offset, pattern_thickness,
					skb_rgba(0,0,0,128), SKB_RASTERIZE_ALPHA_SDF);
				debug_render_stroked_rect(ctx->rc, quad.geom.x, quad.geom.y, quad.geom.width, quad.geom.height, skb_rgba(0,0,0,128), -1.f);
			}

			render_draw_decoration_line(ctx->rc,
				ax, ay,
				style, SKB_DECORATION_LINE_UNDER, pattern_length, pattern_offset, pattern_thickness,
				skb_rgba(0,0,0, 128), SKB_RASTERIZE_ALPHA_SDF);
		}
	}

	render_pop_transform(ctx->rc);

	// Draw atlas
	render_update_atlas(ctx->rc);
	debug_render_atlas_overlay(ctx->rc, 20.f, 50.f, ctx->atlas_scale, 1);

	// Draw info
	debug_render_text(ctx->rc, (float)view_width - 20.f, (float)view_height - 15.f, 13.f, RENDER_ALIGN_END, skb_rgba(0,0,0,255),
		"F10: Atlas %.1f%%",
		ctx->atlas_scale * 100.f);
}
