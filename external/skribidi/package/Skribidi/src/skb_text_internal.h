// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_TEXT_INTERNAL_H
#define SKB_TEXT_INTERNAL_H

#include <stdint.h>

typedef struct skb_text_t {
	uint32_t* text;
	int32_t text_count;
	int32_t text_cap;

	uint8_t* text_props;	// grapheme breaks

	skb_attribute_span_t* spans;
	int32_t spans_count;
	int32_t spans_cap;

	skb_temp_alloc_t* temp_alloc;

	uint8_t should_free_instance;
} skb_text_t;

/**
 * Makes an empty attributed text.
 * This can be used by internal code to avoid extra allocation and indirection of embedded attributed text.
 * The text needs to be released using skb_text_destroy(), should_free_instance is used to differentiate between skb_text_create(), and skb_text_make_empty().
 * @return
 */
skb_text_t skb_text_make_empty(void);

#endif // SKB_TEXT_INTERNAL_H
