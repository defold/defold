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
#include "utils.h"
#include "render.h"

#include "skb_common.h"
#include "skb_font_collection.h"
#include "skb_attribute_collection.h"
#include "skb_rasterizer.h"
#include "skb_layout.h"
#include "skb_image_atlas.h"


typedef struct baseattribs_context_t {
	example_t base;

	skb_font_collection_t* font_collection;
	skb_attribute_collection_t* attribute_collection;

	skb_temp_alloc_t* temp_alloc;
	render_context_t* rc;

	skb_layout_t* layout;
	skb_layout_t* layout_text;
	skb_layout_t* layout_ref;

	view_t view;
	bool drag_view;
	bool drag_text;

	bool show_glyph_bounds;
	float atlas_scale;

} baseattribs_context_t;


void baseattribs_destroy(void* ctx_ptr);
void baseattribs_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void baseattribs_on_char(void* ctx_ptr, unsigned int codepoint);
void baseattribs_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void baseattribs_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void baseattribs_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void baseattribs_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void* baseattribs_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	baseattribs_context_t* ctx = skb_malloc(sizeof(baseattribs_context_t));
	memset(ctx, 0, sizeof(baseattribs_context_t));

	ctx->base.create = baseattribs_create;
	ctx->base.destroy = baseattribs_destroy;
	ctx->base.on_key = baseattribs_on_key;
	ctx->base.on_char = baseattribs_on_char;
	ctx->base.on_mouse_button = baseattribs_on_mouse_button;
	ctx->base.on_mouse_move = baseattribs_on_mouse_move;
	ctx->base.on_mouse_scroll = baseattribs_on_mouse_scroll;
	ctx->base.on_update = baseattribs_on_update;

	ctx->rc = rc;
	render_reset_atlas(rc, NULL);

	ctx->atlas_scale = 0.0f;

	ctx->font_collection = skb_font_collection_create();
	assert(ctx->font_collection);

	const skb_font_create_params_t fake_italic_params = {
		.slant = SKB_DEFAULT_SLANT
	};

	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSansCondensed-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Italic.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Bold.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_PARAMS_OR_FAIL("data/IBMPlexSans-Bold.ttf", SKB_FONT_FAMILY_DEFAULT, &fake_italic_params);
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

	// Base style for the whole layout, each run can override these attributes.
	skb_color_t ink_color = skb_rgba(64,64,64,255);
	const skb_attribute_t layout_attributes[] = {
		skb_attribute_make_text_wrap(SKB_WRAP_WORD_CHAR),
		skb_attribute_make_baseline_align(SKB_BASELINE_MIDDLE),
		skb_attribute_make_font_size(25.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
	};

	skb_layout_params_t params = {
		.font_collection = ctx->font_collection,
		.layout_width = 600.f,
		.layout_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes),
	};

	const skb_attribute_t underline_attributes[] = {
		skb_attribute_make_paint_color(SKB_PAINT_DECORATION_UNDERLINE, SKB_PAINT_STATE_DEFAULT, skb_rgba(0,160,92,255)),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_UNDER, SKB_DECORATION_STYLE_SOLID, 3.f, 0.f, SKB_PAINT_DECORATION_UNDERLINE),
	};

	const skb_attribute_t italic_attributes[] = {
		skb_attribute_make_font_style(SKB_STYLE_ITALIC),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(0,160,92,255)),
	};

	const skb_attribute_t bold_attributes[] = {
		skb_attribute_make_font_weight(SKB_WEIGHT_BOLD),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(0,64,220,255)),
	};

	skb_content_run_t runs[] = {
		skb_content_run_make_utf8("Some text with ", -1, (skb_attribute_set_t){0}, 0),
		skb_content_run_make_utf8("bold", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(bold_attributes), 0),
		skb_content_run_make_utf8(" and ", -1, (skb_attribute_set_t){0}, 0),
		skb_content_run_make_utf8("italic", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(italic_attributes), 0),
		skb_content_run_make_utf8(" and ", -1, (skb_attribute_set_t){0}, 0),
		skb_content_run_make_utf8("underline", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(underline_attributes), 0),
		skb_content_run_make_utf8(".", -1, (skb_attribute_set_t){0}, 0),
	};

	ctx->layout = skb_layout_create_from_runs(ctx->temp_alloc, &params, runs, SKB_COUNTOF(runs));
	assert(ctx->layout);

	//
	// Base style with attributed text.
	//
	skb_text_t* text = skb_text_create();

	skb_text_append_utf8(text, "Yellow mellow submarine", -1, (skb_attribute_set_t){0});

	skb_text_add_attribute(text, (skb_text_range_t){ .start.offset = 0, .end.offset = 13 }, skb_attribute_make_font_weight(SKB_WEIGHT_BOLD));
	skb_text_add_attribute(text, (skb_text_range_t){ .start.offset = 7, .end.offset = 17 }, skb_attribute_make_font_style(SKB_STYLE_ITALIC));

	ctx->layout_text = skb_layout_create_from_text(ctx->temp_alloc, &params, text, (skb_attribute_set_t){0});
	assert(ctx->layout_text);

	skb_text_destroy(text);

	//
	// Attribute collection
	//
	ctx->attribute_collection = skb_attribute_collection_create();
	{
		// Add attribute sets
		skb_attribute_collection_add_set(ctx->attribute_collection, "BODY", SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes));
		skb_attribute_collection_add_set(ctx->attribute_collection, "u", SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(underline_attributes));
		skb_attribute_collection_add_set(ctx->attribute_collection, "i", SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(italic_attributes));
		skb_attribute_collection_add_set(ctx->attribute_collection, "b", SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(bold_attributes));
	}

	{
		skb_attribute_set_t body = skb_attribute_set_make_reference_by_name(ctx->attribute_collection, "BODY");
		skb_attribute_set_t underline = skb_attribute_set_make_reference_by_name(ctx->attribute_collection, "u");
		skb_attribute_set_t italic = skb_attribute_set_make_reference_by_name(ctx->attribute_collection, "i");
		skb_attribute_set_t bold = skb_attribute_set_make_reference_by_name(ctx->attribute_collection, "b");

		skb_layout_params_t params_ref = {
			.font_collection = ctx->font_collection,
			.attribute_collection = ctx->attribute_collection,
			.layout_width = 600.f,
			.layout_attributes = body,
		};

		skb_content_run_t runs_ref[] = {
			skb_content_run_make_utf8("Some text with ", -1, (skb_attribute_set_t){0}, 0),
			skb_content_run_make_utf8("bold", -1, bold, 0),
			skb_content_run_make_utf8(" and ", -1, (skb_attribute_set_t){0}, 0),
			skb_content_run_make_utf8("italic", -1, italic, 0),
			skb_content_run_make_utf8(" and ", -1, (skb_attribute_set_t){0}, 0),
			skb_content_run_make_utf8("underline", -1, underline, 0),
			skb_content_run_make_utf8(".", -1, (skb_attribute_set_t){0}, 0),
		};

		ctx->layout_ref = skb_layout_create_from_runs(ctx->temp_alloc, &params_ref, runs_ref, SKB_COUNTOF(runs_ref));
		assert(ctx->layout_ref);

	}

	ctx->view = (view_t) { .cx = 400.f, .cy = 120.f, .scale = 1.f, .zoom_level = 0.f, };

	return ctx;

error:
	baseattribs_destroy(ctx);
	return NULL;
}

void baseattribs_destroy(void* ctx_ptr)
{
	baseattribs_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_layout_destroy(ctx->layout);
	skb_layout_destroy(ctx->layout_text);
	skb_layout_destroy(ctx->layout_ref);
	skb_font_collection_destroy(ctx->font_collection);
	skb_attribute_collection_destroy(ctx->attribute_collection);
	skb_temp_alloc_destroy(ctx->temp_alloc);

	memset(ctx, 0, sizeof(baseattribs_context_t));

	skb_free(ctx);
}

void baseattribs_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	baseattribs_context_t* ctx = ctx_ptr;
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

void baseattribs_on_char(void* ctx_ptr, unsigned int codepoint)
{
	baseattribs_context_t* ctx = ctx_ptr;
	assert(ctx);
}

void baseattribs_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	baseattribs_context_t* ctx = ctx_ptr;
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

void baseattribs_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	baseattribs_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->drag_view) {
		view_drag_move(&ctx->view, mouse_x, mouse_y);
	}
}

void baseattribs_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	baseattribs_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

void baseattribs_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	baseattribs_context_t* ctx = ctx_ptr;
	assert(ctx);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		debug_render_text(ctx->rc, (float)view_width - 20,20, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
		skb_temp_alloc_stats_t render_stats = skb_temp_alloc_stats(render_get_temp_alloc(ctx->rc));
		debug_render_text(ctx->rc, (float)view_width - 20,40, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Render Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)render_stats.used / 1024.f, (float)render_stats.allocated / 1024.f);
	}

	// Draw visual result
	render_push_transform(ctx->rc, ctx->view.cx, ctx->view.cy, ctx->view.scale);

	const skb_color_t ink_color_trans = skb_rgba(32,32,32,128);

	render_draw_layout(ctx->rc, NULL, 0.f, 0.f, ctx->layout, SKB_RASTERIZE_ALPHA_SDF);

	render_draw_layout(ctx->rc, NULL, 0.f, 100.f, ctx->layout_text, SKB_RASTERIZE_ALPHA_SDF);

	render_draw_layout(ctx->rc, NULL, 0.f, 200.f, ctx->layout_ref, SKB_RASTERIZE_ALPHA_SDF);

	if (ctx->show_glyph_bounds) {
		// Draw layout details
		const skb_layout_run_t* layout_runs = skb_layout_get_layout_runs(ctx->layout);
		const int32_t layout_runs_count = skb_layout_get_layout_runs_count(ctx->layout);
		const skb_glyph_t* glyphs = skb_layout_get_glyphs(ctx->layout);
		const skb_layout_params_t* layout_params = skb_layout_get_params(ctx->layout);

		skb_rect2_t layout_bounds = skb_layout_get_bounds(ctx->layout);
		debug_render_stroked_rect(ctx->rc, layout_bounds.x, layout_bounds.y, layout_bounds.width, layout_bounds.height, skb_rgba(255,128,64,128), -1.5f);

		// Draw glyphs bounds
		for (int32_t ri = 0; ri < layout_runs_count; ri++) {
			const skb_layout_run_t* run = &layout_runs[ri];
			for (int32_t gi = run->glyph_range.start; gi < run->glyph_range.end; gi++) {
				const skb_glyph_t* glyph = &glyphs[gi];
				const float gx = glyph->offset_x;
				const float gy = glyph->offset_y;
				if (ctx->show_glyph_bounds) {
					debug_render_tick(ctx->rc, gx, gy, 5.f, ink_color_trans, -1.5f);
					skb_rect2_t bounds = skb_font_get_glyph_bounds(layout_params->font_collection, run->font_handle, glyph->gid, run->font_size);
					bounds.x += gx;
					bounds.y += gy;
					debug_render_stroked_rect(ctx->rc, bounds.x, bounds.y, bounds.width, bounds.height, skb_rgba(255,128,64,128), -1.5f);
				}
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
		ctx->show_glyph_bounds ? "ON" : "OFF", ctx->atlas_scale * 100.f);
}
