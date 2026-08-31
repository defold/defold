// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_EDITOR_RULES_H
#define SKB_EDITOR_RULES_H

#include <stdint.h>
#include <stdbool.h>
#include "skb_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct skb_editor_t skb_editor_t;
typedef struct skb_attribute_collection_t skb_attribute_collection_t;

/**
 * @defgroup editor_rules Editor Rules
 *
 * Editor rules can be used to define contextual behavior for keys.
 * For example pressing enter after a "header" paragraph may change the paragraph style to "body".
 * Editor rules can be also used to implement hotkeys.
 *
 * Editor rule set is a container for number of rules. The rules are processed in the order they are added.
 *
 * @{
 */

/** Opaque type for the edtior rule set. Use skb_editor_rule_set_create() to create. */
typedef struct skb_editor_rule_set_t skb_editor_rule_set_t;

/** Struct that is passed to the rule apply function, which contains commonly used data. */
typedef struct skb_editor_rule_context_t {
	/** Current editor. */
	skb_editor_t* editor;
	/** Temp allocator to use for editor modify functions. */
	skb_temp_alloc_t* temp_alloc;
	/** Cached attribute collection used by the editor. */
	skb_attribute_collection_t* attribute_collection;
	/** Current selection count. */
	int32_t selection_count;
	/** Caret's current paragraph position. */
	skb_paragraph_position_t caret_paragraph_pos;
	/** Range of matched prefix text, is specified in rule. */
	skb_text_range_t prefix_text_range;
	/** Key mods that triggered the rule. */
	uint32_t key_mods;
} skb_editor_rule_context_t;

typedef struct skb_editor_rule_t skb_editor_rule_t;

/**
 * Signature of rule apply function. The function is called if all rule preconditions match.
 * @param rule matched rule.
 * @param rule_context pointer to editor context (can be used to access editor and other data)
 * @param context context pointer that was passed to skb_editor_rule_set_process.
 * @return true of the rule was applied and we should stop, or false if futher rule should be tried.
 */
typedef bool skb_editor_rule_apply_func_t(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context);

/**
 * Definition of editor rule.
 * The rule contains number of preconditions that must pass for the rule to be applied.
 * The rule's apply callback can do further tests, and return false if the rule cannot be applied.
 * In that case the rule matching will continue until a rule's apply callback returns true, or no rules are left.
 */
typedef struct skb_editor_rule_t {
	/** Key to match. The key does not have any particular meaning, it will be matched against the key value passed to skb_editor_rule_set_process. */
	int32_t key;
	/** Key modifiers, must be 0 or combination of skb_editor_key_mod_t. The mods will be checked exactly. */
	uint32_t key_mods;
	/** If true, mods are not checked, the actualy mods will be available via skb_editor_rule_context_t.key_mods.  */
	bool any_mods;
	/** Expect empty current selection. */
	bool empty_selection;
	/** Expect non-empty selection. */
	bool has_selection;
	/** Expected prefix at the current caret location.  */
	const char* prefix_utf8;
	/** Expect that the prefix is at paragraph start. */
	bool prefix_at_paragraph_start;
	/** Expect that the paragraph under the caret contains this attribute. */
	const char* on_paragraph_attribute_name;
	/** Function to call when all of the preconditions match. */
	skb_editor_rule_apply_func_t* apply;
	/** Optional attribute name parameter that can be used by the apply callback. */
	const char* applied_attribute_name;
	/** Optional value that can be used by the apply callback. */
	int32_t applied_value;
} skb_editor_rule_t;

/**
 * Creates new empty editor rule set. Use skb_editor_rule_set_destroy() to destroy.
 * @return pointer to the newly created rule set.
 */
SKB_API skb_editor_rule_set_t* skb_editor_rule_set_create(void);

/**
 * Cleans up and frees an editor rule set.
 * @param rule_set ruleset to destroy.
 */
SKB_API void skb_editor_rule_set_destroy(skb_editor_rule_set_t* rule_set);

/**
 * Appends editor rules to the rule set.
 * @param rule_set rule set to append to.
 * @param rules pointer to array of rules to append.
 * @param rules_count number of rules in the rules array.
 */
SKB_API void skb_editor_rule_set_append(skb_editor_rule_set_t* rule_set, const skb_editor_rule_t* rules, int32_t rules_count);

/**
 * Processed the rules in the rule set.
 * If the function returns true, the application should not process the provided key further.
 * @param rule_set rule set to process.
 * @param editor editor to apply the rules for.
 * @param temp_alloc temp alloc used by editor modification functions.
 * @param key key to process.
 * @param key_mods key modifiers to process. Must be 0 or combination of skb_editor_key_mod_t.
 * @param context user context pointer passed to the apply callbacks.
 * @return true of a rule was succesfully processed.
 */
SKB_API bool skb_editor_rule_set_process(const skb_editor_rule_set_t* rule_set, skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, int32_t key, uint32_t key_mods, void* context);


/**
 * Makes rule to insert a codepoint on key press.
 * @param key input key to press.
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param codepoint codepoint to insert.
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_insert_codepoint(int32_t key, uint32_t key_mods, uint32_t codepoint);

/**
 * Makes rule to process specific key on key press with modifiers
 * @param key input key to press.
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param edit_key editor key to process (see skb_editor_key_t).
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_process_key(int32_t key, uint32_t key_mods, int32_t edit_key);

/**
 * Makes rule to process specific key on keypress.
 * This rule will match with any key mods, and will pass the key mods to the editor process key function.
 * @param key input key to press.
 * @param edit_key editor key to process (see skb_editor_key_t).
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_process_key_pass_mod(int32_t key, int32_t edit_key);

/**
 * Makes rule that matches specific prefix at paragraph start, and if match is found the prefix is removed and paragraph style is changed.
 * This rule can be used to convert Markdown like paragraph styles to rich text paragraph styles on the fly.
 * @param key key to press (usually space).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param prefix_utf8 expected prefix as null terminated utf-8 string (assumes static string, it will not be copied).
 * @param on_attribute_name expected attribute on the paragraph under the caret.
 * @param applied_attribute_name paragraph style to apply.
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_convert_start_prefix_to_paragraph_style(int32_t key, uint32_t key_mods, const char* prefix_utf8, const char* on_attribute_name, const char* applied_attribute_name);

/**
 * Makes rule to change the indent level of all selected paragraphs.
 * @param key key to press (usually tab).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param on_attribute_name expected attribute on the paragraph under the caret.
 * @param delta indent delta to apply for the paragraphs
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_change_indent(int32_t key, uint32_t key_mods, const char* on_attribute_name, int32_t delta);

/**
 * Makes rule to change the indent level when the caret is at the start of a paragraph.
 * @param key key to press (usually tab).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param on_attribute_name expected attribute on the paragraph under the caret.
 * @param delta indent delta to apply for the paragraphs
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_change_indent_at_paragraph_start(int32_t key, uint32_t key_mods, const char* on_attribute_name, int32_t delta);

/**
 * Makes rule to remove one indent level when the caret is at the start of a paragraph.
 * If the indent level is 0 when indent is removed, then new paragraph attribute is applied.
 * @param key key to press (usually backspace).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param on_attribute_name expected attribute on the paragraph under the caret.
 * @param applied_attribute_name paragraph style to apply if the indent level is 0.
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_remove_indent_at_paragraph_start(int32_t key, uint32_t key_mods, const char* on_attribute_name, const char* applied_attribute_name);

/**
 * Makes rule to add a new paragraph when key is pressed on an empty paragraph.
 * Can be used for example to remove a list style when empty list item is added.
 * @param key key to press (usually enter).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param on_attribute_name expected attribute on the paragraph under the caret.
 * @param applied_attribute_name paragraph style to apply to the next new paragraph.
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_change_style_on_empty_paragraph(int32_t key, uint32_t key_mods, const char* on_attribute_name, const char* applied_attribute_name);

/**
 * Makes rule to add a new paragraph when key is pressed at the end of a paragraph.
 * Can be used for example to change paragraph style from "header" to "body" when enter is pressed at the end of the "header" paragraph.
 * @param key key to press (usually enter).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param on_attribute_name expected attribute on the paragraph under the caret.
 * @param applied_attribute_name paragraph style to apply to the next new paragraph.
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_change_style_at_paragraph_end(int32_t key, uint32_t key_mods, const char* on_attribute_name, const char* applied_attribute_name);

/**
 * Makes rule to use tabs to indent the selected paragraphs.
 * This rule is usually used for code blocks.
 * @param key key to press (usually tab).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param on_attribute_name expected attribute on the paragraph under the caret.
 * @param delta indent delta.
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_code_change_indent(int32_t key, uint32_t key_mods, const char* on_attribute_name, int32_t delta);

/**
 * Makes rule to add a new paragraph when key is pressed on an empty paragraph.
 * This rule expects 2 consecutive empty paragraphs for the style to be applied. Both of the paragraphs will be removed and new paragraph with new style is added.
 * This rule is usualy used for code blocks.
 * @param key key to press (usually enter).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param on_attribute_name expected attribute on the paragraph under the caret.
 * @param applied_attribute_name paragraph style to apply to the next new paragraph.
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_code_change_style_on_empty_paragraph(int32_t key, uint32_t key_mods, const char* on_attribute_name, const char* applied_attribute_name);

/**
 * Makes rule to add a new paragraph and match the tabs and the start of the new paragraph with current paragraph.
 * @param key key to press (usually enter).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param on_attribute_name expected attribute on the paragraph under the caret.
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_code_match_tabs(int32_t key, uint32_t key_mods, const char* on_attribute_name);

/**
 * Makes rule to set paragraph attribute.
 * @param key key to press (usually enter).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param attribute_name paragraph style to apply.
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_set_paragraph_attribute(int32_t key, uint32_t key_mods, const char* attribute_name);

/**
 * Makes tule to toggle text attributes.
 * @param key key to press (usually enter).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param attribute_name text attribute to toggle.
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_toggle_attribute(int32_t key, uint32_t key_mods, const char* attribute_name);

/** Enum describing options for skb_editor_rule_make_undo_redo. */
typedef enum {
	/** Do undo */
	SKB_EDITOR_RULE_UNDO,
	/** Do redo */
	SKB_EDITOR_RULE_REDO,
} skb_editor_rule_undo_redo_type_t;

/**
 * Makes rule to undo or redo previous change.
 * @param key key to press (usually enter).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param type undo or redo, see skb_editor_rule_undo_redo_type_t.
 * @return initialized rule.
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_undo_redo(int32_t key, uint32_t key_mods, skb_editor_rule_undo_redo_type_t type);

/** Enum describing options for skb_editor_rule_make_select. */
typedef enum {
	/** Select none */
	SKB_EDITOR_RULE_SELECT_NONE,
	/** Select all */
	SKB_EDITOR_RULE_SELECT_ALL,
} skb_editor_rule_select_type_t;

/**
 * makes rule to change selection.
 * @param key key to press (usually enter).
 * @param key_mods input modifier to hold (see skb_editor_key_mod_t).
 * @param type none or all, see skb_editor_rule_select_type_t.
 * @return
 */
SKB_API skb_editor_rule_t skb_editor_rule_make_select(int32_t key, uint32_t key_mods, skb_editor_rule_select_type_t type);

/** @} */

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_EDITOR_RULES_H
