// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_RICH_LAYOUT_INTERNAL_H
#define SKB_RICH_LAYOUT_INTERNAL_H

#include <stdint.h>

#include "skb_common.h"
#include "skb_layout_internal.h"

typedef struct skb_layout_paragraph_t {
	skb_layout_t layout;				// Layout for the paragraph, may contain multiple lines.
	int32_t global_text_offset;
	skb_vec2_t offset;					// Offset of the layout.
	uint32_t version;					// Version of the paragraph, if different from rich text paragraph, needs update.
	int32_t list_marker_counter;
	uint8_t group_flags;
} skb_layout_paragraph_t;

typedef struct skb_rich_layout_t {
	skb_layout_paragraph_t* paragraphs;	// Paragraphs
	int32_t paragraphs_count;
	int32_t paragraphs_cap;

	skb_layout_params_t params;			// Layout params for the whole layout.
	uint64_t params_hash;				// Hash of the Layout params, used to detect if the parmas changes.

	skb_attribute_t* attributes;		// Attributes used by the params
	int32_t attributes_count;
	int32_t attributes_cap;

	skb_rect2_t bounds;					// Bounds of the whole layout.

	uint8_t should_free_instance;
} skb_rich_layout_t;

skb_rich_layout_t skb_rich_layout_make_empty(void);

#endif // SKB_RICH_LAYOUT_INTERNAL_H
