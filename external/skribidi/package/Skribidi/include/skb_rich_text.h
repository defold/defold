// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_RICH_TEXT_H
#define SKB_RICH_TEXT_H

#include <stdint.h>
#include <stdbool.h>
#include "skb_common.h"
#include "skb_attributes.h"
#include "skb_text.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup rich_text Rich Text
 *
 * Rich text contains multiple paragraphs of styled text.
 *
 * Each paragraph can be assigned it's own set of attributes that affect things like text alignment.
 * The text for each paragraph is represented as attributed text (see skb_text_t), where attributes can be assigned to ranges of text (like text weight or color).
 *
 * The rich text has API to add text us utf-8, but internally the text is represented as utf-32.
 *
 * Since rich text is represented as flat list of paragreaphs, it does not support some rich text styles which require hierarchical data.
 * Some common layouts, bullet or ordered lists are achieved by group tag attributes (skb_attribute_group_tag_t). If consecutive paragraphs have same
 * group tag, some styles will treat the group of paragraphs as one. This is applied for example for ordered list numbering, or allowing to set space before and after the group.
 *
 * You can create a rich layout from rich text for rendering and measuring (see skb_rich_layout_t).
 *
 * @{
 */

/** Opaque type for the rich text. Use skb_rich_text_create() to create. */
typedef struct skb_rich_text_t skb_rich_text_t;

/**
 * Struct describing how the paragraph composition changed after and edit. This struct is used when updating the rich text to richt text layout,
 * and allows to optimize which paragraphs need updating.
 */
typedef struct skb_rich_text_change_t {
	/** Index of the first paragraph that changed. */
	int32_t start_paragraph_idx;
	/** Number of paragraphs that were removed (at the start_paragraph_idx) */
	int32_t removed_paragraph_count;
	/** Number of paragraphs that were added (at the start_paragraph_idx) */
	int32_t inserted_paragraph_count;
	/** Text position of the end of the change. Can be used for caret position. */
	skb_text_position_t edit_end_position;
} skb_rich_text_change_t;

/**
 * Creates new empty rich text. Use skb_rich_text_destroy() to destroy.
 * @return pointer to new rich text
 */
SKB_API skb_rich_text_t* skb_rich_text_create(void);

/**
 * Destroys and frees provided rich text.
 * @param rich_text pointer to rich text to destroy.
 */
SKB_API void skb_rich_text_destroy(skb_rich_text_t* rich_text);

/**
 * Reset provided rich text to empty state, keeping allocated buffers.
 * @param rich_text pointer to rich text to reset.
 */
SKB_API void skb_rich_text_reset(skb_rich_text_t* rich_text);

/** @return number of utf-32 codepoints in the rich text. */
SKB_API int32_t skb_rich_text_get_utf32_count(const skb_rich_text_t* rich_text);

/** @return number of utf-8 codeunits in the rich text in given range. */
SKB_API int32_t skb_rich_text_get_utf8_count_in_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range);

/**
 * Returns text in given range as utf-8.
 * @param rich_text rich text to use
 * @param text_range text range (in utf-32 codepoints) to get
 * @param utf8 pointer to utf-8 string to store the result
 * @param utf8_cap capacity if the utf-8 result string
 * @return number of utf-8 codeunits written.
 */
SKB_API int32_t skb_rich_text_get_utf8_in_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range, char* utf8, int32_t utf8_cap);

/** @return number of utf-32 codepoints in the rich text in given range. */
SKB_API int32_t skb_rich_text_get_utf32_count_in_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range);

/**
 * Returns text in given range as utf-32.
 * @param rich_text rich text to use
 * @param text_range text range (in utf-32 codepoints) to get
 * @param utf32 pointer to utf-32 string to store the result
 * @param utf32_cap capacity if the utf-32 result string
 * @return number of utf-32 codepoints written.
 */
SKB_API int32_t skb_rich_text_get_utf32_in_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range, uint32_t* utf32, int32_t utf32_cap);

/** @returns the range of paragraphs the text range represents.*/
SKB_API skb_range_t skb_rich_text_get_paragraphs_range_from_text_range(skb_rich_text_t* rich_text, skb_text_range_t text_range);

/** @return number of paragraphs in the rich text. */
SKB_API int32_t skb_rich_text_get_paragraphs_count(const skb_rich_text_t* rich_text);

/** @return const pointer to the text of specified paragraph. */
SKB_API const skb_text_t* skb_rich_text_get_paragraph_text(const skb_rich_text_t* rich_text, int32_t paragraph_idx);

/** @return paragraph attributes associated with specified paragraph. */
SKB_API skb_attribute_set_t skb_rich_text_get_paragraph_attributes(const skb_rich_text_t* rich_text, int32_t paragraph_idx);

/** @return number of utf-32 codepoints in specified paragraph. */
SKB_API int32_t skb_rich_text_get_paragraph_text_utf32_count(const skb_rich_text_t* rich_text, int32_t paragraph_idx);

/** @return global text offset of specified paragraph. */
SKB_API int32_t skb_rich_text_get_paragraph_text_offset(const skb_rich_text_t* text, int32_t paragraph_idx);

/** @return version of the specified paragraph. The version is updated on each change can can used to externally react to changes. */
SKB_API uint32_t skb_rich_text_get_paragraph_version(const skb_rich_text_t* rich_text, int32_t paragraph_idx);


/**
 * Appends another rich text.
 * @param rich_text rich text to append to
 * @param source_rich_text source rich text to append
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_append(skb_rich_text_t* rich_text, const skb_rich_text_t* source_rich_text);

/**
 * Appends range of text from another rich text.
 * @param rich_text rich text to append to
 * @param source_rich_text source rich text to append
 * @param source_text_range range of text in source to append
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_append_range(skb_rich_text_t* rich_text, const skb_rich_text_t* source_rich_text, skb_text_range_t source_text_range);

/**
 * Appends new empty paragraph.
 * @param rich_text rich text to append to
 * @param paragraph_attributes attributes for the new paragraph.
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_append_paragraph(skb_rich_text_t* rich_text, skb_attribute_set_t paragraph_attributes);

/**
 * Appends text.
 * @param rich_text rich text to append to
 * @param temp_alloc temp alloc used during append.
 * @param source_text source text to append
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_append_text(skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc, const skb_text_t* source_text);

/**
 * Appends range of text from text.
 * @param rich_text rich text to append to
 * @param temp_alloc temp alloc used during append.
 * @param source_text source text to append
 * @param source_text_range range of text in source to append
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_append_text_range(
	skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc,
	const skb_text_t* source_text, skb_text_range_t source_text_range);

/**
 * Appends utf-8 string with attributes.
 * @param rich_text rich text to append to.
 * @param temp_alloc temp alloc used during append.
 * @param utf8 pointer to the utf-8 string.
 * @param utf8_count length of the utf-8 string, or -1 if zero terminated.
 * @param attributes attributes to apply for appended text.
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_append_utf8(
	skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc,
	const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes);

/**
 * Appends utf-8 string with attributes and payload.
 * @param rich_text rich text to append to.
 * @param temp_alloc temp alloc used during append.
 * @param utf8 pointer to the utf-8 string.
 * @param utf8_count length of the utf-8 string, or -1 if zero terminated.
 * @param attributes attributes to apply for appended text.
 * @param span_flags span flags to apply for the text, see skb_attribute_span_flags_t.
 * @param payload payload to attach to the text.
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_append_utf8_with_payload(
	skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc,
	const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes,
	uint8_t span_flags, const skb_data_blob_t* payload);

/**
 * Appends utf-32 string with attributes.
 * @param rich_text rich text to append to.
 * @param temp_alloc temp alloc used during append.
 * @param utf32 pointer to the utf-32 string.
 * @param utf32_count length of the utf-32 string, or -1 if zero terminated.
 * @param attributes attributes to apply for appended text.
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_append_utf32(
	skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc,
	const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes);

/**
 * Appends utf-32 string with attributes and payload.
 * @param rich_text rich text to append to.
 * @param temp_alloc temp alloc used during append.
 * @param utf32 pointer to the utf-32 string.
 * @param utf32_count length of the utf-32 string, or -1 if zero terminated.
 * @param attributes attributes to apply for appended text.
 * @param span_flags span flags to apply for the text, see skb_attribute_span_flags_t.
 * @param payload payload to attach to the text.
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_append_utf32_with_payload(
	skb_rich_text_t* rich_text, skb_temp_alloc_t* temp_alloc,
	const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes,
	uint8_t span_flags, const skb_data_blob_t* payload);

/**
 * Replaces text range with source rich text.
 * @param rich_text rich text to insert to
 * @param text_range range of text to replace
 * @param source_rich_text source text to insert.
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_insert(skb_rich_text_t* rich_text, skb_text_range_t text_range, const skb_rich_text_t* source_rich_text);

/**
 * Replaces specified text range with range of text in source rich text.
 * @param rich_text rich text to insert to
 * @param text_range range of text to replace
 * @param source_rich_text source text to insert.
 * @param source_text_range source text range to insert.
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_insert_range(skb_rich_text_t* rich_text, skb_text_range_t text_range, const skb_rich_text_t* source_rich_text, skb_text_range_t source_text_range);

/**
 * Removes specified range of text.
 * @param rich_text rich to remove from
 * @param text_range text range to remove
 * @return info about changed paragraphs.
 */
SKB_API skb_rich_text_change_t skb_rich_text_remove(skb_rich_text_t* rich_text, skb_text_range_t text_range);


/**
 * Resets the text and copies just the text attributes from source text.
 * Note: this is mainly used by the editor to capture attributes for undo.
 * @param rich_text rich text where to store the attributes
 * @param source_rich_text source rich text to copy from
 * @param source_text_range range of text to copy the attributes from
 */
SKB_API void skb_rich_text_copy_attributes_in_range(skb_rich_text_t* rich_text, const skb_rich_text_t* source_rich_text, skb_text_range_t source_text_range);

/**
 * Replaces attributes in give range with attributes from source text.
 * Does not affect the text length of the target rich text.
 * Note: this is mainly used by the editor to restore attributes for undo.
 * @param rich_text richt text to modify
 * @param text_range text range to insert the attributes to
 * @param source_rich_text source rich text to copy the attributes from
 */
SKB_API void skb_rich_text_insert_attributes(skb_rich_text_t* rich_text, skb_text_range_t text_range, const skb_rich_text_t* source_rich_text);


/**
 * Sets paragraph attributes for all the paragraphs that intersect the text range.
 * @param rich_text richt text to modify
 * @param text_range text range representing the paragraphs to modify
 * @param attribute attribute to add
 */
SKB_API void skb_rich_text_set_paragraph_attribute(skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Sets paragraph attributes delta for all the paragraphs that intersect the text range.
 * Note: currently only SKB_ATTRIBUTE_INDENT_LEVEL is supported for delta modifications, others will behave like skb_rich_text_set_paragraph_attribute()
 * @param rich_text richt text to modify
 * @param text_range text range representing the paragraphs to modify
 * @param attribute attribute delta to apply
 */
SKB_API void skb_rich_text_set_paragraph_attribute_delta(skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Sets text attribute for given text range,
 * @param rich_text richt text to modify.
 * @param text_range range of text to modify.
 * @param attribute attribute to set.
 */
SKB_API void skb_rich_text_set_attribute(skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Sets text attribute for given text range,
 * @param rich_text richt text to modify.
 * @param text_range range of text to modify.
 * @param attribute attribute to set.
 * @param span_flags span flags to apply for the text, see skb_attribute_span_flags_t.
 * @param payload payload to attach to the text range.
 */
SKB_API void skb_rich_text_set_attribute_with_payload(skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute, uint8_t span_flags, const skb_data_blob_t* payload);

/**
 * Clears text attribute for given text range.
 * @param rich_text richt text to modify.
 * @param text_range range of text to modify.
 * @param attribute attribute to remove.
 */
SKB_API void skb_rich_text_clear_attribute(skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Clears all attributes from given text range.
 * @param rich_text richt text to modify.
 * @param text_range range of text to modify.
 */
SKB_API void skb_rich_text_clear_all_attributes(skb_rich_text_t* rich_text, skb_text_range_t text_range);

/**
 * Returns if all of the specified range has the attribute.
 * @param rich_text rich text to check.
 * @param text_range range of text to match.
 * @param attribute attribute to match.
 * @return true if the whole specific range contains the attribute.
 */
SKB_API bool skb_rich_text_has_attribute(const skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Returns attributes that are applied to all of the specified range.
 * @param rich_text rich text to check.
 * @param text_range range of text to match.
 * @param attribute_kind tag of the attribute to find.
 * @param attributes pointer to the attribute array to store the results.
 * @param attributes_cap capacity of the results array.
 * @return number of attributes stored in attributes array.
 */
SKB_API int32_t skb_rich_text_get_attributes(const skb_rich_text_t* rich_text, skb_text_range_t text_range, uint32_t attribute_kind, skb_attribute_t* attributes, int32_t attributes_cap);

/**
 * Returns the whole of the attribute that is applied to specified range.
 * @param rich_text rich text to check
 * @param text_range range of text to match
 * @param attribute attribute to match.
 * @return text range the attribute is applied to or empty range if attribute not found.
 */
SKB_API skb_text_range_t skb_rich_text_get_attribute_text_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Returns the payload of the attribute that is applied to specified range.
 * @param rich_text rich text to check
 * @param text_range range of text to match
 * @param attribute attribute to match.
 * @return pointer to the payload applied to the attribute, or NULL if attribute not found.
 */
SKB_API skb_data_blob_t* skb_rich_text_get_attribute_payload(const skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Signature of text remove callback.
 * @param codepoint codepoint to consider
 * @param paragraph_idx paragraph index of the codepoint
 * @param text_offset text offset of the codepoint
 * @param context context pointer passed to skb_rich_text_remove_if
 * @return true of the codepoint should be removed.
 */
typedef bool skb_rich_text_remove_func_t(uint32_t codepoint, int32_t paragraph_idx, int32_t text_offset, void* context);

/**
 * Removes ranges of text that pass the filter function.
 * @param rich_text rich text to modify
 * @param filter_func filter function used as predicate to check if a codepoint should be removed.
 * @param context context pointer passed to the filter function.
 */
SKB_API void skb_rich_text_remove_if(skb_rich_text_t* rich_text, skb_rich_text_remove_func_t* filter_func, void* context);

/**
 * Returns paragraph position from text position.
 * @param rich_text rich text to use.
 * @param text_pos text position to convert.
 * @param affinity_usage if SKB_AFFINITY_USE the text affinity is applied when converting text position to text offset.
 * @return paragraph position corresponding the specified text position.
 */
SKB_API skb_paragraph_position_t skb_rich_text_get_paragraph_position_from_text_position(const skb_rich_text_t* rich_text, skb_text_position_t text_pos, skb_affinity_usage_t affinity_usage);

/**
 * Returns paragraph range from text range.
 * @param rich_text rich text to use
 * @param text_range text range to convert.
 * @param affinity_usage if SKB_AFFINITY_USE the text affinity is applied when converting text position to text offset.
 * @return paragraph range corresponding the specified text range.
 */
SKB_API skb_paragraph_range_t skb_rich_text_get_paragraph_range_from_text_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range, skb_affinity_usage_t affinity_usage);

/** @returns text offset from text position. */
SKB_API int32_t skb_rich_text_get_offset_from_text_position(const skb_rich_text_t* rich_text, skb_text_position_t text_pos);

/** @returns text offset range from text range. */
SKB_API skb_range_t skb_rich_text_get_offset_range_from_text_range(const skb_rich_text_t* rich_text, skb_text_range_t text_range);

/**
 * Get the start of the next grapheme in the rich text based on text position.
 * @param rich_text rich text to use
 * @param text_pos text position where to start looking.
 * @return text position of the start of the next grapheme.
 */
SKB_API skb_text_position_t skb_rich_text_get_next_grapheme_pos(const skb_rich_text_t* rich_text, skb_text_position_t text_pos);

/**
 * Get the start of the previous grapheme in the rich text based on text position.
 * @param rich_text rich text to use
 * @param text_pos text position where to start looking.
 * @return text position of the start of the previous grapheme.
 */
SKB_API skb_text_position_t skb_rich_text_get_prev_grapheme_pos(const skb_rich_text_t* rich_text, skb_text_position_t text_pos);

/**
 * Get the start of the current grapheme in the rich text based on text position.
 * @param rich_text rich text to use
 * @param text_pos text position where to start looking.
 * @return text position of the start of the current grapheme.
 */
SKB_API skb_text_position_t skb_rich_text_align_grapheme_pos(const skb_rich_text_t* rich_text, skb_text_position_t text_pos);

/** @} */

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_RICH_TEXT_H
