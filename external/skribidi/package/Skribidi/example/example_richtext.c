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
#include "skb_rasterizer.h"
#include "skb_layout.h"
#include "skb_image_atlas.h"


typedef struct richtext_context_t {
	example_t base;

	skb_font_collection_t* font_collection;
	skb_temp_alloc_t* temp_alloc;
	render_context_t* rc;

	skb_layout_t* layout;

	view_t view;
	bool drag_view;
	bool drag_text;

	bool show_glyph_bounds;
	float atlas_scale;

} richtext_context_t;


void richtext_destroy(void* ctx_ptr);
void richtext_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void richtext_on_char(void* ctx_ptr, unsigned int codepoint);
void richtext_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void richtext_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void richtext_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void richtext_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

enum {
	PAINT_DECORATION_0 = SKB_TAG('d','e','c','0'),
	PAINT_DECORATION_1 = SKB_TAG('d','e','c','1'),
	PAINT_DECORATION_2 = SKB_TAG('d','e','c','2'),
	PAINT_DECORATION_3 = SKB_TAG('d','e','c','3'),
};

void* richtext_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	richtext_context_t* ctx = skb_malloc(sizeof(richtext_context_t));
	memset(ctx, 0, sizeof(richtext_context_t));

	ctx->base.create = richtext_create;
	ctx->base.destroy = richtext_destroy;
	ctx->base.on_key = richtext_on_key;
	ctx->base.on_char = richtext_on_char;
	ctx->base.on_mouse_button = richtext_on_mouse_button;
	ctx->base.on_mouse_move = richtext_on_mouse_move;
	ctx->base.on_mouse_scroll = richtext_on_mouse_scroll;
	ctx->base.on_update = richtext_on_update;

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
		skb_attribute_make_lang("zh-hans"),
		skb_attribute_make_text_wrap(SKB_WRAP_WORD_CHAR),
		skb_attribute_make_baseline_align(SKB_BASELINE_MIDDLE),
	};

	skb_layout_params_t params = {
		.font_collection = ctx->font_collection,
		.layout_width = 600.f,
		.layout_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes)
	};

	const skb_attribute_t small_attributes[] = {
		skb_attribute_make_font_size(15.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
	};

	const skb_attribute_t ja_jp_attributes[] = {
		skb_attribute_make_font_size(15.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_lang("ja-jp"),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
	};

	const skb_attribute_t deco1_attributes[] = {
		skb_attribute_make_font_size(15.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
		skb_attribute_make_paint_color(PAINT_DECORATION_0, SKB_PAINT_STATE_DEFAULT, skb_rgba(255,64,0,128)),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_THROUGH, SKB_DECORATION_STYLE_SOLID, 2.f, 0.f, PAINT_DECORATION_0),
	};

	const skb_attribute_t deco2_attributes[] = {
		skb_attribute_make_font_size(25.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
		skb_attribute_make_paint_color(PAINT_DECORATION_0, SKB_PAINT_STATE_DEFAULT, skb_rgba(255,64,0,128)),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_UNDER, SKB_DECORATION_STYLE_SOLID, 0.f, 0.f, PAINT_DECORATION_0),
	};

	const skb_attribute_t deco3_attributes[] = {
		skb_attribute_make_font_size(18.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
		skb_attribute_make_paint_color(PAINT_DECORATION_0, SKB_PAINT_STATE_DEFAULT, skb_rgba(255,64,0,255)),
		skb_attribute_make_paint_color(PAINT_DECORATION_1, SKB_PAINT_STATE_DEFAULT, skb_rgba(0,0,0,255)),
		skb_attribute_make_paint_color(PAINT_DECORATION_2, SKB_PAINT_STATE_DEFAULT, skb_rgba(0,64,255,255)),
		skb_attribute_make_paint_color(PAINT_DECORATION_3, SKB_PAINT_STATE_DEFAULT, skb_rgba(0,192,64,255)),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_THROUGH, SKB_DECORATION_STYLE_DASHED, 2.f, 0.f, PAINT_DECORATION_0),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_UNDER, SKB_DECORATION_STYLE_SOLID, 0.f, 0.f, PAINT_DECORATION_1),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_BOTTOM, SKB_DECORATION_STYLE_DASHED, 0.f, 0.f, PAINT_DECORATION_2),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_OVER, SKB_DECORATION_STYLE_WAVY, 0.f, 0.f, PAINT_DECORATION_3),
	};

	const skb_attribute_t italic_attributes[] = {
		skb_attribute_make_font_size(64.f),
		skb_attribute_make_font_style(SKB_STYLE_ITALIC),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
		skb_attribute_make_letter_spacing(20.f),
	};

	const skb_attribute_t big_attributes[] = {
		skb_attribute_make_font_size(128.f),
		skb_attribute_make_font_weight(SKB_WEIGHT_BOLD),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 0.75f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(220,40,40,255)),
	};

	const skb_attribute_t fracts_attributes[] = {
		skb_attribute_make_font_size(48.f),
		skb_attribute_make_font_weight(SKB_WEIGHT_BOLD),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(180,110,190,255)),
		skb_attribute_make_font_feature(SKB_TAG_STR("frac"), 1),
		skb_attribute_make_font_feature(SKB_TAG_STR("numr"), 1),
		skb_attribute_make_font_feature(SKB_TAG_STR("dmon"), 1),
	};

	const char* ipsum =
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam eget blandit purus, sit amet faucibus quam. Morbi vulputate tellus in nulla fermentum feugiat id eu diam. Sed id orci sapien. "
		"Donec sodales vitae odio dapibus pulvinar. Maecenas molestie lorem vulputate, gravida ex sed, dignissim erat. Suspendisse vel magna sed libero fringilla tincidunt id eget nisl. "
		"Suspendisse potenti. Maecenas fringilla magna sollicitudin, porta ipsum sed, rutrum magna. Sed ac semper magna. Phasellus porta nunc nulla, non dignissim magna pretium a. "
		"Aenean condimentum, nisi vitae sollicitudin ullamcorper, tellus elit suscipit risus, aliquet hendrerit sem velit in leo. Sed ut est pellentesque, vehicula ligula consectetur, tincidunt tellus. "
		"Aliquam erat volutpat. Etiam efficitur consequat turpis, vitae faucibus erat porta sed.\n"
		"Aenean euismod ante sed mi pellentesque dictum. Ut dapibus, nisl at dapibus egestas, enim metus semper lectus, ut dictum sapien leo et ligula. In et lorem quis nunc rutrum aliquet eget non velit. "
		"Ut a luctus metus. Morbi vestibulum sapien vitae velit feugiat feugiat. Interdum et malesuada fames ac ante ipsum primis in faucibus. Donec sit amet sapien quam.\n"
		"Donec at sodales est, sit amet rutrum ante. Cras tincidunt auctor nunc, id ullamcorper ligula facilisis non. Curabitur auctor mi at feugiat porta. Vestibulum aliquet molestie velit vehicula cursus. "
		"Donec vitae tristique libero. Etiam eget pellentesque nisi, in porta lectus. Donec accumsan ligula mauris. Nulla consectetur tortor at sem rutrum, non dapibus libero interdum. "
		"Nunc blandit molestie neque, quis porttitor lectus. Pellentesque consectetur augue sed velit suscipit pretium. In nec massa eros. Fusce non justo efficitur metus auctor pretium efficitur mattis enim.\n";

	skb_content_run_t runs[] = {
		skb_content_run_make_utf8(ipsum, -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(small_attributes), 0),
		skb_content_run_make_utf8("moikkelis!\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(italic_attributes), 0),

		skb_content_run_make_utf8("این یک 😬👀🚨 تست است\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(deco2_attributes), 0),

		skb_content_run_make_utf8("Donec sodales ", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(deco1_attributes), 0),
		skb_content_run_make_utf8("vitae odio ", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(deco2_attributes), 0),
		skb_content_run_make_utf8("dapibus pulvinar\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(deco3_attributes), 0),

		skb_content_run_make_utf8("ہے۔ kofi یہ ایک\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(small_attributes), 0),
		skb_content_run_make_utf8("POKS! 🧁\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(big_attributes), 0),
		skb_content_run_make_utf8("11/17\n", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(fracts_attributes), 0),
		skb_content_run_make_utf8("शकति शक्ति ", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(italic_attributes), 0),
		skb_content_run_make_utf8("こんにちは世界。 ", -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(ja_jp_attributes), 0),
	};

	ctx->layout = skb_layout_create_from_runs(ctx->temp_alloc, &params, runs, SKB_COUNTOF(runs));
	assert(ctx->layout);

	ctx->view = (view_t) { .cx = 400.f, .cy = 120.f, .scale = 1.f, .zoom_level = 0.f, };

	return ctx;

error:
	richtext_destroy(ctx);
	return NULL;
}

void richtext_destroy(void* ctx_ptr)
{
	richtext_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_layout_destroy(ctx->layout);
	skb_font_collection_destroy(ctx->font_collection);
	skb_temp_alloc_destroy(ctx->temp_alloc);

	memset(ctx, 0, sizeof(richtext_context_t));

	skb_free(ctx);
}

void richtext_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	richtext_context_t* ctx = ctx_ptr;
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

void richtext_on_char(void* ctx_ptr, unsigned int codepoint)
{
	richtext_context_t* ctx = ctx_ptr;
	assert(ctx);
}

void richtext_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	richtext_context_t* ctx = ctx_ptr;
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

void richtext_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	richtext_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->drag_view) {
		view_drag_move(&ctx->view, mouse_x, mouse_y);
	}
}

void richtext_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	richtext_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

void richtext_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	richtext_context_t* ctx = ctx_ptr;
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

	render_draw_layout(ctx->rc, NULL, 0, 0, ctx->layout, SKB_RASTERIZE_ALPHA_SDF);

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
