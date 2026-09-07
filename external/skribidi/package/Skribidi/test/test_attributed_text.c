// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include <string.h>

#include "test_macros.h"
#include "skb_text.h"

static int test_create(void)
{
	skb_text_t* text = skb_text_create();

	ENSURE(text);

	ENSURE(skb_text_get_utf32_count(text) == 0);

	skb_text_destroy(text);

	return 0;
}

static bool text_cmp(const uint32_t* a, int32_t a_count, const char* b)
{
	int32_t b_count = strlen(b);
	int32_t b32_count = skb_utf8_to_utf32_count(b, b_count);
	if (b32_count != a_count)
		return false;

	uint32_t* b32 = skb_malloc(b32_count * sizeof(uint32_t));
	skb_utf8_to_utf32(b, b_count, b32, b32_count);

	for (int32_t i = 0; i < a_count; i++) {
		if (a[i] != b32[i])
			return false;
	}

	skb_free(b32);

	return true;
}

static int test_add_remove(void)
{

	skb_text_t* text = skb_text_create();

	ENSURE(text);

	ENSURE(skb_text_get_utf32_count(text) == 0);

	skb_attribute_t attributes1[] = {
		skb_attribute_make_font_size(15.f),
	};
	const char* str1 = "Hello";
	skb_text_append_utf8(text, str1, -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(attributes1));

	{
		ENSURE(skb_text_get_utf32_count(text) == 5);
		ENSURE(text_cmp(skb_text_get_utf32(text), skb_text_get_utf32_count(text), "Hello"));
	}

	{
		ENSURE(skb_text_get_attribute_spans_count(text) == 1);
		const skb_attribute_span_t* spans = skb_text_get_attribute_spans(text);
		ENSURE(spans[0].text_range.start == 0);
		ENSURE(spans[0].text_range.end == 5);
		ENSURE(spans[0].attribute.font_size.size == 15.f);
	}


	skb_text_remove(text, (skb_text_range_t){ .start.offset = 1,.end.offset = 3 }); // end non-inclusive

	{
		ENSURE(skb_text_get_utf32_count(text) == 3);
		ENSURE(text_cmp(skb_text_get_utf32(text), skb_text_get_utf32_count(text), "Hlo"));
	}

	{
		ENSURE(skb_text_get_attribute_spans_count(text) == 1);
		const skb_attribute_span_t* spans = skb_text_get_attribute_spans(text);
		ENSURE(spans[0].text_range.start == 0);
		ENSURE(spans[0].text_range.end == 3);
		ENSURE(spans[0].attribute.font_size.size == 15.f);
	}

	skb_attribute_t attributes2[] = {
		skb_attribute_make_font_size(30.f),
	};
	const char* str2 = "Turb";
	skb_text_insert_utf8(text, (skb_text_range_t){.start.offset = 0, .end.offset = 2}, str2, -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(attributes2));

	{
		ENSURE(skb_text_get_utf32_count(text) == 5);
		ENSURE(text_cmp(skb_text_get_utf32(text), skb_text_get_utf32_count(text), "Turbo"));
	}

	{
		ENSURE(skb_text_get_attribute_spans_count(text) == 2);
		const skb_attribute_span_t* spans = skb_text_get_attribute_spans(text);
		ENSURE(spans[0].text_range.start == 0);
		ENSURE(spans[0].text_range.end == 4);
		ENSURE(spans[0].attribute.font_size.size == 30.f);

		ENSURE(spans[1].text_range.start == 4);
		ENSURE(spans[1].text_range.end == 5);
		ENSURE(spans[1].attribute.font_size.size == 15.f);
	}

	skb_attribute_t attributes3[] = {
		skb_attribute_make_font_size(90.f),
	};
	const char* str3 = "ku Å";
	skb_text_insert_utf8(text, (skb_text_range_t){.start.offset = 3, .end.offset = 3}, str3, -1, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(attributes3));

	{
		ENSURE(skb_text_get_utf32_count(text) == 9);
		ENSURE(text_cmp(skb_text_get_utf32(text), skb_text_get_utf32_count(text), "Turku Åbo"));
	}

	{
		ENSURE(skb_text_get_attribute_spans_count(text) == 4);
		const skb_attribute_span_t* spans = skb_text_get_attribute_spans(text);
		ENSURE(spans[0].text_range.start == 0);
		ENSURE(spans[0].text_range.end == 3);
		ENSURE(spans[0].attribute.font_size.size == 30.f);

		ENSURE(spans[1].text_range.start == 3);
		ENSURE(spans[1].text_range.end == 7);
		ENSURE(spans[1].attribute.font_size.size == 90.f);

		ENSURE(spans[2].text_range.start == 7);
		ENSURE(spans[2].text_range.end == 8);
		ENSURE(spans[2].attribute.font_size.size == 30.f);

		ENSURE(spans[3].text_range.start == 8);
		ENSURE(spans[3].text_range.end == 9);
		ENSURE(spans[3].attribute.font_size.size == 15.f);
	}

	skb_attribute_t attr_font_size = skb_attribute_make_font_size(90.f);

	skb_text_clear_attribute(text, (skb_text_range_t){ .start.offset =  3, .end.offset = 8}, attr_font_size);
	{
		ENSURE(skb_text_get_utf32_count(text) == 9);

		ENSURE(skb_text_get_attribute_spans_count(text) == 2);
		const skb_attribute_span_t* spans = skb_text_get_attribute_spans(text);
		ENSURE(spans[0].text_range.start == 0);
		ENSURE(spans[0].text_range.end == 3);
		ENSURE(spans[0].attribute.font_size.size == 30.f);

		ENSURE(spans[1].text_range.start == 8);
		ENSURE(spans[1].text_range.end == 9);
		ENSURE(spans[1].attribute.font_size.size == 15.f);
	}

	skb_text_destroy(text);

	return 0;
}


typedef struct attr_range_t {
	skb_text_range_t range;
	int32_t active_span_count;
} attr_range_t;

enum {
	MAX_EXPECTED_RANGES = 10,
};
typedef struct attr_iter_context_t {
	int32_t idx;
	attr_range_t expected_ranges[MAX_EXPECTED_RANGES];
} attr_iter_context_t;

static void iter_test(const skb_text_t* text, skb_text_range_t range, skb_attribute_span_t** active_spans, int32_t active_spans_count, void* context)
{
	attr_iter_context_t* ctx = context;
	if (ctx->idx < MAX_EXPECTED_RANGES) {
		ctx->expected_ranges[ctx->idx].range = range;
		ctx->expected_ranges[ctx->idx].active_span_count = active_spans_count;
	}
	ctx->idx++;
}

static int test_iter(void)
{
	skb_text_t* text = skb_text_create();

	ENSURE(text);

	const char* str1 = "Hamburgerfontstiv";
	skb_text_append_utf8(text, str1, -1, (skb_attribute_set_t){0});

	skb_text_add_attribute(text, (skb_text_range_t){ .start.offset = 1, .end.offset = 9 }, skb_attribute_make_font_size(30.f));
	skb_text_add_attribute(text, (skb_text_range_t){ .start.offset = 4, .end.offset = 7 }, skb_attribute_make_font_weight(SKB_WEIGHT_BOLD));
	skb_text_add_attribute(text, (skb_text_range_t){ .start.offset = 8, .end.offset = 12 }, skb_attribute_make_font_style(SKB_STYLE_ITALIC));

	attr_iter_context_t iter_ctx = { 0 };
	skb_text_iterate_attribute_runs(text, iter_test, &iter_ctx);

	ENSURE(iter_ctx.idx == 7); // Expect 7 runs

	// Empty at start
	ENSURE(iter_ctx.expected_ranges[0].range.start.offset == 0);
	ENSURE(iter_ctx.expected_ranges[0].range.end.offset == 1);
	ENSURE(iter_ctx.expected_ranges[0].active_span_count == 0);

	// Font size
	ENSURE(iter_ctx.expected_ranges[1].range.start.offset == 1);
	ENSURE(iter_ctx.expected_ranges[1].range.end.offset == 4);
	ENSURE(iter_ctx.expected_ranges[1].active_span_count == 1);

	// Font size + Bold
	ENSURE(iter_ctx.expected_ranges[2].range.start.offset == 4);
	ENSURE(iter_ctx.expected_ranges[2].range.end.offset == 7);
	ENSURE(iter_ctx.expected_ranges[2].active_span_count == 2);

	// Font size
	ENSURE(iter_ctx.expected_ranges[3].range.start.offset == 7);
	ENSURE(iter_ctx.expected_ranges[3].range.end.offset == 8);
	ENSURE(iter_ctx.expected_ranges[3].active_span_count == 1);

	// Font size + Italic
	ENSURE(iter_ctx.expected_ranges[4].range.start.offset == 8);
	ENSURE(iter_ctx.expected_ranges[4].range.end.offset == 9);
	ENSURE(iter_ctx.expected_ranges[4].active_span_count == 2);

	// Italic
	ENSURE(iter_ctx.expected_ranges[5].range.start.offset == 9);
	ENSURE(iter_ctx.expected_ranges[5].range.end.offset == 12);
	ENSURE(iter_ctx.expected_ranges[5].active_span_count == 1);

	// Empty at end
	ENSURE(iter_ctx.expected_ranges[6].range.start.offset == 12);
	ENSURE(iter_ctx.expected_ranges[6].range.end.offset == 17);
	ENSURE(iter_ctx.expected_ranges[6].active_span_count == 0);

	skb_text_destroy(text);

	return 0;
}

int attributed_text_tests(void)
{
	RUN_SUBTEST(test_create);
	RUN_SUBTEST(test_add_remove);
	RUN_SUBTEST(test_iter);
	return 0;
}
