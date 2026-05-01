// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "debug_draw.h"
#include "utils.h"

#include "skb_common.h"
#include "skb_font_collection.h"
#include "skb_rasterizer.h"
#include "skb_layout.h"
#include "skb_image_atlas.h"


typedef struct fallback_context_t {
	example_t base;

	skb_font_collection_t* font_collection;
	skb_temp_alloc_t* temp_alloc;
	skb_image_atlas_t* atlas;
	skb_rasterizer_t* rasterizer;

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


#define LOAD_FONT_OR_FAIL(path, font_family) \
	if (!skb_font_collection_add_font(ctx->font_collection, path, font_family)) { \
		skb_debug_log("Failed to load " path "\n"); \
		goto error; \
	}

static void on_create_texture(skb_image_atlas_t* atlas, uint8_t texture_idx, void* context)
{
	const skb_image_t* texture = skb_image_atlas_get_texture(atlas, texture_idx);
	if (texture) {
		uint32_t tex_id = draw_create_texture(texture->width, texture->height, texture->stride_bytes, NULL, texture->bpp);
		skb_image_atlas_set_texture_user_data(atlas, texture_idx, tex_id);
	}
}

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

	skb_layout_params_t params = {
		.lang = "",
		.base_direction = SKB_DIRECTION_AUTO,
		.font_collection = ctx->font_collection,
		.layout_width = 600.f,
		.text_wrap = SKB_WRAP_WORD_CHAR,
		.horizontal_align = SKB_ALIGN_START,
		.baseline_align = SKB_BASELINE_MIDDLE,
	};

	const skb_attribute_t attributes[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 32.f, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
		skb_attribute_make_fill(ink_color),
	};

	skb_layout_set_utf8(ctx->layout, ctx->temp_alloc, &params, text, -1, attributes, SKB_COUNTOF(attributes));

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

void* fallback_create(void)
{
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
		.lang = "",
		.base_direction = SKB_DIRECTION_AUTO,
		.font_collection = ctx->font_collection,
		.layout_width = 600.f,
		.text_wrap = SKB_WRAP_WORD_CHAR,
		.baseline_align = SKB_BASELINE_MIDDLE,
	};

	ctx->layout = skb_layout_create(&params);
	assert(ctx->layout);

	ctx->snippet_idx = 0;
	set_text(ctx, g_snippets[ctx->snippet_idx]);

	ctx->atlas = skb_image_atlas_create(NULL);
	assert(ctx->atlas);
	skb_image_atlas_set_create_texture_callback(ctx->atlas, &on_create_texture, NULL);

	ctx->rasterizer = skb_rasterizer_create(NULL);
	assert(ctx->rasterizer);

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

	skb_image_atlas_destroy(ctx->atlas);
	skb_rasterizer_destroy(ctx->rasterizer);
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

	draw_line_width(1.f);

	skb_image_atlas_compact(ctx->atlas);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		draw_text((float)view_width - 20,20, 12, 1.f, skb_rgba(0,0,0,255), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
	}

	{
		// Draw load time
		draw_text((float)view_width/2,30, 15, 1.f, skb_rgba(0,0,0,255), "Last font load time %.2f ms", (float)ctx->font_load_time_usec / 1000.f);
	}

	// Draw visual result
	const skb_color_t ink_color_trans = skb_rgba(32,32,32,128);

	{
		// Draw layout
		const skb_glyph_run_t* glyph_runs = skb_layout_get_glyph_runs(ctx->layout);
		const int32_t glyph_runs_count = skb_layout_get_glyph_runs_count(ctx->layout);
		const skb_glyph_t* glyphs = skb_layout_get_glyphs(ctx->layout);
		const int32_t glyphs_count = skb_layout_get_glyphs_count(ctx->layout);
		const skb_text_attributes_span_t* attrib_spans = skb_layout_get_attribute_spans(ctx->layout);
		const skb_layout_params_t* layout_params = skb_layout_get_params(ctx->layout);

		if (ctx->show_glyph_bounds) {
			skb_rect2_t layout_bounds = skb_layout_get_bounds(ctx->layout);
			layout_bounds = view_transform_rect(&ctx->view, layout_bounds);
			draw_rect(layout_bounds.x, layout_bounds.y, layout_bounds.width, layout_bounds.height, skb_rgba(255,128,64,128));
		}

		for (int32_t ri = 0; ri < glyph_runs_count; ri++) {
			const skb_glyph_run_t* glyph_run = &glyph_runs[ri];
			const skb_text_attributes_span_t* span = &attrib_spans[glyph_run->span_idx];
			const skb_attribute_fill_t attr_fill = skb_attributes_get_fill(span->attributes, span->attributes_count);
			const skb_attribute_font_t attr_font = skb_attributes_get_font(span->attributes, span->attributes_count);
			for (int32_t gi = glyph_run->glyph_range.start; gi < glyph_run->glyph_range.end; gi++) {
				const skb_glyph_t* glyph = &glyphs[gi];

				const float gx = glyph->offset_x;
				const float gy = glyph->offset_y;

				if (ctx->show_glyph_bounds) {
					draw_tick(view_transform_x(&ctx->view,gx), view_transform_y(&ctx->view,gy), 5.f, ink_color_trans);

					skb_rect2_t bounds = skb_font_get_glyph_bounds(layout_params->font_collection, glyph->font_handle, glyph->gid, attr_font.size);
					bounds.x += gx;
					bounds.y += gy;
					bounds = view_transform_rect(&ctx->view, bounds);
					draw_rect(bounds.x, bounds.y, bounds.width, bounds.height, skb_rgba(255,128,64,128));
				}

				// Glyph image
				skb_quad_t quad = skb_image_atlas_get_glyph_quad(
					ctx->atlas,gx, gy, ctx->view.scale, layout_params->font_collection,
					glyph->font_handle, glyph->gid, attr_font.size, SKB_RASTERIZE_ALPHA_SDF);

				draw_image_quad_sdf(
					view_transform_rect(&ctx->view, quad.geom),
					quad.texture, 1.f / quad.scale, (quad.flags & SKB_QUAD_IS_COLOR) ? skb_rgba(255,255,255, attr_fill.color.a) : attr_fill.color,
					(uint32_t)skb_image_atlas_get_texture_user_data(ctx->atlas, quad.texture_idx));
			}
		}
	}

	// Update atlas and textures
	if (skb_image_atlas_rasterize_missing_items(ctx->atlas, ctx->temp_alloc, ctx->rasterizer)) {
		for (int32_t i = 0; i < skb_image_atlas_get_texture_count(ctx->atlas); i++) {
			skb_rect2i_t dirty_bounds = skb_image_atlas_get_and_reset_texture_dirty_bounds(ctx->atlas, i);
			if (!skb_rect2i_is_empty(dirty_bounds)) {
				const skb_image_t* image = skb_image_atlas_get_texture(ctx->atlas, i);
				assert(image);
				uint32_t tex_id = (uint32_t)skb_image_atlas_get_texture_user_data(ctx->atlas, i);
				if (tex_id == 0) {
					tex_id = draw_create_texture(image->width, image->height, image->stride_bytes, image->buffer, image->bpp);
					assert(tex_id);
					skb_image_atlas_set_texture_user_data(ctx->atlas, i, tex_id);
				} else {
					draw_update_texture(tex_id,
							dirty_bounds.x, dirty_bounds.y, dirty_bounds.width, dirty_bounds.height,
							image->width, image->height, image->stride_bytes, image->buffer);
				}
			}
		}
	}

	// Draw atlas
	debug_draw_atlas(ctx->atlas, 20.f, 50.f, ctx->atlas_scale, 1);

	// Draw info
	draw_text((float)view_width - 20.f, (float)view_height - 15.f, 12.f, 1.f, skb_rgba(0,0,0,255),
		"RMB: Pan view   Wheel: Zoom View   F8: Next Text Snippet   F9: Glyph details %s   F10: Atlas %.1f%%",
		ctx->show_glyph_bounds ? "ON" : "OFF", ctx->atlas_scale * 100.f);

}
