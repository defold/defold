// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "skb_common.h"
#include "skb_common_internal.h"
#include "skb_rich_layout.h"

#include <float.h>

#include "skb_rich_layout_internal.h"
#include "skb_rich_text.h"
#include "skb_layout.h"


static skb_paragraph_position_t skb__rich_layout_get_paragraph_position_from_text_position(const skb_rich_layout_t* rich_layout, skb_text_position_t text_pos, skb_affinity_usage_t affinity_usage)
{
	assert(rich_layout);

	skb_paragraph_position_t result = {0};

	// Find paragraph.
	const int32_t last_paragraph_idx = rich_layout->paragraphs_count - 1;
	const int32_t total_text_count = rich_layout->paragraphs[last_paragraph_idx].global_text_offset + skb_layout_get_text_count(&rich_layout->paragraphs[last_paragraph_idx].layout);
	if (text_pos.offset < 0) {
		result.paragraph_idx = 0;
	} else if (text_pos.offset >= total_text_count) {
		result.paragraph_idx = last_paragraph_idx;
	} else {
		result.paragraph_idx = skb_ub_search(text_pos.offset, &rich_layout->paragraphs[0].global_text_offset, rich_layout->paragraphs_count, sizeof(skb_layout_paragraph_t));
	}

	// Adjust text position withing the paragraph.
	result.text_offset = text_pos.offset - rich_layout->paragraphs[result.paragraph_idx].global_text_offset;
	// Align to nearest grapheme.
	result.text_offset = skb_layout_align_grapheme_offset(&rich_layout->paragraphs[result.paragraph_idx].layout, result.text_offset);

	// Adjust position based on affinity
	if (affinity_usage == SKB_AFFINITY_USE) {
		if (text_pos.affinity == SKB_AFFINITY_LEADING || text_pos.affinity == SKB_AFFINITY_EOL) {
			result.text_offset = skb_layout_get_next_grapheme_offset(&rich_layout->paragraphs[result.paragraph_idx].layout, result.text_offset);
			// Affinity adjustment may push the offset to next paragraph
			if (text_pos.affinity == SKB_AFFINITY_LEADING && result.text_offset >= skb_layout_get_text_count(&rich_layout->paragraphs[result.paragraph_idx].layout)) {
				if ((result.paragraph_idx + 1) < rich_layout->paragraphs_count) {
					result.text_offset = 0;
					result.paragraph_idx++;
				}
			}
		}
	}

	result.global_text_offset = rich_layout->paragraphs[result.paragraph_idx].global_text_offset + result.text_offset;

	return result;
}



static void skb__layout_paragraph_init(skb_layout_paragraph_t* layout_paragraph)
{
	SKB_ZERO_STRUCT(layout_paragraph);
	layout_paragraph->layout = skb_layout_make_empty();
}

static void skb__layout_paragraph_clear(skb_layout_paragraph_t* layout_paragraph)
{
	skb_layout_destroy(&layout_paragraph->layout);
	SKB_ZERO_STRUCT(layout_paragraph);
}

skb_rich_layout_t skb_rich_layout_make_empty(void)
{
	return (skb_rich_layout_t) {
		.should_free_instance = false,
	};
}

skb_rich_layout_t* skb_rich_layout_create(void)
{
	skb_rich_layout_t* rich_layout = skb_malloc(sizeof(skb_rich_layout_t));
	SKB_ZERO_STRUCT(rich_layout);
	rich_layout->should_free_instance = true;
	return rich_layout;
}

void skb_rich_layout_destroy(skb_rich_layout_t* rich_layout)
{
	if (!rich_layout) return;

	for (int32_t i = 0; i < rich_layout->paragraphs_count; i++)
		skb__layout_paragraph_clear(&rich_layout->paragraphs[i]);
	skb_free(rich_layout->paragraphs);

	skb_free(rich_layout->attributes);

	bool should_free_instance = rich_layout->should_free_instance;
	SKB_ZERO_STRUCT(rich_layout);

	if (should_free_instance)
		skb_free(rich_layout);
}

void skb_rich_layout_reset(skb_rich_layout_t* rich_layout)
{
	if (!rich_layout) return;

	rich_layout->bounds = (skb_rect2_t){0};

	SKB_ZERO_STRUCT(&rich_layout->params);
	rich_layout->params_hash = 0;

	rich_layout->attributes_count = 0;

	for (int32_t i = 0; i < rich_layout->paragraphs_count; i++)
		skb__layout_paragraph_clear(&rich_layout->paragraphs[i]);
	rich_layout->paragraphs_count = 0;
}

int32_t skb_rich_layout_get_paragraphs_count(const skb_rich_layout_t* rich_layout)
{
	assert(rich_layout);
	return rich_layout->paragraphs_count;
}

const skb_layout_paragraph_t* skb_rich_layout_get_paragraph(const skb_rich_layout_t* rich_layout, int32_t index)
{
	assert(rich_layout);
	assert(index >= 0 && index < rich_layout->paragraphs_count);
	return &rich_layout->paragraphs[index];
}

const skb_layout_t* skb_rich_layout_get_layout(const skb_rich_layout_t* rich_layout, int32_t paragraph_idx)
{
	assert(rich_layout);
	assert(paragraph_idx >= 0 && paragraph_idx < rich_layout->paragraphs_count);
	return &rich_layout->paragraphs[paragraph_idx].layout;
}

skb_vec2_t skb_rich_layout_get_layout_offset(const skb_rich_layout_t* rich_layout, int32_t paragraph_idx)
{
	assert(rich_layout);
	assert(paragraph_idx >= 0 && paragraph_idx < rich_layout->paragraphs_count);
	return rich_layout->paragraphs[paragraph_idx].offset;
}

float skb_rich_layout_get_layout_advance_y(const skb_rich_layout_t* rich_layout, int32_t paragraph_idx)
{
	assert(rich_layout);
	assert(paragraph_idx >= 0 && paragraph_idx < rich_layout->paragraphs_count);
	const skb_layout_t* layout = &rich_layout->paragraphs[paragraph_idx].layout;
	return layout ? skb_layout_get_advance_y(layout) : 0.f;
}

skb_text_direction_t skb_rich_layout_get_direction(const skb_rich_layout_t* rich_layout, int32_t paragraph_idx)
{
	assert(rich_layout);
	assert(paragraph_idx >= 0 && paragraph_idx < rich_layout->paragraphs_count);
	const skb_layout_t* layout = &rich_layout->paragraphs[paragraph_idx].layout;
	return layout ? skb_layout_get_resolved_direction(layout) : SKB_DIRECTION_LTR;
}

const skb_layout_params_t* skb_rich_layout_get_params(const skb_rich_layout_t* rich_layout)
{
	assert(rich_layout);
	return &rich_layout->params;
}

skb_rect2_t skb_rich_layout_get_bounds(const skb_rich_layout_t* rich_layout)
{
	assert(rich_layout);
	return rich_layout->bounds;
}

void skb_rich_layout_set_from_rich_text(
	skb_rich_layout_t* rich_layout, skb_temp_alloc_t* temp_alloc,
	const skb_layout_params_t* params, const skb_rich_text_t* rich_text,
	int32_t composition_text_offset, const skb_text_t* composition_text)
{
	// Make sure the paragraph counts are in sync. skb_rich_layout_update_with_change() can adjust the array so that
	// paragraphs that are changed in the middle will shift, so that existing paragraphs can be reused.
	const int32_t rich_text_paragraph_count =  skb_rich_text_get_paragraphs_count(rich_text);
	if (rich_text_paragraph_count < rich_layout->paragraphs_count) {
		for (int32_t i = rich_text_paragraph_count; i < rich_layout->paragraphs_count; i++)
			skb__layout_paragraph_clear(&rich_layout->paragraphs[i]);
		rich_layout->paragraphs_count = rich_text_paragraph_count;
	}
	if (rich_text_paragraph_count > rich_layout->paragraphs_count) {
		SKB_ARRAY_RESERVE(rich_layout->paragraphs, rich_text_paragraph_count);
		for (int32_t i = rich_layout->paragraphs_count; i < rich_text_paragraph_count; i++)
			skb__layout_paragraph_init(&rich_layout->paragraphs[i]);
		rich_layout->paragraphs_count = rich_text_paragraph_count;
	}

	// Copy parameters.
	uint64_t params_hash = skb_layout_params_hash_append(skb_hash64_empty(), params);
	bool rebuild_all = params_hash != rich_layout->params_hash; // If parameters have changed, we'll rebuild all.
	rich_layout->params_hash = params_hash;
	rich_layout->params = *params;
	rich_layout->params.layout_attributes = (skb_attribute_set_t){0};
	rich_layout->params.flags |= SKB_LAYOUT_PARAMS_IGNORE_MUST_LINE_BREAKS | SKB_LAYOUT_PARAMS_IGNORE_VERTICAL_ALIGN;

	const bool has_width_constraint = params->layout_width >= 0.f;
	const bool has_height_constraint = params->layout_height >= 0.f;

	rich_layout->attributes_count = skb_attributes_get_copy_flat_count(params->layout_attributes);
	if (rich_layout->attributes_count > 0) {
		SKB_ARRAY_RESERVE(rich_layout->attributes, rich_layout->attributes_count);
		skb_attributes_copy_flat(params->layout_attributes, rich_layout->attributes, rich_layout->attributes_count);
		rich_layout->params.layout_attributes = (skb_attribute_set_t) {
			.attributes = rich_layout->attributes,
			.attributes_count = rich_layout->attributes_count,
		};
	}

	skb_layout_params_t layout_params = rich_layout->params;

	if (!composition_text || skb_text_get_utf32_count(composition_text) == 0)
		composition_text_offset = SKB_INVALID_INDEX;

	float min_x = FLT_MAX;
	float max_x = -FLT_MAX;
	float max_y = 0.f;
	float start_y = 0.f;

	enum { SKB_MAX_COUNTER_LEVELS = 8 };
	int32_t marker_counters[SKB_MAX_COUNTER_LEVELS] = {0};

	uint32_t prev_group_tag = 0;
	uint32_t cur_group_tag = 0;

	// Init current group tag, as the loop will update the next (to avoid extra queries).
	if (rich_text_paragraph_count > 0) {
		skb_attribute_set_t paragraph_attributes = skb_rich_text_get_paragraph_attributes(rich_text, 0);
		paragraph_attributes.parent_set = &rich_layout->params.layout_attributes;
		cur_group_tag = skb_attributes_get_group(paragraph_attributes, rich_layout->params.attribute_collection);
	}

	const skb_text_overflow_t text_overflow = skb_attributes_get_text_overflow(rich_layout->params.layout_attributes, rich_layout->params.attribute_collection);
	bool is_truncated = false;

	for (int32_t i = 0; i < rich_text_paragraph_count; i++) {
		skb_layout_paragraph_t* layout_paragraph = &rich_layout->paragraphs[i];

		skb_attribute_set_t paragraph_attributes = skb_rich_text_get_paragraph_attributes(rich_text, i);
		paragraph_attributes.parent_set = &rich_layout->params.layout_attributes;
		layout_params.layout_attributes = paragraph_attributes;

		if (text_overflow != SKB_OVERFLOW_NONE && has_height_constraint)
			layout_params.layout_height = skb_maxf(0.f, rich_layout->params.layout_height - start_y);

		// Group tag
		uint32_t next_group_tag = 0;
		if (i+1 < rich_text_paragraph_count) {
			skb_attribute_set_t next_paragraph_attributes = skb_rich_text_get_paragraph_attributes(rich_text, i + 1);
			next_paragraph_attributes.parent_set = &rich_layout->params.layout_attributes;
			next_group_tag = skb_attributes_get_group(next_paragraph_attributes, rich_layout->params.attribute_collection);
		}
		SKB_SET_FLAG(layout_params.flags, SKB_LAYOUT_PARAMS_SAME_GROUP_BEFORE, cur_group_tag && prev_group_tag == cur_group_tag);
		SKB_SET_FLAG(layout_params.flags, SKB_LAYOUT_PARAMS_SAME_GROUP_AFTER, cur_group_tag && cur_group_tag == next_group_tag);

		// Update ordered list counters.
		const skb_attribute_list_marker_t list_marker = skb_attributes_get_list_marker(paragraph_attributes, rich_layout->params.attribute_collection);
		const int32_t indent_level = skb_clampi(skb_attributes_get_indent_level(paragraph_attributes, rich_layout->params.attribute_collection), 0, SKB_MAX_COUNTER_LEVELS - 1);
		const bool is_list_marker_counter = (list_marker.style == SKB_LIST_MARKER_COUNTER_DECIMAL || list_marker.style == SKB_LIST_MARKER_COUNTER_LOWER_LATIN || list_marker.style == SKB_LIST_MARKER_COUNTER_UPPER_LATIN);

		// Reset counters on deeper levels.
		for (int32_t ci = indent_level + 1; ci < SKB_MAX_COUNTER_LEVELS; ci++)
			marker_counters[ci] = 0;

		int32_t list_marker_counter = 0;
		if (is_list_marker_counter) {
			list_marker_counter = marker_counters[indent_level];
			marker_counters[indent_level]++;
		} else {
			layout_params.list_marker_counter = 0;
			marker_counters[indent_level] = 0;
		}
		layout_params.list_marker_counter = list_marker_counter;
		layout_params.text_content_id_base = layout_paragraph->global_text_offset;

		const skb_text_t* paragraph_text = skb_rich_text_get_paragraph_text(rich_text, i);
		const int32_t paragraph_text_count = skb_text_get_utf32_count(paragraph_text);
		const uint32_t paragraph_id = skb_rich_text_get_paragraph_version(rich_text, i);
		const int32_t global_text_offset = skb_rich_text_get_paragraph_text_offset(rich_text, i);
		const int32_t local_ime_text_offset = composition_text_offset - global_text_offset;

		layout_paragraph->global_text_offset = global_text_offset;

		if (!is_truncated) {
			if (local_ime_text_offset >= 0 && local_ime_text_offset < paragraph_text_count) {
				skb_temp_alloc_mark_t mark = skb_temp_alloc_save(temp_alloc);

				// Combine IME text with the line.
				skb_text_t* combined_text = skb_text_create_temp(temp_alloc);

				// Before
				skb_text_append_range(combined_text, paragraph_text, (skb_text_range_t){ .start.offset = 0, .end.offset = local_ime_text_offset });

				// Composition
				skb_text_append(combined_text, composition_text);

				// After
				skb_text_append_range(combined_text, paragraph_text, (skb_text_range_t){ .start.offset = local_ime_text_offset, .end.offset = paragraph_text_count });

				skb_layout_set_from_text(&layout_paragraph->layout, temp_alloc, &layout_params, combined_text, (skb_attribute_set_t){0});

				skb_text_destroy(combined_text);

				skb_temp_alloc_restore(temp_alloc, mark);

				// Reset ID so that when the IME state changes the paragraph will update.
				layout_paragraph->version = 0;
			} else {
				bool rebuild = rebuild_all;

				// If the contents has changed, rebuild.
				if (layout_paragraph->version != paragraph_id)
					rebuild = true;

				if (layout_paragraph->list_marker_counter != list_marker_counter)
					rebuild = true;

				// TODO: if just this changes we could optimize.
				if (layout_paragraph->group_flags != layout_params.flags)
					rebuild = true;

				if (rebuild) {
					skb_layout_set_from_text(&layout_paragraph->layout, temp_alloc, &layout_params, paragraph_text, (skb_attribute_set_t){0});
					layout_paragraph->version = paragraph_id;
					layout_paragraph->list_marker_counter = list_marker_counter;
					layout_paragraph->group_flags = layout_params.flags;
				}
			}
		} else {
			layout_paragraph->version = 0;
			skb_layout_reset(&layout_paragraph->layout);
		}

		const uint32_t layout_flags = skb_layout_get_flags(&layout_paragraph->layout);
		if (layout_flags & SKB_LAYOUT_IS_TRUNCATED) {
			if (skb_layout_get_lines_count(&layout_paragraph->layout) == 0) {
				// Truncate previous paragraph
				if (text_overflow == SKB_OVERFLOW_ELLIPSIS && i > 0) {
					skb_layout_paragraph_t* prev_layout_paragraph = &rich_layout->paragraphs[i-1];
					skb_layout_add_ellipsis_to_last_line(&prev_layout_paragraph->layout);
				}
				skb_layout_reset(&layout_paragraph->layout);
			}
			is_truncated = true;
		}

		const skb_rect2_t layout_bounds = skb_layout_get_bounds(&layout_paragraph->layout);
		const float layout_advance_y =  skb_layout_get_advance_y(&layout_paragraph->layout);

		layout_paragraph->offset.x = 0.f;
		layout_paragraph->offset.y = start_y;

		min_x = skb_minf(min_x, layout_bounds.x);
		max_x = skb_maxf(max_x, layout_bounds.x + layout_bounds.width);
		max_y = start_y + layout_bounds.y + layout_bounds.height;

		start_y += layout_advance_y;

		prev_group_tag = cur_group_tag;
		cur_group_tag = next_group_tag;
	}

	if (rich_text_paragraph_count > 0) {
		rich_layout->bounds.x = min_x;
		rich_layout->bounds.y = 0.f;
		rich_layout->bounds.width = max_x - min_x;
		rich_layout->bounds.height = max_y;
	} else {
		rich_layout->bounds = (skb_rect2_t){0};
	}

	// Vertical align
	if (has_height_constraint) {
		const skb_align_t vertical_align = skb_attributes_get_vertical_align(rich_layout->params.layout_attributes, rich_layout->params.attribute_collection);
		const float delta_y = skb_calc_align_offset(vertical_align, max_y, rich_layout->params.layout_height);
		if (skb_absf(delta_y) > 1e-6f) {
			for (int32_t i = 0; i < rich_text_paragraph_count; i++) {
				skb_layout_paragraph_t* layout_paragraph = &rich_layout->paragraphs[i];
				layout_paragraph->offset.y += delta_y;
			}
			rich_layout->bounds.y += delta_y;
		}
	}

	// Horizontal align
	if (!has_width_constraint) {
		// The layout did not have width constraint, so we need to align all the layouts to the max width of the content of all paragraphs.
		min_x = FLT_MAX;
		max_x = -FLT_MAX;

		const float container_width = has_width_constraint ? params->layout_width : rich_layout->bounds.width;
		for (int32_t i = 0; i < rich_text_paragraph_count; i++) {
			skb_layout_paragraph_t* layout_paragraph = &rich_layout->paragraphs[i];

			skb_attribute_set_t paragraph_attributes = skb_rich_text_get_paragraph_attributes(rich_text, i);
			paragraph_attributes.parent_set = &rich_layout->params.layout_attributes;
			const skb_align_t horizontal_align = skb_attributes_get_horizontal_align(paragraph_attributes, rich_layout->params.attribute_collection);

			skb_rect2_t layout_bounds = skb_layout_get_bounds(&layout_paragraph->layout);

			layout_paragraph->offset.x = skb_calc_align_offset(horizontal_align, layout_bounds.width, container_width);

			min_x = skb_minf(min_x, layout_paragraph->offset.x + layout_bounds.x);
			max_x = skb_maxf(max_x, layout_paragraph->offset.x + layout_bounds.x + layout_bounds.width);
		}

		if (rich_text_paragraph_count > 0) {
			rich_layout->bounds.x = min_x;
			rich_layout->bounds.width = max_x - min_x;
		}
	}
}

void skb_rich_layout_apply_change(skb_rich_layout_t* rich_layout, skb_rich_text_change_t change)
{
	if (change.removed_paragraph_count == 0 && change.inserted_paragraph_count == 0)
		return;

	// Allocate new lines or prune.
	const int32_t new_paragraphs_count = skb_maxi(0, rich_layout->paragraphs_count - change.removed_paragraph_count + change.inserted_paragraph_count);
	const int32_t old_paragraphs_count = rich_layout->paragraphs_count;
	SKB_ARRAY_RESERVE(rich_layout->paragraphs, new_paragraphs_count);
	rich_layout->paragraphs_count = new_paragraphs_count;

	// Free the paragraphs that will be removed
	for (int32_t i = change.start_paragraph_idx + change.inserted_paragraph_count; i < change.start_paragraph_idx + change.removed_paragraph_count; i++)
		skb__layout_paragraph_clear(&rich_layout->paragraphs[i]);

	// Move tail of the paragraphs to create space for the paragraphs to be inserted, accounting for the removed paragraphs.
	const int32_t old_tail_idx = change.start_paragraph_idx + change.removed_paragraph_count; // range_end is the last one to remove.
	const int32_t tail_count = old_paragraphs_count - old_tail_idx;
	const int32_t new_tail_idx = change.start_paragraph_idx + change.inserted_paragraph_count;
	if (new_tail_idx != old_tail_idx && tail_count > 0)
		memmove(rich_layout->paragraphs + new_tail_idx, rich_layout->paragraphs + old_tail_idx, tail_count * sizeof(skb_layout_paragraph_t));

	// Init the new paragraphs that were created
	for (int32_t i = change.start_paragraph_idx + change.removed_paragraph_count; i < change.start_paragraph_idx + change.inserted_paragraph_count; i++)
		skb__layout_paragraph_init(&rich_layout->paragraphs[i]);
}

skb_caret_info_t skb_rich_layout_get_caret_info_at(const skb_rich_layout_t* rich_layout, skb_text_position_t text_pos)
{
	assert(rich_layout);
	if (rich_layout->paragraphs_count == 0)
		return (skb_caret_info_t) {0};

	skb_paragraph_position_t paragraph_pos = skb__rich_layout_get_paragraph_position_from_text_position(rich_layout, text_pos, SKB_AFFINITY_IGNORE);
	const skb_layout_paragraph_t* paragraph = &rich_layout->paragraphs[paragraph_pos.paragraph_idx];

	text_pos.offset = paragraph_pos.text_offset;

	skb_caret_info_t caret = skb_layout_get_caret_info_at(&paragraph->layout, text_pos);
	caret.x += paragraph->offset.x;
	caret.y += paragraph->offset.y;

	return caret;
}

void skb_rich_layout_get_text_range_bounds(const skb_rich_layout_t* rich_layout, skb_text_range_t text_range, skb_text_range_bounds_func_t* callback, void* context)
{
	assert(rich_layout);

	skb_paragraph_position_t start_pos = skb__rich_layout_get_paragraph_position_from_text_position(rich_layout, text_range.start, SKB_AFFINITY_USE);
	skb_paragraph_position_t end_pos = skb__rich_layout_get_paragraph_position_from_text_position(rich_layout, text_range.end, SKB_AFFINITY_USE);
	if (!skb_paragraph_position_less_or_equal(start_pos, end_pos)) {
		skb_paragraph_position_t tmp = start_pos;
		start_pos = end_pos;
		end_pos = tmp;
	}

	if (start_pos.paragraph_idx == end_pos.paragraph_idx) {
		const skb_layout_paragraph_t* paragraph = &rich_layout->paragraphs[start_pos.paragraph_idx];
		skb_text_range_t line_sel = {
			.start = { .offset = start_pos.text_offset },
			.end = { .offset = end_pos.text_offset },
		};
		skb_layout_iterate_text_range_bounds_with_offset(&paragraph->layout, paragraph->offset, line_sel, callback, context);
		return;
	}

	// First paragraph
	const skb_layout_paragraph_t* first_paragraph = &rich_layout->paragraphs[start_pos.paragraph_idx];
	skb_text_range_t first_paragraph_sel = {
		.start = { .offset = start_pos.text_offset },
		.end = { .offset = skb_layout_get_text_count(&first_paragraph->layout) },
	};
	skb_layout_iterate_text_range_bounds_with_offset(&first_paragraph->layout, first_paragraph->offset, first_paragraph_sel, callback, context);

	// Middle paragraphs
	for (int32_t i = start_pos.paragraph_idx + 1; i < end_pos.paragraph_idx; i++) {
		const skb_layout_paragraph_t* paragraph = &rich_layout->paragraphs[i];
		skb_text_range_t line_sel = {
			.start = { .offset = 0 },
			.end = { .offset = skb_layout_get_text_count(&paragraph->layout) },
		};
		skb_layout_iterate_text_range_bounds_with_offset(&paragraph->layout, paragraph->offset, line_sel, callback, context);
	}

	// Last paragraph
	const skb_layout_paragraph_t* last_paragraph = &rich_layout->paragraphs[end_pos.paragraph_idx];
	skb_text_range_t last_paragraph_sel = {
		.start = { .offset = 0 },
		.end = { .offset = end_pos.text_offset },
	};
	skb_layout_iterate_text_range_bounds_with_offset(&last_paragraph->layout, last_paragraph->offset, last_paragraph_sel, callback, context);
}

skb_text_position_t skb_rich_layout_hit_test(const skb_rich_layout_t* rich_layout, skb_movement_type_t type, float hit_x, float hit_y)
{
	assert(rich_layout);
	if (rich_layout->paragraphs_count == 0)
		return (skb_text_position_t){0};

	int32_t hit_paragraph_idx = SKB_INVALID_INDEX;
	int32_t hit_line_idx = SKB_INVALID_INDEX;

	const int32_t last_paragraph_idx = rich_layout->paragraphs_count - 1;

	const skb_rect2_t first_paragraph_bounds = skb_layout_get_bounds(&rich_layout->paragraphs[0].layout);
	const skb_rect2_t last_paragraph_bounds = skb_layout_get_bounds(&rich_layout->paragraphs[last_paragraph_idx].layout);

	const float first_top_y = rich_layout->paragraphs[0].offset.y + first_paragraph_bounds.y;
	const float last_bot_y = rich_layout->paragraphs[last_paragraph_idx].offset.y + last_paragraph_bounds.y + last_paragraph_bounds.height;

	if (hit_y < first_top_y) {
		hit_paragraph_idx = 0;
		hit_line_idx = 0;
	} else if (hit_y >= last_bot_y) {
		hit_paragraph_idx = last_paragraph_idx;
		hit_line_idx = skb_layout_get_lines_count(&rich_layout->paragraphs[last_paragraph_idx].layout) - 1;
	} else {
		for (int32_t i = 0; i < rich_layout->paragraphs_count; i++) {
			const skb_layout_paragraph_t* paragraph = &rich_layout->paragraphs[i];
			const skb_layout_line_t* lines = skb_layout_get_lines(&paragraph->layout);
			const int32_t lines_count = skb_layout_get_lines_count(&paragraph->layout);
			for (int32_t j = 0; j < lines_count; j++) {
				const skb_layout_line_t* line = &lines[j];
				const float bot_y = paragraph->offset.y + line->bounds.y + -line->ascender + line->descender;
				if (hit_y < bot_y) {
					hit_line_idx = j;
					break;
				}
			}
			if (hit_line_idx != SKB_INVALID_INDEX) {
				hit_paragraph_idx = i;
				break;
			}
		}
		if (hit_line_idx == SKB_INVALID_INDEX) {
			hit_paragraph_idx = last_paragraph_idx;
			hit_line_idx = skb_layout_get_lines_count(&rich_layout->paragraphs[last_paragraph_idx].layout) - 1;
		}
	}

	assert(hit_paragraph_idx != SKB_INVALID_INDEX);

	const skb_layout_paragraph_t* hit_paragraph = &rich_layout->paragraphs[hit_paragraph_idx];
	skb_text_position_t pos = skb_layout_hit_test_at_line(&hit_paragraph->layout, type, hit_line_idx, hit_x - hit_paragraph->offset.x);
	pos.offset += hit_paragraph->global_text_offset;

	return pos;
}
