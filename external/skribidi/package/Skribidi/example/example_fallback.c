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


typedef struct fallback_context_t {
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

	int64_t font_load_time_usec;

	int32_t snippet_idx;

} fallback_context_t;

static const char* g_snippets[] = {
	"This is a test.",
	"😬👀🚨",
	"این یک تست است",
	"शकति शक्ति ",
	"今天天气晴朗。 ",
};
static const int32_t g_snippets_count = SKB_COUNTOF(g_snippets);


void fallback_destroy(void* ctx_ptr);
void fallback_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void fallback_on_char(void* ctx_ptr, unsigned int codepoint);
void fallback_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void fallback_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void fallback_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void fallback_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);


static void set_text(fallback_context_t* ctx, const char* text)
{
	skb_color_t ink_color = skb_rgba(64,64,64,255);

	const skb_attribute_t layout_attributes[] = {
		skb_attribute_make_text_wrap(SKB_WRAP_WORD_CHAR),
		skb_attribute_make_baseline_align(SKB_BASELINE_MIDDLE),
	};

	skb_layout_params_t params = {
		.font_collection = ctx->font_collection,
		.layout_width = 600.f,
		.layout_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes)
	};

	const skb_attribute_t attributes[] = {
		skb_attribute_make_font_size(32.f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
	};

	skb_layout_set_utf8(ctx->layout, ctx->temp_alloc, &params, text, -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(attributes));

}

bool handle_font_fallback(skb_font_collection_t* font_collection, const char* lang, uint8_t script, uint8_t font_family, void* context)
{
	fallback_context_t* ctx = (fallback_context_t*)context;

	const char* font_family_str = "--";
	if (font_family == SKB_FONT_FAMILY_EMOJI)
		font_family_str = "emoji";
	else if (font_family == SKB_FONT_FAMILY_DEFAULT)
		font_family_str = "default";

	uint32_t script_tag = skb_script_to_iso15924_tag(script);

	skb_debug_log("Font fallback: %s %c%c%c%c %s\n", lang, SKB_UNTAG(script_tag), font_family_str);

	// Naive for selection based on font family and script.
	//
	// A real app might want to preprocess a list of fonts that are known to cover specific scripts, or even do system font fallback.
	//
	// And example of curated list can be found in Chrome:
	//	https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/platform/fonts/win/font_fallback_win.cc
	//
	// Fontique is good example on how system font fallback is implemented:
	//	https://github.com/linebender/parley/tree/main/fontique
	//

	if (font_family == SKB_FONT_FAMILY_EMOJI) {
		int64_t t0 = skb_perf_timer_get();
		LOAD_FONT_OR_FAIL("data/NotoColorEmoji-Regular.ttf", SKB_FONT_FAMILY_EMOJI);
		int64_t t1 = skb_perf_timer_get();
		ctx->font_load_time_usec = skb_perf_timer_elapsed_us(t0, t1);
		return true;
	}
	if (script_tag == SKB_TAG_STR("Arab")) {
		int64_t t0 = skb_perf_timer_get();
		LOAD_FONT_OR_FAIL("data/IBMPlexSansArabic-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
		int64_t t1 = skb_perf_timer_get();
		ctx->font_load_time_usec = skb_perf_timer_elapsed_us(t0, t1);
		return true;
	}
	if (script_tag == SKB_TAG_STR("Deva")) {
		int64_t t0 = skb_perf_timer_get();
		LOAD_FONT_OR_FAIL("data/IBMPlexSansDevanagari-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
		int64_t t1 = skb_perf_timer_get();
		ctx->font_load_time_usec = skb_perf_timer_elapsed_us(t0, t1);
		return true;
	}
	if (script_tag == SKB_TAG_STR("Hani")) {
		int64_t t0 = skb_perf_timer_get();
		LOAD_FONT_OR_FAIL("data/IBMPlexSansJP-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
		int64_t t1 = skb_perf_timer_get();
		ctx->font_load_time_usec = skb_perf_timer_elapsed_us(t0, t1);
		return true;
	}

	return true;

error:
	return false;
}

void* fallback_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	fallback_context_t* ctx = skb_malloc(sizeof(fallback_context_t));
	memset(ctx, 0, sizeof(fallback_context_t));

	ctx->base.create = fallback_create;
	ctx->base.destroy = fallback_destroy;
	ctx->base.on_key = fallback_on_key;
	ctx->base.on_char = fallback_on_char;
	ctx->base.on_mouse_button = fallback_on_mouse_button;
	ctx->base.on_mouse_move = fallback_on_mouse_move;
	ctx->base.on_mouse_scroll = fallback_on_mouse_scroll;
	ctx->base.on_update = fallback_on_update;

	ctx->rc = rc;
	render_reset_atlas(rc, NULL);

	ctx->atlas_scale = 0.0f;

	// Create empty font collection, we'll add to it as we need.
	ctx->font_collection = skb_font_collection_create();
	assert(ctx->font_collection);


	int64_t t0 = skb_perf_timer_get();

	// Load just one font initially, more are loaded when needed.
	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);

	int64_t t1 = skb_perf_timer_get();
	ctx->font_load_time_usec = skb_perf_timer_elapsed_us(t0, t1);

	skb_font_collection_set_on_font_fallback(ctx->font_collection, handle_font_fallback, ctx);

	ctx->temp_alloc = skb_temp_alloc_create(512*1024);
	assert(ctx->temp_alloc);

	skb_layout_params_t params = {
		.font_collection = ctx->font_collection,
		.layout_width = 600.f,
	};

	ctx->layout = skb_layout_create(&params);
	assert(ctx->layout);

	ctx->snippet_idx = 0;
	set_text(ctx, g_snippets[ctx->snippet_idx]);

	ctx->view = (view_t) { .cx = 400.f, .cy = 120.f, .scale = 1.f, .zoom_level = 0.f, };

	return ctx;

error:
	fallback_destroy(ctx);
	return NULL;
}

void fallback_destroy(void* ctx_ptr)
{
	fallback_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_layout_destroy(ctx->layout);
	skb_font_collection_destroy(ctx->font_collection);
	skb_temp_alloc_destroy(ctx->temp_alloc);

	memset(ctx, 0, sizeof(fallback_context_t));

	skb_free(ctx);
}

void fallback_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	fallback_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_F9) {
			ctx->show_glyph_bounds = !ctx->show_glyph_bounds;
		}
		if (key == GLFW_KEY_F8) {
			ctx->snippet_idx = (ctx->snippet_idx + 1) % g_snippets_count;
			set_text(ctx, g_snippets[ctx->snippet_idx]);
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

void fallback_on_char(void* ctx_ptr, unsigned int codepoint)
{
	fallback_context_t* ctx = ctx_ptr;
	assert(ctx);
}

void fallback_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	fallback_context_t* ctx = ctx_ptr;
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

void fallback_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	fallback_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->drag_view) {
		view_drag_move(&ctx->view, mouse_x, mouse_y);
	}
}

void fallback_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	fallback_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

void fallback_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	fallback_context_t* ctx = ctx_ptr;
	assert(ctx);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		debug_render_text(ctx->rc, (float)view_width - 20,20, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
		skb_temp_alloc_stats_t render_stats = skb_temp_alloc_stats(render_get_temp_alloc(ctx->rc));
		debug_render_text(ctx->rc, (float)view_width - 20,40, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Render Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)render_stats.used / 1024.f, (float)render_stats.allocated / 1024.f);
	}

	{
		// Draw load time
		debug_render_text(ctx->rc, (float)view_width/2,20, 13, RENDER_ALIGN_CENTER, skb_rgba(255,64,64,255), "Last font load time %.2f ms", (float)ctx->font_load_time_usec / 1000.f);
	}

	render_push_transform(ctx->rc, ctx->view.cx, ctx->view.cy, ctx->view.scale);

	render_draw_layout(ctx->rc, NULL, 0, 0, ctx->layout, SKB_RASTERIZE_ALPHA_SDF);

	// Draw visual result

	const skb_color_t ink_color_trans = skb_rgba(32,32,32,128);

	if (ctx->show_glyph_bounds) {
		// Draw layout
		const skb_layout_run_t* layout_runs = skb_layout_get_layout_runs(ctx->layout);
		const int32_t layout_runs_count = skb_layout_get_layout_runs_count(ctx->layout);
		const skb_glyph_t* glyphs = skb_layout_get_glyphs(ctx->layout);
		const skb_layout_params_t* layout_params = skb_layout_get_params(ctx->layout);

		skb_rect2_t layout_bounds = skb_layout_get_bounds(ctx->layout);
		debug_render_stroked_rect(ctx->rc, layout_bounds.x, layout_bounds.y, layout_bounds.width, layout_bounds.height, skb_rgba(255,128,64,128), -1.f);

		for (int32_t ri = 0; ri < layout_runs_count; ri++) {
			const skb_layout_run_t* layout_run = &layout_runs[ri];
			for (int32_t gi = layout_run->glyph_range.start; gi < layout_run->glyph_range.end; gi++) {
				const skb_glyph_t* glyph = &glyphs[gi];
				const float gx = glyph->offset_x;
				const float gy = glyph->offset_y;
				if (ctx->show_glyph_bounds) {
					debug_render_tick(ctx->rc, gx, gy, 5.f, ink_color_trans, -1.f);
					skb_rect2_t bounds = skb_font_get_glyph_bounds(layout_params->font_collection, layout_run->font_handle, glyph->gid, layout_run->font_size);
					bounds.x += gx;
					bounds.y += gy;
					debug_render_stroked_rect(ctx->rc, bounds.x, bounds.y, bounds.width, bounds.height, skb_rgba(255,128,64,128), -1.f);
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
		"F8: Next Text Snippet   F9: Glyph details %s   F10: Atlas %.1f%%",
		ctx->show_glyph_bounds ? "ON" : "OFF", ctx->atlas_scale * 100.f);
}
