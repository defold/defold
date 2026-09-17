// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "test_macros.h"
#include "skb_image_atlas.h"

static int test_init(void)
{
	skb_image_atlas_t* atlas = skb_image_atlas_create(NULL);
	ENSURE(atlas != NULL);

	skb_image_atlas_destroy(atlas);

	return 0;
}

int image_atlas_tests(void)
{
	RUN_SUBTEST(test_init);
	return 0;
}
