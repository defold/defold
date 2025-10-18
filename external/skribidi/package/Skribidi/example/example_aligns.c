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
#include "skb_layout.h"
#include "skb_layout_cache.h"
#include "skb_rasterizer.h"
#include "skb_image_atlas.h"


typedef struct aligns_context_t {
	example_t base;

	skb_font_collection_t* font_collection;
	skb_temp_alloc_t* temp_alloc;
	skb_image_atlas_t* atlas;
	skb_rasterizer_t* rasterizer;

	skb_layout_cache_t* layout_cache;

	view_t view;
	bool drag_view;
	bool drag_text;

	uint8_t wrap;
	uint8_t overflow;
	uint8_t vert_trim;
	uint8_t layout_size_idx;
	uint8_t example_text_idx;

	bool show_glyph_bounds;
	float atlas_scale;

} aligns_context_t;


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

void aligns_destroy(void* ctx_ptr);
void aligns_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void aligns_on_char(void* ctx_ptr, unsigned int codepoint);
void aligns_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void aligns_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void aligns_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void aligns_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void* aligns_create(void)
{
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

	ctx->atlas = skb_image_atlas_create(NULL);
	assert(ctx->atlas);
	skb_image_atlas_set_create_texture_callback(ctx->atlas, &on_create_texture, NULL);

	ctx->rasterizer = skb_rasterizer_create(NULL);
	assert(ctx->rasterizer);

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
	skb_image_atlas_destroy(ctx->atlas);
	skb_rasterizer_destroy(ctx->rasterizer);
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
			ctx->layout_size_idx = inc_wrap(ctx->layout_size_idx, 3);
		if (key == GLFW_KEY_F5)
			ctx->example_text_idx = inc_wrap(ctx->example_text_idx, 3);
		if (key == GLFW_KEY_F6)
			ctx->wrap = inc_wrap(ctx->wrap, 3);
		if (key == GLFW_KEY_F7)
			ctx->overflow = inc_wrap(ctx->overflow, 3);
		if (key == GLFW_KEY_F8)
			ctx->vert_trim = inc_wrap(ctx->vert_trim, 2);

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

static void render_text(
	aligns_context_t* ctx,
	float x, float y, float width, float height,
	skb_align_t h_align, skb_align_t v_align, skb_baseline_t baseline_align,
	skb_text_wrap_t text_wrap, skb_text_overflow_t text_overflow, skb_vertical_trim_t vertical_trim,
	float font_size, skb_weight_t font_weight, skb_color_t color, const char* text)
{
	skb_layout_params_t params = {
		.base_direction = SKB_DIRECTION_AUTO,
		.font_collection = ctx->font_collection,
		.layout_width = width,
		.layout_height = height,
		.horizontal_align = (uint8_t)h_align,
		.vertical_align =  (uint8_t)v_align,
		.text_wrap = (uint8_t)text_wrap,
		.text_overflow = (uint8_t)text_overflow,
		.baseline_align = (uint8_t)baseline_align,
		.vertical_trim = (uint8_t)vertical_trim,
	};

	const skb_attribute_t attributes[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, font_size, font_weight, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
		skb_attribute_make_fill(color),
	};

	const skb_layout_t* layout = skb_layout_cache_get_utf8(ctx->layout_cache, ctx->temp_alloc, &params, text, -1, attributes, SKB_COUNTOF(attributes));
	assert(layout);

	skb_rect2_t bounds = skb_layout_get_bounds(layout);
	bounds.x += x;
	bounds.y += y;
	bounds = view_transform_rect(&ctx->view, bounds);
	draw_rect(bounds.x, bounds.y, bounds.width, bounds.height, skb_rgba(0,0,0,64));

	// Draw layout
	const skb_glyph_run_t* glyph_runs = skb_layout_get_glyph_runs(layout);
	const int32_t glyph_runs_count = skb_layout_get_glyph_runs_count(layout);
	const skb_glyph_t* glyphs = skb_layout_get_glyphs(layout);
	const int32_t glyphs_count = skb_layout_get_glyphs_count(layout);
	const skb_text_attributes_span_t* attrib_spans = skb_layout_get_attribute_spans(layout);
	const skb_layout_params_t* layout_params = skb_layout_get_params(layout);

	for (int32_t ri = 0; ri < glyph_runs_count; ri++) {
		const skb_glyph_run_t* glyph_run = &glyph_runs[ri];
		const skb_text_attributes_span_t* span = &attrib_spans[glyph_run->span_idx];
		const skb_attribute_fill_t attr_fill = skb_attributes_get_fill(span->attributes, span->attributes_count);
		const skb_attribute_font_t attr_font = skb_attributes_get_font(span->attributes, span->attributes_count);
		for (int32_t gi = glyph_run->glyph_range.start; gi < glyph_run->glyph_range.end; gi++) {
			const skb_glyph_t* glyph = &glyphs[gi];

			const float gx = x + glyph->offset_x;
			const float gy = y + glyph->offset_y;

			// Glyph image
			skb_quad_t quad = skb_image_atlas_get_glyph_quad(
				ctx->atlas,gx, gy, ctx->view.scale,
				layout_params->font_collection, glyph_run->font_handle, glyph->gid,
				attr_font.size, SKB_RASTERIZE_ALPHA_SDF);

			draw_image_quad_sdf(
				view_transform_rect(&ctx->view, quad.geom),
				quad.texture, 1.f / quad.scale, (quad.flags & SKB_QUAD_IS_COLOR) ? skb_rgba(255,255,255, attr_fill.color.a) : attr_fill.color,
				(uint32_t)skb_image_atlas_get_texture_user_data(ctx->atlas, quad.texture_idx));
		}
	}
}

void aligns_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	aligns_context_t* ctx = ctx_ptr;
	assert(ctx);

	draw_line_width(1.f);

	skb_layout_cache_compact(ctx->layout_cache);
	skb_image_atlas_compact(ctx->atlas);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		draw_text((float)view_width - 20,20, 12, 1.f, skb_rgba(0,0,0,255), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
	}

	// Draw visual result
	const skb_color_t ink_color_trans = skb_rgba(32,32,32,128);


	const float layout_sizes[] = {
		200.f, 40.f,
		100.f, 100.f,
		300.f, 200.f,
	};

	float x = 0.f;
	float y = 0.f;

	float layout_width = layout_sizes[ctx->layout_size_idx * 2 + 0];
	float layout_height = layout_sizes[ctx->layout_size_idx * 2 + 1];

	const char* align_labels[] = { "Start", "Center", "End" };
	const char* wrap_labels[] = { "None", "Word", "Word & Char" };
	const char* overflow_labels[] = { "None", "Clip", "Ellipsis" };
	const char* vert_trim_labels[] = { "Ascender to Descender", "Cap Height to Baseline" };

	const char* example_text[] = { "Halló fjörður!", "Quick brown hamburgerfontstiv with aïoli.", "أَفَإِستَسقَينَاكُمُوها این یک" };

	{
		{
			skb_vec2_t p = view_transform_pt(&ctx->view, (skb_vec2_t){x, y});
			draw_text(p.x,p.y, 12 * ctx->view.scale, 0.f, skb_rgba(0,0,0,255), "Wrap (F6): %s   Overflow (F7): %s   Vert trim (F8): %s",
				wrap_labels[ctx->wrap], overflow_labels[ctx->overflow], vert_trim_labels[ctx->vert_trim]);
			y += 50.f;
		}

		for (uint8_t valign = 0; valign < 3; valign++) {

			{
				skb_vec2_t p = view_transform_pt(&ctx->view, (skb_vec2_t){x - 10.f, y + layout_height * 0.5f + 6});
				draw_text(p.x,p.y, 12 * ctx->view.scale, 1.f, skb_rgba(0,0,0,128), align_labels[valign]);
			}

			for (uint8_t halign = 0; halign < 3; halign++) {
				float tx = x + halign * (layout_width + 120.f);
				float ty = y;

				if (valign == 0) {
					skb_vec2_t p = view_transform_pt(&ctx->view, (skb_vec2_t){tx + layout_width * 0.5f, ty - 10.f});
					draw_text(p.x,p.y, 12 * ctx->view.scale, 0.5f, skb_rgba(0,0,0,128), align_labels[halign]);
				}

				skb_rect2_t rect = {
					.x = tx,
					.y = ty,
					.width = layout_width,
					.height = layout_height,
				};
				rect = view_transform_rect(&ctx->view, rect);

				draw_rect(rect.x, rect.y, rect.width, rect.height, skb_rgba(255,192,0,255));

				render_text(ctx, tx, ty, layout_width, layout_height,
					(skb_align_t)halign, (skb_align_t)valign, SKB_BASELINE_ALPHABETIC,
					(skb_text_wrap_t)ctx->wrap, (skb_text_overflow_t)ctx->overflow, (skb_vertical_trim_t)ctx->vert_trim,
					24.f, SKB_WEIGHT_REGULAR, skb_rgba(0,0,0,255), example_text[ctx->example_text_idx]);

			}
			y += layout_height + 120.f;
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
		"RMB: Pan view   Wheel: Zoom View   F4: Change layout size   F5: Change example text   F9: Glyph details %s   F10: Atlas %.1f%%",
		ctx->show_glyph_bounds ? "ON" : "OFF", ctx->atlas_scale * 100.f);

}
