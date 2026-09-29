// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_LAYOUT_INTERNAL_H
#define SKB_LAYOUT_INTERNAL_H

#include <stdint.h>

// Internal representation of a content run.
typedef struct skb__content_run_t {
	float content_width;				// Width of object or icon specified by the run
	float content_height;				// Height of object or icon specified by the run
	intptr_t content_data;				// Data of object or icon specified by the run
	intptr_t content_id;				// Custom identifier for a content run.
	skb_range_t text_range;				// Range of text the attributes apply to.
	skb_range_t attributes_range;		// The content attributes
	uint8_t type;						// Type of the content run which described the attributes. See skb_content_run_type_t.
	bool has_text_background;
} skb__content_run_t;

// Represents run of text in same script, font and style, for shaping.
typedef struct skb__shaping_run_t {
	skb_range_t text_range;
	skb_range_t glyph_range;			// Glyphs are in visual oder.
	skb_range_t cluster_range;			// Clusters are in logical order.
	int32_t content_run_idx;
	uint8_t script;
	uint8_t direction;
	uint8_t bidi_level;
	bool is_emoji;
	bool has_baseline_shift;
	float font_size;					// Cached font size for the run.
	skb_font_handle_t font_handle;
	float padding_start;
	float padding_end;
} skb__shaping_run_t;

typedef struct skb_layout_t {
	skb_layout_params_t params;	// Note: params has 'base_attributes' slice which points to attributes in the 'attributes' array.

	skb_rect2_t bounds;
	skb_padding2_t padding;
	float advance_y;
	uint8_t resolved_direction;
	uint32_t flags; // See skb_layout_flags_t

	// Text, text props, content_runs, and attributes are create based on the input text.
	uint32_t* text;
	skb_text_property_t* text_props;
	int32_t text_count;
	int32_t text_cap;

	skb__content_run_t* content_runs;
	int32_t content_runs_count;
	int32_t content_runs_cap;

	skb_attribute_t* attributes;
	int32_t attributes_count;
	int32_t attributes_cap;

	// Shaping runs is the output if itemization. The shaping runs are in logical order.
	skb__shaping_run_t* shaping_runs;
	int32_t shaping_runs_count;
	int32_t shaping_runs_cap;

	// Glyphs and clusters are output of shaping.
	skb_glyph_t* glyphs;
	int32_t glyphs_count;
	int32_t glyphs_cap;

	skb_cluster_t* clusters;
	int32_t clusters_count;
	int32_t clusters_cap;

	// Lines, layout runs, and decorations are output of line layout.
	skb_layout_line_t* lines;
	int32_t lines_count;
	int32_t lines_cap;

	// The layout runs are in visual order.
	skb_layout_run_t* layout_runs;
	int32_t layout_runs_count;
	int32_t layout_runs_cap;

	skb_decoration_t* decorations;
	int32_t decorations_count;
	int32_t decorations_cap;

	uint8_t should_free_instance;
} skb_layout_t;

skb_layout_t skb_layout_make_empty(void);
bool skb_layout_add_ellipsis_to_last_line(skb_layout_t* layout);

#endif // SKB_LAYOUT_INTERNAL_H
