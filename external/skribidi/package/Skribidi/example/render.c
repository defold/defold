// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "render.h"
#include <string.h>
#include <glad/gl.h>
#include <stdlib.h>
#include "skb_image_atlas.h"
#include "skb_rasterizer.h"
#include "skb_common.h"
#include "skb_layout.h"
#include "skb_rich_layout.h"


typedef struct render__vertex_t {
	skb_vec2_t pos;
	skb_vec2_t uv;
	skb_vec2_t atlas_pos;
	skb_vec2_t atlas_size;
	skb_color_t color;
	float scale;
} render__vertex_t;

typedef struct render__batch_t {
	int32_t offset;
	int32_t count;
	uint32_t image_id;
	uint32_t sdf_id;
	uint32_t scissor_id;
} render__batch_t;

typedef struct render__texture_t {
	uint32_t id;
	int32_t width;
	int32_t height;
	uint8_t bpp;
	// GL
	GLuint tex_id;
} render__texture_t;

typedef struct render__transform_t {
	float dx;
	float dy;
	float scale;
} render__transform_t;

enum {
	RENDER_TRANSFORM_STACK_SIZE = 10,
	RENDER_SCISSOR_STACK_SIZE = 10,
};

typedef struct render_context_t {
	skb_temp_alloc_t* temp_alloc;
	skb_image_atlas_t* atlas;
	skb_rasterizer_t* rasterizer;

	render__vertex_t* verts;
	int32_t verts_count;
	int32_t verts_cap;

	render__batch_t* batches;
	int32_t batches_count;
	int32_t batches_cap;

	skb_rect2_t* scissors;
	int32_t scissors_count;
	int32_t scissors_cap;

	render__texture_t* textures;
	int32_t textures_count;
	int32_t textures_cap;

	int32_t view_width;
	int32_t view_height;

	render__transform_t transform_stack[RENDER_TRANSFORM_STACK_SIZE];
	int32_t transform_stack_idx;

	uint32_t scissor_stack[RENDER_SCISSOR_STACK_SIZE];
	int32_t scissor_stack_idx;

	// GL
	GLuint program;
	GLuint vbo;
} render_context_t;


static bool gl__init(render_context_t* rc);
static void gl__destroy(render_context_t* rc);
static void gl__flush(render_context_t* rc);
static void gl__create_texture(render__texture_t* tex, int32_t width, int32_t height, int32_t stride_bytes, const uint8_t* image, uint8_t bpp);
static void gl__update_texture(render__texture_t* tex, int32_t offset_x, int32_t offset_y, int32_t width, int32_t height, int32_t stride_bytes, const uint8_t* image);


static void renderer__on_create_texture(skb_image_atlas_t* atlas, uint8_t texture_idx, void* context)
{
	render_context_t* r = (render_context_t*)context;
	const skb_image_t* texture = skb_image_atlas_get_texture(atlas, texture_idx);
	if (texture) {
		uint32_t tex_id = render_create_texture(r, texture->width, texture->height, texture->stride_bytes, NULL, texture->bpp);
		skb_image_atlas_set_texture_user_data(atlas, texture_idx, tex_id);
	}
}

static render__texture_t* renderer__find_texture(const render_context_t* r, uint32_t id)
{
	if (id == 0)
		return NULL;
	for (int32_t i = 0; i < r->textures_count; i++) {
		if (r->textures[i].id == id) {
			return &r->textures[i];
		}
	}
	return NULL;
}

static void renderer__push_triangles(render_context_t* r, const render__vertex_t* verts, int32_t verts_count, uint32_t image_id, uint32_t sdf_id, uint32_t scissor_id)
{
	render__batch_t* batch = NULL;

	render__batch_t* prev_batch = r->batches_count > 0 ? &r->batches[r->batches_count-1] : NULL;
	if (prev_batch && prev_batch->image_id == image_id && prev_batch->sdf_id == sdf_id && prev_batch->scissor_id == scissor_id) {
		batch = prev_batch;
	} else {
		SKB_ARRAY_RESERVE(r->batches, r->batches_count + 1);
		batch = &r->batches[r->batches_count++];
		batch->offset = r->verts_count;
		batch->count = 0;
		batch->image_id = image_id;
		batch->sdf_id = sdf_id;
		batch->scissor_id = scissor_id;
	}

	SKB_ARRAY_RESERVE(r->verts, r->verts_count + verts_count);
	memcpy(&r->verts[r->verts_count], verts, verts_count * sizeof(render__vertex_t));

	batch->count += verts_count;
	r->verts_count += verts_count;
}

//
// API
//

render_context_t* render_create(const skb_image_atlas_config_t* config)
{
	render_context_t* r = SKB_MALLOC_STRUCT(render_context_t);

	r->temp_alloc = skb_temp_alloc_create(512*1024);
	assert(r->temp_alloc);

	r->atlas = skb_image_atlas_create(config);
	assert(r->atlas);
	skb_image_atlas_set_create_texture_callback(r->atlas, &renderer__on_create_texture, r);

	r->rasterizer = skb_rasterizer_create(NULL);
	assert(r->rasterizer);

	const bool init_ok = gl__init(r);
	assert(init_ok);

	return r;
}

void render_destroy(render_context_t* rc)
{
	if (!rc)
		return;

	gl__destroy(rc);

	skb_free(rc->verts);
	skb_free(rc->batches);
	skb_free(rc->scissors);
	skb_free(rc->textures);

	skb_image_atlas_destroy(rc->atlas);
	skb_rasterizer_destroy(rc->rasterizer);
	skb_temp_alloc_destroy(rc->temp_alloc);

	SKB_ZERO_STRUCT(rc);
	skb_free(rc);
}

void render_reset_atlas(render_context_t* rc, const skb_image_atlas_config_t* config)
{
	// Reset atlas
	skb_image_atlas_destroy(rc->atlas);
	rc->atlas = skb_image_atlas_create(config);
	assert(rc->atlas);
	skb_image_atlas_set_create_texture_callback(rc->atlas, &renderer__on_create_texture, rc);

	// Reset textures.
	for (int32_t i = 0; i < rc->textures_count; i++) {
		render__texture_t* tex = &rc->textures[i];
		glDeleteTextures(1, &tex->tex_id);
	}
	rc->textures_count = 0;
}

void render_begin_frame(render_context_t* rc, int32_t view_width, int32_t view_height)
{
	assert(rc);

	rc->view_width = view_width;
	rc->view_height = view_height;

	rc->transform_stack_idx = 0;
	rc->transform_stack[0].dx = 0.f;
	rc->transform_stack[0].dy = 0.f;
	rc->transform_stack[0].scale = 1.f;

	rc->scissors_count = 0;
	SKB_ARRAY_RESERVE(rc->scissors, rc->scissors_count + 1);
	skb_rect2_t* scissor = &rc->scissors[rc->scissors_count++];
	scissor->x = 0.f;
	scissor->y = 0.f;
	scissor->width = (float)view_width;
	scissor->height = (float)view_height;
	rc->scissor_stack_idx = 0;
	rc->scissor_stack[0] = 0;

	skb_image_atlas_compact(rc->atlas);
}

void render_push_scissor(render_context_t* rc, float x, float y, float width, float height)
{
	assert(rc);

	skb_rect2_t scissor_rect = {
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	// Apply current transform to the rect
	scissor_rect = render_transform_rect(rc, scissor_rect);

	// Clip to current
	const uint32_t current_scissor_idx = rc->scissor_stack[rc->scissor_stack_idx];
	const skb_rect2_t current_scissor_rect = rc->scissors[current_scissor_idx];
	scissor_rect = skb_rect2_intersection(current_scissor_rect, scissor_rect);

	SKB_ARRAY_RESERVE(rc->scissors, rc->scissors_count + 1);
	rc->scissors[rc->scissors_count++] = scissor_rect;

	uint32_t scissor_idx = (uint32_t)(rc->scissors_count - 1);
	if (rc->scissor_stack_idx+1 < RENDER_SCISSOR_STACK_SIZE) {
		rc->scissor_stack_idx++;
		rc->scissor_stack[rc->scissor_stack_idx] = scissor_idx;
	}

}

void render_pop_scissor(render_context_t* rc)
{
	assert(rc);
	if (rc->scissor_stack_idx > 0)
		rc->scissor_stack_idx--;
}


void render_push_transform(render_context_t* rc, float offset_x, float offset_y, float scale)
{
	assert(rc);
	if (rc->transform_stack_idx+1 < RENDER_TRANSFORM_STACK_SIZE) {
		rc->transform_stack_idx++;
		rc->transform_stack[rc->transform_stack_idx].dx = offset_x;
		rc->transform_stack[rc->transform_stack_idx].dy = offset_y;
		rc->transform_stack[rc->transform_stack_idx].scale = scale;
	}
}

void render_pop_transform(render_context_t* rc)
{
	assert(rc);
	if (rc->transform_stack_idx > 0)
		rc->transform_stack_idx--;
}

float render_get_transform_scale(const render_context_t* rc)
{
	assert(rc);
	return rc->transform_stack[rc->transform_stack_idx].scale;
}

skb_vec2_t render_get_transform_offset(const render_context_t* rc)
{
	assert(rc);
	return (skb_vec2_t) {
		.x = rc->transform_stack[rc->transform_stack_idx].dx,
		.y = rc->transform_stack[rc->transform_stack_idx].dy
	};
}

skb_rect2_t render_transform_rect(const render_context_t* rc, skb_rect2_t rect)
{
	const render__transform_t xform = rc->transform_stack[rc->transform_stack_idx];
	return (skb_rect2_t) {
		.x = xform.dx + rect.x * xform.scale,
		.y = xform.dy + rect.y * xform.scale,
		.width = rect.width * xform.scale,
		.height = rect.height * xform.scale,
	};
}

skb_rect2_t render_inv_transform_rect(const render_context_t* rc, skb_rect2_t rect)
{
	const render__transform_t xform = rc->transform_stack[rc->transform_stack_idx];
	return (skb_rect2_t) {
		.x = (rect.x - xform.dx) / xform.scale,
		.y = (rect.y - xform.dy) / xform.scale,
		.width = rect.width / xform.scale,
		.height = rect.height / xform.scale,
	};
}

skb_vec2_t render_transform_point(const render_context_t* rc, skb_vec2_t pt)
{
	const render__transform_t xform = rc->transform_stack[rc->transform_stack_idx];
	return (skb_vec2_t) {
		.x = xform.dx + pt.x * xform.scale,
		.y = xform.dy + pt.y * xform.scale,
	};
}

skb_vec2_t render_inv_transform_point(const render_context_t* rc, skb_vec2_t pt)
{
	const render__transform_t xform = rc->transform_stack[rc->transform_stack_idx];
	return (skb_vec2_t) {
		.x = (pt.x - xform.dx) / xform.scale,
		.y = (pt.y - xform.dy) / xform.scale,
	};
}

skb_image_atlas_t* render_get_atlas(render_context_t* rc)
{
	assert(rc);
	return rc->atlas;
}

skb_rasterizer_t* render_get_rasterizer(render_context_t* rc)
{
	assert(rc);
	return rc->rasterizer;
}

skb_temp_alloc_t* render_get_temp_alloc(render_context_t* rc)
{
	assert(rc);
	return rc->temp_alloc;
}

uint32_t render_create_texture(render_context_t* rc, int32_t img_width, int32_t img_height, int32_t img_stride_bytes, const uint8_t* img_data, uint8_t bpp)
{
	assert(rc);

	SKB_ARRAY_RESERVE(rc->textures, rc->textures_count + 1);
	render__texture_t* tex = &rc->textures[rc->textures_count++];
	memset(tex, 0, sizeof(render__texture_t));
	tex->id = rc->textures_count;

	gl__create_texture(tex, img_width, img_height, img_stride_bytes, img_data, bpp);

	return tex->id;
}

void render_update_texture(render_context_t* rc, uint32_t tex_handle,
	int32_t offset_x, int32_t offset_y, int32_t width, int32_t height,
	int32_t img_width, int32_t img_height, int32_t img_stride_bytes, const uint8_t* img_data)
{
	assert(rc);

	render__texture_t* tex = renderer__find_texture(rc, tex_handle);
	if (tex) {
		if (tex->width != img_width || tex->height != img_height) {
			gl__create_texture(tex, img_width, img_height, img_stride_bytes, img_data, tex->bpp);
		} else {
			gl__update_texture(tex, offset_x, offset_y, width, height, img_stride_bytes, img_data);
		}
	}
}

void render_draw_debug_tris(render_context_t* rc, const render_vert_t* verts, int32_t verts_count)
{
	assert(rc);

	const render__transform_t xform = rc->transform_stack[rc->transform_stack_idx];
	const uint32_t scissor_id = rc->scissor_stack[rc->scissor_stack_idx];

	render__vertex_t transformed_verts[16*3];
	int32_t transformed_verts_count = 0;

	for (int32_t i = 0; i < verts_count; i++) {
		const skb_vec2_t pos = verts[i].pos;
		const skb_color_t col = verts[i].col;

		transformed_verts[transformed_verts_count++] = (render__vertex_t) {
			.pos = (skb_vec2_t) {
				.x = xform.dx + pos.x * xform.scale,
				.y = xform.dy + pos.y * xform.scale
			},
			.color = col,
		};

		if (transformed_verts_count >= (16*3)) {
			renderer__push_triangles(rc, transformed_verts, transformed_verts_count, 0, 0, scissor_id);
			transformed_verts_count = 0;
		}
	}

	if (transformed_verts_count > 0)
		renderer__push_triangles(rc, transformed_verts, transformed_verts_count, 0, 0, scissor_id);
}

void render_draw_quad(render_context_t* rc, const skb_quad_t* quad)
{
	assert(rc);

	const skb_rect2_t geom = render_transform_rect(rc, quad->geom);
	const uint32_t scissor_id = rc->scissor_stack[rc->scissor_stack_idx];

	const float x0 = geom.x;
	const float y0 = geom.y;
	const float x1 = geom.x + geom.width;
	const float y1 = geom.y + geom.height;
	const float u0 = quad->pattern.x;
	const float v0 = quad->pattern.y;
	const float u1 = quad->pattern.x + quad->pattern.width;
	const float v1 = quad->pattern.y + quad->pattern.height;
	const skb_vec2_t tex_pos = {quad->texture.x, quad->texture.y };
	const skb_vec2_t tex_size = {quad->texture.width, quad->texture.height };
	const float scale = quad->scale;
	const skb_color_t tint = quad->color;

	const render__vertex_t verts[] = {
		{ .pos = {x0, y0}, .uv = {u0, v0 }, .atlas_pos = tex_pos, .atlas_size = tex_size, .color = tint, .scale = scale },
		{ .pos = {x1, y0}, .uv = {u1, v0 }, .atlas_pos = tex_pos, .atlas_size = tex_size, .color = tint, .scale = scale },
		{ .pos = {x1, y1}, .uv = {u1, v1 }, .atlas_pos = tex_pos, .atlas_size = tex_size, .color = tint, .scale = scale },

		{ .pos = {x0, y0}, .uv = {u0, v0 }, .atlas_pos = tex_pos, .atlas_size = tex_size, .color = tint, .scale = scale },
		{ .pos = {x1, y1}, .uv = {u1, v1 }, .atlas_pos = tex_pos, .atlas_size = tex_size, .color = tint, .scale = scale },
		{ .pos = {x0, y1}, .uv = {u0, v1 }, .atlas_pos = tex_pos, .atlas_size = tex_size, .color = tint, .scale = scale },
	};

	const uint32_t tex_id = (uint32_t)skb_image_atlas_get_texture_user_data(rc->atlas, quad->texture_idx);

	if (quad->flags & SKB_QUAD_IS_SDF)
		renderer__push_triangles(rc, verts, SKB_COUNTOF(verts), 0, tex_id, scissor_id);
	else
		renderer__push_triangles(rc, verts, SKB_COUNTOF(verts), tex_id, 0, scissor_id);
}

void render_update_atlas(render_context_t* rc)
{
	// Update atlas and textures
	if (skb_image_atlas_rasterize_missing_items(rc->atlas, rc->temp_alloc, rc->rasterizer)) {
		for (int32_t i = 0; i < skb_image_atlas_get_texture_count(rc->atlas); i++) {
			skb_rect2i_t dirty_bounds = skb_image_atlas_get_and_reset_texture_dirty_bounds(rc->atlas, i);
			if (!skb_rect2i_is_empty(dirty_bounds)) {
				const skb_image_t* image = skb_image_atlas_get_texture(rc->atlas, i);
				assert(image);
				uint32_t tex_id = (uint32_t)skb_image_atlas_get_texture_user_data(rc->atlas, i);
				if (tex_id == 0) {
					tex_id = render_create_texture(rc, image->width, image->height, image->stride_bytes, image->buffer, image->bpp);
					assert(tex_id);
					skb_image_atlas_set_texture_user_data(rc->atlas, i, tex_id);
				} else {
					render_update_texture(rc, tex_id,
						dirty_bounds.x, dirty_bounds.y, dirty_bounds.width, dirty_bounds.height,
						image->width, image->height, image->stride_bytes, image->buffer);
				}
			}
		}
	}
}

void render_end_frame(render_context_t* rc)
{
	assert(rc);

	render_update_atlas(rc);

	if (rc->verts_count == 0 || rc->batches_count == 0) {
		rc->verts_count = 0;
		rc->batches_count = 0;
		return;
	}

	gl__flush(rc);
}

//
// High level API
//

void render_draw_glyph(render_context_t* rc,
	float offset_x, float offset_y,
	skb_font_collection_t* font_collection, skb_font_handle_t font_handle, uint32_t glyph_id, float font_size,
	skb_color_t color, skb_rasterize_alpha_mode_t alpha_mode)
{
	assert(rc);
	assert(font_collection);
	skb_image_atlas_t* atlas = render_get_atlas(rc);

	// Glyph image
	skb_quad_t quad = skb_image_atlas_get_glyph_quad(
		atlas, offset_x, offset_y, render_get_transform_scale(rc),
		font_collection, font_handle, glyph_id, font_size,
		color, alpha_mode);

	render_draw_quad(rc, &quad);
}

void render_draw_icon(render_context_t* rc,
	float offset_x, float offset_y,
	skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle, float width, float height,
	skb_color_t color, skb_rasterize_alpha_mode_t alpha_mode)
{
	assert(rc);

	skb_image_atlas_t* atlas = render_get_atlas(rc);

	skb_quad_t quad = skb_image_atlas_get_icon_quad(
		atlas, offset_x, offset_y, render_get_transform_scale(rc),
		icon_collection, icon_handle, width, height,
		color, alpha_mode);

	render_draw_quad(rc, &quad);
}

void render_draw_decoration_line(render_context_t* rc,
	float offset_x, float offset_y,
	skb_decoration_style_t style, skb_decoration_position_t position, float length, float pattern_offset, float thickness,
	skb_color_t color, skb_rasterize_alpha_mode_t alpha_mode)
{
	assert(rc);

	skb_image_atlas_t* atlas = render_get_atlas(rc);

	skb_quad_t quad = skb_image_atlas_get_decoration_quad(
		atlas, offset_x, offset_y, render_get_transform_scale(rc),
		position, style, length, pattern_offset, thickness,
		color, alpha_mode);
	render_draw_quad(rc, &quad);
}


static void render__filled_rect(render_context_t* rc, float x, float y, float w, float h, skb_color_t col)
{
	const render_vert_t verts[] = {
		{ {x,y}, col },
		{ {x+w,y}, col },
		{ {x+w,y+h}, col },
		{ {x,y}, col },
		{ {x+w,y+h}, col },
		{ {x,y+h}, col },
	};
	render_draw_debug_tris(rc, verts, SKB_COUNTOF(verts));
}

void render_draw_decoration(render_context_t* rc,
	float offset_x, float offset_y,
	const skb_decoration_t* decoration, skb_color_t color, skb_rasterize_alpha_mode_t alpha_mode)
{
	if (decoration->type == SKB_DECORATION_LINE) {
		render_draw_decoration_line(rc, offset_x + decoration->line.x, offset_y + decoration->line.y,
			decoration->line.style, decoration->line.position, decoration->line.length, decoration->line.pattern_offset, decoration->line.thickness,
			color, alpha_mode);
	} else if (decoration->type == SKB_DECORATION_RECT) {
		render__filled_rect(rc, offset_x + decoration->rect.x, offset_y + decoration->rect.y, decoration->rect.width, decoration->rect.height, color);
	}
}

static inline uint32_t render__get_run_state(intptr_t content_id, const render_content_state_t* content_states, int32_t content_states_count)
{
	if (content_id == 0)
		return SKB_PAINT_STATE_DEFAULT;
	for (int32_t i = 0; i < content_states_count; i++) {
		if (content_states[i].content_id == content_id)
			return content_states[i].state;
	}
	return SKB_PAINT_STATE_DEFAULT;
}

static void render__draw_layout_backgrounds(
	render_context_t* rc, const skb_rect2_t* view_bounds, float offset_x, float offset_y,
	const skb_layout_t* layout, skb_rasterize_alpha_mode_t alpha_mode, bool draw_paragraph_background,
	const render_content_state_t* content_states, int32_t content_states_count)
{
	assert(rc);
	assert(layout);

	// Draw layout
	const skb_layout_params_t* layout_params = skb_layout_get_params(layout);
	const skb_rect2_t layout_bounds = skb_layout_get_bounds(layout);

	// Draw paragraph backgrounds
	if (draw_paragraph_background) {
		const skb_attribute_paint_t background_paint = skb_attributes_get_paint(SKB_PAINT_PARAGRAPH_BACKGROUND, SKB_PAINT_STATE_DEFAULT, layout_params->layout_attributes, layout_params->attribute_collection);
		if (background_paint.paint_tag == SKB_PAINT_PARAGRAPH_BACKGROUND && background_paint.color.a > 0)
			render__filled_rect(rc, offset_x + layout_bounds.x, offset_y + layout_bounds.y, layout_bounds.width, layout_bounds.height, background_paint.color);
	}

	// Indent decoration
	const skb_attribute_paint_t bar_paint = skb_attributes_get_paint(SKB_PAINT_INDENT_DECORATION, SKB_PAINT_STATE_DEFAULT, layout_params->layout_attributes, layout_params->attribute_collection);
	if (bar_paint.paint_tag == SKB_PAINT_INDENT_DECORATION) {
		const skb_layout_indent_decoration_info_t indent_info = skb_layout_get_indent_decoration_info(layout);
		for (int32_t i = 0; i < indent_info.levels; i++)
			render__filled_rect(rc, offset_x + indent_info.x + (float)i * indent_info.level_increment, offset_y + indent_info.y, indent_info.width, indent_info.height, bar_paint.color);
	}
}

void render__draw_layout_with_state(
	render_context_t* rc, const skb_rect2_t* view_bounds, float offset_x, float offset_y,
	const skb_layout_t* layout, skb_rasterize_alpha_mode_t alpha_mode, bool draw_paragraph_background,
	const render_content_state_t* content_states, int32_t content_states_count)
{
	assert(rc);
	assert(layout);

	const skb_vec2_t offset = { .x = offset_x, .y = offset_y };

	if (view_bounds) {
		const skb_rect2_t layout_content_bounds = skb_layout_get_content_bounds(layout);
		if (!arb_rect2_overlap(*view_bounds, skb_rect2_translate(layout_content_bounds, offset)))
			return;
	}

	render__draw_layout_backgrounds(rc, view_bounds, offset_x, offset_y, layout, alpha_mode, draw_paragraph_background, content_states, content_states_count);

	// Draw layout
	const skb_layout_params_t* layout_params = skb_layout_get_params(layout);
	const skb_layout_line_t* layout_lines = skb_layout_get_lines(layout);
	const int32_t layout_lines_count = skb_layout_get_lines_count(layout);
	const skb_layout_run_t* layout_runs = skb_layout_get_layout_runs(layout);
	const skb_glyph_t* glyphs = skb_layout_get_glyphs(layout);
	const skb_decoration_t* decorations = skb_layout_get_decorations(layout);

	for (int32_t li = 0; li < layout_lines_count; li++) {
		const skb_layout_line_t* line = &layout_lines[li];
		if (view_bounds && !arb_rect2_overlap(*view_bounds, skb_rect2_translate(line->culling_bounds, offset)))
			continue;

		// Draw underlines
		for (int32_t i = line->decorations_range.start; i < line->decorations_range.end; i++) {
			const skb_decoration_t* decoration = &decorations[i];
			if (decoration->layer == SKB_DECORATION_UNDER) {
				const skb_layout_run_t* run = &layout_runs[decoration->line.layout_run_idx];
				const uint32_t state = render__get_run_state(run->content_id, content_states, content_states_count);
				const skb_attribute_set_t run_attributes = skb_layout_get_layout_run_attributes(layout, run);
				const skb_attribute_paint_t paint = skb_attributes_get_paint(decoration->line.paint_tag, state, run_attributes, layout_params->attribute_collection);
				if (paint.paint_tag == decoration->line.paint_tag)
					render_draw_decoration(rc, offset_x, offset_y, decoration, paint.color, SKB_RASTERIZE_ALPHA_SDF);
			}
		}

		// Draw glyphs
		for (int32_t ri = line->layout_run_range.start; ri < line->layout_run_range.end; ri++) {
			const skb_layout_run_t* run = &layout_runs[ri];
			const skb_attribute_set_t run_attributes = skb_layout_get_layout_run_attributes(layout, run);
			const uint32_t state = render__get_run_state(run->content_id, content_states, content_states_count);
			skb_attribute_paint_t paint = skb_attributes_get_paint(SKB_PAINT_TEXT, state, run_attributes, layout_params->attribute_collection);

			if (run->type == SKB_CONTENT_RUN_OBJECT) {
				// Object
			} else if (run->type == SKB_CONTENT_RUN_ICON) {
				// Icon
				const skb_rect2_t icon_rect = skb_layout_get_layout_run_content_bounds(layout, run);
				if (view_bounds && !arb_rect2_overlap(*view_bounds, skb_rect2_translate(icon_rect, offset)))
					continue;
				render_draw_icon(rc, offset_x + icon_rect.x, offset_y + icon_rect.y,
					layout_params->icon_collection, run->icon_handle, icon_rect.width, icon_rect.height,
					paint.color, alpha_mode);
			} else {
				// Text
				for (int32_t gi = run->glyph_range.start; gi < run->glyph_range.end; gi++) {
					const skb_glyph_t* glyph = &glyphs[gi];
					if (view_bounds) {
						skb_rect2_t coarse_glyph_bounds = line->common_glyph_bounds;
						coarse_glyph_bounds.x += glyph->offset_x;
						coarse_glyph_bounds.y += glyph->offset_y;
						if (!arb_rect2_overlap(*view_bounds, skb_rect2_translate(coarse_glyph_bounds, offset)))
							continue;
					}
					render_draw_glyph(rc, offset_x + glyph->offset_x, offset_y + glyph->offset_y,
						layout_params->font_collection, run->font_handle, glyph->gid, run->font_size,
						paint.color, alpha_mode);
				}
			}
		}

		// Draw through lines.
		for (int32_t i = line->decorations_range.start; i < line->decorations_range.end; i++) {
			const skb_decoration_t* decoration = &decorations[i];
			if (decoration->layer == SKB_DECORATION_OVER) {
				const skb_layout_run_t* run = &layout_runs[decoration->line.layout_run_idx];
				const uint32_t state = render__get_run_state(run->content_id, content_states, content_states_count);
				const skb_attribute_set_t run_attributes = skb_layout_get_layout_run_attributes(layout, run);
				const skb_attribute_paint_t paint = skb_attributes_get_paint(decoration->line.paint_tag, state, run_attributes, layout_params->attribute_collection);
				if (paint.paint_tag == decoration->line.paint_tag)
					render_draw_decoration(rc, offset_x, offset_y, decoration, paint.color, SKB_RASTERIZE_ALPHA_SDF);
			}
		}
	}
}

void render_draw_layout(render_context_t* rc, const skb_rect2_t* view_bounds, float offset_x, float offset_y, const skb_layout_t* layout, skb_rasterize_alpha_mode_t alpha_mode)
{
	render__draw_layout_with_state(rc, view_bounds, offset_x, offset_y, layout, alpha_mode, true, NULL, 0);
}

void render_draw_layout_with_state(
	render_context_t* rc, const skb_rect2_t* view_bounds, float offset_x, float offset_y,
	const skb_layout_t* layout, skb_rasterize_alpha_mode_t alpha_mode, const render_content_state_t* content_states, int32_t content_states_count)
{
	render__draw_layout_with_state(rc, view_bounds, offset_x, offset_y, layout, alpha_mode, true, content_states, content_states_count);
}

void render_draw_rich_layout(
	render_context_t* rc, const skb_rect2_t* view_bounds, float offset_x, float offset_y,
	const skb_rich_layout_t* rich_layout, skb_rasterize_alpha_mode_t alpha_mode)
{
	render_draw_rich_layout_with_state(rc, view_bounds, offset_x, offset_y, rich_layout, alpha_mode, NULL, 0);
}

void render_draw_rich_layout_with_state(
	render_context_t* rc, const skb_rect2_t* view_bounds, float offset_x, float offset_y,
	const skb_rich_layout_t* rich_layout, skb_rasterize_alpha_mode_t alpha_mode, const render_content_state_t* content_states, int32_t content_states_count)
{
	// Draw paragraph backgrounds.
	skb_attribute_paint_t prev_background_paint = { 0 };
	float background_min_x = 0.f;
	float background_max_x = 0.f;
	float background_min_y = 0.f;
	float background_max_y = 0.f;

	for (int32_t pi = 0; pi < skb_rich_layout_get_paragraphs_count(rich_layout); pi++) {
		const skb_layout_t* layout = skb_rich_layout_get_layout(rich_layout, pi);
		const skb_layout_params_t* layout_params = skb_layout_get_params(layout);
		const skb_rect2_t layout_bounds = skb_layout_get_bounds(layout);
		const skb_vec2_t layout_offset = skb_rich_layout_get_layout_offset(rich_layout, pi);
		const float layout_advance_y = skb_layout_get_advance_y(layout);

		const skb_attribute_paint_t background_paint = skb_attributes_get_paint(SKB_PAINT_PARAGRAPH_BACKGROUND, SKB_PAINT_STATE_DEFAULT, layout_params->layout_attributes, layout_params->attribute_collection);

		const bool prev_is_valid = prev_background_paint.paint_tag == SKB_PAINT_PARAGRAPH_BACKGROUND;
		const bool curr_is_valid = background_paint.paint_tag == SKB_PAINT_PARAGRAPH_BACKGROUND;
		const bool paints_are_same = memcmp(&prev_background_paint, &background_paint, sizeof(background_paint)) == 0;

		if (paints_are_same) {
			// Update paint
			background_max_y = layout_offset.y + layout_bounds.y + layout_bounds.height;
			if (layout_params->flags & SKB_LAYOUT_PARAMS_SAME_GROUP_AFTER)
				background_max_y = layout_offset.y + layout_advance_y;
			background_min_x = skb_minf(background_min_x, layout_offset.x + layout_bounds.x);
			background_max_x = skb_maxf(background_max_x, layout_offset.x + layout_bounds.x + layout_bounds.width);
		} else {
			if (prev_is_valid) {
				// Flush paint
				render__filled_rect(rc,
					offset_x + background_min_x, offset_y + background_min_y,
					background_max_x - background_min_x, background_max_y - background_min_y,
					prev_background_paint.color);
			}
			if (curr_is_valid) {
				// Start new paint
				background_min_y = layout_offset.y + layout_bounds.y;
				if (layout_params->flags & SKB_LAYOUT_PARAMS_SAME_GROUP_BEFORE)
					background_min_y = layout_offset.y;

				background_max_y = layout_offset.y + layout_bounds.y + layout_bounds.height;
				if (layout_params->flags & SKB_LAYOUT_PARAMS_SAME_GROUP_AFTER)
					background_max_y = layout_offset.y + layout_advance_y;

				background_min_x = layout_offset.x + layout_bounds.x;
				background_max_x = layout_offset.x + layout_bounds.x + layout_bounds.width;
			}
		}

		prev_background_paint = background_paint;
	}

	if (prev_background_paint.paint_tag == SKB_PAINT_PARAGRAPH_BACKGROUND) {
		// Flush paint
		render__filled_rect(rc,
			offset_x + background_min_x, offset_y + background_min_y,
			background_max_x - background_min_x, background_max_y - background_min_y,
			prev_background_paint.color);
	}

	for (int32_t pi = 0; pi < skb_rich_layout_get_paragraphs_count(rich_layout); pi++) {
		const skb_layout_t* layout = skb_rich_layout_get_layout(rich_layout, pi);
		const skb_vec2_t layout_offset = skb_rich_layout_get_layout_offset(rich_layout, pi);
		render__draw_layout_with_state(rc, view_bounds, offset_x + layout_offset.x, offset_y + layout_offset.y, layout, alpha_mode, false, content_states, content_states_count);
	}
}


typedef struct render__selection_draw_context_t {
	render_context_t* rc;
	const skb_rect2_t* view_bounds;
	float offset_x;
	float offset_y;
	skb_color_t color;
} render__selection_draw_context_t;

static void draw_selection_rect(skb_rect2_t rect, void* context)
{
	render__selection_draw_context_t* ctx = (render__selection_draw_context_t*)context;
	render__filled_rect(ctx->rc, ctx->offset_x + rect.x, ctx->offset_y + rect.y, rect.width, rect.height, ctx->color);
}

void render_draw_text_range_background(
		render_context_t* rc, const skb_rect2_t* view_bounds, float offset_x, float offset_y,
		const skb_rich_layout_t* rich_layout, skb_text_range_t text_range, skb_color_t color)
{
	render__selection_draw_context_t ctx = {
		.rc = rc,
		.view_bounds = view_bounds,
		.offset_x = offset_x,
		.offset_y = offset_y,
		.color = color,
	};

	skb_rich_layout_get_text_range_bounds(rich_layout, text_range, draw_selection_rect, &ctx);
}

void render_draw_caret(
		render_context_t* rc, const skb_rect2_t* view_bounds, float offset_x, float offset_y,
		const skb_caret_info_t* caret_info, float caret_width, skb_color_t color)
{
	float x = caret_info->x + offset_x;
	float y = caret_info->y + offset_y;

	float as = skb_absf(caret_info->ascender) / 5.f;
	float hw = caret_width * 0.5f;
	float top_x = x + caret_info->ascender * caret_info->slope;
	float top_y = y + caret_info->ascender;
	float bot_x = x + caret_info->descender * caret_info->slope;
	float bot_y = y + caret_info->descender;
	float mid_x = top_x + as * caret_info->slope;
	float mid_y = top_y + as;

	top_x += skb_is_rtl(caret_info->direction) ? -as : 0.f;

	const render_vert_t verts[] = {
		// Triangle
		{ {top_x-hw,top_y}, color },
		{ {top_x+hw+as,top_y}, color },
		{ {mid_x+hw,mid_y}, color },

		{ {top_x-hw,top_y}, color },
		{ {mid_x+hw,mid_y}, color },
		{ {mid_x-hw,mid_y}, color },

		// Stem
		{ {mid_x-hw,mid_y}, color },
		{ {mid_x+hw,mid_y}, color },
		{ {bot_x+hw,bot_y}, color },

		{ {mid_x-hw,mid_y}, color },
		{ {bot_x+hw,bot_y}, color },
		{ {bot_x-hw,bot_y}, color },
	};
	render_draw_debug_tris(rc, verts, SKB_COUNTOF(verts));
}

//
// OpengGL specific
//

static void gl__check_error(const char* str)
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		const char* err_str = "";
		switch (err) {
		case GL_INVALID_ENUM: err_str = "GL_INVALID_ENUM"; break;
		case GL_INVALID_VALUE: err_str = "GL_INVALID_VALUE"; break;
		case GL_INVALID_OPERATION: err_str = "GL_INVALID_OPERATION"; break;
		case GL_OUT_OF_MEMORY: err_str = "GL_OUT_OF_MEMORY"; break;
		case GL_INVALID_FRAMEBUFFER_OPERATION: err_str = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
		default: err_str = ""; break;
		}
		skb_debug_log("Error %08x %s after %s\n", err, err_str, str);
		return;
	}
}

static void gl__print_program_log(GLuint program)
{
	if (!glIsProgram(program)) {
		skb_debug_log("Name %d is not a program\n", program);
		return;
	}

	int32_t max_length = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_length);

	char* info_log = skb_malloc(max_length);

	int32_t info_log_length = 0;
	glGetProgramInfoLog(program, max_length, &info_log_length, info_log);
	skb_debug_log("%s\n", info_log);

	skb_free(info_log);
}

static void gl__print_shader_log(GLuint shader)
{
	if (!glIsShader(shader)) {
		skb_debug_log("Name %d is not a shader\n", shader);
		return;
	}

	int32_t max_length = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_length);

	char* info_log = skb_malloc(max_length);

	int32_t info_log_length = 0;
	glGetShaderInfoLog(shader, max_length, &info_log_length, info_log);
	skb_debug_log("%s\n", info_log);

	skb_free(info_log);
}

static bool gl__init(render_context_t* rc)
{
	gl__check_error("init");

	GLint success = GL_FALSE;

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	const GLchar* vs_source = "#version 140\n"
		"in vec3 a_pos;\n"
		"in vec2 a_uv;\n"
		"in vec4 a_color;\n"
		"in vec2 a_atlas_pos;\n"
		"in vec2 a_atlas_size;\n"
		"in float a_scale;\n"
		"out vec4 f_color;\n"
		"out vec2 f_uv;\n"
		"out vec2 f_atlas_pos;\n"
		"out vec2 f_atlas_size;\n"
		"out float f_scale;\n"
		"uniform vec2 u_view_size;\n"
		"uniform vec2 u_tex_size;\n"
		"void main() {\n"
		"	f_color = a_color;\n"
		"	f_scale = a_scale;\n"
		"	f_uv = a_uv;\n"
		"	f_atlas_pos = a_atlas_pos / u_tex_size;\n"
		"	f_atlas_size = a_atlas_size / u_tex_size;\n"
		"	gl_Position = vec4(2.0*a_pos.x/u_view_size.x - 1.0, 1.0 - 2.0*a_pos.y/u_view_size.y, 0, 1);\n"
		"}\n";

	glShaderSource(vs, 1, &vs_source, NULL);
	glCompileShader(vs);

	glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE) {
		skb_debug_log("Unable to compile vertex shader.\n");
		gl__print_shader_log(vs);
		return 0;
	}

	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	const GLchar* fs_source =
		"#version 140\n"
		"in vec4 f_color;\n"
		"in vec2 f_uv;\n"
		"in vec2 f_atlas_pos;\n"
		"in vec2 f_atlas_size;\n"
		"in float f_scale;\n"
		"out vec4 fragment;\n"
		"uniform sampler2D u_tex;\n"
		"uniform int u_tex_type;\n"
		"\n"
		"vec4 sdf(vec4 col) {\n"
		"	float d = (col.a - (128./255.)) / (32./255.);\n"
		"	float w = 0.8 / f_scale;\n"
		"	float a = 1.f - clamp((d + 0.5*w - 0.1) / w, 0., 1.);\n"
		"	return vec4(col.rgb * a, a); // premultiply\n"
		"}\n"
		"\n"
		"void main() {\n"
		"	vec4 color = vec4(f_color.rgb * f_color.a, f_color.a); // premult\n"
		"	// Texture region wrapping\n"
		"	vec2 uv = f_atlas_pos + fract(f_uv) * f_atlas_size;\n"
		"	// Sample texture and handle RGBA vs A\n"
		"	vec4 tex_color = vec4(1.);\n"
		"	if (u_tex_type == 1) {\n"
		"		// RGBA pre-multiplied\n"
		"		tex_color = texture(u_tex, uv);\n"
		"	} else if (u_tex_type == 2) {\n"
		"		// Alpha (make pre-multiplied white)\n"
		"		tex_color = vec4(texture(u_tex, uv).x);\n"
		"	} else if (u_tex_type == 3) {\n"
		"		// RGB SDF\n"
		"		tex_color = sdf(texture(u_tex, uv));\n"
		"	} else if (u_tex_type == 4) {\n"
		"		// SDF\n"
		"		tex_color = sdf(vec4(1.,1.,1.,texture(u_tex, uv).x));\n"
		"	}\n"
		"	fragment = tex_color * color;\n"
		"}\n";

	glShaderSource(fs, 1, &fs_source, NULL);
	glCompileShader(fs);

	glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE) {
		skb_debug_log("Unable to compile fragment shader.\n");
		gl__print_shader_log(fs);
		return false;
	}

	rc->program = glCreateProgram();
	glAttachShader(rc->program, vs);
	glAttachShader(rc->program, fs);

	glBindAttribLocation(rc->program, 0, "a_pos");
	glBindAttribLocation(rc->program, 1, "a_uv");
	glBindAttribLocation(rc->program, 2, "a_atlas_pos");
	glBindAttribLocation(rc->program, 3, "a_atlas_size");
	glBindAttribLocation(rc->program, 4, "a_color");
	glBindAttribLocation(rc->program, 5, "a_scale");

	glLinkProgram(rc->program);

	glGetProgramiv(rc->program, GL_LINK_STATUS, &success);
	if (success != GL_TRUE) {
		skb_debug_log("Error linking program %d!\n", rc->program);
		gl__print_program_log(rc->program);
		return false;
	}

	glDeleteShader(vs);
	glDeleteShader(fs);
	gl__check_error("link");

	glGenBuffers(1, &rc->vbo);
	gl__check_error("gen vbo");

	return true;
}

static void gl__destroy(render_context_t* rc)
{
	glDeleteBuffers(1, &rc->vbo);
	glDeleteProgram(rc->program);
	for (int32_t i = 0; i < rc->textures_count; i++) {
		render__texture_t* tex = &rc->textures[i];
		glDeleteTextures(1, &tex->tex_id);
	}
}

static void gl__flush(render_context_t* rc)
{
	gl__check_error("flush start");

	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premult alpha
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glEnable(GL_SCISSOR_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	gl__check_error("flush state");

	glUseProgram(rc->program);
	glUniform2f(glGetUniformLocation(rc->program, "u_view_size"), (float)rc->view_width, (float)rc->view_height);

	gl__check_error("prog");

	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	gl__check_error("vao");

	glBindBuffer(GL_ARRAY_BUFFER, rc->vbo);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glEnableVertexAttribArray(4);
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(render__vertex_t), (GLvoid*)(0 + offsetof(render__vertex_t, pos)));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(render__vertex_t), (GLvoid*)(0 + offsetof(render__vertex_t, uv)));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(render__vertex_t), (GLvoid*)(0 + offsetof(render__vertex_t, atlas_pos)));
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(render__vertex_t), (GLvoid*)(0 + offsetof(render__vertex_t, atlas_size)));
	glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(render__vertex_t), (GLvoid*)(0 + offsetof(render__vertex_t, color)));
	glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(render__vertex_t), (GLvoid*)(0 + offsetof(render__vertex_t, scale)));

	glBufferData(GL_ARRAY_BUFFER, rc->verts_count * sizeof(render__vertex_t), rc->verts, GL_STREAM_DRAW);

	gl__check_error("vbo");

	uint32_t prev_scissor_id = ~1;

	for (int32_t i = 0; i < rc->batches_count; i++) {
		const render__batch_t* b = &rc->batches[i];
		const render__texture_t* image_tex = renderer__find_texture(rc, b->image_id);
		const render__texture_t* sdf_tex = renderer__find_texture(rc, b->sdf_id);

		if (prev_scissor_id != b->scissor_id) {
			const skb_rect2_t scissor_rect = rc->scissors[b->scissor_id];
			// GL scissor starts from bottom-left.
			const int32_t x = (int32_t)scissor_rect.x;
			const int32_t y = rc->view_height - (int32_t)scissor_rect.y - (int32_t)scissor_rect.height;
			const int32_t w = (int32_t)scissor_rect.width;
			const int32_t h = (int32_t)scissor_rect.height;
			glScissor(x, y, w, h);
			prev_scissor_id = b->scissor_id;
		}

		if (image_tex && image_tex->tex_id) {
			glUniform1i(glGetUniformLocation(rc->program, "u_tex_type"), image_tex->bpp == 4 ? 1: 2);
			glUniform2f(glGetUniformLocation(rc->program, "u_tex_size"), (float)image_tex->width, (float)image_tex->height);
			glUniform1i(glGetUniformLocation(rc->program, "u_tex"), 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, image_tex->tex_id);
			gl__check_error("texture image");
		} else if (sdf_tex && sdf_tex->tex_id) {
			glUniform1i(glGetUniformLocation(rc->program, "u_tex_type"), sdf_tex->bpp == 4 ? 3: 4);
			glUniform2f(glGetUniformLocation(rc->program, "u_tex_size"), (float)sdf_tex->width, (float)sdf_tex->height);
			glUniform1i(glGetUniformLocation(rc->program, "u_tex"), 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sdf_tex->tex_id);
			gl__check_error("texture sdf");
		} else {
			glUniform1i(glGetUniformLocation(rc->program, "u_tex_type"), 0);
			glUniform2f(glGetUniformLocation(rc->program, "u_tex_size"), 1, 1);
			glUniform1i(glGetUniformLocation(rc->program, "u_tex"), 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
			gl__check_error("texture none");
		}

		glDrawArrays(GL_TRIANGLES, b->offset, b->count);
	}

	gl__check_error("draw");

	//    glUseProgram(0);
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vao);

	gl__check_error("end");

	rc->verts_count = 0;
	rc->batches_count = 0;
}

static void gl__create_texture(render__texture_t* tex, int32_t width, int32_t height, int32_t stride_bytes, const uint8_t* image, uint8_t bpp)
{
	// Delete old of exists.
	if (tex->tex_id) {
		glDeleteTextures(1, &tex->tex_id);
		tex->tex_id = 0;
	}

	tex->width = width;
	tex->height = height;
	tex->bpp = bpp;

	// upload image
	glGenTextures(1, &tex->tex_id);
	glBindTexture(GL_TEXTURE_2D, tex->tex_id);

	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_bytes / bpp);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

	assert(bpp == 4 || bpp == 1);
	if (bpp == 4)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	else if (bpp == 1)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);

	gl__check_error("resize texture");
}

static void gl__update_texture(render__texture_t* tex, int32_t offset_x, int32_t offset_y, int32_t width, int32_t height, int32_t stride_bytes, const uint8_t* image)
{
	// upload image
	glBindTexture(GL_TEXTURE_2D, tex->tex_id);

	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_bytes / tex->bpp);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, offset_x);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, offset_y);

	if (tex->bpp == 4)
		glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image);
	else if (tex->bpp == 1)
		glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, width, height, GL_RED, GL_UNSIGNED_BYTE, image);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

	glBindTexture(GL_TEXTURE_2D, 0);

	gl__check_error("update texture");
}
