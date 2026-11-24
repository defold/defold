// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_EDITOR_H
#define SKB_EDITOR_H

#include <stdint.h>
#include "skb_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup editor Text Editor
 * The Text editor provides the logic to handle text editing. It takes mouse movement and key presses as input and
 * modifies the text buffer.
 *
 * The text is internally stored as utf-32 (unicode codepoints), the text positions are also tracked as codepoints.
 * There are functions to get utf-8 version of the text out, and skb_utf8_codepoint_offset() can be used to covert
 * the text positions.
 *
 * In order to support partial updates, the text is split into paragraphs at paragraph break characters.
 * Each paragraph has its own layout, which may consist of multiple lines. Externally text positions are
 * tracked as if the text was one big buffer.
 *
 * An user interface with a lot of text fields can usually have just one text editor. Each text field is rendered
 * using a layout until the user focuses on the field, in which case the text editor is filled with the text, and takes over.
 *
 * @{
 */

// Forward declarations
typedef struct skb_rich_text_t skb_rich_text_t;
typedef struct skb_rich_layout_t skb_rich_layout_t;

/** Opaque type for the text editor. Use skb_editor_create() to create. */
typedef struct skb_editor_t skb_editor_t;

/** Specific text position value that is used in editor to describe current selection end (current caret location). */
#define SKB_CURRENT_SELECTION_END \
	SKB_LITERAL(skb_text_position_t) { .offset = INT32_MIN, .affinity = SKB_AFFINITY_NONE }

/** Specific text range value that is used in editor to describe current selection. */
#define SKB_CURRENT_SELECTION \
	SKB_LITERAL(skb_text_range_t) { .start = {.offset = INT32_MIN, .affinity = SKB_AFFINITY_NONE}, .end = {.offset = INT32_MIN, .affinity = SKB_AFFINITY_NONE} }

/** @return true if given text position is SKB_CURRENT_SELECTION_END. */
static inline bool skb_text_position_is_current_selection_end(skb_text_position_t pos)
{
	return pos.offset == INT32_MIN && pos.affinity == SKB_AFFINITY_NONE;
}

/** @return true if given text range is SKB_CURRENT_SELECTION. */
static inline bool skb_text_range_is_current_selection(skb_text_range_t text_range)
{
	return text_range.start.offset == INT32_MIN && text_range.start.affinity == SKB_AFFINITY_NONE
		&& text_range.end.offset == INT32_MIN && text_range.end.affinity == SKB_AFFINITY_NONE;
}

/** Enum describing text change reason. */
typedef enum {
	/** The editor text was reset to empty or set externally. */
	SKB_EDITOR_TEXT_RESET,
	/** The text is set externally via edit API */
	SKB_EDITOR_TEXT_EXTERNAL,
	/** The text is changed via typing */
	SKB_EDITOR_TEXT_EDIT,
	/** The change is attribute only */
	SKB_EDITOR_TEXT_ATTRIBUTE,
	/** The change is from undo or redo. */
	SKB_EDITOR_TEXT_UNDO,
} skb_editor_text_change_reason_t;

/**
 * Signature of editor text change function.
 * @param editor editor that changed.
 * @param reason reason why the text changed, see skb_editor_text_change_reason_t.
 * @param context context pointer that was passed to skb_editor_set_on_text_change_callback().
 */
typedef void skb_editor_on_text_change_func_t(skb_editor_t* editor, skb_editor_text_change_reason_t reason, void* context);

/** Enum describing text change reason. */
typedef enum {
	/** The editor text was reset to empty or set externally. */
	SKB_EDITOR_SELECTION_RESET,
	/** The selection is set externally via edit API */
	SKB_EDITOR_SELECTION_EXTERNAL,
	/** The selection is grown using mouse or keyboard. */
	SKB_EDITOR_SELECTION_GROW,
	/** The selection is moved (caret) using mouse or keyboard. */
	SKB_EDITOR_SELECTION_MOVE,
	/** The change is from text edit. */
	SKB_EDITOR_SELECTION_EDIT,
	/** The change is from undo or redo. */
	SKB_EDITOR_SELECTION_UNDO,
} skb_editor_selection_change_reason_t;

/**
 * Signature of editor selection change function.
 * @param editor editor that changed.
 * @param reason reason why the selection changed, see skb_editor_selection_change_reason_t.
 * @param context context pointer that was passed to skb_editor_set_on_selection_change_callback().
 */
typedef void skb_editor_on_selection_change_func_t(skb_editor_t* editor, skb_editor_selection_change_reason_t reason, void* context);

/**
 * Signature of input filter function.
 * The input filter is called when text is being input to the editor, but before it is actually placed.
 * The filter function can adjust the input_text as it sees fit.
 * Not called during undo, or when the editor text is reset using set text.
 * @param editor editor that changed.
 * @param input_text text that is being input (mutable).
 * @param text_range range of text that will be replaced with the input text.
 * @param context context pointer that was passed to skb_editor_set_input_filter_callback().
 */
typedef void skb_editor_input_filter_func_t(skb_editor_t* editor, skb_rich_text_t* input_text, skb_text_range_t text_range, void* context);

/** Enum describing the caret movement mode. */
typedef enum {
	/** Skribidi mode, the caret is move in logical order, but the caret makes extra stop when the writing direction
	 * changes to make it easier to place the caret at the start and end of the words. */
	SKB_CARET_MODE_SKRIBIDI = 0,
	/** Simple mode, similar to Windows, the caret moves in logical order, always one grapheme at a time. */
	SKB_CARET_MODE_SIMPLE,
} skb_editor_caret_mode_t;

/** Enum describing the behavior mode for editor operations. */
typedef enum {
	/** Default mode, standard behavior. */
	SKB_BEHAVIOR_DEFAULT = 0,
	/** MacOS mode, option+arrow and command+arrow follow MacOS text editing conventions. */
	SKB_BEHAVIOR_MACOS,
} skb_editor_behavior_t;

/** Struct describing parameters for the text editor. */
typedef struct skb_editor_params_t {
	/** Pointer to font collection to use. */
	skb_font_collection_t* font_collection;
	/** Pointer to the icon collection to use. */
	skb_icon_collection_t* icon_collection;
	/** Pointer to the attribute collection to use. */
	skb_attribute_collection_t* attribute_collection;
	/** Editor box width. Used for alignment, wrapping, and overflow (will be passed to layout width). Set to SKB_AUTO_SIZE, if the width should be unbounded. */
	float editor_width;
	/** Editor box height. Used for alignment, wrapping, and overflow (will be passed to layout height). Set to SKB_AUTO_SIZE, if the height should be unbounded. */
	float editor_height;
	/** Attributes to apply for the layout. Text attributes, and attributes from attributed text are added on top. */
	skb_attribute_set_t layout_attributes;
	/** Attributes to apply for all the text. */
	skb_attribute_set_t paragraph_attributes;
	/** Attributes added for the IME composition text. */
	skb_attribute_set_t composition_attributes;
	/** Care movement mode */
	skb_editor_caret_mode_t caret_mode;
	/** Behavior mode for editor operations (default vs macOS style). This includes how keyboard
	 * navigation works in the text editor. */
	skb_editor_behavior_t editor_behavior;
	/** Maximum number of undo levels, if zero, set to default undo levels, if < 0 undo is disabled. */
	int32_t max_undo_levels;
} skb_editor_params_t;

/** Keys handled by the editor */
typedef enum {
	SKB_KEY_NONE = 0,
	/** Left arrow key */
	SKB_KEY_LEFT,
	/** Right arrow key */
	SKB_KEY_RIGHT,
	/** Up arrow key */
	SKB_KEY_UP,
	/** down arrow key */
	SKB_KEY_DOWN,
	/** Home key */
	SKB_KEY_HOME,
	/** End key */
	SKB_KEY_END,
	/** Backspace key */
	SKB_KEY_BACKSPACE,
	/** Delete key */
	SKB_KEY_DELETE,
	/** Enter key */
	SKB_KEY_ENTER,
} skb_editor_key_t;

/** Key modifiers. */
typedef enum {
	SKB_MOD_NONE = 0,
	SKB_MOD_SHIFT = 0x01,
	SKB_MOD_CONTROL = 0x02,
	SKB_MOD_ALT = 0x04,
	SKB_MOD_OPTION = 0x08,
	SKB_MOD_COMMAND = 0x10,
} skb_editor_key_mod_t;

//
// General
//

/**
 * Creates new text editor.
 * @param params parameters for the editor.
 * @return newly create editor.
 */
SKB_API skb_editor_t* skb_editor_create(const skb_editor_params_t* params);

/**
 * Destroys a text editor.
 * @param editor pointer to the editor to destroy.
 */
SKB_API void skb_editor_destroy(skb_editor_t* editor);

/**
 * Sets text change callback function.
 * @param editor editor to change.
 * @param on_change_func pointer to the on change callback function
 * @param context context pointer that is passed to the callback function each time it is called.
 */
SKB_API void skb_editor_set_on_text_change_callback(skb_editor_t* editor, skb_editor_on_text_change_func_t* on_change_func, void* context);

/**
 * Sets change callback function.
 * @param editor editor to change.
 * @param on_change_func pointer to the on change callback function
 * @param context context pointer that is passed to the callback function each time it is called.
 */
SKB_API void skb_editor_set_on_selection_change_callback(skb_editor_t* editor, skb_editor_on_selection_change_func_t* on_change_func, void* context);

/**
 * Sets input filter function.
 * The function is called when text is input to the editor. The filter function can change the text as needed.
 * @param editor editor to change
 * @param filter_func pointer to the filter functions
 * @param context context pointer that is passed to the callback function each time it is called.
 */
SKB_API void skb_editor_set_input_filter_callback(skb_editor_t* editor, skb_editor_input_filter_func_t* filter_func, void* context);

/** @return the parameters used to create the editor. */
SKB_API const skb_editor_params_t* skb_editor_get_params(const skb_editor_t* editor);

/**
 * Resets text editor to empty state.
 * @param editor editor to change
 * @param params new parameters, or NULL if previous paramters should be kept.
 */
SKB_API void skb_editor_reset(skb_editor_t* editor, const skb_editor_params_t* params);

/**
 * Sets the text of the editor from an utf-8 string.
 * @param editor editor to change.
 * @param temp_alloc temp allocator used while setting layouting the text.
 * @param utf8 pointer to the utf-8 string to set.
 * @param utf8_len length of the string, or -1 if nul terminated.
 */
SKB_API void skb_editor_set_text_utf8(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const char* utf8, int32_t utf8_len);

/**
 * Sets the text of the editor from an utf-32 string.
 * @param editor editor to change.
 * @param temp_alloc temp allocator used while setting layouting the text.
 * @param utf32 pointer to the utf-32 string to set.
 * @param utf32_len length of the string, or -1 if nul terminated.
 */
SKB_API void skb_editor_set_text_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len);

/**
 * Sets the text of the editor from an rich text.
 * @param editor editor to change.
 * @param temp_alloc temp allocator used while setting layouting the text.
 * @param rich_text pointer to the rich text to set.
 */
SKB_API void skb_editor_set_rich_text(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_rich_text_t* rich_text);


//
// Text getters
//

/** @return length of the edited text as utf-8. */
SKB_API int32_t skb_editor_get_text_utf8_count(const skb_editor_t* editor);

/**
 * Gets the edited text as utf-8.
 * @param editor editor to query.
 * @param utf8 buffer where to store the text.
 * @param utf8_cap capacity of the buffer.
 * @return total length of the string (can be larger than buf_cap).
 */
SKB_API int32_t skb_editor_get_text_utf8(const skb_editor_t* editor, char* utf8, int32_t utf8_cap);

/** @return length of the edited text as utf-32. */
SKB_API int32_t skb_editor_get_text_utf32_count(const skb_editor_t* editor);

/**
 * Gets the edited text as utf-32.
 * @param editor editor to query.
 * @param utf32 buffer where to store the text.
 * @param utf32_cap capacity of the buffer.
 * @return total length of the string (can be larger than buf_cap).
 */
SKB_API int32_t skb_editor_get_text_utf32(const skb_editor_t* editor, uint32_t* utf32, int32_t utf32_cap);

/**
 * Gets const pointer to the edited rich text.
 * @param editor editor to query
 * @return const pointer to the rich text being edited.
 */
SKB_API const skb_rich_text_t* skb_editor_get_rich_text(const skb_editor_t* editor);

/** @return return the text length in utf-8 of specified text range. */
SKB_API int32_t skb_editor_get_text_utf8_count_in_range(const skb_editor_t* editor, skb_text_range_t text_range);

/**
 * Gets the text of the specified text range text as utf-8.
 * @param editor editor to query.
 * @param text_range range of text to get.
 * @param utf8 buffer where to store the selected text.
 * @param utf8_cap capacity of the buffer.
 * @return total length of the selected string (can be larger than buf_cap).
 */
SKB_API int32_t skb_editor_get_text_utf8_in_range(const skb_editor_t* editor, skb_text_range_t text_range, char* utf8, int32_t utf8_cap);

/** @return return the text length in utf-32 of specified selection. */
SKB_API int32_t skb_editor_get_text_utf32_count_in_range(const skb_editor_t* editor, skb_text_range_t text_range);

/**
 * Gets the text of the specified text range text as utf-32.
 * @param editor editor to query.
 * @param text_range range of text to get.
 * @param utf32 buffer where to store the selected text.
 * @param utf32_cap capacity of the buffer.
 * @return total length of the selected string (can be larger than buf_cap).
 */
SKB_API int32_t skb_editor_get_text_utf32_in_range(const skb_editor_t* editor, skb_text_range_t text_range, uint32_t* utf32, int32_t utf32_cap);

/**
 * Gets the rich text of the specified text range.
 * @param editor editor to query.
 * @param text_range range of text to get.
 * @param rich_text rich text where to store the selected text.
 */
SKB_API void skb_editor_get_rich_text_in_range(const skb_editor_t* editor, skb_text_range_t text_range, skb_rich_text_t* rich_text);


//
// Layout getters
//

/**
 * Returns the view offset of the editor.
 * When the layout text overflow attributes is set to SKB_OVERFLOW_SCROLL,
 * the caret movement will change the view offset to keep the caret visible.
 * Note: the view offset is always negative.
 * @param editor editor to query
 * @return view offset.
 */
SKB_API skb_vec2_t skb_editor_get_view_offset(const skb_editor_t* editor);

/**
 * Sets the view offset of the editor.
 * The view offset is clamped to always keep the content visible.
 * Note: the view offset is always negative.
 * @param editor editor to change
 * @param view_offset new view offset
 */
SKB_API void skb_editor_set_view_offset(skb_editor_t* editor, skb_vec2_t view_offset);

/** @return view bounds of the editor. */
SKB_API skb_rect2_t skb_editor_get_view_bounds(const skb_editor_t* editor);

/** @return the bounding box of the editor content layout. */
SKB_API skb_rect2_t skb_editor_get_layout_bounds(const skb_editor_t* editor);

/** @return const pointer to the rich layout of edited text. */
SKB_API const skb_rich_layout_t* skb_editor_get_rich_layout(const skb_editor_t* editor);

/** @return number of paragraphs in the editor. */
SKB_API int32_t skb_editor_get_paragraph_count(const skb_editor_t* editor);

/** @return const pointer to layout of specified paragraph. */
SKB_API const skb_layout_t* skb_editor_get_paragraph_layout(const skb_editor_t* editor, int32_t paragraph_idx);

/** @return y-offset of the specified paragraph. */
SKB_API skb_vec2_t skb_editor_get_paragraph_offset(const skb_editor_t* editor, int32_t paragraph_idx);

/** @return y-advance (advance to the start of next paragraph) of the specified paragraph. */
SKB_API float skb_editor_get_paragraph_advance_y(const skb_editor_t* editor, int32_t paragraph_idx);

/** @return const pointer to the text of the specified paragraph. */
SKB_API const skb_text_t* skb_editor_get_paragraph_text(const skb_editor_t* editor, int32_t paragraph_idx);

/** @return text count of specified paragraph. */
SKB_API int32_t skb_editor_get_paragraph_text_count(const skb_editor_t* editor, int32_t paragraph_idx);

/** @return text content count (without paragraph separator) of specified paragraph. */
SKB_API int32_t skb_editor_get_paragraph_text_content_count(const skb_editor_t* editor, int32_t paragraph_idx);

/** @return attribute set applied to specified paragraph. */
SKB_API skb_attribute_set_t skb_editor_get_paragraph_attributes(const skb_editor_t* editor, int32_t paragraph_idx);

/** @return global text offset of specified paragraph. */
SKB_API int32_t skb_editor_get_paragraph_global_text_offset(const skb_editor_t* editor, int32_t paragraph_idx);

/** @return text range of the paragraph text. */
SKB_API skb_text_range_t skb_editor_get_paragraph_text_range(const skb_editor_t* editor, int32_t paragraph_idx);

/** @return text range of the paragraph text content (excluding the paragraph separator). */
SKB_API skb_text_range_t skb_editor_get_paragraph_content_range(const skb_editor_t* editor, int32_t paragraph_idx);

/** @return text position of the first character of the paragraph. */
SKB_API skb_text_position_t skb_editor_get_paragraph_content_start_pos(const skb_editor_t* editor, int32_t paragraph_idx);

/** @return text position of the last character of the paragraph (excluding the paragraph separator).  */
SKB_API skb_text_position_t skb_editor_get_paragraph_content_end_pos(const skb_editor_t* editor, int32_t paragraph_idx);


//
// Text position
//

/** @return text offset of specified text position. */
SKB_API int32_t skb_editor_get_text_offset_from_text_position(const skb_editor_t* editor, skb_text_position_t text_pos);

/**
 * Returns paragraph position based on text position.
 * Paragraph position contains more information how the text position relates to the paragraph at the text position.
 * @param editor editor to query.
 * @param text_pos the text position to convert
 * @return paragraph position info of the specified text position.
 */
SKB_API skb_paragraph_position_t skb_editor_get_paragraph_position_from_text_position(const skb_editor_t* editor, skb_text_position_t text_pos);

/**
 * Returns the range of paragraphs the text range overlaps.
 * @param editor editor to qeury
 * @param text_range text range
 * @return range of paragraphs the text range overlaps.
 */
SKB_API skb_range_t skb_editor_get_paragraphs_range_from_text_range(const skb_editor_t* editor, skb_text_range_t text_range);

/**
 * Returns validated offset range of specified text range.
 * @param editor editor to query.
 * @param text_range text range.
 * @return validated text range.
 */
SKB_API skb_range_t skb_editor_get_offset_range_from_text_range(const skb_editor_t* editor, skb_text_range_t text_range);

/**
 * Returns number of codepoints in the text range.
 * @param editor editor to query.
 * @param text_range text range
 * @return number of codepoints in the text range.
 */
SKB_API int32_t skb_editor_get_text_range_count(const skb_editor_t* editor, skb_text_range_t text_range);


//
// Selection
//

/** @return current selection of the editor. */
SKB_API skb_text_range_t skb_editor_get_current_selection(const skb_editor_t* editor);

/**
 * Sets the current selection of the editor to specific range.
 * @param editor editor to change.
 * @param text_range new selection.
 */
SKB_API void skb_editor_select(skb_editor_t* editor, skb_text_range_t text_range);

/**
 * Sets the current selection of the editor to all the text.
 * @param editor editor to change.
 */
SKB_API void skb_editor_select_all(skb_editor_t* editor);

/**
 * Clears the current selection of the editor.
 * @param editor editor to change.
 */
SKB_API void skb_editor_select_none(skb_editor_t* editor);


 //
// Input handling
//

/**
 * Processes mouse click, and updates internal state.
 * @param editor editor to update.
 * @param x mouse x location.
 * @param y mouse y location.
 * @param mods key modifiers.
 * @param time time stamp in seconds (used to detect double and triple clicks).
 */
SKB_API void skb_editor_process_mouse_click(skb_editor_t* editor, float x, float y, uint32_t mods, double time);

/**
 * Processes mouse drag.
 * @param editor editor to update.
 * @param x mouse x location
 * @param y mouse y location
 */
SKB_API void skb_editor_process_mouse_drag(skb_editor_t* editor, float x, float y);

/**
 * Processes key press.
 * @param editor editor to update.
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param key key pressed.
 * @param mods key modifiers.
 */
SKB_API void skb_editor_process_key_pressed(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_editor_key_t key, uint32_t mods);

/** @return visual caret at specified text position. */
SKB_API skb_caret_info_t skb_editor_get_caret_info_at(const skb_editor_t* editor, skb_text_position_t text_pos);

/** @return line number of specified text position. */
SKB_API int32_t skb_editor_get_line_index_at(const skb_editor_t* editor, skb_text_position_t text_pos);

/** @return column number of specified text position. */
SKB_API int32_t skb_editor_get_column_index_at(const skb_editor_t* editor, skb_text_position_t text_pos);

/**
 * Hit tests the editor, and returns text position of the nearest character.
 * @param editor editor to query.
 * @param type movement type (caret or selection).
 * @param hit_x hit test location x
 * @param hit_y hit test location y
 * @return text position under or nearest to the hit test location.
 */
SKB_API skb_text_position_t skb_editor_hit_test(const skb_editor_t* editor, skb_movement_type_t type, float hit_x, float hit_y);

/**
 * Iterates over set of rectangles that represent the specified text range.
 * Due to bidirectional text, the selection in logical order can span across multiple visual rectangles.
 * @param editor editor to query.
 * @param text_range the text range to gets the rects for.
 * @param callback callback to call on each rectangle
 * @param context context passed to the callback.
 */
SKB_API void skb_editor_iterate_text_range_bounds(const skb_editor_t* editor, skb_text_range_t text_range, skb_text_range_bounds_func_t* callback, void* context);

/**
 * Sets temporary IME composition text as utf-32. The text will be laid out at the current cursor location.
 * The function can be called multiple times during while the user composes the input.
 * Use skb_editor_commit_composition_utf32() to commit or skb_editor_clear_composition() to clear the composition text.
 * @param editor editor to update.
 * @param temp_alloc temp allocator used for updating the editor text.
 * @param utf32 pointer to utf-32 string to set.
 * @param utf32_len length of the string, or -1 if nul terminated.
 * @param caret_position caret position whitin the text. Zero is in front of the first character, and utf32_len is after the last character.
 */
SKB_API void skb_editor_set_composition_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len, int32_t caret_position);

/**
 * Commits the specified string and clears composition text.
 * @param editor editor to update.
 * @param temp_alloc temp allocator used for updating the editor text.
 * @param utf32 pointer to utf-32 string to commit, if NULL previous text set with skb_editor_set_composition_utf32 will be used.
 * @param utf32_len length of the string, or -1 if nul terminated.
 */
SKB_API void skb_editor_commit_composition_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len);

/**
 * Clears composition text.
 * @param editor editor to update.
 * @param temp_alloc temp allocator used for updating the editor text.
 */
SKB_API void skb_editor_clear_composition(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc);

//
// Text edit
//

/**
 * Inserts new paragraph replacing the text range. This is equivalent of pressing enter at the current caret position.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param text_range range of text to replace
 * @param paragraph_attribute attribute to set on the new paragraph, empty attribute will be ignored.
 */
SKB_API void skb_editor_insert_paragraph(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t paragraph_attribute);

/**
 * Inserts codepoint replacing the text range.
 * @param editor editor to update.
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param text_range range of text to replace
 * @param codepoint codepoint to insert.
 */
SKB_API void skb_editor_insert_codepoint(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, uint32_t codepoint);

/**
 * Inserts utf-8 string replacing the text range.
 * The function will adjust the current selection to compensate the changed text.
 * If the range is empty (start_pos == end_pos), the function will act as insert.
 * If the utf-8 string is empty, the function will act as remove.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param text_range range of text to replace
 * @param utf8 pointer to utf-32 string to insert
 * @param utf8_len length of the string, or -1 if nul terminated
 */
SKB_API void skb_editor_insert_text_utf8(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, const char* utf8, int32_t utf8_len);

/**
 * Inserts utf-32 string replacing the text range.
 * The function will adjust the current selection to compensate the changed text.
 * If the range is empty (start_pos == end_pos), the function will act as insert.
 * If the utf-32 string is empty, the function will act as remove.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param text_range range of text to replace
 * @param utf32 pointer to utf-32 string to insert
 * @param utf32_len length of the string, or -1 if nul terminated
 */
SKB_API void skb_editor_insert_text_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, const uint32_t* utf32, int32_t utf32_len);

/**
 * Inserts rich text replacing the text range.
 * The function will adjust the current selection to compensate the changed text.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param text_range range of text to replace
 * @param rich_text pointer to rich text to insert
 */
SKB_API void skb_editor_insert_rich_text(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, const skb_rich_text_t* rich_text);

/**
 * Removes range of text.
 * The function will adjust the current selection to compensate the changed text.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param text_range range of text to remove
 */
SKB_API void skb_editor_remove(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range);

//
// Attribute edit
//

/**
 * Toggles attribute for the current selection.
 * If the whole current selection range has the specified attribute, the attribute is cleared, else the attribute is set.
 * If the current selection is empty, the active attributes will be changed instead. Active attributes define what style is applied to the next text that is inserted.
 * This is useful for attributes like bold, italic, and underline.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param text_range
 * @param attribute attribute to toggle.
 */
SKB_API void skb_editor_toggle_attribute(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Toggles attribute with payload for the current selection.
 * If the whole current selection range has the specified attribute, the attribute is cleared, else the attribute is set.
 * If the current selection is empty, the active attributes will be changed instead. Active attributes define what style is applied to the next text that is inserted.
 * This is useful for attributes like bold, italic, and underline.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param text_range
 * @param attribute attribute to toggle.
 * @param span_flags span flags to apply for the attribute, see skb_attribute_span_flags_t.
 * @param payload payload to apply for the attribute.
 */
SKB_API void skb_editor_toggle_attribute_with_payload(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute, uint8_t span_flags, const skb_data_blob_t* payload);

/**
 * Applies attribute for the current selection.
 * Sets attribute the whole current selection range, overriding any attribute of same kind.
 * If the current selection is empty, the active attributes will be changed instead. Active attributes define what style is applied to the next text that is inserted.
 * This is useful for attributes like font size or color.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param text_range text range to apply the attribute for.
 * @param attribute attribute to apply.
 */
SKB_API void skb_editor_set_attribute(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Applies attribute with payload for the current selection.
 * Sets attribute the whole current selection range, overriding any attribute of same kind.
 * If the current selection is empty, the active attributes will be changed instead. Active attributes define what style is applied to the next text that is inserted.
 * This is useful for attributes like font size or color.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param text_range text range to apply the attribute for.
 * @param attribute attribute to apply.
 * @param span_flags span flags to apply for the attribute, see skb_attribute_span_flags_t.
 * @param payload payload to apply for the attribute.
 */
SKB_API void skb_editor_set_attribute_with_payload(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute, uint8_t span_flags, const skb_data_blob_t* payload);

/**
 * Clears attribute for specified selection range.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout
 * @param text_range range of text to change
 * @param attribute attribute to clear.
 */
SKB_API void skb_editor_clear_attribute(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Clears all attributes for specified selection range.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout
 * @param text_range range of text to change
 */
SKB_API void skb_editor_clear_all_attributes(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range);

/**
 * Sets attribute for paragraphs in specified selection range, overriding any attribute of same kind.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout
 * @param text_range range of text to change
 * @param attribute attribute to set.
 */
SKB_API void skb_editor_set_paragraph_attribute(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Applies delta change to attribute for paragraphs in specified selection range.
 * Note: currently only indent level attribute is supported.
 * @param editor editor to update
 * @param temp_alloc temp alloc to use for text modifications and relayout
 * @param text_range range of text to change
 * @param attribute attribute delta to apply.
 */
SKB_API void skb_editor_set_paragraph_attribute_delta(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Checks if all the paragraphs overlapping the text range has the specified attribute.
 * @param editor editor to query
 * @param text_range text range to check
 * @param attribute attribute to check
 * @return true if all paragraph in the text range has specified attribute.
 */
SKB_API bool skb_editor_has_paragraph_attribute(const skb_editor_t* editor, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Checks if the text range has the specified attribute.
 * If the text range is empty, and equals current selection, the active attributes are tested.
 * Active attributes are picker from the text before the empty selection.
 * This function can be used to check what the current text style is under the caret or selection for most styles.
 * Some content-like styles, like hyperlinks or hilights, should use skb_editor_has_text_attribute() instead.
 * @param editor editor to query
 * @param text_range text range to check
 * @param attribute attribute to check
 * @return true the whole text range contains the specified style.
 */
SKB_API bool skb_editor_has_attribute(const skb_editor_t* editor, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Checks if the text range has the specified attribute.
 * If the text range is empty, attribute of the text (or at) the text location is returned.
 * This function explicityl does not check the active attributes.
 * It can be used to check the current status for content-like style, like hyperlinks or hilights.
 * Most text styles, like bold and italic, should use skb_editor_has_attribute() instead.
 * @param editor editor to query
 * @param text_range text range to check
 * @param attribute attribute to check
 * @return true the whole text range contains the specified style.
 */
SKB_API bool skb_editor_has_text_attribute(const skb_editor_t* editor, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Returns all unique attributes that cover the specified range.
 * If the text range is empty, and equals current selection, the matching active attributes are returned.
 * @param editor editor to query
 * @param text_range text range to check
 * @param attribute_kind attribute kind tag.
 * @param attributes pointer to result array
 * @param attributes_cap capacity of result array.
 * @return number of attributes found in range.
 */
SKB_API int32_t skb_editor_get_attributes(const skb_editor_t* editor, skb_text_range_t text_range, uint32_t attribute_kind, skb_attribute_t* attributes, int32_t attributes_cap);

/**
 * Returns payload of specified attribute in given range.
 * The queried range must be completely inside inside the specified attribute span, for a valid result to be returned.
 * @param editor editor to query
 * @param text_range text range to check
 * @param attribute attribute to check
 * @return pointer to the found payload.
 */
SKB_API skb_data_blob_t* skb_editor_get_attribute_payload(const skb_editor_t* editor, skb_text_range_t text_range, skb_attribute_t attribute);

/**
 * Returns the whole text range of specified attribute in given range.
 * The queried range must be completely inside inside the specified attribute span, for a valid result to be returned.
 * @param editor editor to query
 * @param text_range text range to check
 * @param attribute attribute to check
 * @return text range of the text, or empty range if not found.
 */
SKB_API skb_text_range_t skb_editor_get_attribute_text_range(const skb_editor_t* editor, skb_text_range_t text_range, skb_attribute_t attribute);

/** @return number of active attributes. Active attributes define what style is applied to the next text that is inserted. */
SKB_API int32_t skb_editor_get_active_attributes_count(const skb_editor_t* editor);

/** @return const pointer to active attributes. Active attributes define what style is applied to the next text that is inserted. */
SKB_API const skb_attribute_t* skb_editor_get_active_attributes(const skb_editor_t* editor);


//
// Undo
//

/**
 * Begins undo transaction. All changes done within an transaction will be undo or redo as one change.
 * @param editor editor to update.
 * @return transaction id.
 */
SKB_API int32_t skb_editor_undo_transaction_begin(skb_editor_t* editor);

/**
 * Ends undo transaction. All changes done within an transaction will be undo or redo as one change.
 * @param editor editor to update.
 * @param transaction_id transaction id from matching skb_editor_undo_transaction_begin().
 */
SKB_API void skb_editor_undo_transaction_end(skb_editor_t* editor, int32_t transaction_id);

/** @return True, if the last change can be undone. */
SKB_API bool skb_editor_can_undo(skb_editor_t* editor);

/**
 * Undo the last change.
 * @param editor editor to update.
 * @param temp_alloc temp allocator used to relayout the text.
 */
SKB_API void skb_editor_undo(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc);

/** @return True, if the last undone change can be redone. */
SKB_API bool skb_editor_can_redo(skb_editor_t* editor);

/**
 * Redo the last undone change.
 * @param editor editor to update.
 * @param temp_alloc temp allocator used to relayout the text.
 */
SKB_API void skb_editor_redo(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc);


/** @} */

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_EDITOR_H
