// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_common.h"
#include "skb_common_internal.h"
#include "test_macros.h"

static int test_add_get(void)
{
	skb_hash_table_t* ht = skb_hash_table_create();

	uint64_t h0 = 123;
	uint64_t h1 = 456;

	{
		int32_t d0 = 0xf00;
		int32_t d1 = 0xabba;

		bool exists0 = skb_hash_table_add(ht, h0, d0);
		bool exists1 = skb_hash_table_add(ht, h1, d1);

		ENSURE(!exists0);
		ENSURE(!exists1);

		int32_t find_d1 = 0;
		skb_hash_table_find(ht, h1, &find_d1);
		ENSURE(find_d1 == d1);
		int32_t find_d0 = 0;
		skb_hash_table_find(ht, h0, &find_d0);
		ENSURE(find_d0 == d0);
	}

	{
		// replace value
		int32_t d2 = 0x123;
		bool exists2 = skb_hash_table_add(ht, h0, d2);
		ENSURE(exists2);

		int32_t find_d2 = 0;
		skb_hash_table_find(ht, h0, &find_d2);
		ENSURE(find_d2 == d2);
	}

	skb_hash_table_destroy(ht);

	return 0;
}

static int test_remove(void)
{
	skb_hash_table_t* ht = skb_hash_table_create();

	uint64_t h0 = 123;
	uint64_t h1 = 456;

	int d0 = 12;
	int d1 = 45;

	{
		skb_hash_table_add(ht, h0, d0);
		skb_hash_table_add(ht, h1, d1);

		bool h0_exists = skb_hash_table_find(ht, h0, NULL);
		ENSURE(h0_exists);
		bool h1_exists = skb_hash_table_find(ht, h1, NULL);
		ENSURE(h1_exists);
	}

	{
		skb_hash_table_remove(ht, h0);

		bool h0_exists = skb_hash_table_find(ht, h0, NULL);
		ENSURE(!h0_exists);
		bool h1_exists = skb_hash_table_find(ht, h1, NULL);
		ENSURE(h1_exists);
	}

	{
		bool remove1a = skb_hash_table_remove(ht, h1);
		bool remove1b = skb_hash_table_remove(ht, h1);
		ENSURE(remove1a);
		ENSURE(!remove1b);

		bool h0_exists = skb_hash_table_find(ht, h0, NULL);
		ENSURE(!h0_exists);
		bool h1_exists = skb_hash_table_find(ht, h1, NULL);
		ENSURE(!h1_exists);
	}

	skb_hash_table_destroy(ht);

	return 0;
}

int hashtable_tests(void)
{
	RUN_SUBTEST(test_add_get);
	RUN_SUBTEST(test_remove);

	return 0;
}
