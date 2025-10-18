// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_FONT_COLLECTION_INTERNAL_H
#define SKB_FONT_COLLECTION_INTERNAL_H

#include <stdint.h>

#include "skb_font_collection.h"

// harfbuzz forward declarations
typedef struct hb_font_t hb_font_t;

typedef struct skb_font_collection_t {
	uint32_t id;				// ID of the font collection.
	skb_font_t* fonts;			// Array of fonts in the collection.
	int32_t fonts_count;		// Number of fonts in the collection.
	int32_t fonts_cap;			// Capacity of the fonts array.
	int32_t empty_fonts_count;	// Number of empty fonts.
	skb_font_fallback_func_t* fallback_func;	// Function to call when no fonts are found.
	void* fallback_context;						// Context passed to the fallback function.

} skb_font_collection_t;

typedef struct skb_font_t {
	hb_font_t* hb_font;			// Associate harfbuzz font.
	char* name;					// Name of the font (file name)
	uint64_t hash;				// Hash of the name, used as unique identifier.
	int32_t upem;				// units per em square.
	float upem_scale;			// 1 / upem.
	skb_font_metrics_t metrics;			// Font metrics (ascender, etc).
	skb_caret_metrics_t caret_metrics;	// Caret metrics (offset, slope)
	uint8_t* scripts;			// Supported scripts
	int32_t scripts_count;		// Number of supported scripts
	uint8_t font_family;		// font family identifier.
	uint8_t style;				// Normal, italic, oblique (skb_font_style_t)
	float stretch;				// From 0.5 (ultra condensed) -> 1.0 (normal) -> 2.0 (ultra wide).
	int32_t weight;				// weight of the font (400 = regular).
	uint32_t generation;		// Generation index used to identify stale handles.
	skb_font_handle_t handle;	// Unique identifier of the font.
	skb_baseline_set_t* baseline_sets;	// Baseline sets (one for each requested script/direction combo).
	int32_t baseline_sets_count;		// Number of baseline sets
	int32_t baseline_sets_cap;			// Capacity of the baseline sets
} skb_font_t;

#endif // SKB_FONT_COLLECTION_INTERNAL_H
