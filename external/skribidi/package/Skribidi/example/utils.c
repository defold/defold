// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "utils.h"
#include "debug_draw.h"
#include "skb_image_atlas.h"

typedef struct draw_atlas_rect_context_t {
	float x;
	float y;
	float scale;
	skb_color_t color;
	view_t* view;
	uint32_t tex_id;
} draw_atlas_rect_context_t;

static void draw_atlas_rects(int32_t x, int32_t y, int32_t width, int32_t height, void* context)
{
	draw_atlas_rect_context_t* ctx = (draw_atlas_rect_context_t*)context;
	skb_rect2_t r = {
		.x = ctx->x + (float)x * ctx->scale,
		.y = ctx->y + (float)y * ctx->scale,
		.width = (float)width * ctx->scale - 1.f,
		.height = (float)height * ctx->scale - 1.f
	};
	if (ctx->view)
		r = view_transform_rect(ctx->view, r);

	draw_filled_rect(r.x, r.y, r.width, r.height, ctx->color);
}

static void draw_used_rects(int32_t x, int32_t y, int32_t width, int32_t height, void* context)
{
	draw_atlas_rect_context_t* ctx = (draw_atlas_rect_context_t*)context;
	skb_rect2_t r = {
		.x = ctx->x + (float)x * ctx->scale,
		.y = ctx->y + (float)y * ctx->scale,
		.width = (float)width * ctx->scale - 1.f,
		.height = (float)height * ctx->scale - 1.f
	};
	if (ctx->view)
		r = view_transform_rect(ctx->view, r);

	draw_rect(r.x, r.y, r.width, r.height, ctx->color);
}

void debug_draw_atlas(skb_image_atlas_t* atlas, float sx, float sy, float scale, int32_t columns)
{
	if (scale < 0.01f)
		return;

	float row_y = sy;

	for (int32_t i = 0; i < skb_image_atlas_get_texture_count(atlas); i += columns) {

		float row_height = 0.f;
		float col_x = sx;

		for (int32_t j = 0; j < columns; j++) {
			int32_t texture_idx = i+j;
			if (texture_idx >= skb_image_atlas_get_texture_count(atlas))
				break;
			const skb_image_t* image = skb_image_atlas_get_texture(atlas, texture_idx);
			const uint32_t tex_id = (uint32_t)skb_image_atlas_get_texture_user_data(atlas, texture_idx);

			float ax = col_x;
			float ay = row_y;

			draw_text(ax, ay+12,12, 0, skb_rgba(0,0,0,255), "[%d] %s (%d x %d)", texture_idx, image->bpp == 4 ? "RGBA" : "A", image->width, image->height);
			ay += 20.f;

			const float img_width = (float)image->width * scale;
			const float img_height = (float)image->height * scale;

			draw_filled_rect(ax,ay,img_width, img_height,skb_rgba(0,0,0,255));
			draw_image_quad((skb_rect2_t) {ax,ay,img_width,img_height}, (skb_rect2_t) {0, 0, (float)image->width, (float)image->height}, skb_rgba(255,255,255,255), tex_id);

			draw_atlas_rect_context_t context = {
				.x = ax,
				.y = ay,
				.scale = scale,
				.color = skb_rgba(96,96,128,192),
				.tex_id = tex_id,
			};
			skb_image_atlas_debug_iterate_free_rects(atlas, texture_idx, draw_atlas_rects, &context);

			context.color = skb_rgba(32,192,255,255);
			skb_image_atlas_debug_iterate_used_rects(atlas, texture_idx, draw_used_rects, &context);

			// previous updated
			skb_rect2i_t dirty_bounds = skb_image_atlas_debug_get_texture_prev_dirty_bounds(atlas, texture_idx);
			context.color = skb_rgba(255,220,32,255);
			draw_used_rects(dirty_bounds.x, dirty_bounds.y, dirty_bounds.width, dirty_bounds.height, &context);

			row_height = skb_maxf(row_height, img_height + 20.f);
			col_x += img_width + 20.f;
		}

		row_y += row_height + 20.f;
	}

}
