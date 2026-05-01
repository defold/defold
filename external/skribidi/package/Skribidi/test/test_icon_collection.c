// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "test_macros.h"
#include "skb_icon_collection.h"
#include "skb_layout.h"

static int test_init(void)
{
	skb_icon_collection_t* icon_collection = skb_icon_collection_create();
	ENSURE(icon_collection != NULL);

	skb_icon_collection_destroy(icon_collection);

	return 0;
}

static int test_add_remove(void)
{
	skb_icon_collection_t* icon_collection = skb_icon_collection_create();
	ENSURE(icon_collection != NULL);

	skb_icon_handle_t icon_handle = skb_icon_collection_add_icon(icon_collection, "icon", 0.f, 0.f);
	ENSURE(icon_handle);

	const skb_icon_t* ptr = skb_icon_collection_get_icon(icon_collection, icon_handle);
	ENSURE(ptr);

	skb_icon_handle_t icon_handle2 = skb_icon_collection_find_icon(icon_collection, "icon");
	ENSURE(icon_handle2);

	skb_icon_collection_remove_icon(icon_collection, icon_handle);

	const skb_icon_t* ptr2 = skb_icon_collection_get_icon(icon_collection, icon_handle);
	ENSURE(ptr2 == NULL);

	skb_icon_handle_t icon_handle3 = skb_icon_collection_find_icon(icon_collection, "icon");
	ENSURE(icon_handle3 == 0);

	skb_icon_collection_destroy(icon_collection);

	return 0;
}

int icon_collection_tests(void)
{
	RUN_SUBTEST(test_init);
	RUN_SUBTEST(test_add_remove);

	return 0;
}
