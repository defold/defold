// SPDX-License-Identifier: MIT

#include "skb_text.h"
#include "skb_common.h"
#include "skb_text_internal.h"

#include <assert.h>
#include <string.h>

#include "graphemebreak.h"
#include "skb_attribute_collection.h"

skb_text_t* skb_text_create(void)
{
	skb_text_t* result = skb_malloc(sizeof(skb_text_t));
	memset(result, 0, sizeof(skb_text_t));
	result->should_free_instance = true;
	return result;
}

skb_text_t* skb_text_create_temp(skb_temp_alloc_t* temp_alloc)
{
	skb_text_t* result = skb_temp_alloc_alloc(temp_alloc, sizeof(skb_text_t));
	assert(result);
	memset(result, 0, sizeof(skb_text_t));
	result->temp_alloc = temp_alloc;
	result->should_free_instance = true;
	return result;
}


skb_text_t skb_text_make_empty(void)
{
	return (skb_text_t){0};
}

void skb_text_destroy(skb_text_t* text)
{
	if (!text) return;

	skb_temp_alloc_t* temp_alloc = text->temp_alloc;
	if (temp_alloc) {
		for (int32_t i = 0; i < text->spans_count; i++)
			skb_data_blob_destroy(text->spans[i].payload);
		skb_temp_alloc_free(temp_alloc, text->spans);
		skb_temp_alloc_free(temp_alloc, text->text_props);
		skb_temp_alloc_free(temp_alloc, text->text);
	} else {
		for (int32_t i = 0; i < text->spans_count; i++)
			skb_data_blob_destroy(text->spans[i].payload);
		skb_free(text->text);
		skb_free(text->text_props);
		skb_free(text->spans);
	}

	bool should_free_instance = text->should_free_instance;
	SKB_ZERO_STRUCT(text);

	if (should_free_instance) {
		if (temp_alloc)
			skb_temp_alloc_free(temp_alloc, text);
		else
			skb_free(text);
	}
}

void skb_text_reset(skb_text_t* text)
{
	assert(text);
	text->text_count = 0;
	text->spans_count = 0;
}

static void skb__text_reserve(skb_text_t* text, int32_t text_count)
{
	if (text->temp_alloc) {
		if (text_count > text->text_cap) {
			const int32_t new_cap = skb_maxi(text_count, text->text_cap ? (text->text_cap + text->text_cap / 2) : 4);
			text->text = skb_temp_alloc_realloc(text->temp_alloc, text->text, sizeof(text->text[0]) * new_cap);
			text->text_props = skb_temp_alloc_realloc(text->temp_alloc, text->text_props, sizeof(text->text_props[0]) * new_cap);
			assert(text->text);
			memset(&text->text[text->text_cap], 0, sizeof((text->text)[0]) * (new_cap - text->text_cap));
			memset(&text->text_props[text->text_cap], 0, sizeof((text->text_props)[0]) * (new_cap - text->text_cap));
			text->text_cap = new_cap;
		}
	} else {
		if (text_count > text->text_cap) {
			const int32_t new_cap = skb_maxi(text_count, text->text_cap ? (text->text_cap + text->text_cap / 2) : 4);
			text->text = skb_realloc(text->text, sizeof(text->text[0]) * new_cap);
			text->text_props = skb_realloc(text->text_props, sizeof(text->text_props[0]) * new_cap);
			assert(text->text);
			memset(&text->text[text->text_cap], 0, sizeof((text->text)[0]) * (new_cap - text->text_cap));
			memset(&text->text_props[text->text_cap], 0, sizeof((text->text_props)[0]) * (new_cap - text->text_cap));
			text->text_cap = new_cap;
		}
	}
}

static void skb__spans_reserve(skb_text_t* text, int32_t spans_count)
{
	if (text->temp_alloc) {
		SKB_TEMP_RESERVE(text->temp_alloc, text->spans, spans_count);
	} else {
		SKB_ARRAY_RESERVE(text->spans, spans_count);
	}
}

void skb_text_reserve(skb_text_t* text, int32_t text_count, int32_t spans_count)
{
	assert(text);
	skb__text_reserve(text, text_count);
	skb__spans_reserve(text, spans_count);
}

int32_t skb_text_get_utf32_count(const skb_text_t* text)
{
	return text ? text->text_count : 0;
}

const uint32_t* skb_text_get_utf32(const skb_text_t* text)
{
	return text ? text->text : NULL;
}

const uint8_t* skb_text_get_props(const skb_text_t* text)
{
	return text ? text->text_props : NULL;
}

int32_t skb_text_get_attribute_spans_count(const skb_text_t* text)
{
	return text ? text->spans_count : 0;
}

const skb_attribute_span_t* skb_text_get_attribute_spans(const skb_text_t* text)
{
	return text ? text->spans : NULL;
}

int32_t skb_text_get_next_grapheme_offset(const skb_text_t* text, int32_t text_offset)
{
	text_offset = skb_clampi(text_offset, 0, text->text_count); // We allow one past the last codepoint as valid insertion point.

	// Find end of the current grapheme.
	while (text_offset < text->text_count && !(text->text_props[text_offset] & SKB_TEXT_PROP_GRAPHEME_BREAK))
		text_offset++;

	if (text_offset >= text->text_count)
		return text->text_count;

	// Step over.
	text_offset++;

	return text_offset;
}

int32_t skb_text_get_prev_grapheme_offset(const skb_text_t* text, int32_t text_offset)
{
	text_offset = skb_clampi(text_offset, 0, text->text_count); // We allow one past the last codepoint as valid insertion point.

	if (!text->text_count)
		return text_offset;

	// Find begining of the current grapheme.
	if (text->text_count) {
		while ((text_offset - 1) >= 0 && !(text->text_props[text_offset - 1] & SKB_TEXT_PROP_GRAPHEME_BREAK))
			text_offset--;
	}

	if (text_offset <= 0)
		return 0;

	// Step over.
	text_offset--;

	// Find beginning of the previous grapheme.
	while ((text_offset - 1) >= 0 && !(text->text_props[text_offset - 1] & SKB_TEXT_PROP_GRAPHEME_BREAK))
		text_offset--;

	return text_offset;
}


int32_t skb_text_align_grapheme_offset(const skb_text_t* text, int32_t text_offset)
{
	text_offset = skb_clampi(text_offset, 0, text->text_count); // We allow one past the last codepoint as valid insertion point.

	if (!text->text_count)
		return text_offset;

	// Find beginning of the current grapheme.
	while ((text_offset - 1) >= 0 && !(text->text_props[text_offset - 1] & SKB_TEXT_PROP_GRAPHEME_BREAK))
		text_offset--;

	if (text_offset <= 0)
		return 0;

	return text_offset;
}

int32_t skb_text_get_offset_range_from_text_position(const skb_text_t* text, skb_text_position_t pos)
{
	assert(text);
	if (pos.affinity == SKB_AFFINITY_LEADING || pos.affinity == SKB_AFFINITY_EOL)
		return skb_text_get_next_grapheme_offset(text, pos.offset);
	return skb_clampi(pos.offset, 0, text->text_count);
}

skb_range_t skb_text_get_range_from_text_range(const skb_text_t* text, skb_text_range_t text_range)
{
	assert(text);
	int32_t start_offset = skb_text_get_offset_range_from_text_position(text, text_range.start);
	int32_t end_offset = skb_text_get_offset_range_from_text_position(text, text_range.end);

	if (start_offset > end_offset)
		skb_swapi(&start_offset, &end_offset);

	return (skb_range_t) {
		.start = skb_clampi(start_offset, 0, text->text_count),
		.end = skb_clampi(end_offset, start_offset, text->text_count),
	};
}


static void skb__span_remove(skb_text_t* text, int32_t idx)
{
	assert(idx >= 0 && idx < text->spans_count);

	skb_data_blob_destroy(text->spans[idx].payload);

	for (int32_t i = idx; i < text->spans_count - 1; i++)
		text->spans[i] = text->spans[i + 1];
	text->spans_count--;
}

static int32_t skb__spans_lower_bound(const skb_text_t* text, int32_t start_idx, int32_t pos)
{
	int32_t low = start_idx;
	int32_t high = text->spans_count;
	while (low < high) {
		const int32_t mid = low + (high - low) / 2;
		if (text->spans[mid].text_range.start < pos)
			low = mid + 1;
		else
			high = mid;
	}
	return low;
}

static int32_t skb__span_insert(skb_text_t* text, skb_range_t text_range, skb_attribute_t attribute, uint8_t span_flags, const skb_data_blob_t* associated_data)
{
	assert(text);
	assert(text_range.start <= text_range.end);

	// Find location to insert at
	const int32_t idx = skb__spans_lower_bound(text, 0, text_range.start);

	skb__spans_reserve(text, text->spans_count + 1);
	text->spans_count++;

	for (int32_t i = text->spans_count - 1; i > idx; i--)
		text->spans[i] = text->spans[i - 1];

	text->spans[idx].text_range = text_range;
	text->spans[idx].attribute = attribute;
	text->spans[idx].flags = span_flags;

	if (text->temp_alloc)
		text->spans[idx].payload = skb_data_blob_duplicate_temp(associated_data, text->temp_alloc);
	else
		text->spans[idx].payload = skb_data_blob_duplicate(associated_data);

	return idx;
}


static int32_t skb__remove_from_active(skb_attribute_span_t** active_spans, int32_t active_spans_count, int32_t active_idx)
{
	// Remove, keep order.
	active_spans_count--;
	for (int32_t i = active_idx; i < active_spans_count; i++)
		active_spans[i] = active_spans[i + 1];
	return active_spans_count;
}

static int32_t skb__insert_to_active(skb_attribute_span_t** active_spans, int32_t active_spans_count, skb_attribute_span_t* span)
{
	assert(active_spans_count < SKB_MAX_ACTIVE_ATTRIBUTES);

	// Keep in order of first to expire first.
	const int32_t end = span->text_range.end;
	int32_t idx = 0;
	while (idx < active_spans_count) {
		if (end < active_spans[idx]->text_range.end)
			break;
		idx++;
	}

	active_spans_count++;

	// Make space
	for (int32_t j = active_spans_count - 1; j > idx; j--)
		active_spans[j] = active_spans[j - 1];
	active_spans[idx] = span;

	return active_spans_count;
}

static int32_t skb__find_active(skb_attribute_span_t** active_spans, int32_t active_spans_count, int32_t ends_at, const skb_attribute_t* attribute)
{
	for (int32_t i = 0; i < active_spans_count; i++) {
		if (active_spans[i]->text_range.end == ends_at && memcmp(&active_spans[i]->attribute, attribute, sizeof(skb_attribute_t)) == 0)
			return i;
	}
	return SKB_INVALID_INDEX;
}

static void skb__attributes_merge_adjacent(skb_text_t* text)
{
	assert(text);

	skb_attribute_span_t* active_spans[SKB_MAX_ACTIVE_ATTRIBUTES];
	int32_t active_spans_count = 0;

	int32_t span_idx = 0;
	while (span_idx < text->spans_count) {
		const int32_t pos = text->spans[span_idx].text_range.start;

		// Add new active spans that start at this event.
		while (span_idx < text->spans_count && text->spans[span_idx].text_range.start == pos) {
			// If a span of same type is adjacent to span of same type, merge.
			const int32_t adjacent_idx = skb__find_active(active_spans, active_spans_count, pos, &text->spans[span_idx].attribute);
			if (adjacent_idx != SKB_INVALID_INDEX) {
				// Merge, and remove current span.
				active_spans[adjacent_idx]->text_range.end = text->spans[span_idx].text_range.end;
				skb__span_remove(text, span_idx);
			} else {
				// Add, keep in order of first to expire first.
				active_spans_count = skb__insert_to_active(active_spans, active_spans_count, &text->spans[span_idx]);
				span_idx++;
			}
		}

		// Expire active spans
		for (int32_t i = 0; i < active_spans_count; i++) {
			if (active_spans[i]->text_range.end <= pos) {
				// Remove, keep order.
				active_spans_count = skb__remove_from_active(active_spans, active_spans_count, i);
				i--;
			}
		}
	}
}


// Clear removes attribute from specified range. Does not alter length.
static void skb__attributes_clear(skb_text_t* text, skb_range_t range, skb_attribute_t attribute)
{
	assert(text);

	if (range.start >= range.end)
		return;

	// Remove existing
	for (int32_t i = 0; i < text->spans_count; i++) {
		skb_attribute_span_t* span = &text->spans[i];

		if (attribute.kind != 0 && !skb_attributes_match(&span->attribute, &attribute))
			continue;

		// If clear completely range is before, skip.
		if (range.end <= span->text_range.start)
			continue;
		// If clear completely range is after, skip.
		if (range.start >= span->text_range.end)
			continue;

		if (range.start <= span->text_range.start) {
			if (range.end >= span->text_range.end) {
				// Text range completely covers the whole span, remove
				skb__span_remove(text, i);
				i--;
			} else {
				// Covers start partially, trim start, and move to new position.
				const int32_t new_idx = skb_mini(skb__spans_lower_bound(text, i, range.end), text->spans_count-1);
				skb_attribute_span_t moved_span = *span;
				moved_span.text_range.start = range.end;
				// Move items in between down.
				for (int32_t j = i; j < new_idx; j++)
					text->spans[j] = text->spans[j+1];
				// Place to new position.
				text->spans[new_idx] = moved_span;
			}
		} else {
			if (range.end >= span->text_range.end) {
				// Covers end partially, trim.
				span->text_range.end = range.start;
			} else {
				// Is inside the span, split.
				const skb_range_t tail_range = {range.end, span->text_range.end};
				// Trim head
				span->text_range.end = range.start;
				// Add tail
				skb__span_insert(text, tail_range, span->attribute, span->flags, span->payload);
			}
		}
	}
}

// Removes all attributes from 'range' and replaces it with empty segment.
void skb__attributes_replace_with_empty(skb_text_t* text, skb_range_t range, int32_t empty_count)
{
	assert(text);
	assert(empty_count >= 0);
	assert(range.start <= range.end);

	const int32_t offset = -(range.end - range.start) + empty_count;

	for (int32_t i = 0; i < text->spans_count; i++) {
		skb_attribute_span_t* span = &text->spans[i];

		// If text range is before, offset and skip.
		if (range.end <= span->text_range.start) {
			span->text_range.start += offset;
			span->text_range.end += offset;
			continue;
		}

		// If text range is after, skip.
		if (range.start >= span->text_range.end)
			continue;

		if (range.start <= span->text_range.start) {
			if (range.end >= span->text_range.end) {
				// Text range completely covers the whole span, remove
				skb__span_remove(text, i);
				i--;
			} else {
				// Covers start partially, trim.
				span->text_range.start = range.end + offset;
				span->text_range.end += offset;
			}
		} else {
			if (range.end >= span->text_range.end) {
				// Covers end partially, trim.
				span->text_range.end = range.start;
			} else {
				// Is inside the span, split.
				const skb_range_t tail_range = {range.end, span->text_range.end}; // No offset for the tail span, as the span is assumed to be placed somewhere after the current span.
				// Trim head
				span->text_range.end = range.start;
				// Add tail
				int32_t idx = skb__span_insert(text, tail_range, span->attribute, span->flags, span->payload);
				assert(idx > i); // we assume that the tail will come after this span, but not sure where.
			}
		}
	}
}

static void skb__insert_attributes(skb_text_t* text, skb_range_t range, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload)
{
	if (attributes.parent_set)
		skb__insert_attributes(text, range, *attributes.parent_set, span_flags, payload);

	if (attributes.set_handle)
		skb__span_insert(text, range, skb_attribute_make_reference(attributes.set_handle), span_flags, payload);

	for (int32_t i = 0; i < attributes.attributes_count; i++)
		skb__span_insert(text, range, attributes.attributes[i], span_flags, payload);
}

void skb__attributes_replace(skb_text_t* text, skb_range_t range, int32_t text_count, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload)
{
	skb__attributes_replace_with_empty(text, range, text_count);

	skb_range_t insert_range = {.start = range.start, .end = range.start + text_count};
	skb__insert_attributes(text, insert_range, attributes, span_flags, payload);

	skb__attributes_merge_adjacent(text);
}

static void skb__set_grapheme_breaks(const uint32_t* text, int32_t start_offset, int32_t text_count, uint8_t* text_props)
{
	if (!text_count) return;

	set_graphemebreaks_utf32(text + start_offset, text_count, NULL, (char*)text_props + start_offset);

	const int32_t end = start_offset + text_count;
	for (int32_t i = start_offset; i < end; i++) {
		const uint8_t prop = text_props[i];
		text_props[i] = prop == GRAPHEMEBREAK_BREAK ? SKB_TEXT_PROP_GRAPHEME_BREAK : 0;
	}
}

void skb_text_append(skb_text_t* text, const skb_text_t* text_from)
{
	assert(text);

	if (!text_from || !text_from->text_count)
		return;

	skb__text_reserve(text, text->text_count + text_from->text_count);
	const int32_t start_offset = text->text_count;

	// Copy text
	memcpy(text->text + start_offset, text_from->text, text_from->text_count * sizeof(uint32_t));
	skb__set_grapheme_breaks(text->text, start_offset, text_from->text_count, text->text_props);

	text->text_count += text_from->text_count;

	// Copy attributes
	if (text_from->spans_count > 0) {
		skb__spans_reserve(text, text->spans_count + text_from->spans_count);
		for (int32_t i = 0; i < text_from->spans_count; i++) {
			const skb_attribute_span_t* span = &text_from->spans[i];
			skb_range_t span_range = span->text_range;
			span_range.start += start_offset;
			span_range.end += start_offset;
			skb__span_insert(text, span_range, span->attribute, span->flags, span->payload);
		}
		skb__attributes_merge_adjacent(text);
	}
}

void skb_text_append_range(skb_text_t* text, const skb_text_t* source_text, skb_text_range_t source_text_range)
{
	assert(text);

	if (!source_text || !source_text->text_count)
		return;

	const skb_range_t from_range = skb_text_get_range_from_text_range(source_text, source_text_range);

	const int32_t copy_offset = from_range.start;
	const int32_t copy_count = from_range.end - from_range.start;

	if (copy_count <= 0)
		return;

	skb__text_reserve(text, text->text_count + copy_count);
	const int32_t start_offset = text->text_count;

	// Copy text
	memcpy(text->text + start_offset, source_text->text + copy_offset, copy_count * sizeof(uint32_t));
	skb__set_grapheme_breaks(text->text, start_offset, copy_count, text->text_props);
	text->text_count += copy_count;

	// Copy attributes
	if (source_text->spans_count > 0) {
		const int32_t span_offset = start_offset - copy_offset;
		for (int32_t i = 0; i < source_text->spans_count; i++) {
			const skb_attribute_span_t* span = &source_text->spans[i];
			skb_range_t span_range = {
				.start = skb_maxi(span->text_range.start, from_range.start) + span_offset,
				.end = skb_mini(span->text_range.end, from_range.end) + span_offset,
			};
			if (span_range.end > span_range.start)
				skb__span_insert(text, span_range, span->attribute, span->flags, span->payload);
		}
		skb__attributes_merge_adjacent(text);
	}
}

void skb_text_append_utf8(skb_text_t* text, const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes)
{
	skb_text_append_utf8_with_payload(text, utf8, utf8_count, attributes, 0, NULL);
}

void skb_text_append_utf8_with_payload(skb_text_t* text, const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload)
{
	assert(text);

	if (!utf8) return;
	if (utf8_count < 0) utf8_count = (int32_t)strlen(utf8);

	const int32_t utf32_count = skb_utf8_to_utf32_count(utf8, utf8_count);
	skb__text_reserve(text, text->text_count + utf32_count);

	const skb_range_t range = {
		.start = text->text_count,
		.end = text->text_count + utf32_count,
	};

	skb_utf8_to_utf32(utf8, utf8_count, text->text + text->text_count, utf32_count);
	skb__set_grapheme_breaks(text->text, text->text_count, utf32_count, text->text_props);
	text->text_count += utf32_count;

	skb__spans_reserve(text, text->spans_count + skb_attributes_get_copy_flat_count(attributes));
	skb__insert_attributes(text, range, attributes, span_flags, payload);
}

void skb_text_append_utf32(skb_text_t* text, const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes)
{
	skb_text_append_utf32_with_payload(text, utf32, utf32_count, attributes, 0, NULL);
}

void skb_text_append_utf32_with_payload(skb_text_t* text, const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload)
{
	assert(text);

	if (!utf32) return;
	if (utf32_count < 0) utf32_count = skb_utf32_strlen(utf32);

	skb__text_reserve(text, text->text_count + utf32_count);

	const skb_range_t range = {
		.start = text->text_count,
		.end = text->text_count + utf32_count,
	};

	memcpy(text->text + text->text_count, utf32, utf32_count * sizeof(uint32_t));
	skb__set_grapheme_breaks(text->text, text->text_count, utf32_count, text->text_props);
	text->text_count += utf32_count;

	skb__spans_reserve(text, text->spans_count + skb_attributes_get_copy_flat_count(attributes));
	skb__insert_attributes(text, range, attributes, span_flags, payload);
}

void skb_text_insert(skb_text_t* text, skb_text_range_t text_range, const skb_text_t* source_text)
{
	assert(text);

	const skb_range_t range = skb_text_get_range_from_text_range(text, text_range);

	const int32_t remove_count = range.end - range.start;

	const int32_t source_text_count = source_text ? source_text->text_count : 0;

	skb__text_reserve(text, text->text_count + source_text_count - remove_count);

	const int32_t start_offset = text->text_count;

	// Make space for the inserted text
	memmove(text->text + range.start + source_text_count, text->text + range.end, (text->text_count - range.end) * sizeof(uint32_t));

	// Copy
	if (source_text_count > 0) {
		memcpy(text->text + range.start, source_text->text, source_text_count * sizeof(uint32_t));
		skb__set_grapheme_breaks(text->text, range.start, source_text_count, text->text_props);
	}
	text->text_count += source_text_count - remove_count;

	// Make space for attributes.
	skb__attributes_replace_with_empty(text, range, text->text_count);

	// Insert existing spans
	if (source_text_count > 0) {
		skb__spans_reserve(text, text->spans_count + source_text->spans_count);
		for (int32_t i = 0; i < source_text->spans_count; i++) {
			const skb_attribute_span_t* span = &source_text->spans[i];
			skb_range_t span_range = source_text->spans[i].text_range;
			span_range.start += start_offset;
			span_range.end += start_offset;
			skb__span_insert(text, span_range, span->attribute, span->flags, span->payload);
		}
	}

	skb__attributes_merge_adjacent(text);
}

void skb_text_insert_utf8(skb_text_t* text, skb_text_range_t text_range, const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes)
{
	skb_text_insert_utf8_with_payload(text, text_range, utf8, utf8_count, attributes, 0, NULL);
}

void skb_text_insert_utf8_with_payload(skb_text_t* text, skb_text_range_t text_range, const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload)
{
	assert(text);

	const skb_range_t range = skb_text_get_range_from_text_range(text, text_range);

	const int32_t remove_count = range.end - range.start;

	if (!utf8) utf8_count = 0;
	if (utf8_count < 0) utf8_count = (int32_t)strlen(utf8);
	const int32_t utf32_count = skb_utf8_to_utf32_count(utf8, utf8_count);

	skb__text_reserve(text, text->text_count + utf32_count - remove_count);

	// Make space for the inserted text
	memmove(text->text + range.start + utf32_count, text->text + range.end, (text->text_count - range.end) * sizeof(uint32_t));

	// Copy
	if (utf32_count > 0) {
		skb_utf8_to_utf32(utf8, utf8_count, text->text + range.start, utf32_count);
		skb__set_grapheme_breaks(text->text, range.start, utf32_count, text->text_props);
	}
	text->text_count += utf32_count - remove_count;

	// Replace attributes
	skb__attributes_replace(text, range, utf32_count, attributes, span_flags, payload);
}

void skb_text_insert_utf32(skb_text_t* text, skb_text_range_t text_range, const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes)
{
	skb_text_insert_utf32_with_payload(text, text_range, utf32, utf32_count, attributes, 0, NULL);
}

void skb_text_insert_utf32_with_payload(skb_text_t* text, skb_text_range_t text_range, const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload)
{
	assert(text);

	const skb_range_t range = skb_text_get_range_from_text_range(text, text_range);

	const int32_t remove_count = range.end - range.start;

	if (!utf32) utf32_count = 0;
	if (utf32_count < 0) utf32_count = skb_utf32_strlen(utf32);

	skb__text_reserve(text, text->text_count + utf32_count - remove_count);

	// Make space for the inserted text
	memmove(text->text + range.start + utf32_count, text->text + range.end, (text->text_count - range.end) * sizeof(uint32_t));

	// Copy
	if (utf32_count > 0) {
		memcpy(text->text + range.start, utf32, utf32_count * sizeof(uint32_t));
		skb__set_grapheme_breaks(text->text, range.start, utf32_count, text->text_props);
	}
	text->text_count += utf32_count - remove_count;

	// Replace attributes
	skb__attributes_replace(text, range, utf32_count, attributes, span_flags, payload);
}

void skb_text_remove(skb_text_t* text, skb_text_range_t text_range)
{
	assert(text);

	const skb_range_t range = skb_text_get_range_from_text_range(text, text_range);

	if (range.end <= range.start) return;

	// Remove text
	memmove(text->text + range.start, text->text + range.end, (text->text_count - range.end) * sizeof(uint32_t));
	text->text_count -= range.end - range.start;

	// Remove attributes
	skb__attributes_replace(text, range, 0, (skb_attribute_set_t){0}, 0, NULL);
}

void skb_text_remove_if(skb_text_t* text, skb_text_remove_func_t* filter_func, void* context)
{
	assert(filter_func);

	int32_t remove_start = SKB_INVALID_INDEX;
	for (int32_t i = 0; i < text->text_count; i++) {
		const bool should_remove = filter_func(text->text[i], i, context);
		if (should_remove) {
			if (remove_start == SKB_INVALID_INDEX)
				remove_start = i;
		} else {
			if (remove_start != SKB_INVALID_INDEX) {
				skb_text_remove(text, (skb_text_range_t){.start.offset = remove_start, .end.offset = i});
				i = remove_start;
			}
			remove_start = SKB_INVALID_INDEX;
		}
	}

	if (remove_start != SKB_INVALID_INDEX)
		skb_text_remove(text, (skb_text_range_t){.start.offset = remove_start, .end.offset = text->text_count});
}

skb_text_range_t skb_text_find_reverse_utf32(const skb_text_t* text, skb_text_range_t search_text_range, const uint32_t* value_utf32, int32_t value_utf32_count)
{
	assert(text);

	const skb_range_t search_range = skb_text_get_range_from_text_range(text, search_text_range);

	if (value_utf32_count < 0)
		value_utf32_count = skb_utf32_strlen(value_utf32);

	uint32_t value_last = value_utf32[value_utf32_count - 1];
	int32_t text_offset = skb_clampi(search_range.end - 1, 0, text->text_count - 1); // Make sure the offset is in range.

	while (text_offset >= search_range.start) {
		if (text->text[text_offset] == value_last) {
			// Try to match the value
			int32_t end_text_offset = text_offset;
			int32_t value_offset = value_utf32_count - 1;
			while (text_offset >= 0 && value_offset >= 0 && value_utf32[value_offset] == text->text[text_offset]) {
				value_offset--;
				text_offset--;
			}

			if (value_offset == -1) {
				// Matched all codepoints in the value.
				return (skb_text_range_t){
					.start.offset = text_offset + 1,
					.end.offset = end_text_offset + 1,
				};
			}
		} else {
			text_offset--;
		}
	}

	return (skb_text_range_t){0};
}

void skb_text_copy_attributes_range(skb_text_t* text, const skb_text_t* from_text, skb_text_range_t from_text_range)
{
	assert(text);

	if (!from_text || !from_text->text_count)
		return;

	const skb_range_t from_range = skb_text_get_range_from_text_range(from_text, from_text_range);

	const int32_t copy_offset = from_range.start;
	const int32_t copy_count = from_range.end - from_range.start;

	if (copy_count <= 0)
		return;

	skb_text_reset(text);

	// Copy attributes
	if (from_text->spans_count > 0) {
		const int32_t span_offset = -copy_offset;
		for (int32_t i = 0; i < from_text->spans_count; i++) {
			const skb_attribute_span_t* span = &from_text->spans[i];
			const skb_range_t span_range = {
				.start = skb_maxi(span->text_range.start, from_range.start) + span_offset,
				.end = skb_mini(span->text_range.end, from_range.end) + span_offset,
			};
			if (span_range.end > span_range.start)
				skb__span_insert(text, span_range, span->attribute, span->flags, span->payload);
		}
		skb__attributes_merge_adjacent(text);
	}
}

void skb_text_insert_attributes(skb_text_t* text, skb_text_range_t text_range, const skb_text_t* from_text)
{
	assert(text);
	assert(from_text);

	const skb_range_t range = skb_text_get_range_from_text_range(text, text_range);
	if (skb_range_is_empty(range))
		return;

	// Clear existing attributes.
	skb__attributes_clear(text, range, (skb_attribute_t){0});

	// Insert existing spans
	skb__spans_reserve(text, text->spans_count + from_text->spans_count);
	for (int32_t i = 0; i < from_text->spans_count; i++) {
		const skb_attribute_span_t* span = &from_text->spans[i];
		const skb_range_t span_range = {
			.start = skb_maxi(span->text_range.start + range.start, range.start),
			.end = skb_mini(span->text_range.end + range.start, range.end),
		};
		if (span_range.end > span_range.start)
			skb__span_insert(text, span_range, span->attribute, span->flags, span->payload);
	}

	skb__attributes_merge_adjacent(text);
}

void skb_text_clear_attribute(skb_text_t* text, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(text);
	const skb_range_t range = skb_text_get_range_from_text_range(text, text_range);
	skb__attributes_clear(text, range, attribute);
}

void skb_text_clear_all_attributes(skb_text_t* text, skb_text_range_t text_range)
{
	assert(text);
	const skb_range_t range = skb_text_get_range_from_text_range(text, text_range);
	skb__attributes_clear(text, range, (skb_attribute_t){0});
}

void skb_text_add_attribute(skb_text_t* text, skb_text_range_t text_range, skb_attribute_t attribute)
{
	skb_text_add_attribute_with_payload(text, text_range, attribute, 0, NULL);
}

void skb_text_add_attribute_with_payload(skb_text_t* text, skb_text_range_t text_range, skb_attribute_t attribute, uint8_t span_flags, const skb_data_blob_t* payload)
{
	assert(text);

	const skb_range_t range = skb_text_get_range_from_text_range(text, text_range);

	skb__attributes_clear(text, range, attribute);
	skb__span_insert(text, range, attribute, span_flags, payload);
	skb__attributes_merge_adjacent(text);
}

void skb_text_iterate_attribute_runs(const skb_text_t* text, skb_attribute_run_iterator_func_t* callback, void* context)
{
	assert(text);
	assert(callback);

	skb_attribute_span_t* active_spans[SKB_MAX_ACTIVE_ATTRIBUTES];
	int32_t active_spans_count = 0;
	int32_t start_pos = 0;

	int32_t span_idx = 0;
	while (span_idx < text->spans_count) {
		const int32_t pos = text->spans[span_idx].text_range.start;

		// Expire active spans
		for (int32_t i = 0; i < active_spans_count; i++) {
			if (active_spans[i]->text_range.end <= pos) {
				const skb_text_range_t range = {.start.offset = start_pos, .end.offset = active_spans[i]->text_range.end};
				callback(text, range, active_spans, active_spans_count, context);
				start_pos = active_spans[i]->text_range.end;

				// Remove, keep order.
				active_spans_count = skb__remove_from_active(active_spans, active_spans_count, i);
				i--;
			}
		}

		if (start_pos < pos) {
			const skb_text_range_t range = {.start.offset = start_pos, .end.offset = pos};
			callback(text, range, active_spans, active_spans_count, context);
			start_pos = pos;
		}

		// Add new active spans that start at this event.
		while (span_idx < text->spans_count && text->spans[span_idx].text_range.start == pos) {
			// Add, keep in order of first to expire first.
			active_spans_count = skb__insert_to_active(active_spans, active_spans_count, &text->spans[span_idx]);
			span_idx++;
		}
	}

	// Expire remaining active spans
	for (int32_t i = 0; i < active_spans_count; i++) {
		int32_t pos = active_spans[i]->text_range.end;
		if (start_pos < pos) {
			const skb_text_range_t range = {.start.offset = start_pos, .end.offset = pos};
			callback(text, range, active_spans + i, active_spans_count - i, context);
			start_pos = pos;
		}
	}

	// The rest of the text, or if empty report at least one empty run.
	if (start_pos < text->text_count || text->text_count == 0) {
		const skb_text_range_t range = {.start.offset = start_pos, .end.offset = text->text_count};
		callback(text, range, NULL, 0, context);
	}
}
