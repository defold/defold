// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "skb_common.h"
#include "skb_rich_text.h"
#include "skb_text_internal.h"
#include "skb_rich_text_internal.h"


static skb_range_t* skb__split_text_into_paragraphs(skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len, int32_t* out_paragraphs_count)
{
	// Split text to paragraphs.
	int32_t start_offset = 0;
	int32_t offset = 0;

	int32_t paragraphs_count = 0;
	int32_t paragraphs_cap = 8;
	skb_range_t* paragraphs = SKB_TEMP_ALLOC(temp_alloc, skb_range_t, paragraphs_cap);

	while (offset < utf32_len) {
		if (skb_is_paragraph_separator(utf32[offset])) {
			// Handle CRLF
			if (offset + 1 < utf32_len && utf32[offset] == SKB_CHAR_CARRIAGE_RETURN && utf32[offset + 1] == SKB_CHAR_LINE_FEED)
				offset++; // Skip over CR
			offset++; // Skip over the separator

			// Create new paragraph
			SKB_TEMP_RESERVE(temp_alloc, paragraphs, paragraphs_count + 1);
			skb_range_t* new_paragraph = &paragraphs[paragraphs_count++];
			SKB_ZERO_STRUCT(new_paragraph);
			new_paragraph->start = start_offset;
			new_paragraph->end = offset;
			start_offset = offset;
		} else {
			offset++;
		}
	}

	// The rest
	SKB_TEMP_RESERVE(temp_alloc, paragraphs, paragraphs_count + 1);
	skb_range_t* new_paragraph = &paragraphs[paragraphs_count++];
	SKB_ZERO_STRUCT(new_paragraph);
	new_paragraph->start = start_offset;
	new_paragraph->end = offset;

	*out_paragraphs_count = paragraphs_count;
	return paragraphs;
}

static skb_paragraph_position_t skb__get_paragraph_position(const skb_rich_text_t* rich_text, int32_t text_offset)
{
	if (rich_text->paragraphs_count == 0 || text_offset <= 0)
		return (skb_paragraph_position_t){.paragraph_idx = 0, .text_offset = 0, .global_text_offset = 0};

	const int32_t last_paragraph_idx = rich_text->paragraphs_count - 1;
	const int32_t total_text_count = rich_text->paragraphs[last_paragraph_idx].global_text_offset + skb_text_get_utf32_count(&rich_text->paragraphs[last_paragraph_idx].text);
	if (text_offset >= total_text_count) {
		// Past the last paragraph.
		const int32_t local_text_offset = skb_text_get_utf32_count(&rich_text->paragraphs[rich_text->paragraphs_count - 1].text);
		return (skb_paragraph_position_t){
			.paragraph_idx = rich_text->paragraphs_count - 1,
			.text_offset = local_text_offset,
			.global_text_offset = rich_text->paragraphs[rich_text->paragraphs_count - 1].global_text_offset + local_text_offset,
		};
	}

	// Binary search the paragraph which contains the text offset.
	const int32_t paragraph_idx = skb_ub_search(text_offset, &rich_text->paragraphs[0].global_text_offset, rich_text->paragraphs_count, sizeof(skb_text_paragraph_t));

	const skb_text_paragraph_t* paragraph = &rich_text->paragraphs[paragraph_idx];
	const int32_t text_count = skb_text_get_utf32_count(&paragraph->text);
	const int32_t start_text_offset = paragraph->global_text_offset;
	return (skb_paragraph_position_t){
		.paragraph_idx = paragraph_idx,
		.text_offset = skb_clampi(text_offset - start_text_offset, 0, text_count - 1),
		.global_text_offset = text_offset,
	};
}

typedef bool sb__iterate_paragraphs_func_t(skb_rich_text_t* rich_text, int32_t paragraph_idx, skb_text_range_t text_range, void* context);

static void skb__iterate_paragraphs(skb_rich_text_t* rich_text, skb_text_range_t text_range, sb__iterate_paragraphs_func_t* func, void* context)
{
	skb_paragraph_range_t paragraph_range = skb_rich_text_get_paragraph_range_from_text_range(rich_text, text_range, SKB_AFFINITY_USE);

	if (paragraph_range.start.paragraph_idx == paragraph_range.end.paragraph_idx) {
		const skb_text_range_t range = {
			.start.offset = paragraph_range.start.text_offset,
			.end.offset = paragraph_range.end.text_offset,
		};
		func(rich_text, paragraph_range.start.paragraph_idx, range, context);
		return;
	}

	int32_t paragraph_idx = paragraph_range.start.paragraph_idx;

	// First paragraph
	const skb_text_range_t first_range = {
		.start.offset = paragraph_range.start.text_offset,
		.end.offset = skb_text_get_utf32_count(&rich_text->paragraphs[paragraph_range.start.paragraph_idx].text),
	};
	if (!func(rich_text, paragraph_idx, first_range, context))
		return;
	paragraph_idx++;

	// Middle paragraphs
	while (paragraph_idx < paragraph_range.end.paragraph_idx) {
		const skb_text_range_t range = {
			.start.offset = 0,
			.end.offset = skb_text_get_utf32_count(&rich_text->paragraphs[paragraph_idx].text),
		};
		if (!func(rich_text, paragraph_idx, range, context))
			return;
		paragraph_idx++;
	}

	// Last paragraph
	const skb_text_range_t last_range = {
		.start.offset = 0,
		.end.offset = skb_mini(paragraph_range.end.text_offset, skb_text_get_utf32_count(&rich_text->paragraphs[paragraph_range.end.paragraph_idx].text)),
	};
	func(rich_text, paragraph_idx, last_range, context);
}

static void skb__text_paragraph_copy_attributes(skb_text_paragraph_t* text_paragraph, skb_attribute_set_t attributes)
{
	text_paragraph->attributes_count = skb_attributes_get_copy_flat_count(attributes);
	SKB_ARRAY_RESERVE(text_paragraph->attributes, text_paragraph->attributes_count);
	skb_attributes_copy_flat(attributes, text_paragraph->attributes, text_paragraph->attributes_count);
}

static skb_attribute_set_t skb__text_paragraph_get_attributes(const skb_text_paragraph_t* text_paragraph)
{
	return (skb_attribute_set_t){
		.attributes = text_paragraph->attributes,
		.attributes_count = text_paragraph->attributes_count,
	};
}

static void skb__text_paragraph_init(skb_rich_text_t* rich_text, skb_text_paragraph_t* text_paragraph, skb_attribute_set_t attributes)
{
	SKB_ZERO_STRUCT(text_paragraph);

	text_paragraph->text = skb_text_make_empty();
	text_paragraph->version = ++rich_text->version_counter;
	skb__text_paragraph_copy_attributes(text_paragraph, attributes);
}

static void skb__text_paragraph_clear(skb_text_paragraph_t* text_paragraph)
{
	skb_text_destroy(&text_paragraph->text);
	skb_free(text_paragraph->attributes);
	SKB_ZERO_STRUCT(text_paragraph);
}

skb_rich_text_t skb_rich_text_make_empty(void)
{
	return (skb_rich_text_t){
		.should_free_instance = false,
		.version_counter = 1,
	};
}

skb_rich_text_t* skb_rich_text_create(void)
{
	skb_rich_text_t* rich_text = skb_malloc(sizeof(skb_rich_text_t));
	SKB_ZERO_STRUCT(rich_text);
	rich_text->should_free_instance = true;
	return rich_text;
}

void skb_rich_text_destroy(skb_rich_text_t* rich_text)
{
	if (!rich_text) return;
	for (int32_t i = 0; i < rich_text->paragraphs_count; i++)
		skb__text_paragraph_clear(&rich_text->paragraphs[i]);
	skb_free(rich_text->paragraphs);

	bool should_free_instance = rich_text->should_free_instance;
	SKB_ZERO_STRUCT(rich_text);

	if (should_free_instance)
		skb_free(rich_text);
}

void skb_rich_text_reset(skb_rich_text_t* rich_text)
{
	for (int32_t i = 0; i < rich_text->paragraphs_count; i++)
		skb__text_paragraph_clear(&rich_text->paragraphs[i]);
	rich_text->paragraphs_count = 0;
}

int32_t skb_rich_text_get_utf32_count(const skb_rich_text_t* rich_text)
{
	if (!rich_text)
		return 0;

	int32_t count = 0;
	for (int32_t i = 0; i < rich_text->paragraphs_count; i++)
		count += skb_text_get_utf32_count(&rich_text->paragraphs[i].text);
	return count;
}

int32_t skb_rich_text_get_utf8_count_in_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range)
{
	const skb_paragraph_position_t start_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.start, SKB_AFFINITY_USE);
	const skb_paragraph_position_t end_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.end, SKB_AFFINITY_USE);

	if (start_pos.paragraph_idx == end_pos.paragraph_idx) {
		const skb_text_t* paragraph_text = &rich_text->paragraphs[start_pos.paragraph_idx].text;
		const int32_t count = skb_maxi(0, end_pos.text_offset - start_pos.text_offset);
		return skb_utf32_to_utf8_count(skb_text_get_utf32(paragraph_text) + start_pos.text_offset, count);
	}

	int32_t count = 0;
	// First paragraph
	const skb_text_t* first_paragraph_text = &rich_text->paragraphs[start_pos.paragraph_idx].text;
	const int32_t first_count = skb_maxi(0, skb_text_get_utf32_count(first_paragraph_text) - start_pos.text_offset);
	count += skb_utf32_to_utf8_count(skb_text_get_utf32(first_paragraph_text) + start_pos.text_offset, first_count);
	// Middle paragraphs
	for (int32_t i = start_pos.paragraph_idx + 1; i < end_pos.paragraph_idx; i++) {
		const skb_text_t* paragraph_text = &rich_text->paragraphs[i].text;
		count += skb_utf32_to_utf8_count(skb_text_get_utf32(paragraph_text), skb_text_get_utf32_count(paragraph_text));
	}
	// Last paragraph
	const skb_text_t* last_paragraph_text = &rich_text->paragraphs[end_pos.paragraph_idx].text;
	const int32_t last_count = skb_mini(end_pos.text_offset, skb_text_get_utf32_count(last_paragraph_text));
	count += skb_utf32_to_utf8_count(skb_text_get_utf32(last_paragraph_text), last_count);

	return count;
}

int32_t skb_rich_text_get_utf8_in_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range, char* utf8, int32_t utf8_cap)
{
	const skb_paragraph_position_t start_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.start, SKB_AFFINITY_USE);
	const skb_paragraph_position_t end_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.end, SKB_AFFINITY_USE);

	if (start_pos.paragraph_idx == end_pos.paragraph_idx) {
		const skb_text_t* paragraph_text = &rich_text->paragraphs[start_pos.paragraph_idx].text;
		const int32_t count = skb_maxi(0, end_pos.text_offset - start_pos.text_offset);
		return skb_utf32_to_utf8(skb_text_get_utf32(paragraph_text) + start_pos.text_offset, count, utf8, utf8_cap);
	}

	int32_t count = 0;
	// First paragraph
	const skb_text_t* first_paragraph_text = &rich_text->paragraphs[start_pos.paragraph_idx].text;
	const int32_t first_count = skb_maxi(0, skb_text_get_utf32_count(first_paragraph_text) - start_pos.text_offset);
	count += skb_utf32_to_utf8(skb_text_get_utf32(first_paragraph_text) + start_pos.text_offset, first_count, utf8 + count, utf8_cap - count);
	// Middle paragraphs
	for (int32_t i = start_pos.paragraph_idx + 1; i < end_pos.paragraph_idx; i++) {
		const skb_text_t* paragraph_text = &rich_text->paragraphs[i].text;
		count += skb_utf32_to_utf8(skb_text_get_utf32(paragraph_text), skb_text_get_utf32_count(paragraph_text), utf8 + count, utf8_cap - count);
	}
	// Last paragraph
	const skb_text_t* last_paragraph_text = &rich_text->paragraphs[end_pos.paragraph_idx].text;
	const int32_t last_count = skb_mini(end_pos.text_offset, skb_text_get_utf32_count(last_paragraph_text));
	count += skb_utf32_to_utf8(skb_text_get_utf32(last_paragraph_text), last_count, utf8 + count, utf8_cap - count);

	return count;
}

int32_t skb_rich_text_get_utf32_count_in_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range)
{
	const skb_paragraph_position_t start_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.start, SKB_AFFINITY_USE);
	const skb_paragraph_position_t end_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.end, SKB_AFFINITY_USE);

	if (start_pos.paragraph_idx == end_pos.paragraph_idx)
		return skb_maxi(0, end_pos.text_offset - start_pos.text_offset);

	int32_t count = 0;
	// First paragraph
	const skb_text_t* first_paragraph_text = &rich_text->paragraphs[start_pos.paragraph_idx].text;
	const int32_t first_count = skb_maxi(0, skb_text_get_utf32_count(first_paragraph_text) - start_pos.text_offset);
	count += first_count;
	// Middle paragraphs
	for (int32_t i = start_pos.paragraph_idx + 1; i <= end_pos.paragraph_idx - 1; i++) {
		const skb_text_t* paragraph_text = &rich_text->paragraphs[i].text;
		count += skb_text_get_utf32_count(paragraph_text);
	}
	// Last paragraph
	const skb_text_t* last_paragraph_text = &rich_text->paragraphs[end_pos.paragraph_idx].text;
	const int32_t last_count = skb_mini(end_pos.text_offset, skb_text_get_utf32_count(last_paragraph_text));
	count += last_count;

	return count;
}

int32_t skb_rich_text_get_utf32_in_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range, uint32_t* utf32, int32_t utf32_cap)
{
	const skb_paragraph_position_t start_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.start, SKB_AFFINITY_USE);
	const skb_paragraph_position_t end_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.end, SKB_AFFINITY_USE);

	if (start_pos.paragraph_idx == end_pos.paragraph_idx) {
		const skb_text_t* paragraph_text = &rich_text->paragraphs[start_pos.paragraph_idx].text;
		const int32_t count = skb_maxi(0, end_pos.text_offset - start_pos.text_offset);
		return skb_utf32_copy(skb_text_get_utf32(paragraph_text) + start_pos.text_offset, count, utf32, utf32_cap);
	}

	int32_t count = 0;
	// First paragraph
	const skb_text_t* first_paragraph_text = &rich_text->paragraphs[start_pos.paragraph_idx].text;
	const int32_t first_count = skb_maxi(0, skb_text_get_utf32_count(first_paragraph_text) - start_pos.text_offset);
	count += skb_utf32_copy(skb_text_get_utf32(first_paragraph_text) + start_pos.text_offset, first_count, utf32 + count, utf32_cap - count);
	// Middle paragraphs
	for (int32_t i = start_pos.paragraph_idx + 1; i <= end_pos.paragraph_idx - 1; i++) {
		const skb_text_t* paragraph_text = &rich_text->paragraphs[i].text;
		count += skb_utf32_copy(skb_text_get_utf32(paragraph_text), skb_text_get_utf32_count(paragraph_text), utf32 + count, utf32_cap - count);
	}
	// Last paragraph
	const skb_text_t* last_paragraph_text = &rich_text->paragraphs[end_pos.paragraph_idx].text;
	const int32_t last_count = skb_mini(end_pos.text_offset, skb_text_get_utf32_count(last_paragraph_text));
	count += skb_utf32_copy(skb_text_get_utf32(last_paragraph_text), last_count, utf32 + count, utf32_cap - count);

	return count;
}

skb_range_t skb_rich_text_get_paragraphs_range_from_text_range(skb_rich_text_t* rich_text, skb_text_range_t text_range)
{
	assert(rich_text);
	if (rich_text->paragraphs_count == 0)
		return (skb_range_t){0};

	const skb_paragraph_position_t start_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.start, SKB_AFFINITY_USE);
	const skb_paragraph_position_t end_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.end, SKB_AFFINITY_USE);

	return (skb_range_t){
		.start = start_pos.paragraph_idx,
		.end = end_pos.paragraph_idx + 1,
	};
}

int32_t skb_rich_text_get_paragraphs_count(const skb_rich_text_t* rich_text)
{
	return rich_text ? rich_text->paragraphs_count : 0;
}

const skb_text_t* skb_rich_text_get_paragraph_text(const skb_rich_text_t* rich_text, int32_t paragraph_idx)
{
	assert(rich_text);
	if (paragraph_idx < 0 || paragraph_idx >= rich_text->paragraphs_count)
		return NULL;
	return &rich_text->paragraphs[paragraph_idx].text;
}

uint32_t skb_rich_text_get_paragraph_version(const skb_rich_text_t* rich_text, int32_t paragraph_idx)
{
	assert(rich_text);
	if (paragraph_idx < 0 || paragraph_idx >= rich_text->paragraphs_count)
		return 0;
	return rich_text->paragraphs[paragraph_idx].version;
}

skb_attribute_set_t skb_rich_text_get_paragraph_attributes(const skb_rich_text_t* rich_text, int32_t paragraph_idx)
{
	assert(rich_text);

	if (paragraph_idx < 0 || paragraph_idx >= rich_text->paragraphs_count)
		return (skb_attribute_set_t){0};

	return skb__text_paragraph_get_attributes(&rich_text->paragraphs[paragraph_idx]);
}

int32_t skb_rich_text_get_paragraph_text_utf32_count(const skb_rich_text_t* rich_text, int32_t paragraph_idx)
{
	if (!rich_text)
		return 0;
	if (paragraph_idx < 0 || paragraph_idx >= rich_text->paragraphs_count)
		return 0;
	return skb_text_get_utf32_count(&rich_text->paragraphs[paragraph_idx].text);
}

int32_t skb_rich_text_get_paragraph_text_offset(const skb_rich_text_t* rich_text, int32_t paragraph_idx)
{
	assert(rich_text);
	if (paragraph_idx < 0 || paragraph_idx >= rich_text->paragraphs_count)
		return 0;
	return rich_text->paragraphs[paragraph_idx].global_text_offset;
}

skb_rich_text_change_t skb_rich_text_append(skb_rich_text_t* rich_text, const skb_rich_text_t* source_rich_text)
{
	assert(rich_text);
	if (!source_rich_text)
		return (skb_rich_text_change_t){0};

	const int32_t text_count = skb_rich_text_get_utf32_count(rich_text);
	return skb_rich_text_insert(rich_text, (skb_text_range_t){.start.offset = text_count, .end.offset = text_count}, source_rich_text);
}

skb_rich_text_change_t skb_rich_text_append_range(skb_rich_text_t* rich_text, const skb_rich_text_t* source_rich_text, skb_text_range_t source_text_range)
{
	const int32_t text_count = skb_rich_text_get_utf32_count(rich_text);
	return skb_rich_text_insert_range(rich_text, (skb_text_range_t){.start.offset = text_count, .end.offset = text_count}, source_rich_text, source_text_range);
}

skb_rich_text_change_t skb_rich_text_append_paragraph(skb_rich_text_t* rich_text, skb_attribute_set_t paragraph_attributes)
{
	assert(rich_text);

	// Make sure the current last block is terminated.
	if (rich_text->paragraphs_count > 0) {
		const uint32_t codepoint = SKB_CHAR_LINE_FEED;
		skb_text_append_utf32(&rich_text->paragraphs[rich_text->paragraphs_count - 1].text, &codepoint, 0, (skb_attribute_set_t){0});
		// Mark text as changed.
		rich_text->paragraphs[rich_text->paragraphs_count - 1].version = ++rich_text->version_counter;
	}

	SKB_ARRAY_RESERVE(rich_text->paragraphs, rich_text->paragraphs_count + 1);
	skb_text_paragraph_t* new_paragraph = &rich_text->paragraphs[rich_text->paragraphs_count++];
	skb__text_paragraph_init(rich_text, new_paragraph, paragraph_attributes);

	return (skb_rich_text_change_t){
		.start_paragraph_idx = rich_text->paragraphs_count - 1,
		.inserted_paragraph_count = 1,
		.removed_paragraph_count = 0,
		.edit_end_position = {.offset = 0},
	};
}

skb_rich_text_change_t skb_rich_text_append_text(skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc, const skb_text_t* source_text)
{
	assert(rich_text);
	assert(source_text);

	const skb_text_range_t text_range = {
		.start.offset = 0,
		.end.offset = skb_text_get_utf32_count(source_text),
	};
	return skb_rich_text_append_text_range(rich_text, temp_alloc, source_text, text_range);
}

skb_rich_text_change_t skb_rich_text_append_text_range(skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc, const skb_text_t* source_text, skb_text_range_t source_text_range)
{
	assert(rich_text);
	assert(source_text);

	skb_range_t source_range = skb_text_get_range_from_text_range(source_text, source_text_range);

	const uint32_t* utf32 = skb_text_get_utf32(source_text) + source_range.start;
	const int32_t utf32_count = source_range.end - source_range.start;

	int32_t inserted_paragraph_count = 0;
	skb_range_t* inserted_paragraph_ranges = skb__split_text_into_paragraphs(temp_alloc, utf32, utf32_count, &inserted_paragraph_count);
	assert(inserted_paragraph_count > 0); // We assume that even for empty input text there's one paragraph created.

	SKB_ARRAY_RESERVE(rich_text->paragraphs, rich_text->paragraphs_count + inserted_paragraph_count);


	int32_t text_offset = (rich_text->paragraphs_count > 0) ? rich_text->paragraphs[rich_text->paragraphs_count - 1].global_text_offset : 0;
	int32_t range_idx = 0;

	int32_t old_paragraph_count = rich_text->paragraphs_count;

	skb_attribute_set_t paragraph_attributes = {0};

	// If the last block is not terminated, append the first range there.
	if (rich_text->paragraphs_count > 0) {
		skb_text_paragraph_t* last_paragraph = &rich_text->paragraphs[rich_text->paragraphs_count - 1];

		const skb_text_range_t block_range = {
			.start.offset = source_range.start + inserted_paragraph_ranges[range_idx].start,
			.end.offset = source_range.start + inserted_paragraph_ranges[range_idx].end,
		};
		skb_text_append_range(&last_paragraph->text, source_text, block_range);

		text_offset += block_range.end.offset - block_range.start.offset;
		range_idx++;

		paragraph_attributes.attributes = last_paragraph->attributes;
		paragraph_attributes.attributes_count = last_paragraph->attributes_count;

		// Mark as changed
		last_paragraph->version = ++rich_text->version_counter;
	}

	skb_rich_text_change_t change = {
		.start_paragraph_idx = rich_text->paragraphs_count
	};

	while (range_idx < inserted_paragraph_count) {
		// Create new paragraph
		skb_text_range_t paragraph_range = {
			.start.offset = source_range.start + inserted_paragraph_ranges[range_idx].start,
			.end.offset = source_range.start + inserted_paragraph_ranges[range_idx].end,
		};
		skb_text_paragraph_t* new_paragraph = &rich_text->paragraphs[rich_text->paragraphs_count++];
		assert(rich_text->paragraphs_count <= rich_text->paragraphs_cap);
		skb__text_paragraph_init(rich_text, new_paragraph, paragraph_attributes);
		new_paragraph->global_text_offset = text_offset;

		skb_text_append_range(&new_paragraph->text, source_text, paragraph_range);

		text_offset += paragraph_range.end.offset - paragraph_range.start.offset;
		range_idx++;
	}

	change.inserted_paragraph_count = rich_text->paragraphs_count - old_paragraph_count;
	change.edit_end_position = (skb_text_position_t){.offset = text_offset - 1};

	SKB_TEMP_FREE(temp_alloc, inserted_paragraph_ranges);

	return change;
}

skb_rich_text_change_t skb_rich_text_append_utf8(skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc, const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes)
{
	return skb_rich_text_append_utf8_with_payload(rich_text, temp_alloc, utf8, utf8_count, attributes, 0, NULL);
}

skb_rich_text_change_t skb_rich_text_append_utf8_with_payload(skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc, const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload)
{
	assert(rich_text);
	assert(utf8);

	if (utf8_count < 0) utf8_count = (int32_t)strlen(utf8);

	const int32_t utf32_count = skb_utf8_to_utf32(utf8, utf8_count, NULL, 0);
	uint32_t* utf32 = SKB_TEMP_ALLOC(temp_alloc, uint32_t, utf32_count);
	skb_utf8_to_utf32(utf8, utf8_count, utf32, utf32_count);

	skb_rich_text_change_t change = skb_rich_text_append_utf32_with_payload(rich_text, temp_alloc, utf32, utf32_count, attributes, span_flags, payload);

	SKB_TEMP_FREE(temp_alloc, utf32);

	return change;
}

skb_rich_text_change_t skb_rich_text_append_utf32(skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes)
{
	return skb_rich_text_append_utf32_with_payload(rich_text, temp_alloc, utf32, utf32_count, attributes, 0, NULL);
}

skb_rich_text_change_t skb_rich_text_append_utf32_with_payload(skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload)
{
	assert(rich_text);

	if (!utf32)
		utf32_count = 0;
	if (utf32_count == -1)
		utf32_count = skb_utf32_strlen(utf32);

	int32_t inserted_paragraph_count = 0;
	skb_range_t* inserted_paragraph_ranges = skb__split_text_into_paragraphs(temp_alloc, utf32, utf32_count, &inserted_paragraph_count);
	assert(inserted_paragraph_count > 0); // We assume that even for empty input text there's one paragraph created.

	SKB_ARRAY_RESERVE(rich_text->paragraphs, rich_text->paragraphs_count + inserted_paragraph_count);

	int32_t text_offset = (rich_text->paragraphs_count > 0) ? rich_text->paragraphs[rich_text->paragraphs_count - 1].global_text_offset : 0;
	int32_t range_idx = 0;

	int32_t old_paragraph_count = rich_text->paragraphs_count;
	skb_attribute_set_t paragraph_attributes = {0};

	// If the last block is not terminated, append the first range there.
	if (rich_text->paragraphs_count > 0) {
		skb_text_paragraph_t* last_paragraph = &rich_text->paragraphs[rich_text->paragraphs_count - 1];

		const int32_t text_count = inserted_paragraph_ranges[range_idx].end - inserted_paragraph_ranges[range_idx].start;
		skb_text_append_utf32_with_payload(&last_paragraph->text, utf32 + inserted_paragraph_ranges[range_idx].start, text_count, attributes, span_flags, payload);

		text_offset += text_count;
		range_idx++;

		paragraph_attributes.attributes = last_paragraph->attributes;
		paragraph_attributes.attributes_count = last_paragraph->attributes_count;

		// Mark as changed
		last_paragraph->version = ++rich_text->version_counter;
	}

	skb_rich_text_change_t change = {
		.start_paragraph_idx = rich_text->paragraphs_count
	};

	while (range_idx < inserted_paragraph_count) {
		// Create new paragraph
		skb_range_t paragraph_range = inserted_paragraph_ranges[range_idx];
		skb_text_paragraph_t* new_paragraph = &rich_text->paragraphs[rich_text->paragraphs_count++];
		assert(rich_text->paragraphs_count <= rich_text->paragraphs_cap);
		skb__text_paragraph_init(rich_text, new_paragraph, paragraph_attributes);
		new_paragraph->global_text_offset = text_offset;

		skb_text_append_utf32_with_payload(&new_paragraph->text, utf32 + paragraph_range.start, paragraph_range.end - paragraph_range.start, attributes, span_flags, payload);

		text_offset += paragraph_range.end - paragraph_range.start;
		range_idx++;
	}

	change.inserted_paragraph_count = rich_text->paragraphs_count - old_paragraph_count;
	change.edit_end_position = (skb_text_position_t){.offset = text_offset - 1};

	SKB_TEMP_FREE(temp_alloc, inserted_paragraph_ranges);

	return change;
}


static skb_rich_text_change_t skb__rich_text_replace(
	skb_rich_text_t* rich_text, skb_text_range_t text_range,
	const skb_text_paragraph_t* source_paragraphs, int32_t source_paragraphs_count,
	skb_paragraph_range_t source_range)
{
	assert(rich_text);
	assert(source_paragraphs);
	assert(source_paragraphs_count > 0);

	// Adjust source pointer to the used range.
	source_paragraphs += source_range.start.paragraph_idx;
	source_paragraphs_count = skb_mini((source_range.end.paragraph_idx + 1) - source_range.start.paragraph_idx, source_paragraphs_count);

	skb_paragraph_range_t paragraph_range = skb_rich_text_get_paragraph_range_from_text_range(rich_text, text_range, SKB_AFFINITY_USE);

	// Save start and end blocks from being freed (these may be the same).
	skb_text_paragraph_t start_paragraph_copy = {0};
	skb_text_paragraph_t end_paragraph_copy = {0};

	if (paragraph_range.start.paragraph_idx < rich_text->paragraphs_count)
		start_paragraph_copy = rich_text->paragraphs[paragraph_range.start.paragraph_idx];
	if (paragraph_range.end.paragraph_idx < rich_text->paragraphs_count)
		end_paragraph_copy = rich_text->paragraphs[paragraph_range.end.paragraph_idx];

	if (paragraph_range.start.paragraph_idx < rich_text->paragraphs_count)
		SKB_ZERO_STRUCT(&rich_text->paragraphs[paragraph_range.start.paragraph_idx]);
	if (paragraph_range.end.paragraph_idx < rich_text->paragraphs_count)
		SKB_ZERO_STRUCT(&rich_text->paragraphs[paragraph_range.end.paragraph_idx]);

	// Free lines that we'll remove or rebuild.
	for (int32_t i = paragraph_range.start.paragraph_idx; i < skb_mini(paragraph_range.end.paragraph_idx + 1, rich_text->paragraphs_count); i++)
		skb__text_paragraph_clear(&rich_text->paragraphs[i]);

	// Allocate new blocks or prune.
	const int32_t removed_paragraphs_count = skb_mini(paragraph_range.end.paragraph_idx + 1, rich_text->paragraphs_count) - paragraph_range.start.paragraph_idx;
	const int32_t new_paragraphs_count = skb_maxi(0, rich_text->paragraphs_count - removed_paragraphs_count + source_paragraphs_count);
	const int32_t old_paragraphs_count = rich_text->paragraphs_count;
	SKB_ARRAY_RESERVE(rich_text->paragraphs, new_paragraphs_count);
	rich_text->paragraphs_count = new_paragraphs_count;

	// Move tail of the blocks to create space for the new blocks to be inserted, accounting for the removed blocks.
	const int32_t old_tail_idx = skb_mini(paragraph_range.end.paragraph_idx + 1, old_paragraphs_count); // end_block_idx is the last one to remove.
	const int32_t tail_count = old_paragraphs_count - old_tail_idx;
	const int32_t new_tail_idx = paragraph_range.start.paragraph_idx + source_paragraphs_count;
	if (new_tail_idx != old_tail_idx && tail_count > 0)
		memmove(rich_text->paragraphs + new_tail_idx, rich_text->paragraphs + old_tail_idx, tail_count * sizeof(skb_text_paragraph_t));

	// Create new blocks.
	const int32_t start_paragraph_copy_count = paragraph_range.start.text_offset;
	const int32_t end_paragraph_copy_offset = paragraph_range.end.text_offset;
	const int32_t end_paragraph_copy_count = skb_maxi(0, skb_text_get_utf32_count(&end_paragraph_copy.text) - end_paragraph_copy_offset);

	skb_text_paragraph_t* last_text_paragraph = NULL;
	int32_t last_paragraph_offset = 0;

	if (source_paragraphs_count == 1) {
		assert(paragraph_range.start.paragraph_idx < rich_text->paragraphs_count);
		skb_text_paragraph_t* new_paragraph = &rich_text->paragraphs[paragraph_range.start.paragraph_idx];

		skb_attribute_set_t start_paragraph_attributes = {
			.attributes = (start_paragraph_copy_count > 0) ? start_paragraph_copy.attributes : end_paragraph_copy.attributes,
			.attributes_count = (start_paragraph_copy_count > 0) ? start_paragraph_copy.attributes_count : end_paragraph_copy.attributes_count,
		};
		skb__text_paragraph_init(rich_text, new_paragraph, start_paragraph_attributes);
		skb_text_append_range(&new_paragraph->text, &start_paragraph_copy.text, (skb_text_range_t){.start.offset = 0, .end.offset = start_paragraph_copy_count});
		skb_text_append_range(&new_paragraph->text, &source_paragraphs[0].text, (skb_text_range_t){.start.offset = source_range.start.text_offset, .end.offset = source_range.end.text_offset});
		skb_text_append_range(&new_paragraph->text, &end_paragraph_copy.text, (skb_text_range_t){.start.offset = end_paragraph_copy_offset, .end.offset = end_paragraph_copy_offset + end_paragraph_copy_count});

		// Keep track of last paragraph and last codepoint inserted for caret positioning.
		last_text_paragraph = new_paragraph;
		last_paragraph_offset = start_paragraph_copy_count + source_range.end.text_offset - source_range.start.text_offset - 1;
	} else if (source_paragraphs_count > 1) {
		int32_t source_paragraph_idx = 0;
		int32_t paragraph_idx = paragraph_range.start.paragraph_idx;

		// Start
		{
			skb_attribute_set_t start_paragraph_attributes = {
				.attributes = (start_paragraph_copy_count > 0) ? start_paragraph_copy.attributes : source_paragraphs[source_paragraph_idx].attributes,
				.attributes_count = (start_paragraph_copy_count > 0) ? start_paragraph_copy.attributes_count : source_paragraphs[source_paragraph_idx].attributes_count,
			};
			assert(paragraph_idx < rich_text->paragraphs_count);
			skb_text_paragraph_t* new_start_paragraph = &rich_text->paragraphs[paragraph_idx++];
			skb__text_paragraph_init(rich_text, new_start_paragraph, start_paragraph_attributes);
			skb_text_append_range(&new_start_paragraph->text, &start_paragraph_copy.text, (skb_text_range_t){.start.offset = 0, .end.offset = start_paragraph_copy_count});
			skb_text_append_range(&new_start_paragraph->text, &source_paragraphs[source_paragraph_idx].text, (skb_text_range_t){.start.offset = source_range.start.text_offset, .end.offset = skb_text_get_utf32_count(&source_paragraphs[source_paragraph_idx].text)});
			source_paragraph_idx++;
		}

		// Middle
		while (source_paragraph_idx < source_paragraphs_count - 1) {
			assert(paragraph_idx < rich_text->paragraphs_count);
			skb_text_paragraph_t* new_paragraph = &rich_text->paragraphs[paragraph_idx++];
			skb_attribute_set_t paragraph_attributes = {
				.attributes = source_paragraphs[source_paragraph_idx].attributes,
				.attributes_count = source_paragraphs[source_paragraph_idx].attributes_count,
			};
			skb__text_paragraph_init(rich_text, new_paragraph, paragraph_attributes);
			skb_text_append(&new_paragraph->text, &source_paragraphs[source_paragraph_idx].text);
			source_paragraph_idx++;
		}

		// End
		{
			assert(paragraph_idx < rich_text->paragraphs_count);
			skb_text_paragraph_t* new_end_paragraph = &rich_text->paragraphs[paragraph_idx++];
			skb_attribute_set_t end_paragraph_attributes = {
				.attributes = source_paragraphs[source_paragraph_idx].attributes,
				.attributes_count = source_paragraphs[source_paragraph_idx].attributes_count,
			};
			skb__text_paragraph_init(rich_text, new_end_paragraph, end_paragraph_attributes);
			skb_text_append_range(&new_end_paragraph->text, &source_paragraphs[source_paragraph_idx].text, (skb_text_range_t){.start.offset = 0, .end.offset = source_range.end.text_offset/* + 1*/});
			skb_text_append_range(&new_end_paragraph->text, &end_paragraph_copy.text, (skb_text_range_t){.start.offset = end_paragraph_copy_offset, .end.offset = end_paragraph_copy_offset + end_paragraph_copy_count});
			// Keep track of last paragraph and last codepoint inserted for caret positioning.
			last_text_paragraph = new_end_paragraph;
			last_paragraph_offset = source_range.end.text_offset - 1;
		}
	}

	// Update start offsets.
	int32_t global_text_offset = (paragraph_range.start.paragraph_idx > 0)
		                             ? (rich_text->paragraphs[paragraph_range.start.paragraph_idx - 1].global_text_offset + skb_text_get_utf32_count(&rich_text->paragraphs[paragraph_range.start.paragraph_idx - 1].text))
		                             : 0;

	for (int32_t i = paragraph_range.start.paragraph_idx; i < rich_text->paragraphs_count; i++) {
		rich_text->paragraphs[i].global_text_offset = global_text_offset;
		global_text_offset += skb_text_get_utf32_count(&rich_text->paragraphs[i].text);
	}

	// Free saved blocks.
	if (paragraph_range.start.paragraph_idx != paragraph_range.end.paragraph_idx)
		skb__text_paragraph_clear(&end_paragraph_copy);
	skb__text_paragraph_clear(&start_paragraph_copy);

	skb_rich_text_change_t change = {
		.start_paragraph_idx = paragraph_range.start.paragraph_idx,
		.removed_paragraph_count = removed_paragraphs_count,
		.inserted_paragraph_count = source_paragraphs_count,
	};

	// Note: The last_paragraph_offset may not perfectly align with a grapheme at this point. We will do the alignment on next layout update.
	if (last_paragraph_offset < 0) {
		// This can happen when we delete the first character.
		change.edit_end_position = (skb_text_position_t){
			.offset = last_text_paragraph->global_text_offset,
			.affinity = SKB_AFFINITY_TRAILING
		};
	} else {
		assert(last_text_paragraph);
		// We prefer to use leading edge of last grapheme so that the caret stays in context when typing at the direction change of a bidi text.
		change.edit_end_position = (skb_text_position_t){
			.offset = last_text_paragraph->global_text_offset + last_paragraph_offset,
			.affinity = SKB_AFFINITY_LEADING
		};
	}

	return change;
}

skb_rich_text_change_t skb_rich_text_insert(skb_rich_text_t* rich_text, skb_text_range_t text_range, const skb_rich_text_t* source_rich_text)
{
	assert(rich_text);

	skb_paragraph_range_t source_range = {
		.start = {.paragraph_idx = 0, .text_offset = 0},
		.end = {.paragraph_idx = 0, .text_offset = 0},
	};
	if (source_rich_text && source_rich_text->paragraphs_count > 0) {
		source_range.end.paragraph_idx = source_rich_text->paragraphs_count - 1;
		source_range.end.text_offset = skb_text_get_utf32_count(&source_rich_text->paragraphs[source_rich_text->paragraphs_count - 1].text);
	}

	// The skb__rich_text_replace expects valid paragraph, even if empty.
	const skb_text_paragraph_t empty_paragraph = {0};
	const skb_text_paragraph_t* source_paragraphs = &empty_paragraph;
	int32_t source_paragraphs_count = 1;
	if (source_rich_text && source_rich_text->paragraphs_count > 0) {
		source_paragraphs = source_rich_text->paragraphs;
		source_paragraphs_count = source_rich_text->paragraphs_count;
	}

	return skb__rich_text_replace(rich_text, text_range, source_paragraphs, source_paragraphs_count, source_range);
}

skb_rich_text_change_t skb_rich_text_insert_range(skb_rich_text_t* rich_text, skb_text_range_t text_range, const skb_rich_text_t* source_rich_text, skb_text_range_t source_text_range)
{
	const skb_paragraph_range_t source_range = skb_rich_text_get_paragraph_range_from_text_range(source_rich_text, source_text_range, SKB_AFFINITY_USE);

	// The skb__rich_text_replace expects valid paragraph, even if empty.
	const skb_text_paragraph_t empty_paragraph = {0};
	const skb_text_paragraph_t* source_paragraphs = &empty_paragraph;
	int32_t source_paragraphs_count = 1;
	if (source_rich_text && source_rich_text->paragraphs_count > 0) {
		source_paragraphs = source_rich_text->paragraphs;
		source_paragraphs_count = source_rich_text->paragraphs_count;
	}

	return skb__rich_text_replace(rich_text, text_range, source_paragraphs, source_paragraphs_count, source_range);
}

skb_rich_text_change_t skb_rich_text_remove(skb_rich_text_t* rich_text, skb_text_range_t text_range)
{
	const skb_text_paragraph_t empty_paragraph = {0};
	return skb__rich_text_replace(rich_text, text_range, &empty_paragraph, 1, (skb_paragraph_range_t){0});
}


void skb_rich_text_copy_attributes_in_range(skb_rich_text_t* rich_text, const skb_rich_text_t* source_rich_text, skb_text_range_t source_text_range)
{
	assert(rich_text);
	skb_rich_text_reset(rich_text);

	if (!source_rich_text)
		return;

	const skb_paragraph_range_t source_range = skb_rich_text_get_paragraph_range_from_text_range(source_rich_text, source_text_range, SKB_AFFINITY_USE);

	const skb_text_paragraph_t* source_paragraphs = source_rich_text->paragraphs + source_range.start.paragraph_idx;
	const int32_t source_paragraphs_count = skb_mini((source_range.end.paragraph_idx + 1) - source_range.start.paragraph_idx, source_rich_text->paragraphs_count);

	if (source_paragraphs_count == 0)
		return;

	SKB_ARRAY_RESERVE(rich_text->paragraphs, source_paragraphs_count);
	rich_text->paragraphs_count = source_paragraphs_count;

	int32_t paragraph_idx = 0;
	int32_t source_paragraph_idx = 0;

	if (source_paragraphs_count == 1) {
		assert(paragraph_idx < rich_text->paragraphs_count);
		skb_text_paragraph_t* new_paragraph = &rich_text->paragraphs[paragraph_idx];
		skb__text_paragraph_init(rich_text, new_paragraph, skb__text_paragraph_get_attributes(&source_paragraphs[source_paragraph_idx]));
		const skb_text_range_t source_copy_range = {
			.start.offset = source_range.start.text_offset,
			.end.offset = source_range.end.text_offset
		};
		skb_text_copy_attributes_range(&new_paragraph->text, &source_paragraphs[source_paragraph_idx].text, source_copy_range);

		new_paragraph->global_text_offset = 0;
	} else if (source_paragraphs_count > 1) {
		int32_t global_text_offset = 0;

		// Start
		{
			assert(paragraph_idx < rich_text->paragraphs_count);
			skb_text_paragraph_t* new_start_paragraph = &rich_text->paragraphs[paragraph_idx++];
			skb__text_paragraph_init(rich_text, new_start_paragraph, skb__text_paragraph_get_attributes(&source_paragraphs[source_paragraph_idx]));
			const skb_text_range_t source_copy_range = {
				.start.offset = source_range.start.text_offset,
				.end.offset = skb_text_get_utf32_count(&source_paragraphs[source_paragraph_idx].text)
			};
			skb_text_copy_attributes_range(&new_start_paragraph->text, &source_paragraphs[source_paragraph_idx].text, source_copy_range);
			new_start_paragraph->global_text_offset = global_text_offset;
			global_text_offset += source_copy_range.end.offset - source_copy_range.start.offset;
			source_paragraph_idx++;
		}

		// Middle
		while (source_paragraph_idx < source_paragraphs_count - 1) {
			assert(paragraph_idx < rich_text->paragraphs_count);
			skb_text_paragraph_t* new_paragraph = &rich_text->paragraphs[paragraph_idx++];
			skb__text_paragraph_init(rich_text, new_paragraph, skb__text_paragraph_get_attributes(&source_paragraphs[source_paragraph_idx]));
			const skb_text_range_t source_copy_range = {
				.start.offset = 0,
				.end.offset = skb_text_get_utf32_count(&source_paragraphs[source_paragraph_idx].text)
			};
			skb_text_copy_attributes_range(&new_paragraph->text, &source_paragraphs[source_paragraph_idx].text, source_copy_range);
			new_paragraph->global_text_offset = global_text_offset;
			global_text_offset += source_copy_range.end.offset - source_copy_range.start.offset;
			source_paragraph_idx++;
		}

		// End
		{
			assert(paragraph_idx < rich_text->paragraphs_count);
			skb_text_paragraph_t* new_end_paragraph = &rich_text->paragraphs[paragraph_idx++];
			skb__text_paragraph_init(rich_text, new_end_paragraph, skb__text_paragraph_get_attributes(&source_paragraphs[source_paragraph_idx]));
			const skb_text_range_t source_copy_range = {
				.start.offset = 0,
				.end.offset = source_range.end.text_offset
			};
			skb_text_copy_attributes_range(&new_end_paragraph->text, &source_paragraphs[source_paragraph_idx].text, source_copy_range);
			new_end_paragraph->global_text_offset = global_text_offset;
		}
	}
}

void skb_rich_text_insert_attributes(skb_rich_text_t* rich_text, skb_text_range_t text_range, const skb_rich_text_t* source_rich_text)
{
	assert(rich_text);
	if (!source_rich_text)
		return;

	const skb_paragraph_range_t paragraph_range = skb_rich_text_get_paragraph_range_from_text_range(rich_text, text_range, SKB_AFFINITY_USE);
	const int32_t insert_paragraph_count = (paragraph_range.end.paragraph_idx + 1) - paragraph_range.start.paragraph_idx;

	const skb_text_paragraph_t* source_paragraphs = source_rich_text->paragraphs;
	const int32_t source_paragraphs_count = skb_mini(source_rich_text->paragraphs_count, insert_paragraph_count);

	int32_t source_paragraph_idx = 0;
	int32_t paragraph_idx = paragraph_range.start.paragraph_idx;

	if (source_paragraphs_count == 1) {
		assert(paragraph_idx < rich_text->paragraphs_count);
		skb_text_paragraph_t* paragraph = &rich_text->paragraphs[paragraph_range.start.paragraph_idx];
		const skb_text_range_t insert_range = {
			.start.offset = paragraph_range.start.text_offset,
			.end.offset = paragraph_range.end.text_offset
		};
		skb_text_insert_attributes(&paragraph->text, insert_range, &source_paragraphs[source_paragraph_idx].text);
		skb__text_paragraph_copy_attributes(paragraph, skb__text_paragraph_get_attributes(&source_paragraphs[source_paragraph_idx]));
		paragraph->version = ++rich_text->version_counter; // Mark as changed
	} else if (source_paragraphs_count > 1) {
		// Start
		{
			assert(paragraph_idx < rich_text->paragraphs_count);
			skb_text_paragraph_t* start_paragraph = &rich_text->paragraphs[paragraph_idx++];
			const skb_text_range_t insert_range = {
				.start.offset = paragraph_range.start.text_offset,
				.end.offset = skb_text_get_utf32_count(&source_paragraphs[source_paragraph_idx].text)
			};
			skb_text_insert_attributes(&start_paragraph->text, insert_range, &source_paragraphs[source_paragraph_idx].text);
			skb__text_paragraph_copy_attributes(start_paragraph, skb__text_paragraph_get_attributes(&source_paragraphs[source_paragraph_idx]));
			start_paragraph->version = ++rich_text->version_counter; // Mark as changed
			source_paragraph_idx++;
		}

		// Middle
		while (source_paragraph_idx < source_paragraphs_count - 1) {
			assert(paragraph_idx < rich_text->paragraphs_count);
			skb_text_paragraph_t* paragraph = &rich_text->paragraphs[paragraph_idx++];
			const skb_text_range_t insert_range = {
				.start.offset = 0,
				.end.offset = skb_text_get_utf32_count(&source_paragraphs[source_paragraph_idx].text)
			};
			skb_text_insert_attributes(&paragraph->text, insert_range, &source_paragraphs[source_paragraph_idx].text);
			skb__text_paragraph_copy_attributes(paragraph, skb__text_paragraph_get_attributes(&source_paragraphs[source_paragraph_idx]));
			paragraph->version = ++rich_text->version_counter; // Mark as changed
			source_paragraph_idx++;
		}

		// End
		{
			assert(paragraph_idx < rich_text->paragraphs_count);
			skb_text_paragraph_t* end_paragraph = &rich_text->paragraphs[paragraph_idx++];
			const skb_text_range_t insert_range = {
				.start.offset = 0,
				.end.offset = paragraph_range.end.text_offset
			};
			skb_text_insert_attributes(&end_paragraph->text, insert_range, &source_paragraphs[source_paragraph_idx].text);
			skb__text_paragraph_copy_attributes(end_paragraph, skb__text_paragraph_get_attributes(&source_paragraphs[source_paragraph_idx]));
			end_paragraph->version = ++rich_text->version_counter; // Mark as changed
		}
	}
}

typedef struct skb__paragraph_attribute_context_t {
	skb_attribute_t attribute;
	uint8_t span_flags;
	int32_t count;
	skb_text_range_t text_range;
	const skb_data_blob_t* payload;
} skb__paragraph_attribute_context_t;

static bool skb__iter_set_paragraph_attribute(skb_rich_text_t* rich_text, int32_t paragraph_idx, skb_text_range_t text_range, void* context)
{
	skb__paragraph_attribute_context_t* ctx = context;
	skb_text_paragraph_t* text_paragraph = &rich_text->paragraphs[paragraph_idx];

	// Overwrite matching attributes.
	bool overwritten = false;
	for (int32_t i = 0; i < text_paragraph->attributes_count; i++) {
		if (skb_attributes_match(&ctx->attribute, &text_paragraph->attributes[i])) {
			if (!overwritten) {
				// Overwrite first.
				text_paragraph->attributes[i] = ctx->attribute;
				overwritten = true;
			} else {
				// Remove the rest.
				for (int32_t j = i + 1; j < text_paragraph->attributes_count; j++)
					text_paragraph->attributes[j - 1] = text_paragraph->attributes[j];
				text_paragraph->attributes_count--;
				i--;
			}
		}
	}

	if (!overwritten) {
		SKB_ARRAY_RESERVE(text_paragraph->attributes, text_paragraph->attributes_count+1);
		text_paragraph->attributes[text_paragraph->attributes_count] = ctx->attribute;
		text_paragraph->attributes_count++;
	}

	// Mark as changed
	text_paragraph->version = ++rich_text->version_counter;

	return true;
}

void skb_rich_text_set_paragraph_attribute(skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(rich_text);
	skb__paragraph_attribute_context_t ctx = {.attribute = attribute};
	skb__iterate_paragraphs(rich_text, text_range, skb__iter_set_paragraph_attribute, &ctx);
}

static bool skb__iter_set_paragraph_attribute_delta(skb_rich_text_t* rich_text, int32_t paragraph_idx, skb_text_range_t text_range, void* context)
{
	skb__paragraph_attribute_context_t* ctx = context;
	skb_text_paragraph_t* text_paragraph = &rich_text->paragraphs[paragraph_idx];

	// Modify matching attributes.
	bool modified = false;
	for (int32_t i = 0; i < text_paragraph->attributes_count; i++) {
		skb_attribute_t* attribute = &text_paragraph->attributes[i];
		if (attribute->kind == ctx->attribute.kind) {
			if (ctx->attribute.kind == SKB_ATTRIBUTE_INDENT_LEVEL) {
				attribute->indent_level.level = skb_maxi(0, attribute->indent_level.level + ctx->attribute.indent_level.level);
			} else {
				*attribute = ctx->attribute;
			}
			modified = true;
		}
	}

	if (!modified) {
		SKB_ARRAY_RESERVE(text_paragraph->attributes, text_paragraph->attributes_count+1);
		if (ctx->attribute.kind == SKB_ATTRIBUTE_INDENT_LEVEL) {
			text_paragraph->attributes[text_paragraph->attributes_count] = skb_attribute_make_indent_level(skb_maxi(0, ctx->attribute.indent_level.level));
		} else {
			text_paragraph->attributes[text_paragraph->attributes_count] = ctx->attribute;
		}
		text_paragraph->attributes_count++;
	}

	// Mark as changed
	text_paragraph->version = ++rich_text->version_counter;

	return true;
}

void skb_rich_text_set_paragraph_attribute_delta(skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(rich_text);
	skb__paragraph_attribute_context_t ctx = {.attribute = attribute};
	skb__iterate_paragraphs(rich_text, text_range, skb__iter_set_paragraph_attribute_delta, &ctx);
}

static bool skb__iter_set_attribute(skb_rich_text_t* rich_text, int32_t paragraph_idx, skb_text_range_t text_range, void* context)
{
	skb__paragraph_attribute_context_t* ctx = context;
	skb_text_paragraph_t* text_paragraph = &rich_text->paragraphs[paragraph_idx];
	skb_text_add_attribute_with_payload(&text_paragraph->text, text_range, ctx->attribute, ctx->span_flags, ctx->payload);

	// Mark as changed
	text_paragraph->version = ++rich_text->version_counter;

	return true;
}

void skb_rich_text_set_attribute(skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute)
{
	skb_rich_text_set_attribute_with_payload(rich_text, text_range, attribute, 0, NULL);
}

void skb_rich_text_set_attribute_with_payload(skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute, uint8_t span_flags, const skb_data_blob_t* payload)
{
	assert(rich_text);
	skb__paragraph_attribute_context_t ctx = {.attribute = attribute, .span_flags = span_flags, .payload = payload};
	skb__iterate_paragraphs(rich_text, text_range, skb__iter_set_attribute, &ctx);
}

static bool skb__iter_clear_attribute(skb_rich_text_t* rich_text, int32_t paragraph_idx, skb_text_range_t text_range, void* context)
{
	skb__paragraph_attribute_context_t* ctx = context;
	skb_text_paragraph_t* text_paragraph = &rich_text->paragraphs[paragraph_idx];
	skb_text_clear_attribute(&text_paragraph->text, text_range, ctx->attribute);

	// Mark as changed
	text_paragraph->version = ++rich_text->version_counter;

	return true;
}

void skb_rich_text_clear_attribute(skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(rich_text);
	skb__paragraph_attribute_context_t ctx = {.attribute = attribute};
	skb__iterate_paragraphs(rich_text, text_range, skb__iter_clear_attribute, &ctx);
}

static bool skb__iter_clear_all_attributes(skb_rich_text_t* rich_text, int32_t paragraph_idx, skb_text_range_t text_range, void* context)
{
	skb_text_paragraph_t* text_paragraph = &rich_text->paragraphs[paragraph_idx];
	skb_text_clear_all_attributes(&text_paragraph->text, text_range);

	// Mark as changed
	text_paragraph->version = ++rich_text->version_counter;

	return true;
}

void skb_rich_text_clear_all_attributes(skb_rich_text_t* rich_text, skb_text_range_t text_range)
{
	assert(rich_text);
	skb__iterate_paragraphs(rich_text, text_range, skb__iter_clear_all_attributes, NULL);
}

static bool skb__span_contains_range(const skb_attribute_span_t* attribute_span, skb_text_range_t text_range)
{
	return (text_range.start.offset >= attribute_span->text_range.start && text_range.start.offset < attribute_span->text_range.end)
			&& text_range.end.offset <= attribute_span->text_range.end;
}

static bool skb__iter_has_attribute(skb_rich_text_t* rich_text, int32_t paragraph_idx, skb_text_range_t text_range, void* context)
{
	skb__paragraph_attribute_context_t* ctx = context;
	const skb_text_paragraph_t* text_paragraph = &rich_text->paragraphs[paragraph_idx];
	const skb_attribute_span_t* attribute_spans = skb_text_get_attribute_spans(&text_paragraph->text);
	const int32_t attribute_spans_count = skb_text_get_attribute_spans_count(&text_paragraph->text);

	for (int32_t si = 0; si < attribute_spans_count; si++) {
		const skb_attribute_span_t* attribute_span = &attribute_spans[si];
		if (attribute_span->attribute.kind == ctx->attribute.kind && memcmp(&attribute_span->attribute, &ctx->attribute, sizeof(skb_attribute_t)) == 0) {
			if (skb__span_contains_range(attribute_span, text_range))
				ctx->count++;
		}
	}

	return true;
}

bool skb_rich_text_has_attribute(const skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(rich_text);
	skb__paragraph_attribute_context_t ctx = {.attribute = attribute, .count = 0};
	skb__iterate_paragraphs((skb_rich_text_t*)rich_text, text_range, skb__iter_has_attribute, &ctx);
	return ctx.count > 0;
}

typedef struct skb__get_attributes_context_t {
	uint32_t attribute_kind;
	int32_t attributes_cap;
	int32_t attributes_count;
	skb_attribute_t* attributes;
} skb__get_attributes_context_t;

static bool skb__iter_get_attribute(skb_rich_text_t* rich_text, int32_t paragraph_idx, skb_text_range_t text_range, void* context)
{
	skb__get_attributes_context_t* ctx = context;
	const skb_text_paragraph_t* text_paragraph = &rich_text->paragraphs[paragraph_idx];
	const skb_attribute_span_t* attribute_spans = skb_text_get_attribute_spans(&text_paragraph->text);
	const int32_t attribute_spans_count = skb_text_get_attribute_spans_count(&text_paragraph->text);

	for (int32_t si = 0; si < attribute_spans_count; si++) {
		const skb_attribute_span_t* attribute_span = &attribute_spans[si];
		if (attribute_span->attribute.kind == ctx->attribute_kind) {
			if (skb__span_contains_range(attribute_span, text_range)) {
				// Add unique
					bool found = false;
				for (int32_t i = 0; i < ctx->attributes_count; i++) {
					if (memcmp(&ctx->attributes[i], &attribute_span->attribute, sizeof(skb_attribute_t)) == 0) {
						found = true;
						break;;
					}
				}
				if (!found && ctx->attributes_count < ctx->attributes_cap)
					ctx->attributes[ctx->attributes_count++] = attribute_span->attribute;
			}
		}
	}

	return true;
}

int32_t skb_rich_text_get_attributes(const skb_rich_text_t* rich_text, skb_text_range_t text_range, uint32_t attribute_kind, skb_attribute_t* attributes, int32_t attributes_cap)
{
	assert(rich_text);
	skb__get_attributes_context_t ctx = {.attribute_kind = attribute_kind, .attributes = attributes, .attributes_cap = attributes_cap };
	skb__iterate_paragraphs((skb_rich_text_t*)rich_text, text_range, skb__iter_get_attribute, &ctx);
	return ctx.attributes_count;
}


static bool skb__iter_get_text_range(skb_rich_text_t* rich_text, int32_t paragraph_idx, skb_text_range_t text_range, void* context)
{
	skb__paragraph_attribute_context_t* ctx = context;
	const skb_text_paragraph_t* text_paragraph = &rich_text->paragraphs[paragraph_idx];
	const skb_attribute_span_t* attribute_spans = skb_text_get_attribute_spans(&text_paragraph->text);
	const int32_t attribute_spans_count = skb_text_get_attribute_spans_count(&text_paragraph->text);

	for (int32_t si = 0; si < attribute_spans_count; si++) {
		const skb_attribute_span_t* attribute_span = &attribute_spans[si];
		if (attribute_span->attribute.kind == ctx->attribute.kind && memcmp(&attribute_span->attribute, &ctx->attribute, sizeof(skb_attribute_t)) == 0) {
			if ((text_range.start.offset >= attribute_span->text_range.start && text_range.start.offset < attribute_span->text_range.end) && text_range.end.offset <= attribute_span->text_range.end) {
				ctx->text_range = (skb_text_range_t) {
					.start = { .offset = text_paragraph->global_text_offset + attribute_span->text_range.start },
					.end = { .offset = text_paragraph->global_text_offset + attribute_span->text_range.end },
				};
				ctx->count++;
				return false; // stop iterating
			}
		}
	}

	return true;
}

skb_text_range_t skb_rich_text_get_attribute_text_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(rich_text);
	skb__paragraph_attribute_context_t ctx = {.attribute = attribute, .count = 0};
	skb__iterate_paragraphs((skb_rich_text_t*)rich_text, text_range, skb__iter_get_text_range, &ctx);
	return ctx.count > 0 ? ctx.text_range : (skb_text_range_t){0};
}

static bool skb__iter_get_attribute_payload(skb_rich_text_t* rich_text, int32_t paragraph_idx, skb_text_range_t text_range, void* context)
{
	skb__paragraph_attribute_context_t* ctx = context;
	const skb_text_paragraph_t* text_paragraph = &rich_text->paragraphs[paragraph_idx];
	const skb_attribute_span_t* attribute_spans = skb_text_get_attribute_spans(&text_paragraph->text);
	const int32_t attribute_spans_count = skb_text_get_attribute_spans_count(&text_paragraph->text);

	for (int32_t si = 0; si < attribute_spans_count; si++) {
		const skb_attribute_span_t* attribute_span = &attribute_spans[si];
		if ((text_range.start.offset >= attribute_span->text_range.start && text_range.start.offset < attribute_span->text_range.end) && text_range.end.offset <= attribute_span->text_range.end) {
			if (attribute_span->attribute.kind == ctx->attribute.kind && memcmp(&attribute_span->attribute, &ctx->attribute, sizeof(skb_attribute_t)) == 0) {
				ctx->payload = attribute_span->payload;
				return false; // Stop iterating
			}
		}
	}

	return true;
}

skb_data_blob_t* skb_rich_text_get_attribute_payload(const skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(rich_text);
	skb__paragraph_attribute_context_t ctx = {.attribute = attribute };
	skb__iterate_paragraphs((skb_rich_text_t*)rich_text, text_range, skb__iter_get_attribute_payload, &ctx);
	return (skb_data_blob_t*)ctx.payload;
}


void skb_rich_text_remove_if(skb_rich_text_t* rich_text, skb_rich_text_remove_func_t* filter_func, void* context)
{
	assert(filter_func);

	for (int32_t pi = 0; pi < rich_text->paragraphs_count; pi++) {
		const uint32_t* utf32 = skb_text_get_utf32(&rich_text->paragraphs[pi].text);
		int32_t utf32_count = skb_text_get_utf32_count(&rich_text->paragraphs[pi].text);
		int32_t global_text_offset = rich_text->paragraphs[pi].global_text_offset;

		int32_t remove_start = SKB_INVALID_INDEX;

		for (int32_t i = 0; i < utf32_count; i++) {
			const bool should_remove = filter_func(utf32[i], pi, i, context);
			if (should_remove) {
				if (remove_start == SKB_INVALID_INDEX)
					remove_start = i;
			} else {
				if (remove_start != SKB_INVALID_INDEX) {
					skb_rich_text_remove(rich_text, (skb_text_range_t){.start.offset = global_text_offset + remove_start, .end.offset = global_text_offset + i});
					i = remove_start;
					// Refresh text after remove may have changed it.
					utf32 = skb_text_get_utf32(&rich_text->paragraphs[pi].text);
					utf32_count = skb_text_get_utf32_count(&rich_text->paragraphs[pi].text);
				}
				remove_start = SKB_INVALID_INDEX;
			}
		}

		if (remove_start != SKB_INVALID_INDEX) {
			skb_rich_text_change_t change = skb_rich_text_remove(rich_text, (skb_text_range_t){.start.offset = global_text_offset + remove_start, .end.offset = global_text_offset + utf32_count});
			// We removed the very end of the paragraph, the next paragraph will get merged to this one, so filter the paragraph again.
			if (change.removed_paragraph_count > change.inserted_paragraph_count)
				pi--;
		}
	}
}

skb_paragraph_position_t skb_rich_text_get_paragraph_position_from_text_position(const skb_rich_text_t* rich_text, skb_text_position_t text_pos, skb_affinity_usage_t affinity_usage)
{
	assert(rich_text);

	if (rich_text->paragraphs_count == 0 || text_pos.offset < 0)
		return (skb_paragraph_position_t){.paragraph_idx = 0, .text_offset = 0, .global_text_offset = 0};

	skb_paragraph_position_t result = {0};

	// Find paragraph.
	const int32_t last_paragraph_idx = rich_text->paragraphs_count - 1;
	const int32_t total_text_count = rich_text->paragraphs[last_paragraph_idx].global_text_offset + skb_text_get_utf32_count(&rich_text->paragraphs[last_paragraph_idx].text);
	if (text_pos.offset < 0) {
		result.paragraph_idx = 0;
	} else if (text_pos.offset >= total_text_count) {
		result.paragraph_idx = last_paragraph_idx;
	} else {
		result.paragraph_idx = skb_ub_search(text_pos.offset, &rich_text->paragraphs[0].global_text_offset, rich_text->paragraphs_count, sizeof(skb_text_paragraph_t));
	}

	// Adjust text position withing the paragraph.
	result.text_offset = text_pos.offset - rich_text->paragraphs[result.paragraph_idx].global_text_offset;
	// Align to nearest grapheme.
	result.text_offset = skb_text_align_grapheme_offset(&rich_text->paragraphs[result.paragraph_idx].text, result.text_offset);

	// Adjust position based on affinity
	if (affinity_usage == SKB_AFFINITY_USE) {
		if (text_pos.affinity == SKB_AFFINITY_LEADING || text_pos.affinity == SKB_AFFINITY_EOL) {
			result.text_offset = skb_text_get_next_grapheme_offset(&rich_text->paragraphs[result.paragraph_idx].text, result.text_offset);
			// Affinity adjustment may push the offset to next paragraph
			if (text_pos.affinity == SKB_AFFINITY_LEADING && result.text_offset >= skb_text_get_utf32_count(&rich_text->paragraphs[result.paragraph_idx].text)) {
				if ((result.paragraph_idx + 1) < rich_text->paragraphs_count) {
					result.text_offset = 0;
					result.paragraph_idx++;
				}
			}
		}
	}

	result.global_text_offset = rich_text->paragraphs[result.paragraph_idx].global_text_offset + result.text_offset;

	return result;
}

skb_paragraph_range_t skb_rich_text_get_paragraph_range_from_text_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_affinity_usage_t affinity_usage)
{
	skb_paragraph_position_t start_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.start, affinity_usage);
	skb_paragraph_position_t end_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.end, affinity_usage);

	skb_paragraph_range_t result = {0};
	if (skb_paragraph_position_less_or_equal(start_pos, end_pos)) {
		result.start = start_pos;
		result.end = end_pos;
	} else {
		result.start = end_pos;
		result.end = start_pos;
	}
	return result;
}

int32_t skb_rich_text_get_offset_from_text_position(const skb_rich_text_t* rich_text, skb_text_position_t text_pos)
{
	skb_paragraph_position_t pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_pos, SKB_AFFINITY_USE);
	return pos.global_text_offset;
}

skb_range_t skb_rich_text_get_offset_range_from_text_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range)
{
	skb_paragraph_position_t start_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.start, SKB_AFFINITY_USE);
	skb_paragraph_position_t end_pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_range.end, SKB_AFFINITY_USE);
	if (start_pos.global_text_offset > end_pos.global_text_offset)
		return (skb_range_t){.start = end_pos.global_text_offset, .end = start_pos.global_text_offset};
	return (skb_range_t){.start = start_pos.global_text_offset, .end = end_pos.global_text_offset};
}

skb_text_position_t skb_rich_text_get_next_grapheme_pos(const skb_rich_text_t* rich_text, skb_text_position_t text_pos)
{
	assert(rich_text);
	if (rich_text->paragraphs_count == 0 || text_pos.offset < 0)
		return (skb_text_position_t) {0};

	skb_paragraph_position_t pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_pos, SKB_AFFINITY_USE);
	const skb_text_paragraph_t* paragraph = &rich_text->paragraphs[pos.paragraph_idx];
	int32_t next_offset = skb_text_get_next_grapheme_offset(&paragraph->text, pos.text_offset);

	// Affinity adjustment may push the offset to next edit line
	if (next_offset >= skb_text_get_utf32_count(&paragraph->text)) {
		if ((pos.paragraph_idx + 1) < rich_text->paragraphs_count) {
			next_offset = 0;
			pos.paragraph_idx++;
			paragraph++;
		}
	}

	const int32_t text_count = skb_text_get_utf32_count(&paragraph->text);
	if (pos.paragraph_idx == rich_text->paragraphs_count-1 && next_offset >= text_count) {
		return (skb_text_position_t) {
			.offset = paragraph->global_text_offset + text_count - 1,
			.affinity = SKB_AFFINITY_EOL,
		};
	}

	return (skb_text_position_t) {
		.offset = paragraph->global_text_offset + next_offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
}

skb_text_position_t skb_rich_text_get_prev_grapheme_pos(const skb_rich_text_t* rich_text, skb_text_position_t text_pos)
{
	assert(rich_text);
	if (rich_text->paragraphs_count == 0 || text_pos.offset < 0)
		return (skb_text_position_t) {0};

	skb_paragraph_position_t pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_pos, SKB_AFFINITY_USE);
	const skb_text_paragraph_t* paragraph = &rich_text->paragraphs[pos.paragraph_idx];
	if (pos.text_offset == 0) {
		if ((pos.paragraph_idx - 1) >= 0) {
			pos.paragraph_idx--;
			paragraph--;
			pos.text_offset = skb_text_get_utf32_count(&paragraph->text);
		}
	}

	const int32_t prev_offset = skb_text_get_prev_grapheme_offset(&paragraph->text, pos.text_offset);

	if (pos.paragraph_idx == 0 && prev_offset <= 0) {
		return (skb_text_position_t) {
			.offset = 0,
			.affinity = SKB_AFFINITY_SOL,
		};
	}

	return (skb_text_position_t) {
		.offset = paragraph->global_text_offset + prev_offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
}

skb_text_position_t skb_rich_text_align_grapheme_pos(const skb_rich_text_t* rich_text, skb_text_position_t text_pos)
{
	assert(rich_text);
	if (rich_text->paragraphs_count == 0 || text_pos.offset < 0)
		return (skb_text_position_t) {0};

	skb_paragraph_position_t pos = skb_rich_text_get_paragraph_position_from_text_position(rich_text, text_pos, SKB_AFFINITY_USE);
	const skb_text_paragraph_t* paragraph = &rich_text->paragraphs[pos.paragraph_idx];
	const int32_t cur_offset = skb_text_align_grapheme_offset(&paragraph->text, pos.text_offset);

	if (pos.paragraph_idx == 0 && cur_offset <= 0) {
		return (skb_text_position_t) {
			.offset = 0,
			.affinity = SKB_AFFINITY_SOL,
		};
	}

	const int32_t text_count = skb_text_get_utf32_count(&paragraph->text);
	if (pos.paragraph_idx == rich_text->paragraphs_count-1 && cur_offset >= text_count) {
		return (skb_text_position_t) {
			.offset = paragraph->global_text_offset + text_count - 1,
			.affinity = SKB_AFFINITY_EOL,
		};
	}

	return (skb_text_position_t) {
		.offset = paragraph->global_text_offset + cur_offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
}
