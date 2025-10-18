// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include <string.h>
#include "test_macros.h"
#include "skb_editor.h"
#include "skb_font_collection.h"

static int test_init(void)
{
	skb_attribute_t attributes[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 15.f, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
	};

	skb_editor_params_t params = {
		 .layout_params = {
		 	.font_collection = NULL,
		 },
		.text_attributes = attributes,
		.text_attributes_count = SKB_COUNTOF(attributes),
		.base_direction = SKB_DIRECTION_LTR,
		.caret_mode = SKB_CARET_MODE_SKRIBIDI,
	};

	skb_editor_t* editor = skb_editor_create(&params);
	ENSURE(editor != NULL);

	skb_editor_destroy(editor);

	return 0;
}

static int test_command_line_navigation_macos(void)
{
	skb_temp_alloc_t* temp_alloc = skb_temp_alloc_create(1024);
	ENSURE(temp_alloc != NULL);

	skb_font_collection_t* font_collection = skb_font_collection_create();
	ENSURE(font_collection != NULL);
	skb_font_handle_t font_handle = skb_font_collection_add_font(font_collection, "data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	ENSURE(font_handle);

	skb_attribute_t attributes[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 15.f, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
	};

	skb_editor_params_t params = {
		 .layout_params = {
		 	.font_collection = font_collection,
		 },
		.text_attributes = attributes,
		.text_attributes_count = SKB_COUNTOF(attributes),
		.base_direction = SKB_DIRECTION_LTR,
		.caret_mode = SKB_CARET_MODE_SKRIBIDI,
		.editor_behavior = SKB_BEHAVIOR_MACOS,
	};

	skb_editor_t* editor = skb_editor_create(&params);
	ENSURE(editor != NULL);

	// Initialize editor with test text
	const char* test_text = "Hello world\nThis is a test\nabout line jumping";
	skb_editor_set_text_utf8(editor, temp_alloc, test_text, (int32_t)strlen(test_text));

	// Get initial selection - should be at document start
	skb_text_selection_t initial_selection = skb_editor_get_current_selection(editor);
	ENSURE(initial_selection.start_pos.offset == 0);
	ENSURE(initial_selection.end_pos.offset == 0);

	// Test Command+Right (should jump to end of line on macOS)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_RIGHT, SKB_MOD_COMMAND);
	
	skb_text_selection_t after_command_right = skb_editor_get_current_selection(editor);
	// Should be at end of first line (position 11, after "Hello world")
	ENSURE(after_command_right.start_pos.offset == 11);
	ENSURE(after_command_right.end_pos.offset == 11);

	// Test Command+Left (should jump to beginning of line on macOS)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_LEFT, SKB_MOD_COMMAND);
	
	skb_text_selection_t after_command_left = skb_editor_get_current_selection(editor);
	// Should be back at beginning of line (position 0)
	ENSURE(after_command_left.start_pos.offset == 0);
	ENSURE(after_command_left.end_pos.offset == 0);

	skb_editor_destroy(editor);
	skb_font_collection_destroy(font_collection);
	skb_temp_alloc_destroy(temp_alloc);

	return 0;
}

static int test_command_document_navigation_macos(void)
{
	skb_temp_alloc_t* temp_alloc = skb_temp_alloc_create(1024);
	ENSURE(temp_alloc != NULL);

	skb_font_collection_t* font_collection = skb_font_collection_create();
	ENSURE(font_collection != NULL);
	skb_font_handle_t font_handle = skb_font_collection_add_font(font_collection, "data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	ENSURE(font_handle);

	skb_attribute_t attributes[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 15.f, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
	};

	skb_editor_params_t params = {
		 .layout_params = {
		 	.font_collection = font_collection,
		 },
		.text_attributes = attributes,
		.text_attributes_count = SKB_COUNTOF(attributes),
		.base_direction = SKB_DIRECTION_LTR,
		.caret_mode = SKB_CARET_MODE_SKRIBIDI,
		.editor_behavior = SKB_BEHAVIOR_MACOS,
	};

	skb_editor_t* editor = skb_editor_create(&params);
	ENSURE(editor != NULL);

	// Initialize editor with test text
	const char* test_text = "Hello world\nThis is a test\nabout line jumping";
	skb_editor_set_text_utf8(editor, temp_alloc, test_text, (int32_t)strlen(test_text));

	// Get initial selection - should be at document start
	skb_text_selection_t initial_selection = skb_editor_get_current_selection(editor);
	ENSURE(initial_selection.start_pos.offset == 0);
	ENSURE(initial_selection.end_pos.offset == 0);

	// Test Command+Down (should navigate to document end on macOS)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_DOWN, SKB_MOD_COMMAND);
	
	skb_text_selection_t after_command_down = skb_editor_get_current_selection(editor);
	int32_t expected_end = (int32_t)strlen(test_text);
	ENSURE(after_command_down.start_pos.offset == expected_end);
	ENSURE(after_command_down.end_pos.offset == expected_end);

	// Test Command+Up (should navigate to document start on macOS)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_UP, SKB_MOD_COMMAND);
	
	skb_text_selection_t after_command_up = skb_editor_get_current_selection(editor);
	ENSURE(after_command_up.start_pos.offset == 0);
	ENSURE(after_command_up.end_pos.offset == 0);

	// Position caret in the middle of the document for more comprehensive testing
	skb_text_position_t middle_pos = {.offset = 20, .affinity = SKB_AFFINITY_TRAILING}; // Around "This is a test"
	skb_text_selection_t middle_selection = {.start_pos = middle_pos, .end_pos = middle_pos};
	skb_editor_select(editor, middle_selection);

	skb_text_selection_t middle_check = skb_editor_get_current_selection(editor);
	ENSURE(middle_check.start_pos.offset == 20);
	ENSURE(middle_check.end_pos.offset == 20);

	// Test Command+Up from middle (should go to document start)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_UP, SKB_MOD_COMMAND);
	
	skb_text_selection_t from_middle_up = skb_editor_get_current_selection(editor);
	ENSURE(from_middle_up.start_pos.offset == 0);
	ENSURE(from_middle_up.end_pos.offset == 0);

	// Reset to middle position
	skb_editor_select(editor, middle_selection);

	// Test Command+Down from middle (should go to document end)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_DOWN, SKB_MOD_COMMAND);
	
	skb_text_selection_t from_middle_down = skb_editor_get_current_selection(editor);
	ENSURE(from_middle_down.start_pos.offset == expected_end);
	ENSURE(from_middle_down.end_pos.offset == expected_end);

	skb_editor_destroy(editor);
	skb_font_collection_destroy(font_collection);
	skb_temp_alloc_destroy(temp_alloc);

	return 0;
}

static int test_shift_command_text_selection_macos(void)
{
	skb_temp_alloc_t* temp_alloc = skb_temp_alloc_create(1024);
	ENSURE(temp_alloc != NULL);

	skb_font_collection_t* font_collection = skb_font_collection_create();
	ENSURE(font_collection != NULL);
	skb_font_handle_t font_handle = skb_font_collection_add_font(font_collection, "data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	ENSURE(font_handle);

	skb_attribute_t attributes[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 15.f, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
	};

	skb_editor_params_t params = {
		 .layout_params = {
		 	.font_collection = font_collection,
		 },
		.text_attributes = attributes,
		.text_attributes_count = SKB_COUNTOF(attributes),
		.base_direction = SKB_DIRECTION_LTR,
		.caret_mode = SKB_CARET_MODE_SKRIBIDI,
		.editor_behavior = SKB_BEHAVIOR_MACOS,
	};

	skb_editor_t* editor = skb_editor_create(&params);
	ENSURE(editor != NULL);

	// Initialize editor with test text
	const char* test_text = "Hello world\nThis is a test\nabout line jumping";
	skb_editor_set_text_utf8(editor, temp_alloc, test_text, (int32_t)strlen(test_text));

	// Get initial selection - should be at document start
	skb_text_selection_t initial_selection = skb_editor_get_current_selection(editor);
	ENSURE(initial_selection.start_pos.offset == 0);
	ENSURE(initial_selection.end_pos.offset == 0);

	// Test Shift+Command+Down (should select from start to document end)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_DOWN, SKB_MOD_SHIFT | SKB_MOD_COMMAND);
	
	skb_text_selection_t full_selection = skb_editor_get_current_selection(editor);
	int32_t expected_end = (int32_t)strlen(test_text);
	ENSURE(full_selection.start_pos.offset == 0);
	ENSURE(full_selection.end_pos.offset == expected_end);

	// Reset to end position and test Shift+Command+Up (should select from end to document start)
	skb_text_position_t end_pos = {.offset = expected_end, .affinity = SKB_AFFINITY_TRAILING};
	skb_text_selection_t end_selection = {.start_pos = end_pos, .end_pos = end_pos};
	skb_editor_select(editor, end_selection);

	skb_text_selection_t end_check = skb_editor_get_current_selection(editor);
	ENSURE(end_check.start_pos.offset == expected_end);
	ENSURE(end_check.end_pos.offset == expected_end);

	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_UP, SKB_MOD_SHIFT | SKB_MOD_COMMAND);
	
	skb_text_selection_t reverse_selection = skb_editor_get_current_selection(editor);
	// Should select from end to start
	ENSURE((reverse_selection.start_pos.offset == 0 && reverse_selection.end_pos.offset == expected_end) ||
	       (reverse_selection.start_pos.offset == expected_end && reverse_selection.end_pos.offset == 0));

	// Reset to start position for line-level testing
	skb_text_position_t start_pos = {.offset = 0, .affinity = SKB_AFFINITY_TRAILING};
	skb_text_selection_t start_selection = {.start_pos = start_pos, .end_pos = start_pos};
	skb_editor_select(editor, start_selection);

	skb_text_selection_t reset_check = skb_editor_get_current_selection(editor);
	ENSURE(reset_check.start_pos.offset == 0);
	ENSURE(reset_check.end_pos.offset == 0);

	// Test Shift+Command+Right (should select from start to end of line)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_RIGHT, SKB_MOD_SHIFT | SKB_MOD_COMMAND);
	
	skb_text_selection_t line_selection = skb_editor_get_current_selection(editor);
	// Should select from position 0 to end of first line (position 11, after "Hello world")
	ENSURE(line_selection.start_pos.offset == 0);
	ENSURE(line_selection.end_pos.offset == 11);

	// Test Shift+Command+Left (should select from current position to start of line)
	// First move to end of first line
	skb_text_position_t end_line_pos = {.offset = 11, .affinity = SKB_AFFINITY_TRAILING};
	skb_text_selection_t end_line_selection = {.start_pos = end_line_pos, .end_pos = end_line_pos};
	skb_editor_select(editor, end_line_selection);

	skb_text_selection_t end_line_check = skb_editor_get_current_selection(editor);
	ENSURE(end_line_check.start_pos.offset == 11);
	ENSURE(end_line_check.end_pos.offset == 11);

	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_LEFT, SKB_MOD_SHIFT | SKB_MOD_COMMAND);
	
	skb_text_selection_t reverse_line_selection = skb_editor_get_current_selection(editor);
	// Should select from position 11 back to position 0
	ENSURE((reverse_line_selection.start_pos.offset == 0 && reverse_line_selection.end_pos.offset == 11) ||
	       (reverse_line_selection.start_pos.offset == 11 && reverse_line_selection.end_pos.offset == 0));

	skb_editor_destroy(editor);
	skb_font_collection_destroy(font_collection);
	skb_temp_alloc_destroy(temp_alloc);

	return 0;
}

static int test_option_word_navigation_macos(void)
{
	skb_temp_alloc_t* temp_alloc = skb_temp_alloc_create(1024);
	ENSURE(temp_alloc != NULL);

	skb_font_collection_t* font_collection = skb_font_collection_create();
	ENSURE(font_collection != NULL);
	skb_font_handle_t font_handle = skb_font_collection_add_font(font_collection, "data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	ENSURE(font_handle);

	skb_attribute_t attributes[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 15.f, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
	};

	skb_editor_params_t params = {
		 .layout_params = {
		 	.font_collection = font_collection,
		 },
		.text_attributes = attributes,
		.text_attributes_count = SKB_COUNTOF(attributes),
		.base_direction = SKB_DIRECTION_LTR,
		.caret_mode = SKB_CARET_MODE_SKRIBIDI,
		.editor_behavior = SKB_BEHAVIOR_MACOS,
	};

	skb_editor_t* editor = skb_editor_create(&params);
	ENSURE(editor != NULL);

	// Initialize editor with test text
	const char* test_text = "Hello world\nThis is a test\nabout line jumping";
	skb_editor_set_text_utf8(editor, temp_alloc, test_text, (int32_t)strlen(test_text));

	// Get initial selection - should be at document start
	skb_text_selection_t initial_selection = skb_editor_get_current_selection(editor);
	ENSURE(initial_selection.start_pos.offset == 0);
	ENSURE(initial_selection.end_pos.offset == 0);

	// Test Option+Right (should jump to next word boundary)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_RIGHT, SKB_MOD_OPTION);
	
	skb_text_selection_t after_first_jump = skb_editor_get_current_selection(editor);
	// Should move to next word boundary (position depends on word boundary algorithm)
	ENSURE(after_first_jump.start_pos.offset > 0);
	ENSURE(after_first_jump.start_pos.offset == after_first_jump.end_pos.offset);
	int32_t first_word_end = after_first_jump.start_pos.offset;

	// Test Option+Right again (should jump to next word)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_RIGHT, SKB_MOD_OPTION);
	
	skb_text_selection_t after_second_jump = skb_editor_get_current_selection(editor);
	ENSURE(after_second_jump.start_pos.offset > first_word_end);
	ENSURE(after_second_jump.start_pos.offset == after_second_jump.end_pos.offset);
	int32_t second_word_end = after_second_jump.start_pos.offset;

	// Test Option+Left (should jump back to previous word boundary)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_LEFT, SKB_MOD_OPTION);
	
	skb_text_selection_t after_left_jump = skb_editor_get_current_selection(editor);
	ENSURE(after_left_jump.start_pos.offset < second_word_end);
	ENSURE(after_left_jump.start_pos.offset == after_left_jump.end_pos.offset);

	// Test Option+Left again (should jump back further)
	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_LEFT, SKB_MOD_OPTION);
	
	skb_text_selection_t after_second_left = skb_editor_get_current_selection(editor);
	ENSURE(after_second_left.start_pos.offset < after_left_jump.start_pos.offset);
	ENSURE(after_second_left.start_pos.offset == after_second_left.end_pos.offset);

	// Reset to start and test Shift+Option+Right (should select word)
	skb_text_position_t start_pos = {.offset = 0, .affinity = SKB_AFFINITY_TRAILING};
	skb_text_selection_t start_selection = {.start_pos = start_pos, .end_pos = start_pos};
	skb_editor_select(editor, start_selection);

	skb_text_selection_t reset_check = skb_editor_get_current_selection(editor);
	ENSURE(reset_check.start_pos.offset == 0);
	ENSURE(reset_check.end_pos.offset == 0);

	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_RIGHT, SKB_MOD_SHIFT | SKB_MOD_OPTION);
	
	skb_text_selection_t word_selection = skb_editor_get_current_selection(editor);
	// Should have a selection that spans some text
	ENSURE(word_selection.start_pos.offset == 0);
	ENSURE(word_selection.end_pos.offset > 0);

	// Test Shift+Option+Left from a position in the middle
	// Move to middle of text first
	skb_text_position_t middle_pos = {.offset = 20, .affinity = SKB_AFFINITY_TRAILING};
	skb_text_selection_t middle_selection = {.start_pos = middle_pos, .end_pos = middle_pos};
	skb_editor_select(editor, middle_selection);

	skb_text_selection_t middle_check = skb_editor_get_current_selection(editor);
	ENSURE(middle_check.start_pos.offset == 20);
	ENSURE(middle_check.end_pos.offset == 20);

	skb_temp_alloc_reset(temp_alloc);
	skb_editor_process_key_pressed(editor, temp_alloc, SKB_KEY_LEFT, SKB_MOD_SHIFT | SKB_MOD_OPTION);
	
	skb_text_selection_t reverse_word_selection = skb_editor_get_current_selection(editor);
	// Should have a selection going backwards
	ENSURE(reverse_word_selection.start_pos.offset != reverse_word_selection.end_pos.offset);
	ENSURE((reverse_word_selection.start_pos.offset == 20 && reverse_word_selection.end_pos.offset < 20) ||
	       (reverse_word_selection.start_pos.offset < 20 && reverse_word_selection.end_pos.offset == 20));

	skb_editor_destroy(editor);
	skb_font_collection_destroy(font_collection);
	skb_temp_alloc_destroy(temp_alloc);

	return 0;
}

int editor_tests(void)
{
	RUN_SUBTEST(test_init);
	RUN_SUBTEST(test_command_line_navigation_macos);
	RUN_SUBTEST(test_command_document_navigation_macos);
	RUN_SUBTEST(test_shift_command_text_selection_macos);
	RUN_SUBTEST(test_option_word_navigation_macos);
	return 0;
}
