// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "test_macros.h"
#include "skb_layout_cache.h"

static int test_init(void)
{
	skb_layout_cache_t* layout_cache = skb_layout_cache_create();
	ENSURE(layout_cache != NULL);

	skb_layout_cache_destroy(layout_cache);

	return 0;
}

int layout_cache_tests(void)
{
	RUN_SUBTEST(test_init);
	return 0;
}
