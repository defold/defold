// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "test_macros.h"
#include "skb_rasterizer.h"

static int test_init(void)
{
	skb_rasterizer_t* rasterizer = skb_rasterizer_create(NULL);
	ENSURE(rasterizer != NULL);

	skb_rasterizer_destroy(rasterizer);

	return 0;
}

int rasterizer_tests(void)
{
	RUN_SUBTEST(test_init);
	return 0;
}
