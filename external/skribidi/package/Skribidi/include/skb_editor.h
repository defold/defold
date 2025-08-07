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

/** Opaque type for the text editor. Use skb_editor_create() to create. */
typedef struct skb_editor_t skb_editor_t;

/**
 * Signature of text editor change function.
 * @param editor editor that changed.
 * @param context context pointer that was passed to skb_editor_set_on_change_callback().
 */
typedef void skb_editor_on_change_func_t(skb_editor_t* editor, void* context);

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
	/** Layout parameters used for each paragraph layout. */
	skb_layout_params_t layout_params;
	/** Text attributes for all the text. */
	const skb_attribute_t* text_attributes;
	/** Number of text attributes. */
	int32_t text_attributes_count;
	/** Text attributes for the IME composition text. */
	const skb_attribute_t* composition_attributes;
	/** Number of compositoin text attributes. */
	int32_t composition_attributes_count;
	/** Base direction of the text editor. */
	uint8_t base_direction;
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
	SKB_MOD_OPTION = 0x04,
	SKB_MOD_COMMAND = 0x08,
} skb_editor_key_mod_t;

/**
 * Creates new text editor.
 * @param params parameters for the editor.
 * @return newly create editor.
 */
skb_editor_t* skb_editor_create(const skb_editor_params_t* params);

/**
 * Sets change callback function.
 * @param editor editor to change.
 * @param callback pointer to the callback function
 * @param context context pointer that is passed to the callback function each time it is called.
 */
void skb_editor_set_on_change_callback(skb_editor_t* editor, skb_editor_on_change_func_t* callback, void* context);

/**
 * Destroys a text editor.
 * @param editor pointer to the editor to destroy.
 */
void skb_editor_destroy(skb_editor_t* editor);

/**
 * Resets text editor to empty state.
 * @param editor editor to change
 * @param params new parameters, or NULL if previous paramters should be kept.
 */
void skb_editor_reset(skb_editor_t* editor, const skb_editor_params_t* params);

/**
 * Sets the text of the editor from an utf-8 string.
 * @param editor editor to change.
 * @param temp_alloc temp allocator used while setting layouting the text.
 * @param utf8 pointer to the utf-8 string to set.
 * @param utf8_len length of the string, or -1 if nul terminated.
 */
void skb_editor_set_text_utf8(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const char* utf8, int32_t utf8_len);

/**
 * Sets the text of the editor from an utf-32 string.
 * @param editor editor to change.
 * @param temp_alloc temp allocator used while setting layouting the text.
 * @param utf32 pointer to the utf-32 string to set.
 * @param utf32_len length of the string, or -1 if nul terminated.
 */
void skb_editor_set_text_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len);

/** @return length of the edited text as utf-8. */
int32_t skb_editor_get_text_utf8_count(const skb_editor_t* editor);

/**
 * Gets the edited text as utf-8.
 * @param editor editor to query.
 * @param buf buffer where to store the text.
 * @param buf_cap capacity of the buffer.
 * @return total length of the string (can be larger than buf_cap).
 */
int32_t skb_editor_get_text_utf8(const skb_editor_t* editor, char* buf, int32_t buf_cap);

/** @return length of the edited text as utf-32. */
int32_t skb_editor_get_text_utf32_count(const skb_editor_t* editor);

/**
 * Gets the edited text as utf-32.
 * @param editor editor to query.
 * @param buf buffer where to store the text.
 * @param buf_cap capacity of the buffer.
 * @return total length of the string (can be larger than buf_cap).
 */
int32_t skb_editor_get_text_utf32(const skb_editor_t* editor, uint32_t* buf, int32_t buf_cap);

/** @return number of paragraphs in the editor. */
int32_t skb_editor_get_paragraph_count(skb_editor_t* editor);
/** @return const pointer to specified paragraph. */
const skb_layout_t* skb_editor_get_paragraph_layout(skb_editor_t* editor, int32_t index);
/** @return y-offset of the specified paragraph. */
float skb_editor_get_paragraph_offset_y(skb_editor_t* editor, int32_t index);
/** @return text offset of specified paragraph. */
int32_t skb_editor_get_paragraph_text_offset(skb_editor_t* editor, int32_t index);
/** @return the parameters used to create the editor. */
const skb_editor_params_t* skb_editor_get_params(skb_editor_t* editor);

/** @return line number of specified text position. */
int32_t skb_editor_get_line_index_at(const skb_editor_t* editor, skb_text_position_t pos);
/** @return column number of specified text position. */
int32_t skb_editor_get_column_index_at(const skb_editor_t* editor, skb_text_position_t pos);
/** @return text offset of specified text position. */
int32_t skb_editor_get_text_offset_at(const skb_editor_t* editor, skb_text_position_t pos);
/** @return text direction at specified text position. */
skb_text_direction_t skb_editor_get_text_direction_at(const skb_editor_t* editor, skb_text_position_t pos);
/** @return visual caret at specified text position. */
skb_visual_caret_t skb_editor_get_visual_caret(const skb_editor_t* editor, skb_text_position_t pos);
/**
 * Hit tests the editor, and returns text position of the nearest character.
 * @param editor editor to query.
 * @param type movement type (caret or selection).
 * @param hit_x hit test location x
 * @param hit_y hit test location y
 * @return text position under or nearest to the hit test location.
 */
skb_text_position_t skb_editor_hit_test(const skb_editor_t* editor, skb_movement_type_t type, float hit_x, float hit_y);

/**
 * Sets the current selection of the editor to all the text.
 * @param editor editor to change.
 */
void skb_editor_select_all(skb_editor_t* editor);

/**
 * Clears the current selection of the editor.
 * @param editor editor to change.
 */
void skb_editor_select_none(skb_editor_t* editor);

/**
 * Sets the current selection of the editor to specific range.
 * @param editor editor to change.
 * @param selection new selection.
 */
void skb_editor_select(skb_editor_t* editor, skb_text_selection_t selection);

/** @return current selection of the editor. */
skb_text_selection_t skb_editor_get_current_selection(skb_editor_t* editor);

/**
 * Returns validated text range of specified selection range.
 * @param editor editor to query.
 * @param selection selection range.
 * @return validated text range.
 */
skb_range_t skb_editor_get_selection_text_offset_range(const skb_editor_t* editor, skb_text_selection_t selection);

/**
 * Returns number of codepoints in the selection.
 * @param editor editor to query.
 * @param selection selection
 * @return number of codepoints in the selection.
 */
int32_t skb_editor_get_selection_count(const skb_editor_t* editor, skb_text_selection_t selection);


/**
 * Returns set of rectangles that represent the specified selection.
 * Due to bidirectional text, the selection in logical order can span across multiple visual rectangles.
 * @param editor editor to query.
 * @param selection selection to get.
 * @param callback callback to call on each rectangle
 * @param context context passed to the callback.
 */
void skb_editor_get_selection_bounds(const skb_editor_t* editor, skb_text_selection_t selection, skb_selection_rect_func_t* callback, void* context);

/** @return return the text length in utf-8 of specified selection. */
int32_t skb_editor_get_selection_text_utf8_count(const skb_editor_t* editor, skb_text_selection_t selection);

/**
 * Gets the text of the specified selection text as utf-8.
 * @param editor editor to query.
 * @param selection range of text to get.
 * @param buf buffer where to store the text.
 * @param buf_cap capacity of the buffer.
 * @return total length of the selected string (can be larger than buf_cap).
 */
int32_t skb_editor_get_selection_text_utf8(const skb_editor_t* editor, skb_text_selection_t selection, char* buf, int32_t buf_cap);

/** @return return the text length in utf-32 of specified selection. */
int32_t skb_editor_get_selection_text_utf32_count(const skb_editor_t* editor, skb_text_selection_t selection);

/**
 * Gets the text of the specified selection text as utf-8.
 * @param editor editor to query.
 * @param selection range of text to get.
 * @param buf buffer where to store the text.
 * @param buf_cap capacity of the buffer.
 * @return total length of the selected string (can be larger than buf_cap).
 */
int32_t skb_editor_get_selection_text_utf32(const skb_editor_t* editor, skb_text_selection_t selection, uint32_t* buf, int32_t buf_cap);

/**
 * Processes mouse click, and updates internal state.
 * @param editor editor to update.
 * @param x mouse x location.
 * @param y mouse y location.
 * @param mods key modifiers.
 * @param time time stamp in seconds (used to detect double and triple clicks).
 */
void skb_editor_process_mouse_click(skb_editor_t* editor, float x, float y, uint32_t mods, double time);

/**
 * Processes mouse drag.
 * @param editor editor to update.
 * @param x mouse x location
 * @param y mouse y location
 */
void skb_editor_process_mouse_drag(skb_editor_t* editor, float x, float y);

/**
 * Processes key press.
 * @param editor editor to update.
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param key key pressed.
 * @param mods key modifiers.
 */
void skb_editor_process_key_pressed(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_editor_key_t key, uint32_t mods);

/**
 * Inserts codepoint to the text at current caret position.
 * @param editor editor to update.
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param codepoint codepoint to insert.
 */
void skb_editor_insert_codepoint(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, uint32_t codepoint);

/**
 * Paste utf-8 text to the current caret position.
 * @param editor editor to update.
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param utf8 pointer to utf-8 string to paste
 * @param utf8_len length of the string, or -1 if nul terminated.
 */
void skb_editor_paste_utf8(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const char* utf8, int32_t utf8_len);

/**
 * Paste utf-32 text to the current caret position.
 * @param editor editor to update.
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 * @param utf32 pointer to utf-32 string to paste
 * @param utf32_len length of the string, or -1 if nul terminated.
 */
void skb_editor_paste_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len);

/**
 * Deletes current selection.
 * @param editor editor to update.
 * @param temp_alloc temp alloc to use for text modifications and relayout.
 */
void skb_editor_cut(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc);

/** @return True, if the last change can be undone. */
bool skb_editor_can_undo(skb_editor_t* editor);

/**
 * Undo the last change.
 * @param editor editor to update.
 * @param temp_alloc temp allocator used to relayout the text.
 */
void skb_editor_undo(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc);

/** @return True, if the last undone change can be redone. */
bool skb_editor_can_redo(skb_editor_t* editor);

/**
 * Redo the last undone change.
 * @param editor editor to update.
 * @param temp_alloc temp allocator used to relayout the text.
 */
void skb_editor_redo(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc);

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
void skb_editor_set_composition_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len, int32_t caret_position);

/**
 * Commits the specified string and clears composition text.
 * @param editor editor to update.
 * @param temp_alloc temp allocator used for updating the editor text.
 * @param utf32 pointer to utf-32 string to commit, if NULL previous text set with skb_editor_set_composition_utf32 will be used.
 * @param utf32_len length of the string, or -1 if nul terminated.
 */
void skb_editor_commit_composition_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len);

/**
 * Clears composition text.
 * @param editor editor to update.
 * @param temp_alloc temp allocator used for updating the editor text.
 */
void skb_editor_clear_composition(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc);


/** @} */

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_EDITOR_H
