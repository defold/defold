// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_RICH_LAYOUT_H
#define SKB_RICH_LAYOUT_H

#include <stdint.h>
#include "skb_common.h"
#include "skb_layout.h"
#include "skb_text.h"
#include "skb_rich_text.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup rich_layout Rich Layout
 *
 * Rich layout contains layout for multiple paragraphs of text. Each paragraph has separate layout (see skb_layout_t), which can be updated incrementally from Rich text (see skb_rich_text_t).
 *
 * @{
 */

/** Opaque type for the rich layout. Use skb_rich_layout_create() to create. */
typedef struct skb_rich_layout_t skb_rich_layout_t;

/**
 * Creates a new empty rich layout.
 * @return
 */
SKB_API skb_rich_layout_t* skb_rich_layout_create(void);

/**
 * Destroys and frees existing rich layout.
 * @param rich_layout rich layout to destroy
 */
SKB_API void skb_rich_layout_destroy(skb_rich_layout_t* rich_layout);

/**
 * Reset the rich layout to empty, keeping the allocated buffes.
 * @param rich_layout rich layout to reset
 */
SKB_API void skb_rich_layout_reset(skb_rich_layout_t* rich_layout);


/** @returns numner of paragraphs in the rich layout */
SKB_API int32_t skb_rich_layout_get_paragraphs_count(const skb_rich_layout_t* rich_layout);

/** @returns const pointer to the layout for specified paragraph. */
SKB_API const skb_layout_t* skb_rich_layout_get_layout(const skb_rich_layout_t* rich_layout, int32_t paragraph_idx);

/** @returns rendering offset of the layout of specified paragraph */
SKB_API skb_vec2_t skb_rich_layout_get_layout_offset(const skb_rich_layout_t* rich_layout, int32_t paragraph_idx);

/** @retruns the Y advance to next paragraphs of specified paragraph. */
SKB_API float skb_rich_layout_get_layout_advance_y(const skb_rich_layout_t* rich_layout, int32_t paragraph_idx);

/** @returns the text direction of specified paragraph */
SKB_API skb_text_direction_t skb_rich_layout_get_direction(const skb_rich_layout_t* rich_layout, int32_t paragraph_idx);

/** @returns layout parameters of rich layout. */
SKB_API const skb_layout_params_t* skb_rich_layout_get_params(const skb_rich_layout_t* rich_layout);

/** @returns the bounds of the whole rich layout. */
SKB_API skb_rect2_t skb_rich_layout_get_bounds(const skb_rich_layout_t* rich_layout);

/**
 * Updates the rich layout to from rich text.
 *
 * If updating after a change to the rich text, please call skb_rich_layout_apply_change() with the skb_rich_text_change_t from the modification function before updating the layout.
 * That will ensure that the paragraphs will match between rich text and rich layout and we relayout only the changed portion.
 *
 * The function will insert the composition text at the specified text location.
 * To remove the composition text, call update again with NULL composition text.
 * That will update just the affected paragraph.
 *
 * @param rich_layout rich layout to update
 * @param temp_alloc pointer temp allocator used during the update
 * @param params layout params to use for the whole layout
 * @param rich_text rich text to update from
 * @param composition_text_offset offset of the composition (IME) text, 0 if not used.
 * @param composition_text the composition text to insert at composition_text_offset, NULL if not used.
 */
SKB_API void skb_rich_layout_set_from_rich_text(
	skb_rich_layout_t* rich_layout, skb_temp_alloc_t* temp_alloc,
	const skb_layout_params_t* params, const skb_rich_text_t* rich_text,
	int32_t composition_text_offset, const skb_text_t* composition_text);

/**
 * Removes and inserts empty paragraphs based on change.
 * This function should be called before skb_rich_layout_set_from_rich_text() if skb_rich_text_change_t is available from rich text change.
 * That will ensure that the paragraphs will match between rich text and rich layout and we relayout only the changed portion.
 * @param rich_layout rich layout to update.
 * @param change change to apply.
 */
SKB_API void skb_rich_layout_apply_change(skb_rich_layout_t* rich_layout, skb_rich_text_change_t change);

/**
 * Returns caret info at the text position.
 * @param rich_layout rich layout to use
 * @param text_pos text postion to query
 * @return caret info at text position.
 */
SKB_API skb_caret_info_t skb_rich_layout_get_caret_info_at(const skb_rich_layout_t* rich_layout, skb_text_position_t text_pos);

/**
 * Iterates over set of bounding rectangles that represent the text range.
 * Due to bidirectional text the selection in logical order can span across multiple visual rectangles.
 * @param rich_layout rich layout to use.
 * @param text_range the text range to gets the rects for.
 * @param callback callback to call on each rectangle
 * @param context context passed to the callback.
 */
SKB_API void skb_rich_layout_get_text_range_bounds(const skb_rich_layout_t* rich_layout, skb_text_range_t text_range, skb_text_range_bounds_func_t* callback, void* context);

/**
 * Returns caret text position under the hit location.
 * First or last line is tested if the hit location is outside the vertical bounds.
 * Start or end of the line is returned if the hit location is outside the horizontal bounds.
 * @param rich_layout rich layout to use.
 * @param type type of interaction
 * @param hit_x hit X location
 * @param hit_y hit Y location
 * @return caret text position under the specified hit location.
 */
SKB_API skb_text_position_t skb_rich_layout_hit_test(const skb_rich_layout_t* rich_layout, skb_movement_type_t type, float hit_x, float hit_y);


/** @} */

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_RICH_LAYOUT_H
