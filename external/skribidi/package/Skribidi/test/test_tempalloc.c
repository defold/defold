// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "test_macros.h"
#include "skb_common.h"
#include "skb_common_internal.h"

static int test_alloc(void)
{
	skb_temp_alloc_t* a = skb_temp_alloc_create(128);

	uint8_t* ptr = skb_temp_alloc_alloc(a, 64);

	ENSURE(ptr);
	ENSURE(a->block_list != NULL);
	ENSURE(a->block_list->offset > 0);

	skb_temp_alloc_free(a, ptr);

	// Expect block to be returned to free blocks.
	ENSURE(a->block_list == NULL);
	ENSURE(a->free_list != NULL);

	skb_temp_alloc_destroy(a);

	return 0;
}

static int test_alloc_large_block(void)
{
	skb_temp_alloc_t* a = skb_temp_alloc_create(128);

	uint8_t* ptr = skb_temp_alloc_alloc(a, 256);

	ENSURE(ptr);
	ENSURE(a->block_list != NULL);
	ENSURE(a->block_list->cap >= 256);

	skb_temp_alloc_free(a, ptr);

	// This should not fit into the existing free block.
	uint8_t* ptr2 = skb_temp_alloc_alloc(a, 512);
	ENSURE(a->block_list != NULL);
	ENSURE(a->block_list->cap >= 512);
	ENSURE(a->free_list != NULL);

	skb_temp_alloc_destroy(a);

	return 0;
}

static int test_alloc_free_order(void)
{
	skb_temp_alloc_t* a = skb_temp_alloc_create(128);

	// Allocating and freeing in order should unwind the whole allocation.
	{
		uint8_t* ptr0 = skb_temp_alloc_alloc(a, 48);
		uint8_t* ptr1 = skb_temp_alloc_alloc(a, 48);
		uint8_t* ptr2 = skb_temp_alloc_alloc(a, 48);

		skb_temp_alloc_free(a, ptr2);
		skb_temp_alloc_free(a, ptr1);
		skb_temp_alloc_free(a, ptr0);

		ENSURE(a->block_list == NULL);
		ENSURE(a->free_list != NULL);
	}

	// Allocating in mixed order cannot free all.
	{
		uint8_t* ptr0 = skb_temp_alloc_alloc(a, 48);
		uint8_t* ptr1 = skb_temp_alloc_alloc(a, 48);
		uint8_t* ptr2 = skb_temp_alloc_alloc(a, 48);

		// Should have reused all the blocks
		ENSURE(a->free_list == NULL);

		skb_temp_alloc_free(a, ptr2);
		skb_temp_alloc_free(a, ptr0);	// This is out of order and cannot be rolled back.
		skb_temp_alloc_free(a, ptr1);

		ENSURE(a->block_list != NULL);
	}

	skb_temp_alloc_destroy(a);

	return 0;
}

static int num_used_blocks(skb_temp_alloc_t* a)
{
	int count = 0;
	for (skb_temp_alloc_block_t* b = a->block_list; b; b = b->next)
		count++;
	return count;
}

static int num_free_blocks(skb_temp_alloc_t* a)
{
	int count = 0;
	for (skb_temp_alloc_block_t* b = a->free_list; b; b = b->next)
		count++;
	return count;
}

static int test_alloc_reuse(void)
{
	skb_temp_alloc_t* a = skb_temp_alloc_create(128);

	uint8_t* ptr0 = skb_temp_alloc_alloc(a, 72);
	uint8_t* ptr1 = skb_temp_alloc_alloc(a, 345);
	uint8_t* ptr2 = skb_temp_alloc_alloc(a, 72);

	ENSURE(num_used_blocks(a) == 3);

	skb_temp_alloc_free(a, ptr2);
	skb_temp_alloc_free(a, ptr1);
	skb_temp_alloc_free(a, ptr0);

	ENSURE(num_used_blocks(a) == 0);
	ENSURE(num_free_blocks(a) == 3);

	// Expect the largest middle block to be reused, no new block allocations.
	uint8_t* ptr3 = skb_temp_alloc_alloc(a, 256);

	ENSURE(num_used_blocks(a) == 1);
	ENSURE(num_free_blocks(a) == 2);

	skb_temp_alloc_destroy(a);

	return 0;
}

static int test_alloc_reset(void)
{
	skb_temp_alloc_t* a = skb_temp_alloc_create(128);

	uint8_t* ptr0 = skb_temp_alloc_alloc(a, 72);
	uint8_t* ptr1 = skb_temp_alloc_alloc(a, 345);
	uint8_t* ptr2 = skb_temp_alloc_alloc(a, 72);

	// Reset should free all memory.
	skb_temp_alloc_reset(a);
	ENSURE(num_free_blocks(a) == 3);

	// Pages should be reused, even if allocated in different order.
	uint8_t* ptr3 = skb_temp_alloc_alloc(a, 345);
	uint8_t* ptr4 = skb_temp_alloc_alloc(a, 72);
	uint8_t* ptr5 = skb_temp_alloc_alloc(a, 72);

	ENSURE(num_used_blocks(a) == 3);
	ENSURE(num_free_blocks(a) == 0);

	skb_temp_alloc_destroy(a);

	return 0;
}

static int test_alloc_mark(void)
{
	skb_temp_alloc_t* a = skb_temp_alloc_create(32);

	uint8_t* ptr0 = skb_temp_alloc_alloc(a, 22); // block 0
	ENSURE(num_used_blocks(a) == 1);

	uint8_t* ptr1 = skb_temp_alloc_alloc(a, 4); // block 1
	ENSURE(num_used_blocks(a) == 2);

	skb_temp_alloc_mark_t mark = skb_temp_alloc_save(a);

	uint8_t* ptr2 = skb_temp_alloc_alloc(a, 4); // block 1
	ENSURE(num_used_blocks(a) == 2);

	uint8_t* ptr3 = skb_temp_alloc_alloc(a, 20); // block 2
	ENSURE(num_used_blocks(a) == 3);

	skb_temp_alloc_restore(a, mark);
	ENSURE(num_free_blocks(a) == 1);

	uint8_t* ptr4 = skb_temp_alloc_alloc(a, 4); // block 1
	ENSURE(num_free_blocks(a) == 1);

	uint8_t* ptr5 = skb_temp_alloc_alloc(a, 20); // block 2
	ENSURE(num_free_blocks(a) == 0);

	skb_temp_alloc_destroy(a);

	return 0;
}

int tempalloc_tests(void)
{
	RUN_SUBTEST(test_alloc);
	RUN_SUBTEST(test_alloc_large_block);
	RUN_SUBTEST(test_alloc_free_order);
	RUN_SUBTEST(test_alloc_reuse);
	RUN_SUBTEST(test_alloc_reset);
	RUN_SUBTEST(test_alloc_mark);

	return 0;
}
