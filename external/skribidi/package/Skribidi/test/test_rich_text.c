// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_rich_text.h"
#include "test_macros.h"

static int test_rich_text_create(void)
{
	skb_rich_text_t* rich_text = skb_rich_text_create();

	ENSURE(rich_text);

	ENSURE(skb_rich_text_get_paragraphs_count(rich_text) == 0);

	skb_rich_text_destroy(rich_text);

	return 0;
}

static int test_rich_text_replace(void)
{
	skb_temp_alloc_t* temp_alloc = skb_temp_alloc_create(1024);
	ENSURE(temp_alloc != NULL);

	int32_t text_count = 0;

	skb_rich_text_t* rich_text = skb_rich_text_create();

	ENSURE(rich_text);

	ENSURE(skb_rich_text_get_paragraphs_count(rich_text) == 0);

	skb_rich_text_t* ins_rich_text = skb_rich_text_create();
	skb_rich_text_append_utf8(ins_rich_text, temp_alloc, "Foo\nbar", -1, (skb_attribute_set_t){0});
	skb_rich_text_append_utf8(ins_rich_text, temp_alloc, "baz", -1, (skb_attribute_set_t){0});
	text_count = skb_rich_text_get_utf32_count(ins_rich_text);
	ENSURE(text_count == 10);
	ENSURE(skb_rich_text_get_paragraphs_count(ins_rich_text) == 2); // Foo\n | bar

	// Insert front
	skb_rich_text_insert(rich_text, (skb_text_range_t){0}, ins_rich_text);
	text_count = skb_rich_text_get_utf32_count(rich_text);
	ENSURE(text_count == 10);
	ENSURE(skb_rich_text_get_paragraphs_count(rich_text) == 2); // Foo\n | barbaz

	// Insert back
	skb_rich_text_insert(rich_text, (skb_text_range_t){.start.offset = text_count,.end.offset = text_count}, ins_rich_text);
	text_count = skb_rich_text_get_utf32_count(rich_text);
	ENSURE(text_count == 20);
	ENSURE(skb_rich_text_get_paragraphs_count(rich_text) == 3); // Foo\n | barbazFoo\n | barbaz

	// Insert middle
	skb_rich_text_insert(rich_text, (skb_text_range_t){.start.offset = 3,.end.offset = 14}, ins_rich_text);
	text_count = skb_rich_text_get_utf32_count(rich_text);
	ENSURE(text_count == 19);
	ENSURE(skb_rich_text_get_paragraphs_count(rich_text) == 2); // FooFoo\n | barbazbarbaz

	skb_rich_text_destroy(rich_text);
	skb_rich_text_destroy(ins_rich_text);

	skb_temp_alloc_destroy(temp_alloc);

	return 0;
}

static int test_rich_text_append(void)
{
	skb_temp_alloc_t* temp_alloc = skb_temp_alloc_create(1024);
	ENSURE(temp_alloc != NULL);

	skb_rich_text_t* rich_text = skb_rich_text_create();
	skb_rich_text_append_utf8(rich_text, temp_alloc, "123456", -1, (skb_attribute_set_t){0});

	skb_rich_text_t* rich_text2 = skb_rich_text_create();
	skb_rich_text_append_range(rich_text2, rich_text, (skb_text_range_t){ .start.offset = 2, .end.offset = 5 });
	ENSURE(skb_rich_text_get_utf32_count(rich_text2) == 3);

	skb_rich_text_t* rich_text3 = skb_rich_text_create();
	skb_rich_text_append_utf8(rich_text3, temp_alloc, "123\n456\n789", -1, (skb_attribute_set_t){0});

	skb_rich_text_t* rich_text4 = skb_rich_text_create();
	skb_rich_text_append_utf8(rich_text4, temp_alloc, "abc", -1, (skb_attribute_set_t){0});
	skb_rich_text_append_range(rich_text4, rich_text3, (skb_text_range_t){ .start.offset = 4, .end.offset = 10 });
	ENSURE(skb_rich_text_get_utf32_count(rich_text4) == 9);
	ENSURE(skb_rich_text_get_paragraphs_count(rich_text4) == 2); // abc456\n | 78

	skb_rich_text_destroy(rich_text);
	skb_rich_text_destroy(rich_text2);
	skb_rich_text_destroy(rich_text3);
	skb_rich_text_destroy(rich_text4);

	skb_temp_alloc_destroy(temp_alloc);

	return 0;
}

// Verifies that skb_rich_text_get_paragraph_range_from_text_range returns a
// range with start.paragraph_idx <= end.paragraph_idx even when the two
// endpoints tie on global_text_offset across a paragraph boundary.
static int test_paragraph_range_ordering(void)
{
	skb_temp_alloc_t* temp_alloc = skb_temp_alloc_create(1024);
	ENSURE(temp_alloc != NULL);

	skb_rich_text_t* rich_text = skb_rich_text_create();
	skb_rich_text_append_utf8(rich_text, temp_alloc, "\n", -1, (skb_attribute_set_t){0});
	// Produces two paragraphs: ["\n" global=0] and ["" global=1].
	ENSURE(skb_rich_text_get_paragraphs_count(rich_text) == 2);

	// start resolves to {paragraph_idx=1, text_offset=0, global=1} (last/empty paragraph).
	// end   resolves to {paragraph_idx=0, text_offset=1, global=1} (end of '\n' in para 0).
	// Both global offsets are 1, so ordering by global alone inverts start/end.
	skb_text_range_t range = {
		.start = { .offset = 1, .affinity = SKB_AFFINITY_NONE },
		.end   = { .offset = 0, .affinity = SKB_AFFINITY_EOL  },
	};
	skb_paragraph_range_t result = skb_rich_text_get_paragraph_range_from_text_range(rich_text, range, SKB_AFFINITY_USE);
	ENSURE(result.start.paragraph_idx <= result.end.paragraph_idx);

	skb_rich_text_destroy(rich_text);
	skb_temp_alloc_destroy(temp_alloc);

	return 0;
}

int rich_text_tests(void)
{
	RUN_SUBTEST(test_rich_text_create);
	RUN_SUBTEST(test_rich_text_replace);
	RUN_SUBTEST(test_rich_text_append);
	RUN_SUBTEST(test_paragraph_range_ordering);
	return 0;
}
