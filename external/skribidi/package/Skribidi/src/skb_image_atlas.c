// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_image_atlas.h"

#include "skb_common_internal.h"
#include "skb_font_collection.h"
#include "skb_font_collection_internal.h"
#include "skb_icon_collection_internal.h"
#include "skb_rasterizer.h"

#include <assert.h>
#include <string.h>

#include "hb.h"
#include "hb-ot.h"


enum {
	SKB_SHELF_PACKER_NULL = 0xffff,
};

// Atlas row flags
enum skb__shelf_packer_row_flags_t {
	SKB__SHELF_PACKER_ROW_IS_EMPTY = 1 << 0,
	SKB__SHELF_PACKER_ROW_IS_FREED = 1 << 1,
};

typedef struct skb__shelf_packer_row_t {
	uint16_t y;
	uint16_t height;
	uint16_t base_height;
	uint16_t max_diff;
	uint16_t max_empty_item_width;
	uint16_t first_item;
	uint16_t next;
	uint8_t flags;
} skb__shelf_packer_row_t;

// Atlas item flags
enum skb__shelf_packer_item_flags_t {
	SKB__SHELF_PACKER_ITEM_IS_EMPTY = 1 << 0,
	SKB__SHELF_PACKER_ITEM_IS_FREED = 1 << 1,
};

typedef struct skb__shelf_packer_item_t {
	uint16_t x;
	uint16_t width;
	uint16_t next;
	uint16_t generation;
	uint16_t row;
	uint8_t flags;
} skb__shelf_packer_item_t;

typedef struct skb__shelf_packer_handle_t {
	uint16_t index;
	uint16_t generation;
} skb__shelf_packer_handle_t;

typedef struct skb__shelf_packer_t {

	int32_t width;
	int32_t height;

	int32_t occupancy;

	skb__shelf_packer_row_t* rows;
	int32_t rows_count;
	int32_t rows_cap;

	skb__shelf_packer_item_t* items;
	int32_t items_count;
	int32_t items_cap;

	uint16_t first_row;
	uint16_t row_freelist;
	uint16_t item_freelist;

} skb__shelf_packer_t;

static void skb__shelf_packer_init(skb__shelf_packer_t* packer, int32_t width, int32_t height);
static void skb__shelf_packer_destroy(skb__shelf_packer_t* packer);


typedef enum {
	SKB__ITEM_STATE_REMOVED = 0,
	SKB__ITEM_STATE_INITIALIZED,
	SKB__ITEM_STATE_RASTERIZED,
} skb__item_state_t;

typedef enum {
	SKB__ITEM_TYPE_GLYPH = 0,
	SKB__ITEM_TYPE_ICON,
	SKB__ITEM_TYPE_PATTERN,
} skb__item_type_t;

// Item flags
typedef enum {
	SKB__ITEM_IS_COLOR = 1 << 0,
	SKB__ITEM_IS_SDF   = 1 << 1,
} skb__item_flags_t;

typedef struct skb__item_glyph_t {
	const skb_font_t* font;
	uint32_t gid;
	float clamped_font_size;
} skb__item_glyph_t;

typedef struct skb__item_icon_t {
	const skb_icon_t* icon;
	skb_vec2_t icon_scale;
} skb__item_icon_t;

typedef struct skb__item_pattern_t {
	float thickness;
	uint8_t style;
} skb__item_pattern_t;

typedef struct skb__atlas_item_t {
	union {
		skb__item_glyph_t glyph;
		skb__item_icon_t icon;
		skb__item_pattern_t pattern;
	};
	uint64_t hash_id;
	skb__shelf_packer_handle_t packer_handle;
	int32_t last_access_stamp;
	skb_list_item_t lru;
	int16_t width;
	int16_t height;
	int16_t geom_offset_x;
	int16_t geom_offset_y;
	int16_t texture_offset_x;
	int16_t texture_offset_y;
	uint8_t state;
	uint8_t flags;
	uint8_t texture_idx;
	uint8_t type;
} skb__atlas_item_t;

// Note: atlas size is always up to date, image gets resized during rasterization.
typedef struct skb_atlas_texture_t {
	skb_image_t image;
	skb__shelf_packer_t packer;
	skb_rect2i_t dirty_bounds;
	skb_rect2i_t prev_dirty_bounds;
	uint8_t index;
	uintptr_t user_data;
} skb_atlas_texture_t;

typedef struct skb_image_atlas_t {
	bool initialized;
	bool valid;

	skb_atlas_texture_t* textures;
	int32_t textures_count;
	int32_t textures_cap;

	skb_hash_table_t* items_lookup;
	skb__atlas_item_t* items;
	int32_t items_count;
	int32_t items_cap;
	int32_t items_freelist;
	skb_list_t items_lru;
	bool has_new_items;

	int32_t now_stamp;
	int32_t last_evicted_stamp;

	skb_image_atlas_config_t config;
	skb_create_texture_func_t* create_texture_callback;
	void* create_texture_callback_context;

} skb_image_atlas_t;

static skb_list_item_t* skb__get_item(int32_t item_idx, void* context)
{
	skb_image_atlas_t* atlas = (skb_image_atlas_t*)context;
	return &atlas->items[item_idx].lru;
}

static void skb__image_resize(skb_image_t* image, int32_t new_width, int32_t new_height, uint8_t new_bpp)
{
	uint8_t* new_buffer = skb_malloc(new_width * new_height * new_bpp);
	memset(new_buffer, 0xff, new_width * new_height * new_bpp);

	int32_t new_stride_bytes = new_width * new_bpp;
	// Copy old
	if (image->buffer) {
		if (image->bpp == new_bpp) {
			const int32_t min_width = skb_mini(image->width, new_width);
			const int32_t min_height = skb_mini(image->height, new_height);
			for (int32_t y = 0; y < min_height; y++)
				memcpy(&new_buffer[y * new_stride_bytes], &image->buffer[y * image->stride_bytes], min_width * image->bpp);
		}
		skb_free(image->buffer);
	}
	image->buffer = new_buffer;
	image->width = new_width;
	image->height = new_height;
	image->stride_bytes = new_stride_bytes;
	image->bpp = new_bpp;
}

static void skb__image_destroy(skb_image_t* image)
{
	if (!image) return;
	skb_free(image->buffer);
}

static void skb__atlas_texture_init(skb_atlas_texture_t* texture, uint8_t index, int32_t width, int32_t height, int32_t bpp)
{
	memset(texture, 0, sizeof(*texture));
	texture->index = index;

	skb__image_resize(&texture->image, width, height, bpp);
	skb__shelf_packer_init(&texture->packer, width, height);

	texture->prev_dirty_bounds = (skb_rect2i_t){0};
	texture->dirty_bounds = (skb_rect2i_t){ 0, 0, width, height };
}

static void skb__atlas_texture_destroy(skb_atlas_texture_t* texture)
{
	if (!texture) return;
	skb__image_destroy(&texture->image);

	skb__shelf_packer_destroy(&texture->packer);

	memset(texture, 0, sizeof(*texture));
}


skb_image_atlas_t* skb_image_atlas_create(const skb_image_atlas_config_t* config)
{
	skb_image_atlas_t* atlas = skb_malloc(sizeof(skb_image_atlas_t));
	memset(atlas, 0, sizeof(skb_image_atlas_t));

	atlas->items_lookup = skb_hash_table_create();
	atlas->items_freelist = SKB_INVALID_INDEX;
	atlas->items_lru = skb_list_make();

	if (config)
		atlas->config = *config;
	else
		atlas->config = skb_image_atlas_get_default_config();

	return atlas;
}

void skb_image_atlas_destroy(skb_image_atlas_t* atlas)
{
	if (!atlas) return;

	for (int32_t i = 0; i < atlas->textures_count; i++)
		skb__atlas_texture_destroy(&atlas->textures[i]);
	skb_free(atlas->textures);

	skb_hash_table_destroy(atlas->items_lookup);
	skb_free(atlas->items);

	memset(atlas, 0, sizeof(skb_image_atlas_t));

	skb_free(atlas);
}

static int32_t skb__round_up(int32_t x, int32_t n)
{
	return ((x + n-1) / n) * n;
}

static skb_atlas_texture_t* skb__add_texture(skb_image_atlas_t* atlas, int32_t desired_width, int32_t desired_height, int32_t bpp)
{
	assert(bpp == 4 || bpp == 1);

	SKB_ARRAY_RESERVE(atlas->textures, atlas->textures_count+1);
	int32_t texture_idx = atlas->textures_count++;
	assert(texture_idx <= 255); // using uint8_t to store texture_id

	desired_width = skb_mini(skb_maxi(skb__round_up(desired_width, atlas->config.expand_size), atlas->config.init_width), atlas->config.max_width);
	desired_height = skb_mini(skb_maxi(skb__round_up(desired_height, atlas->config.expand_size), atlas->config.init_height), atlas->config.max_height);

	skb_atlas_texture_t* texture = &atlas->textures[texture_idx];
	skb__atlas_texture_init(texture, (uint8_t)texture_idx, desired_width, desired_height, bpp);

	if (atlas->create_texture_callback)
		atlas->create_texture_callback(atlas, (uint8_t)texture_idx, atlas->create_texture_callback_context);

	return texture;
}

skb_image_atlas_config_t skb_image_atlas_get_default_config(void)
{
	return (skb_image_atlas_config_t)  {
		.init_width = 1024,
		.init_height = 1024,
		.expand_size = 512,
		.max_width = 1024,
		.max_height = 4096,
		.item_height_rounding = 8,
		.fit_max_factor = 0.25f,
		.evict_inactive_duration = 10,
		.glyph_sdf = {
			.padding = 8,
			.rounding = 16.f,
			.min_size = 32.f,
			.max_size = 256.f,
		} ,
		.glyph_alpha = {
			.padding = 2,
			.rounding = 1.f,
			.min_size = 8.f,
			.max_size = 256.f,
		},
		.icon_sdf = {
			.padding = 8,
			.rounding = 16.f,
			.min_size = 32.f,
			.max_size = 256.f,
		},
		.icon_alpha = {
			.padding = 2,
			.rounding = 1.f,
			.min_size = 8.f,
			.max_size = 256.f,
		},
		.pattern_sdf = {
			.padding = 8,
			.rounding = 4.f,
			.min_size = 1.f,
			.max_size = 64.f,
		},
		.pattern_alpha = {
			.padding = 2,
			.rounding = 1.f,
			.min_size = 1.f,
			.max_size = 64.f,
		},
	};
}

skb_image_atlas_config_t skb_image_atlas_get_config(skb_image_atlas_t* atlas)
{
	assert(atlas);
	return atlas->config;
}

void skb_image_atlas_set_create_texture_callback(skb_image_atlas_t* atlas, skb_create_texture_func_t* create_texture_callback, void* context)
{
	assert(atlas);
	atlas->create_texture_callback = create_texture_callback;
	atlas->create_texture_callback_context = context;
}

int32_t skb_image_atlas_get_texture_count(skb_image_atlas_t* atlas)
{
	assert(atlas);
	return atlas->textures_count;
}

const skb_image_t* skb_image_atlas_get_texture(skb_image_atlas_t* atlas, int32_t texture_idx)
{
	assert(atlas);
	assert(texture_idx >= 0 && texture_idx < atlas->textures_count);

	return &atlas->textures[texture_idx].image;
}

skb_rect2i_t skb_image_atlas_get_texture_dirty_bounds(skb_image_atlas_t* atlas, int32_t texture_idx)
{
	assert(atlas);
	assert(texture_idx >= 0 && texture_idx < atlas->textures_count);

	return atlas->textures[texture_idx].dirty_bounds;
}

skb_rect2i_t skb_image_atlas_get_and_reset_texture_dirty_bounds(skb_image_atlas_t* atlas, int32_t texture_idx)
{
	assert(atlas);
	assert(texture_idx >= 0 && texture_idx < atlas->textures_count);

	const skb_rect2i_t bounds = atlas->textures[texture_idx].dirty_bounds;

	if (!skb_rect2i_is_empty(bounds))
		atlas->textures[texture_idx].prev_dirty_bounds = bounds;

	atlas->textures[texture_idx].dirty_bounds = skb_rect2i_make_undefined();

	return bounds;
}

uintptr_t skb_image_atlas_get_texture_user_data(skb_image_atlas_t* image_atlas, int32_t texture_idx)
{
	assert(image_atlas);
	assert(texture_idx >= 0 && texture_idx < image_atlas->textures_count);

	return image_atlas->textures[texture_idx].user_data;
}

void skb_image_atlas_set_texture_user_data(skb_image_atlas_t* atlas, int32_t texture_idx, uintptr_t user_data)
{
	assert(atlas);
	assert(texture_idx >= 0 && texture_idx < atlas->textures_count);

	atlas->textures[texture_idx].user_data = user_data;
}

void skb_image_atlas_debug_iterate_free_rects(skb_image_atlas_t* atlas, int32_t texture_idx, skb_debug_rect_iterator_func_t* callback, void* context)
{
	assert(atlas);
	assert(texture_idx >= 0 && texture_idx < atlas->textures_count);

	const skb_atlas_texture_t* atlas_image = &atlas->textures[texture_idx];

	const skb__shelf_packer_t* packer = &atlas_image->packer;
	for (uint16_t row_idx = packer->first_row; row_idx != SKB_SHELF_PACKER_NULL; row_idx = packer->rows[row_idx].next) {
		const skb__shelf_packer_row_t* row = &packer->rows[row_idx];
		for (uint16_t it = row->first_item; it != SKB_SHELF_PACKER_NULL; it = packer->items[it].next) {
			const skb__shelf_packer_item_t* item = &packer->items[it];
			if (item->flags & SKB__SHELF_PACKER_ITEM_IS_EMPTY) {
				callback(item->x, row->y, item->width, row->height, context);
			}
		}
	}

}

void skb_image_atlas_debug_iterate_used_rects(skb_image_atlas_t* atlas, int32_t texture_idx, skb_debug_rect_iterator_func_t* callback, void* context)
{
	assert(atlas);
	assert(texture_idx >= 0 && texture_idx < atlas->textures_count);

	for (int32_t i = 0; i < atlas->items_count; i++) {
		skb__atlas_item_t* item = &atlas->items[i];
		if (item->state == SKB__ITEM_STATE_RASTERIZED) {
			if (item->texture_idx == texture_idx) {
				callback(item->texture_offset_x, item->texture_offset_y, item->width, item->height, context);
			}
		}
	}
}

skb_rect2i_t skb_image_atlas_debug_get_texture_prev_dirty_bounds(skb_image_atlas_t* atlas, int32_t texture_idx)
{
	assert(atlas);
	assert(texture_idx >= 0 && texture_idx < atlas->textures_count);
	return atlas->textures[texture_idx].prev_dirty_bounds;
}


//
// Atlas shelf packer
//

static uint16_t skb__shelf_packer_alloc_row(skb__shelf_packer_t* packer)
{
	uint16_t row_idx = SKB_SHELF_PACKER_NULL;
	if (packer->row_freelist != SKB_SHELF_PACKER_NULL) {
		row_idx = packer->row_freelist;
		packer->row_freelist = packer->rows[packer->row_freelist].next;
	} else {
		SKB_ARRAY_RESERVE(packer->rows, packer->rows_count+1);
		row_idx = (uint16_t)(packer->rows_count++);
	}

	skb__shelf_packer_row_t* row = &packer->rows[row_idx];
	memset(row, 0, sizeof(skb__shelf_packer_row_t));

	return row_idx;
}

static void skb__shelf_packer_free_item(skb__shelf_packer_t* packer, uint16_t item_idx); // fwd decl

static void skb__skelf_packer_free_row(skb__shelf_packer_t* packer, uint16_t row_idx)
{
	assert(row_idx != SKB_SHELF_PACKER_NULL);

	// Free items
	uint16_t item_it = packer->rows[row_idx].first_item;
	while (item_it != SKB_SHELF_PACKER_NULL) {
		const uint16_t next_it = packer->items[item_it].next;
		skb__shelf_packer_free_item(packer, item_it);
		item_it = next_it;
	}

	packer->rows[row_idx].next = packer->row_freelist;
	packer->rows[row_idx].flags |= SKB__SHELF_PACKER_ROW_IS_FREED;
	packer->row_freelist = row_idx;
}

static uint16_t skb__shelf_packer_alloc_item(skb__shelf_packer_t* packer)
{
	uint16_t item_idx = SKB_SHELF_PACKER_NULL;
	uint16_t generation = 1;
	if (packer->item_freelist != SKB_SHELF_PACKER_NULL) {
		item_idx = packer->item_freelist;
		packer->item_freelist = packer->items[packer->item_freelist].next;
		generation = packer->items[item_idx].generation;
	} else {
		SKB_ARRAY_RESERVE(packer->items, packer->items_count+1);
		item_idx = (uint16_t)(packer->items_count++);
	}
	skb__shelf_packer_item_t* item = &packer->items[item_idx];
	memset(item, 0, sizeof(skb__shelf_packer_item_t));
	item->generation = generation;

	return item_idx;
}

static void skb__shelf_packer_free_item(skb__shelf_packer_t* packer, uint16_t item_idx)
{
	packer->items[item_idx].flags |= SKB__SHELF_PACKER_ITEM_IS_FREED;
	packer->items[item_idx].next = packer->item_freelist;
	packer->item_freelist = item_idx;
}

static uint16_t skb__shelf_packer_alloc_empty_row(skb__shelf_packer_t* packer, uint16_t y, uint16_t height)
{
	// Init atlas with empty row and empty item covering the whole area.
	uint16_t row_idx = skb__shelf_packer_alloc_row(packer);
	skb__shelf_packer_row_t* row = &packer->rows[row_idx];
	row->y = y;
	row->height = height;
	row->max_diff = 0;
	row->base_height = 0;
	row->max_empty_item_width = SKB_SHELF_PACKER_NULL; // to be calculated later
	row->flags |= SKB__SHELF_PACKER_ROW_IS_EMPTY;
	row->next = SKB_SHELF_PACKER_NULL;

	uint16_t item_idx = skb__shelf_packer_alloc_item(packer);
	skb__shelf_packer_item_t* item = &packer->items[item_idx];
	item->x = 0;
	item->width = (uint16_t)packer->width;
	item->next = SKB_SHELF_PACKER_NULL;
	item->flags |= SKB__SHELF_PACKER_ITEM_IS_EMPTY;
	item->row = row_idx;

	row->first_item = item_idx;

	return row_idx;
}

static void skb__shelf_packer_init(skb__shelf_packer_t* packer, int32_t width, int32_t height)
{
	memset(packer, 0, sizeof(skb__shelf_packer_t));
	packer->width = width;
	packer->height = height;
	packer->row_freelist = SKB_SHELF_PACKER_NULL;
	packer->item_freelist = SKB_SHELF_PACKER_NULL;

	// Init atlas with empty rown and empty item covering the whole area.
	packer->first_row = skb__shelf_packer_alloc_empty_row(packer, 0, (uint16_t)height);
}

static void skb__shelf_packer_destroy(skb__shelf_packer_t* packer)
{
	if (packer) {
		skb_free(packer->rows);
		skb_free(packer->items);
		memset(packer, 0, sizeof(skb__shelf_packer_t));
	}
}

static bool skb__shelf_packer_handle_is_null(skb__shelf_packer_handle_t handle)
{
	return handle.generation == 0;
}

static void skb__shelf_packer_get_item_offset(const skb__shelf_packer_t* packer, skb__shelf_packer_handle_t handle, int32_t* offset_x, int32_t* offset_y)
{
	if ((int32_t)handle.index >= packer->items_count || packer->items[handle.index].generation != handle.generation) {
		*offset_x = 0;
		*offset_y = 0;
	}

	skb__shelf_packer_item_t* item = &packer->items[handle.index];
	*offset_x = (int32_t)item->x;
	*offset_y = (int32_t)packer->rows[item->row].y;
}

static skb__shelf_packer_handle_t skb__shelf_packer_row_alloc_item(skb__shelf_packer_t* packer, uint16_t row_idx, uint16_t requested_width)
{
	skb__shelf_packer_row_t* row = &packer->rows[row_idx];
	assert(!(row->flags & SKB__SHELF_PACKER_ROW_IS_FREED));

	uint16_t item_idx = SKB_SHELF_PACKER_NULL;
	for (uint16_t item_it = row->first_item; item_it != SKB_SHELF_PACKER_NULL; item_it = packer->items[item_it].next) {
		assert(!(packer->items[item_it].flags & SKB__SHELF_PACKER_ITEM_IS_FREED));
		if ((packer->items[item_it].flags & SKB__SHELF_PACKER_ITEM_IS_EMPTY) && packer->items[item_it].width >= requested_width) {
			item_idx = item_it;
			break;
		}
	}

	if (item_idx == SKB_SHELF_PACKER_NULL)
		return (skb__shelf_packer_handle_t) {0};

	row->flags &= ~SKB__SHELF_PACKER_ROW_IS_EMPTY;
	row->max_empty_item_width = -1;

	// Split
	uint16_t remainder_item_idx = skb__shelf_packer_alloc_item(packer);

	skb__shelf_packer_item_t* item = &packer->items[item_idx];
	skb__shelf_packer_item_t* remainter_item = &packer->items[remainder_item_idx];

	uint16_t available_space = item->width;
	uint16_t next_item_idx = item->next;

	item->width = requested_width;
	item->flags &= ~SKB__SHELF_PACKER_ITEM_IS_EMPTY;
	item->next = remainder_item_idx;

	remainter_item->row = row_idx;
	remainter_item->x = item->x + requested_width;
	remainter_item->width = available_space - requested_width;
	remainter_item->flags |= SKB__SHELF_PACKER_ITEM_IS_EMPTY;
	remainter_item->next = next_item_idx;

	return (skb__shelf_packer_handle_t) {
		.index = item_idx,
		.generation = item->generation,
	};
}

static bool skb__shelf_packer_row_has_space(const skb__shelf_packer_t* packer, skb__shelf_packer_row_t* row, uint16_t requested_width)
{
	if (row->max_empty_item_width == SKB_SHELF_PACKER_NULL) {
		row->max_empty_item_width = 0;
		for (uint16_t item_it = row->first_item; item_it != SKB_SHELF_PACKER_NULL; item_it = packer->items[item_it].next) {
			assert(!(packer->items[item_it].flags & SKB__SHELF_PACKER_ITEM_IS_FREED));
			if ((packer->items[item_it].flags & SKB__SHELF_PACKER_ITEM_IS_EMPTY) && packer->items[item_it].width > row->max_empty_item_width)
				row->max_empty_item_width = packer->items[item_it].width;
		}
	}
	return row->max_empty_item_width >= requested_width;
}

static bool skb__shelf_packer_alloc_rect(
	skb__shelf_packer_t* packer, int32_t requested_width, int32_t requested_height,
	int32_t* offset_x, int32_t* offset_y, skb__shelf_packer_handle_t* handle,
	const skb_image_atlas_config_t* config)
{
	requested_width = skb_maxi(requested_width, 1);
	requested_height = skb_align(requested_height, config->item_height_rounding);

	if (requested_width > packer->width || requested_height > packer->height)
		return false;

	uint16_t best_row_idx = SKB_SHELF_PACKER_NULL;
	int32_t best_row_error = packer->height;

	for (uint16_t row_it = packer->first_row; row_it != SKB_SHELF_PACKER_NULL; row_it = packer->rows[row_it].next) {
		skb__shelf_packer_row_t* row = &packer->rows[row_it];
		assert(!(row->flags & SKB__SHELF_PACKER_ROW_IS_FREED));

		if (row->flags & SKB__SHELF_PACKER_ROW_IS_EMPTY) {
			if (requested_height > (int32_t)row->height)
				continue;

			if (!skb__shelf_packer_row_has_space(packer, row, (uint16_t)requested_width))
				continue;

			const int32_t error = requested_height;
			if (error < best_row_error) {
				best_row_error = requested_height;
				best_row_idx = row_it;
			}
		} else {
			const int32_t min_height = (int32_t)row->base_height - (int32_t)row->max_diff;
			const int32_t max_height = (int32_t)row->base_height + (int32_t)row->max_diff;
			if (requested_height < min_height || requested_height > max_height)
				continue;

			if (!skb__shelf_packer_row_has_space(packer, row, (uint16_t)requested_width))
				continue;

			if (row->height == requested_height) {
				skb__shelf_packer_handle_t item = skb__shelf_packer_row_alloc_item(packer, row_it, (uint16_t)requested_width);
				assert(!skb__shelf_packer_handle_is_null(item));
				skb__shelf_packer_get_item_offset(packer, item, offset_x, offset_y);
				packer->occupancy += (int32_t)row->height * requested_width;
				*handle = item;
				return true;
			}

			if (requested_height <= (int32_t)row->height) {
				// Allow up max_diff size difference to be packed into same row.
				const int32_t error = (int32_t)row->height - requested_height;
				if (error < best_row_error) {
					best_row_error = error;
					best_row_idx = row_it;
				}
			} else if (requested_height > (int32_t)row->height && row->next != SKB_SHELF_PACKER_NULL) {
				// Check to see if we can grow this row to accommodate the height.
				const int32_t error = requested_height - (int32_t)row->height;
				if (error < best_row_error) {
					skb__shelf_packer_row_t* next_row = &packer->rows[row->next];
					if ((next_row->flags & SKB__SHELF_PACKER_ROW_IS_EMPTY) && (int32_t)(row->height + next_row->height) >= requested_height) {
						best_row_error = error;
						best_row_idx = row_it;
					}
				}
			}
		}
	}

	// If no row was found, there's no space in the atlas.
	if (best_row_idx == SKB_SHELF_PACKER_NULL)
		return false;

	if (packer->rows[best_row_idx].flags & SKB__SHELF_PACKER_ROW_IS_EMPTY) {
		// The best row is empty, split it to requested size.
		uint16_t row_y = packer->rows[best_row_idx].y;
		uint16_t row_height = packer->rows[best_row_idx].height;
		uint16_t next_row_idx = packer->rows[best_row_idx].next;

		assert((int32_t)row_height >= requested_height);

		uint16_t remainder_row_idx = skb__shelf_packer_alloc_empty_row(packer, row_y + (uint16_t)requested_height, row_height - (uint16_t)requested_height);

		packer->rows[best_row_idx].height = (uint16_t)requested_height;
		packer->rows[best_row_idx].base_height = (uint16_t)requested_height;
		packer->rows[best_row_idx].max_diff = (uint16_t)((float)requested_height * config->fit_max_factor);
		packer->rows[best_row_idx].next = remainder_row_idx;

		packer->rows[remainder_row_idx].next = next_row_idx;

	} else if (requested_height > packer->rows[best_row_idx].height) {
		// Make the best row larger.
		uint16_t next_row_idx = packer->rows[best_row_idx].next;
		assert(next_row_idx != SKB_SHELF_PACKER_NULL && (packer->rows[next_row_idx].flags & SKB__SHELF_PACKER_ROW_IS_EMPTY));
		assert(!(packer->rows[next_row_idx].flags & SKB__SHELF_PACKER_ROW_IS_FREED));

		uint16_t combined_height = packer->rows[best_row_idx].height + packer->rows[next_row_idx].height;
		assert((int32_t)combined_height >= requested_height);
		uint16_t diff = (uint16_t)requested_height - packer->rows[best_row_idx].height;

		packer->rows[best_row_idx].height += diff;

		packer->rows[next_row_idx].y += diff;
		packer->rows[next_row_idx].height -= diff;
	}

	skb__shelf_packer_handle_t item = skb__shelf_packer_row_alloc_item(packer, best_row_idx, (uint16_t)requested_width);
	assert(!skb__shelf_packer_handle_is_null(item));
	skb__shelf_packer_get_item_offset(packer, item, offset_x, offset_y);
	*handle = item;

	packer->occupancy += (int32_t)packer->rows[best_row_idx].height * requested_width;

	return true;
}

static bool skb__shelf_packer_free_rect(skb__shelf_packer_t* packer, skb__shelf_packer_handle_t handle)
{
	uint16_t item_idx = handle.index;
	if ((int32_t)item_idx >= packer->items_count || packer->items[item_idx].generation != handle.generation)
		return false;

	skb__shelf_packer_item_t* item = &packer->items[item_idx];
	assert(!(item->flags & SKB__SHELF_PACKER_ITEM_IS_FREED));

	uint16_t row_idx = item->row;
	skb__shelf_packer_row_t* row = &packer->rows[row_idx];
	assert(!(row->flags & SKB__SHELF_PACKER_ROW_IS_FREED));

	packer->occupancy -= (int32_t)row->height * (int32_t)item->width;

	// Find prev item index as we don't store it explicitly.
	uint16_t prev_item_idx = SKB_SHELF_PACKER_NULL;
	for (uint16_t item_it = row->first_item; item_it != SKB_SHELF_PACKER_NULL; item_it = packer->items[item_it].next) {
		assert(!(packer->items[item_it].flags & SKB__SHELF_PACKER_ITEM_IS_FREED));
		if (item_it == item_idx)
			break;
		prev_item_idx = item_it;
	}

	// Mark the item empty
	item->flags |= SKB__SHELF_PACKER_ITEM_IS_EMPTY;
	item->generation++; // bump generation to recognize stale access

	// Merge with previous empty
	if (prev_item_idx != SKB_SHELF_PACKER_NULL && (packer->items[prev_item_idx].flags & SKB__SHELF_PACKER_ITEM_IS_EMPTY)) {
		skb__shelf_packer_item_t* prev_item = &packer->items[prev_item_idx];
		prev_item->width += item->width;
		prev_item->next = item->next;
		skb__shelf_packer_free_item(packer, item_idx);
		item = prev_item;
	}

	// Merge with next empty
	if (item->next != SKB_SHELF_PACKER_NULL && (packer->items[item->next].flags & SKB__SHELF_PACKER_ITEM_IS_EMPTY)) {
		uint16_t next_item_idx = item->next;
		skb__shelf_packer_item_t* next_item = &packer->items[next_item_idx];
		item->width += next_item->width;
		item->next = next_item->next;
		skb__shelf_packer_free_item(packer, next_item_idx);
	}

	row->max_empty_item_width = SKB_SHELF_PACKER_NULL; // to be calculated later

	assert(row->first_item != SKB_SHELF_PACKER_NULL);
	const bool is_empty =
		(packer->items[row->first_item].flags & SKB__SHELF_PACKER_ITEM_IS_EMPTY)
		&& packer->items[row->first_item].next == SKB_SHELF_PACKER_NULL;
	SKB_SET_FLAG(row->flags, SKB__SHELF_PACKER_ROW_IS_EMPTY, is_empty);

	// The row became empty
	if (row->flags & SKB__SHELF_PACKER_ROW_IS_EMPTY) {

		row->max_diff = 0;
		row->base_height = 0;

		// Find prev row index as we don't store it explicitly.
		uint16_t prev_row_idx = SKB_SHELF_PACKER_NULL;
		for (uint16_t row_it = packer->first_row; row_it != SKB_SHELF_PACKER_NULL; row_it = packer->rows[row_it].next) {
			assert(!(packer->rows[row_it].flags & SKB__SHELF_PACKER_ROW_IS_FREED));
			if (row_it == row_idx)
				break;
			prev_row_idx = row_it;
		}

		// Merge with previous empty
		if (prev_row_idx != SKB_SHELF_PACKER_NULL && (packer->rows[prev_row_idx].flags & SKB__SHELF_PACKER_ROW_IS_EMPTY)) {
			skb__shelf_packer_row_t* prev_row = &packer->rows[prev_row_idx];
			prev_row->height += row->height;
			prev_row->next = row->next;
			skb__skelf_packer_free_row(packer, row_idx);
			row = prev_row;
		}

		// Merge with next empty
		if (row->next != SKB_SHELF_PACKER_NULL && (packer->rows[row->next].flags & SKB__SHELF_PACKER_ROW_IS_EMPTY)) {
			uint16_t next_row_idx = row->next;
			skb__shelf_packer_row_t* next_row = &packer->rows[next_row_idx];
			row->height += next_row->height;
			row->next = next_row->next;
			skb__skelf_packer_free_row(packer, next_row_idx);
		}
	}

	return true;
}

static float skb__shelf_packer_get_occupancy_percent(skb__shelf_packer_t* packer)
{
	return (float)packer->occupancy / (float)(packer->width * packer->height);
}

static void skb__shelf_packer_expand(skb__shelf_packer_t* packer, int32_t new_width, int32_t new_height)
{
	if (new_width > packer->width) {
		const uint16_t expansion_x = (uint16_t)packer->width;
		const uint16_t expansion_width = (uint16_t)(new_width - packer->width);

		packer->width = new_width;

		for (uint16_t row_it = packer->first_row; row_it != SKB_SHELF_PACKER_NULL; row_it = packer->rows[row_it].next) {
			skb__shelf_packer_row_t* row = &packer->rows[row_it];

			uint16_t last_item_idx = SKB_SHELF_PACKER_NULL;
			for (uint16_t item_it = row->first_item; item_it != SKB_SHELF_PACKER_NULL; item_it = packer->items[item_it].next)
				last_item_idx = item_it;
			assert(last_item_idx != SKB_SHELF_PACKER_NULL);

			if (packer->items[last_item_idx].flags & SKB__SHELF_PACKER_ITEM_IS_EMPTY) {
				// Expand existing empty item.
				packer->items[last_item_idx].width += expansion_width;
			} else {
				// Create empty item at end.
				uint16_t item_idx = skb__shelf_packer_alloc_item(packer);
				skb__shelf_packer_item_t* item = &packer->items[item_idx];
				item->x = expansion_x;
				item->width = expansion_width;
				item->flags |= SKB__SHELF_PACKER_ITEM_IS_EMPTY;
				item->next = SKB_INVALID_INDEX;
				item->row = row_it;
				packer->items[last_item_idx].next = item_idx;
			}
			row->max_empty_item_width = -1;
		}
	}

	if (new_height > packer->height) {
		const uint16_t expansion_y = (uint16_t)packer->height;
		const uint16_t expansion_height = (uint16_t)(new_height - packer->height);

		packer->height = new_height;

		uint16_t last_row_idx = SKB_SHELF_PACKER_NULL;
		for (uint16_t row_it = packer->first_row; row_it != SKB_SHELF_PACKER_NULL; row_it = packer->rows[row_it].next)
			last_row_idx = row_it;

		if (packer->rows[last_row_idx].flags & SKB__SHELF_PACKER_ROW_IS_EMPTY) {
			// Last row is empty, just increase height
			packer->rows[last_row_idx].height += expansion_height;
		} else {
			// Create empty row at end.
			uint16_t row_idx = skb__shelf_packer_alloc_empty_row(packer, expansion_y, expansion_height);
			skb__shelf_packer_row_t* row = &packer->rows[row_idx];
			packer->rows[last_row_idx].next = row_idx;
		}
	}
}

//
// Atlas
//

static bool skb__try_evict_items(skb_image_atlas_t* atlas, int32_t evict_after_duration); // fwd

static int32_t skb__add_rect(skb_image_atlas_t* atlas, int32_t requested_width, int32_t requested_height, const uint8_t requested_bpp, int32_t* offset_x, int32_t* offset_y, skb__shelf_packer_handle_t* handle)
{
	// Try to add to existing images first.
	for (int32_t i = atlas->textures_count - 1; i >= 0; i--) {
		skb_atlas_texture_t* texture = &atlas->textures[i];
		if (texture->image.bpp == requested_bpp) {
			if (skb__shelf_packer_alloc_rect(&texture->packer, requested_width, requested_height, offset_x, offset_y, handle, &atlas->config))
				return texture->index;
		}
	}
	return SKB_INVALID_INDEX;
}

static int32_t skb__add_rect_or_grow(skb_image_atlas_t* atlas, int32_t requested_width, int32_t requested_height, const uint8_t requested_bpp, int32_t* offset_x, int32_t* offset_y, skb__shelf_packer_handle_t* handle)
{
	assert(atlas);

	// If there's no change to fit the rectangle at all, do not even try.
	if (requested_width > atlas->config.max_width || requested_height > atlas->config.max_height)
		return SKB_INVALID_INDEX;

	int32_t texture_idx = SKB_INVALID_INDEX;

	// Try to add to existing images first.
	texture_idx = skb__add_rect(atlas, requested_width, requested_height, requested_bpp, offset_x, offset_y, handle);
	if (texture_idx != SKB_INVALID_INDEX)
		return texture_idx;

	// Could not fit into any existing images, try to aggressively evict unused glyphs, and try again.
	static int32_t urgent_evict_after_duration = 0;
	if (skb__try_evict_items(atlas, urgent_evict_after_duration)) {
		texture_idx = skb__add_rect(atlas, requested_width, requested_height, requested_bpp, offset_x, offset_y, handle);
		if (texture_idx != SKB_INVALID_INDEX)
			return texture_idx;
	}

	// Could not find free space, try to expand the last atlas of matching bpp.
	skb_atlas_texture_t* last_texture = NULL;
	for (int32_t i = atlas->textures_count - 1; i >= 0; i--) {
		if (atlas->textures[i].image.bpp == requested_bpp) {
			last_texture = &atlas->textures[i];
			break;
		}
	}

	if (last_texture) {
		const int32_t expand_size = atlas->config.expand_size;
		const int32_t expanded_width = skb_mini(last_texture->packer.width + expand_size, atlas->config.max_width);
		const int32_t expanded_height = skb_mini(last_texture->packer.height + expand_size, atlas->config.max_height);

		for (int32_t retry = 0; retry < 8; retry++) {
			int32_t new_width = last_texture->packer.width;
			int32_t new_height = last_texture->packer.height;

			if (last_texture->packer.width <= last_texture->packer.height && expanded_width != last_texture->packer.width)
				new_width = expanded_width;
			else
				new_height = expanded_height;

			// Check if we failed to resize
			if (new_width == last_texture->packer.width && new_height == last_texture->packer.height)
				break;

			skb__shelf_packer_expand(&last_texture->packer, new_width, new_height);

			if (skb__shelf_packer_alloc_rect(&last_texture->packer, requested_width, requested_height, offset_x, offset_y, handle, &atlas->config))
				return last_texture->index;
		}
	}

	// Could not expand the last image, create a new one.
	skb_atlas_texture_t* new_texture = skb__add_texture(atlas, requested_width, requested_height, requested_bpp);

	if (skb__shelf_packer_alloc_rect(&new_texture->packer, requested_width, requested_height, offset_x, offset_y, handle, &atlas->config))
		return new_texture->index;

	return SKB_INVALID_INDEX;
}


static uint64_t skb__get_glyph_hash(uint32_t gid, const skb_font_t* font, float font_size, skb_rasterize_alpha_mode_t alpha_mode)
{
	uint64_t hash = skb_hash64_append_uint8(skb_hash64_empty(), SKB__ITEM_TYPE_GLYPH);
	hash = skb_hash64_append_uint64(hash, font->hash);
	hash = skb_hash64_append_uint32(hash, gid);
	hash = skb_hash64_append_float(hash, font_size);
	hash = skb_hash64_append_uint8(hash, (uint8_t)alpha_mode);
	return hash;
}

skb_quad_t skb_image_atlas_get_glyph_quad(
	skb_image_atlas_t* atlas, float x, float y, float pixel_scale,
	skb_font_collection_t* font_collection, skb_font_handle_t font_handle, uint32_t glyph_id, float font_size,
	skb_rasterize_alpha_mode_t alpha_mode)
{
	assert(atlas);

	const skb_font_t* font = skb_font_collection_get_font(font_collection, font_handle);
	if (!font) return (skb_quad_t) {0};

	const skb_image_item_config_t* img_config = alpha_mode == SKB_RASTERIZE_ALPHA_SDF ? &atlas->config.glyph_sdf : &atlas->config.glyph_alpha;

	const float rounded_font_size = ceilf(font_size * pixel_scale / img_config->rounding) * img_config->rounding;
	const float clamped_font_size = skb_clampf(rounded_font_size, img_config->min_size, img_config->max_size);

	const uint64_t hash_id = skb__get_glyph_hash(glyph_id, font, clamped_font_size, alpha_mode);

	skb__atlas_item_t* item = NULL;
	int32_t item_idx = SKB_INVALID_INDEX;

	if (skb_hash_table_find(atlas->items_lookup, hash_id, &item_idx)) {
		// Use existing.
		item = &atlas->items[item_idx];
		assert(item->type == SKB__ITEM_TYPE_GLYPH);
	} else {

		// Calc size
		const skb_rect2i_t bounds = skb_rasterizer_get_glyph_dimensions(glyph_id, font, clamped_font_size, img_config->padding);

		// Add to atlas
		int32_t texture_offset_x = 0;
		int32_t texture_offset_y = 0;
		skb__shelf_packer_handle_t packer_handle = {0};

		hb_face_t* face = hb_font_get_face(font->hb_font);
		const bool is_color = hb_ot_color_glyph_has_paint(face, glyph_id);
		const uint8_t requested_bpp = is_color ? 4 : 1;

		const int32_t texture_idx = skb__add_rect_or_grow(atlas, bounds.width, bounds.height, requested_bpp, &texture_offset_x, &texture_offset_y, &packer_handle);
		if (texture_idx == SKB_INVALID_INDEX)
			return (skb_quad_t){0};

		// Alloc and init the new glyph
		if (atlas->items_freelist != SKB_INVALID_INDEX) {
			item_idx = atlas->items_freelist;
			atlas->items_freelist = atlas->items[item_idx].lru.next;
		} else {
			SKB_ARRAY_RESERVE(atlas->items, atlas->items_count + 1);
			item_idx = atlas->items_count++;
		}
		skb_hash_table_add(atlas->items_lookup, hash_id, item_idx);

		item = &atlas->items[item_idx];
		item->type = SKB__ITEM_TYPE_GLYPH;
		item->glyph.font = font;
		item->glyph.gid = glyph_id;
		item->glyph.clamped_font_size = clamped_font_size;
		item->width = (int16_t)bounds.width;
		item->height = (int16_t)bounds.height;
		item->texture_offset_x = (int16_t)texture_offset_x;
		item->texture_offset_y = (int16_t)texture_offset_y;
		item->packer_handle = packer_handle;
		item->geom_offset_x = (int16_t)bounds.x;
		item->geom_offset_y = (int16_t)bounds.y;
		SKB_SET_FLAG(item->flags, SKB__ITEM_IS_SDF, alpha_mode == SKB_RASTERIZE_ALPHA_SDF);
		SKB_SET_FLAG(item->flags, SKB__ITEM_IS_COLOR, is_color);
		item->state = SKB__ITEM_STATE_INITIALIZED;
		item->texture_idx = (uint8_t)texture_idx;
		item->hash_id = hash_id;
		item->lru = skb_list_item_make();

		atlas->has_new_items = true;
	}

	assert(item);
	assert(item_idx != SKB_INVALID_INDEX);

	// Move glyph to front of the LRU list.
	skb_list_move_to_front(&atlas->items_lru, item_idx, skb__get_item, atlas);
	item->last_access_stamp = atlas->now_stamp;

	const float render_scale = (font_size / item->glyph.clamped_font_size);

	static const int32_t inset = 1; // Inset the rectangle by one texel, so that interpolation will not try to use data outside the atlas rect.

	skb_quad_t quad = {0};
	quad.texture.x = (float)(item->texture_offset_x + inset);
	quad.texture.y = (float)(item->texture_offset_y + inset);
	quad.texture.width = (float)(item->width - inset*2);
	quad.texture.height = (float)(item->height - inset*2);
	quad.geom.x = x + (float)(item->geom_offset_x + inset) * render_scale;
	quad.geom.y = y + (float)(item->geom_offset_y + inset) * render_scale;
	quad.geom.width = (float)(item->width - inset*2) * render_scale;
	quad.geom.height = (float)(item->height - inset*2) * render_scale;
	quad.scale = render_scale * pixel_scale;
	quad.texture_idx = item->texture_idx;
	SKB_SET_FLAG(quad.flags, SKB_QUAD_IS_COLOR, item->flags & SKB__ITEM_IS_COLOR);
	SKB_SET_FLAG(quad.flags, SKB_QUAD_IS_SDF, item->flags & SKB__ITEM_IS_SDF);

	return quad;
}


static uint64_t skb__get_icon_hash(const skb_icon_t* icon, skb_vec2_t icon_scale, skb_rasterize_alpha_mode_t alpha_mode)
{
	uint64_t hash = skb_hash64_append_uint8(skb_hash64_empty(), SKB__ITEM_TYPE_ICON);
	hash = skb_hash64_append_uint64(hash, icon->hash);
	hash = skb_hash64_append_float(hash, icon_scale.x);
	hash = skb_hash64_append_float(hash, icon_scale.y);
	hash = skb_hash64_append_uint8(hash, (uint8_t)alpha_mode);

	return hash;
}

skb_quad_t skb_image_atlas_get_icon_quad(
	skb_image_atlas_t* atlas, float x, float y, float pixel_scale,
	const skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle, skb_vec2_t icon_scale,
    skb_rasterize_alpha_mode_t alpha_mode)
{
	assert(atlas);
	assert(icon_collection);

	const skb_icon_t* icon = skb_icon_collection_get_icon(icon_collection, icon_handle);
	if (!icon) return (skb_quad_t) {0};

	const skb_image_item_config_t* img_config = alpha_mode == SKB_RASTERIZE_ALPHA_SDF ? &atlas->config.icon_sdf : &atlas->config.icon_alpha;

	const float requested_width = icon->view.width * icon_scale.x;
	const float requested_height = icon->view.height * icon_scale.y;

	// Scale proportionally when image is clamped or rounded
	const float max_dim = skb_maxf(requested_width, requested_height);
	const float rounded_max_dim = ceilf(max_dim * pixel_scale / img_config->rounding) * img_config->rounding;
	const float clamped_max_dim = skb_clampf(rounded_max_dim, img_config->min_size, img_config->max_size);
	const float clamp_scale = clamped_max_dim / max_dim;

	const float clamped_width = requested_width * clamp_scale;
	const float clamped_height = requested_height * clamp_scale;
	const skb_vec2_t scale = {
		.x = clamped_width / icon->view.width,
		.y = clamped_height / icon->view.height,
	};

	const uint64_t hash_id = skb__get_icon_hash(icon, scale, alpha_mode);

	skb__atlas_item_t* item = NULL;
	int32_t item_idx = SKB_INVALID_INDEX;

	if (skb_hash_table_find(atlas->items_lookup, hash_id, &item_idx)) {
		// Use existing.
		item = &atlas->items[item_idx];
	} else {
		// Not found, create new.

		// Calc size
		const skb_rect2i_t bounds = skb_rasterizer_get_icon_dimensions(icon, scale, img_config->padding);

		// Add to atlas
		int32_t texture_offset_x = 0;
		int32_t atlas_offset_y = 0;
		skb__shelf_packer_handle_t atlas_handle = {0};

		const uint8_t requested_bpp = icon->is_color ? 4 : 1;

		const int32_t image_idx = skb__add_rect_or_grow(atlas, bounds.width, bounds.height, requested_bpp, &texture_offset_x, &atlas_offset_y, &atlas_handle);
		if (image_idx == SKB_INVALID_INDEX)
			return (skb_quad_t){0};

		// Alloc and init the new icon
		if (atlas->items_freelist != SKB_INVALID_INDEX) {
			item_idx = atlas->items_freelist;
			atlas->items_freelist = atlas->items[item_idx].lru.next;
		} else {
			SKB_ARRAY_RESERVE(atlas->items, atlas->items_count + 1);
			item_idx = atlas->items_count++;
		}
		skb_hash_table_add(atlas->items_lookup, hash_id, item_idx);

		item = &atlas->items[item_idx];
		item->type = SKB__ITEM_TYPE_ICON;
		item->icon.icon = icon;
		item->icon.icon_scale = scale;
		item->width = (int16_t)bounds.width;
		item->height = (int16_t)bounds.height;
		item->texture_offset_x = (int16_t)texture_offset_x;
		item->texture_offset_y = (int16_t)atlas_offset_y;
		item->packer_handle = atlas_handle;
		item->geom_offset_x = (int16_t)bounds.x;
		item->geom_offset_y = (int16_t)bounds.y;
		SKB_SET_FLAG(item->flags, SKB__ITEM_IS_SDF, alpha_mode == SKB_RASTERIZE_ALPHA_SDF);
		SKB_SET_FLAG(item->flags, SKB__ITEM_IS_COLOR, icon->is_color);
		item->state = SKB__ITEM_STATE_INITIALIZED;
		item->texture_idx = (uint8_t)image_idx;
		item->hash_id = hash_id;
		item->lru = skb_list_item_make();

		atlas->has_new_items = true;
	}

	assert(item);
	assert(item_idx != SKB_INVALID_INDEX);

	// Move glyph to front of the LRU list.
	skb_list_move_to_front(&atlas->items_lru, item_idx, skb__get_item, atlas);

	item->last_access_stamp = atlas->now_stamp;

	const float render_scale_x = requested_width / clamped_width;
	const float render_scale_y = requested_height / clamped_height;

	static const int32_t inset = 1; // Inset the rectangle by one texel, so that interpolation will not try to use data outside the atlas rect.

	skb_quad_t quad = {0};
	quad.texture.x = (float)(item->texture_offset_x + inset);
	quad.texture.y = (float)(item->texture_offset_y + inset);
	quad.texture.width = (float)(item->width - inset*2);
	quad.texture.height = (float)(item->height - inset*2);
	quad.geom.x = x + (float)(item->geom_offset_x + inset) * render_scale_x;
	quad.geom.y = y + (float)(item->geom_offset_y + inset) * render_scale_y;
	quad.geom.width = (float)(item->width - inset*2) * render_scale_x;
	quad.geom.height = (float)(item->height - inset*2) * render_scale_y;
	quad.scale = skb_maxf(render_scale_x, render_scale_y) * pixel_scale;
	quad.texture_idx = item->texture_idx;
	quad.flags |= SKB_QUAD_IS_COLOR;
	SKB_SET_FLAG(quad.flags, SKB_QUAD_IS_SDF, item->flags & SKB__ITEM_IS_SDF);

	return quad;
}


static uint64_t skb__get_pattern_hash(uint8_t style, float thickness, skb_rasterize_alpha_mode_t alpha_mode)
{
	uint64_t hash = skb_hash64_append_uint8(skb_hash64_empty(), SKB__ITEM_TYPE_ICON);
	hash = skb_hash64_append_float(hash, thickness);
	hash = skb_hash64_append_uint8(hash, style);
	hash = skb_hash64_append_uint8(hash, (uint8_t)alpha_mode);

	return hash;
}

skb_pattern_quad_t skb_image_atlas_get_decoration_quad(
	skb_image_atlas_t* atlas,
	float x, float y, float width, float offset_x, float pixel_scale,
	uint8_t style, float thickness,
	skb_rasterize_alpha_mode_t alpha_mode)
{
	assert(atlas);

	const skb_image_item_config_t* img_config = alpha_mode == SKB_RASTERIZE_ALPHA_SDF ? &atlas->config.pattern_sdf : &atlas->config.pattern_alpha;

	const float rounded_thickness = ceilf(thickness * pixel_scale / img_config->rounding) * img_config->rounding;
	const float clamped_thickness = skb_clampf(rounded_thickness, img_config->min_size, img_config->max_size);

	const uint64_t hash_id = skb__get_pattern_hash(style, clamped_thickness, alpha_mode);

	skb__atlas_item_t* item = NULL;
	int32_t item_idx = SKB_INVALID_INDEX;

	if (skb_hash_table_find(atlas->items_lookup, hash_id, &item_idx)) {
		// Use existing.
		item = &atlas->items[item_idx];
	} else {
		// Not found, create new.

		// Calc size
		skb_rect2i_t bounds = skb_rasterizer_get_decoration_pattern_dimensions(style, clamped_thickness, img_config->padding);

		// Add to atlas
		int32_t atlas_offset_x = 0;
		int32_t atlas_offset_y = 0;
		skb__shelf_packer_handle_t atlas_handle = {0};

		const uint8_t requested_bpp = 1;

		const int32_t texture_idx = skb__add_rect_or_grow(atlas, bounds.width, bounds.height, requested_bpp, &atlas_offset_x, &atlas_offset_y, &atlas_handle);
		if (texture_idx == SKB_INVALID_INDEX)
			return (skb_pattern_quad_t){0};

		// Alloc and init the new icon
		if (atlas->items_freelist != SKB_INVALID_INDEX) {
			item_idx = atlas->items_freelist;
			atlas->items_freelist = atlas->items[item_idx].lru.next;
		} else {
			SKB_ARRAY_RESERVE(atlas->items, atlas->items_count + 1);
			item_idx = atlas->items_count++;
		}
		skb_hash_table_add(atlas->items_lookup, hash_id, item_idx);

		item = &atlas->items[item_idx];
		item->type = SKB__ITEM_TYPE_PATTERN;
		item->pattern.style = style;
		item->pattern.thickness = clamped_thickness;
		item->width = (int16_t)bounds.width;
		item->height = (int16_t)bounds.height;
		item->texture_offset_x = (int16_t)atlas_offset_x;
		item->texture_offset_y = (int16_t)atlas_offset_y;
		item->packer_handle = atlas_handle;
		item->geom_offset_x = (int16_t)bounds.x;
		item->geom_offset_y = (int16_t)bounds.y;
		SKB_SET_FLAG(item->flags, SKB__ITEM_IS_SDF, alpha_mode == SKB_RASTERIZE_ALPHA_SDF);
		item->state = SKB__ITEM_STATE_INITIALIZED;
		item->texture_idx = (uint8_t)texture_idx;
		item->hash_id = hash_id;
		item->lru = skb_list_item_make();

		atlas->has_new_items = true;
	}

	assert(item);
	assert(item_idx != SKB_INVALID_INDEX);

	// Move glyph to front of the LRU list.
	skb_list_move_to_front(&atlas->items_lru, item_idx, skb__get_item, atlas);

	item->last_access_stamp = atlas->now_stamp;

	const float render_scale = thickness / item->pattern.thickness;

	static const int32_t inset = 1; // Inset the rectangle by one texel, so that interpolation will not try to use data outside the atlas rect.

	const float pattern_width = (float)(item->width - inset*2) * render_scale;

	// Note: x is missing inset intentionally since the quad is tiling.
	skb_pattern_quad_t quad = {0};
	quad.texture.x = (float)(item->texture_offset_x + inset);
	quad.texture.y = (float)(item->texture_offset_y + inset);
	quad.texture.width = (float)(item->width - inset*2);
	quad.texture.height = (float)(item->height - inset*2);
	quad.geom.x = x + (float)(item->geom_offset_x + inset) * render_scale;
	quad.geom.y = y + (float)(item->geom_offset_y + inset) * render_scale;
	quad.geom.width = width;
	quad.geom.height = (float)(item->height - inset*2) * render_scale;

	// Calculate mapping between the image and geom
	quad.pattern.x = -offset_x / pattern_width;
	quad.pattern.y = 0.f;
	quad.pattern.width = width / pattern_width;
	quad.pattern.height = 1.f;

	quad.scale = render_scale * pixel_scale;
	quad.texture_idx = item->texture_idx;
	quad.flags |= SKB_QUAD_IS_COLOR;
	SKB_SET_FLAG(quad.flags, SKB_QUAD_IS_SDF, item->flags & SKB__ITEM_IS_SDF);

	return quad;
}



void skb__image_clear(skb_image_t* image, int32_t offset_x, int32_t offset_y, int32_t width, int32_t height)
{
	for (int32_t y = offset_y; y < offset_y + height; y++) {
		uint8_t* row_buf = &image->buffer[offset_x * image->bpp + y * image->stride_bytes];
		memset(row_buf, 0xff, width * image->bpp);
	}
}

static bool skb__try_evict_items(skb_image_atlas_t* atlas, int32_t evict_after_duration)
{
	assert(atlas);

	int32_t evicted_count = 0;

	// Try to evict unused glyphs.

	int32_t item_idx = atlas->items_lru.tail; // Tail has least used items.
	while (item_idx != SKB_INVALID_INDEX) {
		skb__atlas_item_t* item = &atlas->items[item_idx];

		const int32_t inactive_duration = atlas->now_stamp - item->last_access_stamp;
		if (inactive_duration <= evict_after_duration)
			break;

		int32_t prev_item_idx = item->lru.prev;

		if (item->state == SKB__ITEM_STATE_RASTERIZED) {

			skb_atlas_texture_t* texture = &atlas->textures[item->texture_idx];
			skb__shelf_packer_t* packer = &texture->packer;

			// Remove from lookup.
			skb_hash_table_remove(atlas->items_lookup, item->hash_id);

			// Remove from atlas
			skb__shelf_packer_free_rect(packer, item->packer_handle);

			// Remove from LRU
			skb_list_remove(&atlas->items_lru, item_idx, skb__get_item, atlas);

			if (atlas->config.flags & SKB_IMAGE_ATLAS_DEBUG_CLEAR_REMOVED) {
				const skb_rect2i_t dirty = {
					.x = item->texture_offset_x,
					.y = item->texture_offset_y,
					.width = item->width,
					.height = item->height,
				};
				texture->dirty_bounds = skb_rect2i_union(texture->dirty_bounds, dirty);
				skb__image_clear(&texture->image, item->texture_offset_x, item->texture_offset_y, item->width, item->height);
			}

			// Returns glyph to freelist.
			memset(item, 0, sizeof(skb__atlas_item_t));
			item->state = SKB__ITEM_STATE_REMOVED;
			item->lru.next = atlas->items_freelist;
			atlas->items_freelist = item_idx;

			evicted_count++;
		}

		item_idx = prev_item_idx;
	}

	return evicted_count > 0;
}

bool skb_image_atlas_compact(skb_image_atlas_t* atlas)
{
	assert(atlas);

	atlas->now_stamp++;

	// TODO: smarted eviction strategy.
	// This tries to evict more items from the atlas the higher the max usage is.
	// Maybe better option would be to do this per image. In which case the LRU lists should be per image.

	float max_occupancy = 0.f;
	for (int32_t i = 0; i < atlas->textures_count; i++) {
		skb_atlas_texture_t* texture = &atlas->textures[i];
		const float occupancy = skb__shelf_packer_get_occupancy_percent(&texture->packer);
		max_occupancy = skb_maxf(max_occupancy, occupancy);
	}

	int32_t evict_after_duration = atlas->config.evict_inactive_duration;
	if (max_occupancy > 0.65f) // high pressure
		evict_after_duration = 1;
	else if (max_occupancy > 0.35f) // medium pressure
		evict_after_duration = (evict_after_duration+1)/2;

	skb__try_evict_items(atlas, evict_after_duration);

	return true;
}

bool skb_image_atlas_rasterize_missing_items(skb_image_atlas_t* atlas, skb_temp_alloc_t* temp_alloc, skb_rasterizer_t* rasterizer)
{
	assert(atlas);

	bool updated = false;

	// Check if the atlases have resized, and resize image too.
	for (int32_t i = 0; i < atlas->textures_count; i++) {
		skb_atlas_texture_t* texture = &atlas->textures[i];
		if (texture->image.width != texture->packer.width || texture->image.height != texture->packer.height) {
			// Dirty the whole old texture.
			skb_rect2i_t dirty = { .x = 0, .y = 0, .width = texture->packer.width, .height = texture->packer.height, };
			texture->dirty_bounds = skb_rect2i_union(texture->dirty_bounds, dirty);
			skb__image_resize(&texture->image, texture->packer.width, texture->packer.height, texture->image.bpp);
		}

		if (!skb_rect2i_is_empty(texture->dirty_bounds))
			updated = true;
	}

	// Glyphs
	if (atlas->has_new_items) {
		for (int32_t i = 0; i < atlas->items_count; i++) {
			skb__atlas_item_t* item = &atlas->items[i];
			if (item->state == SKB__ITEM_STATE_INITIALIZED) {

				skb_rect2i_t atlas_bounds = {
					.x = item->texture_offset_x,
					.y = item->texture_offset_y,
					.width = item->width,
					.height = item->height,
				};

				const skb_rasterize_alpha_mode_t alpha_mode = (item->flags & SKB__ITEM_IS_SDF) ? SKB_RASTERIZE_ALPHA_SDF : SKB_RASTERIZE_ALPHA_MASK;
				skb_atlas_texture_t* texture = &atlas->textures[item->texture_idx];

				skb_image_t target = {0};
				target.width = item->width;
				target.height = item->height;
				target.bpp = texture->image.bpp;
				target.buffer = &texture->image.buffer[item->texture_offset_x * texture->image.bpp + item->texture_offset_y * texture->image.stride_bytes];
				target.stride_bytes = texture->image.stride_bytes;

				assert(texture->image.stride_bytes == texture->image.width * texture->image.bpp);

				if (item->type == SKB__ITEM_TYPE_GLYPH) {
					// Rasterize glyph
					if (item->flags & SKB__ITEM_IS_COLOR) {
						skb_rasterizer_draw_color_glyph(
							rasterizer, temp_alloc, item->glyph.gid, item->glyph.font, item->glyph.clamped_font_size, alpha_mode,
							item->geom_offset_x, item->geom_offset_y, &target);
					} else {
						skb_rasterizer_draw_alpha_glyph(
							rasterizer, temp_alloc, item->glyph.gid, item->glyph.font, item->glyph.clamped_font_size, alpha_mode,
							item->geom_offset_x, item->geom_offset_y, &target);
					}
				} else if (item->type == SKB__ITEM_TYPE_ICON) {
					// Rasterize icon
					if (item->flags & SKB__ITEM_IS_COLOR) {
						skb_rasterizer_draw_color_icon( rasterizer, temp_alloc, item->icon.icon, item->icon.icon_scale, alpha_mode, item->geom_offset_x, item->geom_offset_y, &target);
					} else {
						skb_rasterizer_draw_alpha_icon( rasterizer, temp_alloc, item->icon.icon, item->icon.icon_scale, alpha_mode, item->geom_offset_x, item->geom_offset_y, &target);
					}
				} else if (item->type == SKB__ITEM_TYPE_PATTERN) {
					skb_rasterizer_draw_decoration_pattern( rasterizer, temp_alloc, item->pattern.style, item->pattern.thickness, alpha_mode, item->geom_offset_x, item->geom_offset_y, &target);
				}

				texture->dirty_bounds = skb_rect2i_union(texture->dirty_bounds, atlas_bounds);

				item->state = SKB__ITEM_STATE_RASTERIZED;

				updated = true;
			}
		}
		atlas->has_new_items = false;
	}

	return updated;
}
