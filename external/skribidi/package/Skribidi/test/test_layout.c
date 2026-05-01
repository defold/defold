// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "test_macros.h"
#include "skb_layout.h"
#include "skb_font_collection.h"

static int test_init(void)
{
	skb_layout_params_t layout_params = {
		.font_collection = NULL,
	};

	skb_layout_t* layout = skb_layout_create(&layout_params);
	ENSURE(layout != NULL);

	skb_layout_destroy(layout);

	return 0;
}

static int test_missing_script(void)
{
	skb_temp_alloc_t* temp_alloc = skb_temp_alloc_create(1024);
	ENSURE(temp_alloc != NULL);

	skb_font_collection_t* font_collection = skb_font_collection_create();
	skb_font_handle_t font_handle = skb_font_collection_add_font(font_collection, "data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	ENSURE(font_handle);

	skb_layout_params_t layout_params = {
		.font_collection = font_collection,
	};
	skb_attribute_t attributes[] = {
		skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, 15.f, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
	};

	// The loaded font should not support the script of the text. We should still get a valid layout, but with invalid glyphs.
	skb_layout_t* layout = skb_layout_create_utf8(temp_alloc, &layout_params, "今天天气晴朗", -1, attributes, SKB_COUNTOF(attributes));
	ENSURE(layout != NULL);
	ENSURE(skb_layout_get_glyphs_count(layout) > 0);
	const skb_glyph_t* glyphs = skb_layout_get_glyphs(layout);
	ENSURE(glyphs[0].gid == 0);

	skb_layout_destroy(layout);
	skb_font_collection_destroy(font_collection);
	skb_temp_alloc_destroy(temp_alloc);

	return 0;
}

int layout_tests(void)
{
	RUN_SUBTEST(test_init);
	RUN_SUBTEST(test_missing_script);
	return 0;
}
