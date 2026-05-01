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


typedef struct richtext_context_t {
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

} richtext_context_t;


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

void richtext_destroy(void* ctx_ptr);
void richtext_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void richtext_on_char(void* ctx_ptr, unsigned int codepoint);
void richtext_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void richtext_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void richtext_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void richtext_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void* richtext_create(void)
{
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

	skb_layout_params_t params = {
		.lang = "zh-hans",
		.base_direction = SKB_DIRECTION_AUTO,
		.font_collection = ctx->font_collection,
		.layout_width = 600.f,
		.text_wrap = SKB_WRAP_WORD_CHAR,
		.horizontal_align = SKB_ALIGN_START,
		.baseline_align = SKB_BASELINE_MIDDLE,
	};

	const skb_attribute_t attributes_small[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 15.f, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_fill(ink_color),
	};

	const skb_attribute_t attributes_deco1[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 15.f, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_fill(ink_color),
		skb_attribute_make_decoration(SKB_DECORATION_THROUGHLINE, SKB_DECORATION_STYLE_SOLID, 2.f, 0.f, skb_rgba(255,64,0,128)),
	};

	const skb_attribute_t attributes_deco2[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 25.f, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_fill(ink_color),
		skb_attribute_make_decoration(SKB_DECORATION_UNDERLINE, SKB_DECORATION_STYLE_SOLID, 0.f, 0.f, skb_rgba(0,0,0,192)),
	};

	const skb_attribute_t attributes_deco3[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 18.f, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_fill(ink_color),
		skb_attribute_make_decoration(SKB_DECORATION_THROUGHLINE, SKB_DECORATION_STYLE_DASHED, 2.f, 0.f, skb_rgba(255,64,0,255)),
		skb_attribute_make_decoration(SKB_DECORATION_UNDERLINE, SKB_DECORATION_STYLE_SOLID, 0.f, 0.f, skb_rgba(0,0,0,255)),
		skb_attribute_make_decoration(SKB_DECORATION_BOTTOMLINE, SKB_DECORATION_STYLE_DASHED, 0.f, 0.f, skb_rgba(0,64,255,255)),
		skb_attribute_make_decoration(SKB_DECORATION_OVERLINE, SKB_DECORATION_STYLE_WAVY, 0.f, 0.f, skb_rgba(0,192,64,255)),
	};

	const skb_attribute_t attributes_italic[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 64.f, SKB_WEIGHT_NORMAL, SKB_STYLE_ITALIC, SKB_STRETCH_NORMAL),
		skb_attribute_make_fill(ink_color),
		skb_attribute_make_spacing(20.f, 0.f),
	};

	const skb_attribute_t attributes_big[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 128.f, SKB_WEIGHT_BOLD, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 0.75f),
		skb_attribute_make_fill(skb_rgba(220,40,40,255)),
	};

	const skb_attribute_t attributes_fracts[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 48.f, SKB_WEIGHT_BOLD, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
		skb_attribute_make_fill(skb_rgba(180,110,190,255)),
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

	skb_text_run_utf8_t runs[] = {
		{ ipsum, -1, attributes_small, SKB_COUNTOF(attributes_small) },
		{ "moikkelis!\n", -1, attributes_italic, SKB_COUNTOF(attributes_italic) },

		{ "این یک 😬👀🚨 تست است\n", -1, attributes_deco2, SKB_COUNTOF(attributes_deco2) },

		{ "Donec sodales ", -1, attributes_deco1, SKB_COUNTOF(attributes_deco1) },
		{ "vitae odio ", -1, attributes_deco2, SKB_COUNTOF(attributes_deco2) },
		{ "dapibus pulvinar\n", -1, attributes_deco3, SKB_COUNTOF(attributes_deco3) },

		{ "ہے۔ kofi یہ ایک\n", -1, attributes_small, SKB_COUNTOF(attributes_small) },
		{ "POKS! 🧁\n", -1, attributes_big, SKB_COUNTOF(attributes_big) },
		{ "11/17\n", -1, attributes_fracts, SKB_COUNTOF(attributes_fracts) },
		{ "शकति शक्ति ", -1, attributes_italic, SKB_COUNTOF(attributes_italic) },
		{ "今天天气晴朗。 ", -1, attributes_small, SKB_COUNTOF(attributes_small) },
	};

	ctx->layout = skb_layout_create_from_runs_utf8(ctx->temp_alloc, &params, runs, SKB_COUNTOF(runs));
	assert(ctx->layout);


	ctx->atlas = skb_image_atlas_create(NULL);
	assert(ctx->atlas);
	skb_image_atlas_set_create_texture_callback(ctx->atlas, &on_create_texture, NULL);

	ctx->rasterizer = skb_rasterizer_create(NULL);
	assert(ctx->rasterizer);

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

	skb_image_atlas_destroy(ctx->atlas);
	skb_rasterizer_destroy(ctx->rasterizer);
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

	draw_line_width(1.f);

	skb_image_atlas_compact(ctx->atlas);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		draw_text((float)view_width - 20,20, 12, 1.f, skb_rgba(0,0,0,255), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
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


		const int32_t decorations_count = skb_layout_get_decorations_count(ctx->layout);
		const skb_decoration_t* decorations = skb_layout_get_decorations(ctx->layout);

		// Draw underlines
		for (int32_t i = 0; i < decorations_count; i++) {
			const skb_decoration_t* decoration = &decorations[i];
			const skb_text_attributes_span_t* span = &attrib_spans[decoration->span_idx];
			const skb_attribute_decoration_t attr_decoration = span->attributes[decoration->attribute_idx].decoration;
			if (attr_decoration.position != SKB_DECORATION_THROUGHLINE) {
				skb_rect2_t rect = calc_decoration_rect(decoration, attr_decoration);
				skb_pattern_quad_t pat_quad = skb_image_atlas_get_decoration_quad(
					ctx->atlas, rect.x, rect.y, rect.width, decoration->pattern_offset, ctx->view.scale,
					attr_decoration.style, decoration->thickness, SKB_RASTERIZE_ALPHA_SDF);
				draw_image_pattern_quad_sdf(
					view_transform_rect(&ctx->view, pat_quad.geom),
					pat_quad.pattern, pat_quad.texture, 1.f / pat_quad.scale, attr_decoration.color,
					(uint32_t)skb_image_atlas_get_texture_user_data(ctx->atlas, pat_quad.texture_idx));
			}
		}

		// Draw glyphs
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

					skb_rect2_t bounds = skb_font_get_glyph_bounds(layout_params->font_collection, glyph_run->font_handle, glyph->gid, attr_font.size);
					bounds.x += gx;
					bounds.y += gy;
					bounds = view_transform_rect(&ctx->view, bounds);
					draw_rect(bounds.x, bounds.y, bounds.width, bounds.height, skb_rgba(255,128,64,128));
				}

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

		// Draw through lines.
		for (int32_t i = 0; i < decorations_count; i++) {
			const skb_decoration_t* decoration = &decorations[i];
			const skb_text_attributes_span_t* span = &attrib_spans[decoration->span_idx];
			const skb_attribute_decoration_t attr_decoration = span->attributes[decoration->attribute_idx].decoration;
			if (attr_decoration.position == SKB_DECORATION_THROUGHLINE) {
				skb_rect2_t rect = calc_decoration_rect(decoration, attr_decoration);
				skb_pattern_quad_t pat_quad = skb_image_atlas_get_decoration_quad(
					ctx->atlas, rect.x, rect.y, rect.width, decoration->pattern_offset, ctx->view.scale,
					attr_decoration.style, decoration->thickness, SKB_RASTERIZE_ALPHA_SDF);
				draw_image_pattern_quad_sdf(
					view_transform_rect(&ctx->view, pat_quad.geom),
					pat_quad.pattern, pat_quad.texture, 1.f / pat_quad.scale, attr_decoration.color,
					(uint32_t)skb_image_atlas_get_texture_user_data(ctx->atlas, pat_quad.texture_idx));
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
		"RMB: Pan view   Wheel: Zoom View   F9: Glyph details %s   F10: Atlas %.1f%%",
		ctx->show_glyph_bounds ? "ON" : "OFF", ctx->atlas_scale * 100.f);

}
