// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_TEXT_H
#define SKB_TEXT_H

#include "skb_common.h"
#include "skb_attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup text Attributed Text
 *
 * The attributed text describes text in utf-32 format with spans of attributes.
 *
 * The attributes are stored in ordered array of spans. The spans of same type of attribute will split and merge as they are modified.
 *
 * @{
 */

enum {
	/** Maximum number of supported active/overlapping attributes at a run of text. */
	SKB_MAX_ACTIVE_ATTRIBUTES = 64,
};

typedef enum {
	/** The range of the reference should not include the end. This is used i.e. for links, so that typing right after the link will not expand the link. */
	SKB_ATTRIBUTE_SPAN_END_EXCLUSIVE = (1<<0),
	/** The start text position of the span is set as layout content id. */
	SKB_ATTRIBUTE_SPAN_TEXT_POSITION_TO_CONTENT_ID = (1<<1),
	/** The attribute is put to the top of the attribute set, when converted to layout, so that it's picked first. */
	SKB_ATTRIBUTE_SPAN_PRIORITY_HIGH = (1<<2),
	/** The attribute is put to the bottom of the attribute set, when converted to layout, so that it's picked last. */
	SKB_ATTRIBUTE_SPAN_PRIORITY_LOW = (1<<3),
} skb_attribute_span_flags_t;

/** Struct describing attribute applied to a span of text. */
typedef struct skb_attribute_span_t {
	/** Range of text the attribute is applied to. */
	skb_range_t text_range;
	/** The attribute to apply. */
	skb_attribute_t attribute;
	/** Falgs for the span, see skb_attribute_span_flags_t */
	uint8_t flags;
	/** Pointer to payload assigned to the span. */
	skb_data_blob_t* payload;
} skb_attribute_span_t;

/** Opaque type for the text. Use skb_text_create() to create. */
typedef struct skb_text_t skb_text_t;

/**
 * Creates empty attributed text. Use skb_text_destroy() to destroy.
 * @return pointer to the newly created text.
 */
SKB_API skb_text_t* skb_text_create(void);

/**
 * Creates empty attributed text that uses the temp allocator. Use skb_text_destroy() to destroy.
 * Note: the temp allocated text should be only used in very limited scope, e.g. as a text builder, which is disposed right after a layout is created.
 * @param temp_alloc temp alloc to use with the text.
 * @return pointer to the newly created text.
 */
SKB_API skb_text_t* skb_text_create_temp(skb_temp_alloc_t* temp_alloc);

/**
 * Destroys specified text.
 * @param text text to destroy
 */
SKB_API void skb_text_destroy(skb_text_t* text);

/**
 * Makes the text empty, but keeps the underlying buffers.
 * @param text text to reset.
 */
SKB_API void skb_text_reset(skb_text_t* text);

/**
 * Reserves memory in the text buffer for text and attributes. If larger buffer is already allocated, nothing changes.
 * @param text text to change
 * @param text_count reserved number of codepoints
 * @param spans_count reserved number of attribute spans.
 */
SKB_API void skb_text_reserve(skb_text_t* text, int32_t text_count, int32_t spans_count);

/** @return length of the text (utf-32 codeunits).  */
SKB_API int32_t skb_text_get_utf32_count(const skb_text_t* text);
/** @return const pointer to the utf-32 string. */
SKB_API const uint32_t * skb_text_get_utf32(const skb_text_t* text);
/** @return const pointer to the text property flags (current supports only grapheme breaks) */
SKB_API const uint8_t* skb_text_get_props(const skb_text_t* text);


/** @return number of attribute spans of the text. */
SKB_API int32_t skb_text_get_attribute_spans_count(const skb_text_t* text);
/** @return const pointer to the attribute spans of the text. */
SKB_API const skb_attribute_span_t* skb_text_get_attribute_spans(const skb_text_t* text);

/**
 * Get the start of the next grapheme in the layout based on text offset.
 * @param text text to use
 * @param text_offset offset (codepoints) in the text where to start looking.
 * @return offset (codepoints) to the start of the next grapheme.
 */
SKB_API int32_t skb_text_get_next_grapheme_offset(const skb_text_t* text, int32_t text_offset);

/**
 * Get the start of the previous grapheme in the layout based on text offset.
 * @param text text to use
 * @param text_offset offset (codepoints) in the text where to start looking.
 * @return offset (codepoints) to the start of the previous grapheme.
 */
SKB_API int32_t skb_text_get_prev_grapheme_offset(const skb_text_t* text, int32_t text_offset);

/**
 * Get the start of the current grapheme in the layout based on text offset.
 * @param text text to use
 * @param text_offset offset (codepoints) in the text where to start looking.
 * @return offset (codepoints) to the start of the current grapheme.
 */
SKB_API int32_t skb_text_align_grapheme_offset(const skb_text_t* text, int32_t text_offset);

/**
 * Returns the text offset (codepoint) from specific text position, taking affinity into account.
 * @param text text to use
 * @param pos position within the text.
 * @return text offset.
 */
SKB_API int32_t skb_text_get_offset_range_from_text_position(const skb_text_t* text, skb_text_position_t pos);

/** @returns range that is validated against the text. */
SKB_API skb_range_t skb_text_get_range_from_text_range(const skb_text_t* rich_text, skb_text_range_t text_range);

/**
 * Appends the contents from other text.
 * @param text pointer to the text to modify
 * @param text_from text to copy from.
 */
SKB_API void skb_text_append(skb_text_t* text, const skb_text_t* text_from);

/**
 * Appends range of contents from other text.
 * @param text pointer to the text to modify
 * @param source_text text to copy from.
 * @param source_text_range text range to copy (in utf-32 codepoints).
 */
SKB_API void skb_text_append_range(skb_text_t* text, const skb_text_t* source_text, skb_text_range_t source_text_range);

/**
 * Appends utf-8 string with attributes.
 * @param text pointer to the text to modify
 * @param utf8 pointer to a utf-8 string
 * @param utf8_count length of the utf-8 string, or -1 if the string is zero terminated.
 * @param attributes slice of attributes to apply to the appended text.
 */
SKB_API void skb_text_append_utf8(skb_text_t* text, const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes);

/**
 * Appends utf-8 string with attributes and payload.
 * @param text pointer to the text to modify
 * @param utf8 pointer to a utf-8 string
 * @param utf8_count length of the utf-8 string, or -1 if the string is zero terminated.
 * @param attributes slice of attributes to apply to the appended text.
 * @param span_flags span flags to apply for all the attributes, see skb_attribute_span_flags_t.
 * @param payload payload to apply for all the attributes.
 */
SKB_API void skb_text_append_utf8_with_payload(skb_text_t* text, const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload);

/**
 * Appends utf-32 string with attributes.
 * @param text pointer to the text to modify
 * @param utf32 pointer to a utf-32 string
 * @param utf32_count length of the utf-32 string, or -1 if the string is zero terminated.
 * @param attributes slice of attributes to apply to the appended text.
 */
SKB_API void skb_text_append_utf32(skb_text_t* text, const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes);

/**
 * Appends utf-32 string with attributes and payload.
 * @param text pointer to the text to modify
 * @param utf32 pointer to a utf-32 string
 * @param utf32_count length of the utf-32 string, or -1 if the string is zero terminated.
 * @param attributes slice of attributes to apply to the appended text.
 * @param span_flags span flags to apply for all the attributes, see skb_attribute_span_flags_t.
 * @param payload payload to apply for all the attributes.
 */
SKB_API void skb_text_append_utf32_with_payload(skb_text_t* text, const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload);

/**
 * Inserts text replacing the text range.
* @param text pointer to the text to modify
 * @param text_range text range to replace (in utf-32 codepoints)
 * @param source_text text to insert.
 */
SKB_API void skb_text_insert(skb_text_t* text, skb_text_range_t text_range, const skb_text_t* source_text);

/**
 * Inserts utf-8 string replacing the text range.
 * @param text pointer to the text to modify
 * @param text_range text range to replace (in utf-32 codepoints)
 * @param utf8 pointer to the utf-8 string to insert
 * @param utf8_count length of the utf-8 string, or -1 if the string is zero terminated.
 * @param attributes slice of attributes to apply to the inserted text.
 */
SKB_API void skb_text_insert_utf8(skb_text_t* text, skb_text_range_t text_range, const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes);

/**
 * Inserts utf-8 string and payload replacing the text range.
 * @param text pointer to the text to modify
 * @param text_range text range to replace (in utf-32 codepoints)
 * @param utf8 pointer to the utf-8 string to insert
 * @param utf8_count length of the utf-8 string, or -1 if the string is zero terminated.
 * @param attributes slice of attributes to apply to the inserted text.
 * @param span_flags span flags to apply for all the attributes, see skb_attribute_span_flags_t.
 * @param payload payload to apply for all the attributes.
 */
SKB_API void skb_text_insert_utf8_with_payload(skb_text_t* text, skb_text_range_t text_range, const char* utf8, int32_t utf8_count, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload);

/**
 * Inserts utf-32 string replacing the text range.
 * @param text pointer to the text to modify
 * @param text_range text range to replace (in utf-32 codepoints)
 * @param utf32 pointer to the utf-32 string to insert
 * @param utf32_count length of the utf-32 string, or -1 if the string is zero terminated.
 * @param attributes slice of attributes to apply to the inserted text.
 */
SKB_API void skb_text_insert_utf32(skb_text_t* text, skb_text_range_t text_range, const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes);

/**
 * Inserts utf-32 string and payload replacing the text range.
 * @param text pointer to the text to modify
 * @param text_range text range to replace (in utf-32 codepoints)
 * @param utf32 pointer to the utf-32 string to insert
 * @param utf32_count length of the utf-32 string, or -1 if the string is zero terminated.
 * @param attributes slice of attributes to apply to the inserted text.
 * @param span_flags span flags to apply for all the attributes, see skb_attribute_span_flags_t.
 * @param payload payload to apply for all the attributes.
 */
SKB_API void skb_text_insert_utf32_with_payload(skb_text_t* text, skb_text_range_t text_range, const uint32_t* utf32, int32_t utf32_count, skb_attribute_set_t attributes, uint8_t span_flags, const skb_data_blob_t* payload);

/**
 * Removes range of text.
 * @param text pointer to the text to modify
 * @param text_range range of text and attributes (in utf-32 codepoints) to remove.
 */
SKB_API void skb_text_remove(skb_text_t* text, skb_text_range_t text_range);

/**
 * Signature of remove_if predicate.
 * @param codepoint codepoint to filter
 * @param index index of the codepoint in the string
 * @param context pointer to context passed to skb_text_remove_if
 * @return true of the codepoint should be removed
 */
typedef bool skb_text_remove_func_t(uint32_t codepoint, int32_t index, void* context);

/**
 * Removes codepoints that do not pass the filter function
 * @param text text to change.
 * @param filter_func pointer to filter function.
 * @param context context pointer passed to filter functions.
 */
SKB_API void skb_text_remove_if(skb_text_t* text, skb_text_remove_func_t* filter_func, void* context);

/**
 * Tries to find utf-32 string value in the text, and returns the matching range, or empty if not found.
 * @param text text to query.
 * @param search_text_range search range withing the text.
 * @param value_utf32 pointer to the utf-32 string to find.
 * @param value_utf32_count length of the utf-32 string to find, or -1 is zero terminated.
 * @return range of text matching the specfied value, or empty range if value not found.
 */
SKB_API skb_text_range_t skb_text_find_reverse_utf32(const skb_text_t* text, skb_text_range_t search_text_range, const uint32_t* value_utf32, int32_t value_utf32_count);


/**
 * Copies attributes from from_text in specified range.
 * See: skb_text_replace_attributes()
 * @param text text to copy the attributes to (the text will be emptied).
 * @param from_text text to copy the attributes to.
 * @param from_text_range range on from_text where to copy the attributes.
 */
SKB_API void skb_text_copy_attributes_range(skb_text_t* text, const skb_text_t* from_text, skb_text_range_t from_text_range);

/**
 * Replaces attributes in text in specified range with attributes from from_text.
 * See skb_text_copy_attributes_range().
 * @param text pointer to the text to modify
 * @param text_range range of text to replace
 * @param from_text text to copy the properties from
 */
SKB_API void skb_text_insert_attributes(skb_text_t* text, skb_text_range_t text_range, const skb_text_t* from_text);

/**
 * Clears attribute of specific type from specified range.
 * @param text pointer to the text to modify
 * @param text_range text range of attributes to remove (in utf-32 codepoints)
 * @param attribute attribute to remove.
 */
SKB_API void skb_text_clear_attribute(skb_text_t* text, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Clears all attributes from specified range.
 * @param text pointer to the text to modify
 * @param text_range text range of attributes to remove (in utf-32 codepoints)
 */
SKB_API void skb_text_clear_all_attributes(skb_text_t* text, skb_text_range_t text_range);

/**
 * Add attribute to specified text range.
 * @param text pointer to the text to modify
 * @param text_range text range to apply the attributes for (in utf-32 codepoints)
 * @param attribute attribute to add
 */
SKB_API void skb_text_add_attribute(skb_text_t* text, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Add attribute and payload to specified text range.
 * @param text pointer to the text to modify
 * @param text_range text range to apply the attributes for (in utf-32 codepoints)
 * @param attribute attribute to add
 * @param span_flags span flags to apply for the attribute, see skb_attribute_span_flags_t.
 * @param payload payload to apply for the attribute.
 */
SKB_API void skb_text_add_attribute_with_payload(skb_text_t* text, skb_text_range_t text_range, skb_attribute_t attribute, uint8_t span_flags, const skb_data_blob_t* payload);

/**
 * Signature of attribute iterator callback.
 * @param text pointer to the text that is iterated.
 * @param text_range text range of the run of attributes.
 * @param active_spans array of pointers to active attributes for the range
 * @param active_spans_count number of active attributes
 * @param context context pointer passed to skb_text_iterate_attribute_runs().
 */
typedef void skb_attribute_run_iterator_func_t(const skb_text_t* text, skb_text_range_t text_range, skb_attribute_span_t** active_spans, int32_t active_spans_count, void* context);

/**
 * Iterate through combined attributes runs of the text, returning active attributes for given run.
 * @param text text to query.
 * @param callback function to call for each attribute run.
 * @param context context that is passed to the callback.
 */
SKB_API void skb_text_iterate_attribute_runs(const skb_text_t* text, skb_attribute_run_iterator_func_t* callback, void* context);

/** @} */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SKB_TEXT_H
