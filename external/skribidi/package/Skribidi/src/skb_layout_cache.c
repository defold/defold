// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_layout_cache.h"
#include "skb_common.h"

#include <string.h>

typedef struct skb__cached_layout_t {
	skb_layout_t* layout;
	skb_list_item_t lru;
	int32_t last_access_stamp;
	uint64_t hash;
} skb__cached_layout_t;

typedef struct skb_layout_cache_t {
	skb_hash_table_t* layouts_lookup;
	skb__cached_layout_t* layouts;
	int32_t layouts_count;
	int32_t layouts_cap;
	int32_t layouts_freelist;
	skb_list_t lru;
	int32_t now_stamp;
} skb_layout_cache_t;

skb_layout_cache_t* skb_layout_cache_create(void)
{
	skb_layout_cache_t* cache = skb_malloc(sizeof(skb_layout_cache_t));
	memset(cache, 0, sizeof(skb_layout_cache_t));

	cache->layouts_lookup = skb_hash_table_create();
	cache->lru = skb_list_make();
	cache->layouts_freelist = SKB_INVALID_INDEX;

	return cache;
}

void skb_layout_cache_destroy(skb_layout_cache_t* cache)
{
	if (!cache) return;

	for (int32_t i = 0; i < cache->layouts_count; i++)
		skb_layout_destroy(cache->layouts[i].layout);
	skb_free(cache->layouts);

	skb_hash_table_destroy(cache->layouts_lookup);

	memset(cache, 0, sizeof(skb_layout_cache_t));

	skb_free(cache);
}

static skb_list_item_t* skb__get_lru_item(int32_t item_idx, void* context)
{
	skb_layout_cache_t* cache = (skb_layout_cache_t*)context;
	return &cache->layouts[item_idx].lru;
}

skb__cached_layout_t* skb__layout_cache_get_or_insert(skb_layout_cache_t* cache, uint64_t hash)
{
	skb__cached_layout_t* cached_layout = NULL;
	int32_t layout_index = SKB_INVALID_INDEX;
	if (skb_hash_table_find(cache->layouts_lookup, hash, &layout_index)) {
		cached_layout = &cache->layouts[layout_index];
	} else {
		// Create new
		if (cache->layouts_freelist != SKB_INVALID_INDEX) {
			// Pop from freelist
			layout_index = cache->layouts_freelist;
			cache->layouts_freelist = cache->layouts[layout_index].lru.next;
		} else {
			// Create new
			SKB_ARRAY_RESERVE(cache->layouts, cache->layouts_count + 1);
			layout_index = cache->layouts_count++;
		}

		// Register to hash table
		skb_hash_table_add(cache->layouts_lookup, hash, layout_index);

		// Initialize to empty
		cached_layout = &cache->layouts[layout_index];
		memset(cached_layout, 0, sizeof(skb__cached_layout_t));
		cached_layout->lru = skb_list_item_make();
		cached_layout->hash = hash;
	}

	assert(layout_index != SKB_INVALID_INDEX);
	assert(cached_layout);

	// Mark last used time, and add to the front of the LRU list.
	cached_layout->last_access_stamp = cache->now_stamp;
	skb_list_move_to_front(&cache->lru, layout_index, skb__get_lru_item, cache);

	return cached_layout;
}

const skb_layout_t* skb_layout_cache_get_utf8(
	skb_layout_cache_t* cache, skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params,
	const char* text, int32_t text_count, skb_attribute_set_t attributes)
{
	assert(cache);

	if (text_count < 0)
		text_count = (int32_t)strlen(text);

	uint64_t hash = skb_hash64_empty();
	hash = skb_layout_params_hash_append(hash, params);
	hash = skb_hash64_append(hash, text, text_count);
	hash = skb_attributes_hash_append(hash, attributes);

	skb__cached_layout_t* cached_layout = skb__layout_cache_get_or_insert(cache, hash);
	if (!cached_layout->layout) {
		cached_layout->layout = skb_layout_create_utf8(temp_alloc, params, text, text_count, attributes);
	}
	assert(cached_layout);
	assert(cached_layout->layout);

	return cached_layout->layout;
}

const skb_layout_t* skb_layout_cache_get_utf32(
	skb_layout_cache_t* cache, skb_temp_alloc_t* temp_alloc,
	const skb_layout_params_t* params, const uint32_t* text, int32_t text_count, skb_attribute_set_t attributes)
{
	assert(cache);

	if (text_count < 0)
		text_count = skb_utf32_strlen(text);

	uint64_t hash = skb_hash64_empty();
	hash = skb_layout_params_hash_append(hash, params);
	hash = skb_hash64_append(hash, text, text_count * sizeof(uint32_t));
	hash = skb_attributes_hash_append(hash, attributes);

	skb__cached_layout_t* cached_layout = skb__layout_cache_get_or_insert(cache, hash);
	if (!cached_layout->layout) {
		cached_layout->layout = skb_layout_create_utf32(temp_alloc, params, text, text_count, attributes);
	}
	assert(cached_layout);
	assert(cached_layout->layout);

	return cached_layout->layout;
}

const skb_layout_t* skb_layout_cache_get_from_runs(
	skb_layout_cache_t* cache, skb_temp_alloc_t* temp_alloc,
	const skb_layout_params_t* params, const skb_content_run_t* runs, int32_t runs_count)
{
	assert(cache);

	uint64_t hash = skb_hash64_empty();

	hash = skb_layout_params_hash_append(hash, params);

	skb_content_run_t* fixed_runs = SKB_TEMP_ALLOC(temp_alloc, skb_content_run_t, runs_count);
	for (int32_t i = 0; i < runs_count; i++) {
		fixed_runs[i] = runs[i];
		if (fixed_runs[i].type == SKB_CONTENT_RUN_UTF8) {
			if (fixed_runs[i].utf8.text_count < 0)
				fixed_runs[i].utf8.text_count = (int32_t)strlen(fixed_runs[i].utf8.text);
			hash = skb_hash64_append(hash, fixed_runs[i].utf8.text, fixed_runs[i].utf8.text_count);
		} else if (fixed_runs[i].type == SKB_CONTENT_RUN_UTF32) {
			if (fixed_runs[i].utf32.text_count < 0)
				fixed_runs[i].utf32.text_count = skb_utf32_strlen(fixed_runs[i].utf32.text);
			hash = skb_hash64_append(hash, fixed_runs[i].utf32.text, fixed_runs[i].utf32.text_count * sizeof(uint32_t));
		} else if (fixed_runs[i].type == SKB_CONTENT_RUN_OBJECT) {
			hash = skb_hash64_append_float(hash, fixed_runs[i].object.width);
			hash = skb_hash64_append_float(hash, fixed_runs[i].object.height);
			hash = skb_hash64_append_uint64(hash, fixed_runs[i].object.data);
		} else if (fixed_runs[i].type == SKB_CONTENT_RUN_ICON) {
			hash = skb_hash64_append_float(hash, fixed_runs[i].icon.width);
			hash = skb_hash64_append_float(hash, fixed_runs[i].icon.height);
			hash = skb_hash64_append_uint32(hash, fixed_runs[i].icon.icon_handle);
		}
		hash = skb_attributes_hash_append(hash, fixed_runs[i].attributes);
	}

	skb__cached_layout_t* cached_layout = skb__layout_cache_get_or_insert(cache, hash);
	if (!cached_layout->layout)
		cached_layout->layout = skb_layout_create_from_runs(temp_alloc, params, fixed_runs, runs_count);

	assert(cached_layout);
	assert(cached_layout->layout);

	SKB_TEMP_FREE(temp_alloc, fixed_runs);

	return cached_layout->layout;
}

bool skb_layout_cache_compact(skb_layout_cache_t* cache)
{
	assert(cache);
	cache->now_stamp++;

	// TODO: add to config.
	static int32_t evict_after_duration = 100;

	bool compacted = false;

	int32_t layout_idx = cache->lru.tail; // Tail has least used items.
	while (layout_idx != SKB_INVALID_INDEX) {
		skb__cached_layout_t* cached_layout = &cache->layouts[layout_idx];

		const int32_t inactive_duration = cache->now_stamp - cached_layout->last_access_stamp;
		if (inactive_duration <= evict_after_duration)
			break;

		int32_t prev_layout_idx = cached_layout->lru.prev;

		// Remove from hash table and LRU
		skb_hash_table_remove(cache->layouts_lookup, cached_layout->hash);
		skb_list_remove(&cache->lru, layout_idx, skb__get_lru_item, cache);

		// Clear and return to freelist.
		memset(cached_layout, 0, sizeof(skb__cached_layout_t));
		cached_layout->lru.next = cache->layouts_freelist;
		cache->layouts_freelist = layout_idx;

		compacted = true;

		layout_idx = prev_layout_idx;
	}

	return compacted;
}
