// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "test_macros.h"

#include "skb_canvas.h"
#include "skb_common.h"

static int test_init(void)
{
	skb_temp_alloc_t* temp_alloc = skb_temp_alloc_create(512*1024);
	ENSURE(temp_alloc != NULL);

	skb_image_t image = {
		.width = 100,
		.height = 100,
		.bpp = 4,
	};
	image.buffer = skb_malloc(image.width * image.height * image.bpp);
	image.stride_bytes = image.width * image.bpp;

	skb_canvas_t* canvas = skb_canvas_create(temp_alloc, &image);
	ENSURE(canvas != NULL);

	skb_canvas_destroy(canvas);
	skb_free(image.buffer);
	skb_temp_alloc_destroy(temp_alloc);

	return 0;
}

int canvas_tests(void)
{
	RUN_SUBTEST(test_init);
	return 0;
}
