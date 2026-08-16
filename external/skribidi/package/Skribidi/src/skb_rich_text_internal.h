// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_RICH_TEXT_INTERNAL_H
#define SKB_RICH_TEXT_INTERNAL_H

#include <stdint.h>

typedef struct skb_text_paragraph_t {
	skb_text_t text;				// Attributed text for the paragraph.
	int32_t global_text_offset;		// The start offset of the paragraph text in relation to the whole text.
	uint32_t version;				// Version of the paragraph, should change when contents change.

	skb_attribute_t* attributes;
	int32_t attributes_count;
	int32_t attributes_cap;
} skb_text_paragraph_t;

typedef struct skb_rich_text_t {
	skb_text_paragraph_t* paragraphs;
	int32_t paragraphs_count;
	int32_t paragraphs_cap;

	uint32_t version_counter;
	uint8_t should_free_instance;
} skb_rich_text_t;

skb_rich_text_t skb_rich_text_make_empty(void);

#endif // SKB_RICH_TEXT_INTERNAL_H
