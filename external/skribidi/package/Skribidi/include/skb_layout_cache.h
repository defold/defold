// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_LAYOUT_CACHE_H
#define SKB_LAYOUT_CACHE_H

#include <stdint.h>
#include "skb_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup layout_cache Layout Cache
 *
 * The Layout cache can be used to reuse layouts. It is specifically suited for immediate mode APIs.
 *
 * The layouts are located based on hash of the inputs provided for the getter functions.
 *
 * Old entries get evicted by periodically calling skb_layout_cache_compact().
 *
 * @{
 */

/** Opaque type for the text layout cache. Use skb_layout_cache_create() to create. */
typedef struct skb_layout_cache_t skb_layout_cache_t;

/**
 * Creates a new layout cache.
 * @return newly created cache.
 */
skb_layout_cache_t* skb_layout_cache_create(void);

/**
 * Destroy a layout cache.
 * @param cache pointer to layout cache to destroy.
 */
void skb_layout_cache_destroy(skb_layout_cache_t* cache);

/**
 * Layouts specified text, or returns existing layout from cache if one exists.
 * @param cache layout cache to use.
 * @param temp_alloc temp allocator used to during creation of the layout.
 * @param params layout parameters to use for the layout.
 * @param text text to layout in utf-8 encoding.
 * @param text_count length of the text, or -1 if null terminated.
 * @param attributes text attributes to
 * @return const pointer to the requested layout.
 */
const skb_layout_t* skb_layout_cache_get_utf8(
	skb_layout_cache_t* cache, skb_temp_alloc_t* temp_alloc,
	const skb_layout_params_t* params, const char* text, int32_t text_count,
	const skb_attribute_t* attributes, int32_t attributes_count);

/**
 * Layouts specified text, or returns existing layout from cache if one exists.
 * @param cache layout cache to use.
 * @param temp_alloc temp allocator used to during creation of the layout.
 * @param params layout parameters to use for the layout.
 * @param text text to layout in utf-32 encoding.
 * @param text_count length of the text, or -1 if null terminated.
 * @param attributes text attributes to use.
 * @return const pointer to the requested layout.
 */
const skb_layout_t* skb_layout_cache_get_utf32(
	skb_layout_cache_t* cache, skb_temp_alloc_t* temp_alloc,
	const skb_layout_params_t* params, const uint32_t* text, int32_t text_count,
	const skb_attribute_t* attributes, int32_t attributes_count);

/**
 * Layouts specified text, or returns existing layout from cache if one exists.
 * @param cache layout cache to use.
 * @param temp_alloc temp allocator used to during creation of the layout.
 * @param params layout parameters to use for the layout.
 * @param runs array of utf-8 text runs to layout.
 * @param runs_count number of runs in runs array.
 * @return const pointer to the requested layout.
 */
const skb_layout_t* skb_layout_cache_get_from_runs_utf8(
	skb_layout_cache_t* cache, skb_temp_alloc_t* temp_alloc,
	const skb_layout_params_t* params, const skb_text_run_utf8_t* runs, int32_t runs_count);

/**
 * Layouts specified text, or returns existing layout from cache if one exists.
 * @param cache layout cache to use.
 * @param temp_alloc temp allocator used to during creation of the layout.
 * @param params layout parameters to use for the layout.
 * @param runs array of utf-32 text runs to layout.
 * @param runs_count number of runs in runs array.
 * @return const pointer to the requested layout.
 */
const skb_layout_t* skb_layout_cache_get_from_runs_utf32(
	skb_layout_cache_t* cache, skb_temp_alloc_t* temp_alloc,
	const skb_layout_params_t* params, const skb_text_run_utf32_t* runs, int32_t runs_count);

/**
 * Compects the layout cache by evicting old items from the cache.
 * The items are removed in least recently used fashion.
 * @param cache layout cache to compect
 * @return true if layouts were evicted.
 */
bool skb_layout_cache_compact(skb_layout_cache_t* cache);

/** @} */

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_LAYOUT_CACHE_H
