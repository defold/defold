// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "test_macros.h"
#include "skb_font_collection.h"
#include "skb_layout.h"
#include <stdlib.h>
#include <stdbool.h>

static int test_init(void)
{
	skb_font_collection_t* font_collection = skb_font_collection_create();
	ENSURE(font_collection != NULL);

	skb_font_collection_destroy(font_collection);

	return 0;
}

static int test_add_remove(void)
{
	skb_font_collection_t* font_collection = skb_font_collection_create();
	ENSURE(font_collection != NULL);

	skb_font_handle_t font_handle = skb_font_collection_add_font(font_collection, "data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	ENSURE(font_handle);

	uint8_t script = skb_script_from_iso15924_tag(SKB_TAG_STR("Latn"));

	skb_font_handle_t font_handle2 = 0;
	int32_t count = skb_font_collection_match_fonts(font_collection, "", script, SKB_FONT_FAMILY_DEFAULT, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL, &font_handle2, 1);
	ENSURE(count == 1);
	ENSURE(font_handle2);

	bool removed = skb_font_collection_remove_font(font_collection, font_handle);
	ENSURE(removed);

	// Handle should be invalid now
	skb_font_t* font_ptr = skb_font_collection_get_font(font_collection, font_handle);
	ENSURE(font_ptr == NULL);

	// Should not find a font
	skb_font_handle_t font_handle3 = 0;
	int32_t count2 = skb_font_collection_match_fonts(font_collection, "", script, SKB_FONT_FAMILY_DEFAULT, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL, &font_handle3, 1);
	ENSURE(count2 == 0);
	ENSURE(font_handle3 == 0);


	skb_font_collection_destroy(font_collection);

	return 0;
}

// Test context structure for destroy callback
typedef struct {
	bool* destroyed;
	void* data;
} test_destroy_context_t;

// Test destroy callback
static void test_destroy_callback(void* context)
{
	test_destroy_context_t* ctx = (test_destroy_context_t*)context;
	if (ctx->destroyed) {
		*ctx->destroyed = true;
	}
	// In a real scenario, you would free ctx->data here
}

static int test_add_font_from_data(void)
{
	skb_font_collection_t* font_collection = skb_font_collection_create();
	ENSURE(font_collection != NULL);

	// Read font file into memory
	FILE* f = fopen("data/IBMPlexSans-Regular.ttf", "rb");
	ENSURE(f != NULL);

	fseek(f, 0, SEEK_END);
	long file_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	void* font_data = malloc(file_size);
	ENSURE(font_data != NULL);

	size_t read_size = fread(font_data, 1, file_size, f);
	ENSURE(read_size == (size_t)file_size);
	fclose(f);

	// Set up destroy callback tracking
	bool destroyed = false;
	test_destroy_context_t* context = malloc(sizeof(test_destroy_context_t));
	context->destroyed = &destroyed;
	context->data = font_data;

	// Add font from data
	skb_font_handle_t font_handle = skb_font_collection_add_font_from_data(
		font_collection,
		"IBMPlexSans-Regular",
		SKB_FONT_FAMILY_DEFAULT,
		font_data,
		file_size,
		context,
		test_destroy_callback);
	ENSURE(font_handle != 0);

	// Verify font can be found
	uint8_t script = skb_script_from_iso15924_tag(SKB_TAG_STR("Latn"));
	skb_font_handle_t found_handle = 0;
	int32_t count = skb_font_collection_match_fonts(font_collection, "", script, SKB_FONT_FAMILY_DEFAULT, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL, &found_handle, 1);
	ENSURE(count == 1);
	ENSURE(found_handle == font_handle);

	// Remove font - this should eventually trigger the destroy callback
	bool removed = skb_font_collection_remove_font(font_collection, font_handle);
	ENSURE(removed);

	// Destroy collection - this should ensure all resources are freed
	skb_font_collection_destroy(font_collection);

	// Verify destroy callback was called
	ENSURE(destroyed == true);

	// Clean up
	free(font_data);
	free(context);

	return 0;
}

static int test_add_font_failures(void)
{
	skb_font_collection_t* font_collection = skb_font_collection_create();
	ENSURE(font_collection != NULL);

	{
		// This should fail, since the font does not exists.
		skb_font_handle_t font_handle = skb_font_collection_add_font(font_collection, "data/TsippaDui.ttf", SKB_FONT_FAMILY_DEFAULT);
		ENSURE(font_handle == 0);
	}

	{
		// This should fail, since the file is not a font.
		skb_font_handle_t font_handle = skb_font_collection_add_font(font_collection, "data/astronaut_pico.svg", SKB_FONT_FAMILY_DEFAULT);
		ENSURE(font_handle == 0);
	}

	{
		// Null data, should fail.
		skb_font_handle_t font_handle = skb_font_collection_add_font_from_data(font_collection, "Missing", SKB_FONT_FAMILY_DEFAULT, NULL,0, NULL, NULL);
		ENSURE(font_handle == 0);
	}

	{
		// Invalid font data, should fail.
		uint8_t dummy_data[] = { 0, 1, 2, 3, 4, 5, 6 };
		skb_font_handle_t font_handle = skb_font_collection_add_font_from_data(font_collection, "Failing", SKB_FONT_FAMILY_DEFAULT, dummy_data, SKB_COUNTOF(dummy_data), NULL, NULL);
		ENSURE(font_handle == 0);
	}

	skb_font_collection_destroy(font_collection);

	return 0;
}

int font_collection_tests(void)
{
	RUN_SUBTEST(test_init);
	RUN_SUBTEST(test_add_remove);
	RUN_SUBTEST(test_add_font_from_data);
	RUN_SUBTEST(test_add_font_failures);
	return 0;
}
