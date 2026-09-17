// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_COMMON_INTERNAL_H
#define SKB_COMMON_INTERNAL_H

#include <stdint.h>

#define SKB_SET_FLAG(flags, bit_mask, condition) \
	if (condition) \
		(flags) |= (bit_mask); \
	else \
		(flags) &= ~(bit_mask);

typedef struct skb_hashtable_item_t {
	uint64_t hash;				// Unique hash that can be used to locate the item. 0 if item not in use.
	int32_t next;				// The next item in hash lookup or freelist chain, SKB_INVALID_INDEX if no next item.
	int32_t value;				// Value associated with the hash.
} skb_hashtable_item_t;

typedef struct skb_hash_table_t {
	int32_t* buckets;				// Index to first item based on part of the hash.
	skb_hashtable_item_t* items;	// Array of items in the items, items[i].hash is != 0 if the item is in use.
	int32_t items_count;			// Number of items (some may not be active).
	int32_t items_cap;				// Number of allocated items.
	int32_t bucket_count;			// Number of buckets, must be pow2.
	int32_t freelist;				// Index to first free item.
} skb_hash_table_t;


typedef struct skb_temp_alloc_block_t {
	struct skb_temp_alloc_block_t* next;	// Next block in the chain, used for used and free page lists. 
	uint8_t* memory;	// Pointer to the allocated memory.
	int32_t cap;		// Size of the allocated memory.
	int32_t offset;		// Current offset of allocated memory.
	int32_t num;		// Block number
} skb_temp_alloc_block_t;

typedef struct skb_temp_alloc_t {
	int32_t default_block_size;	// Default size for a new block. Larger ones may be allocated if required.
	skb_temp_alloc_block_t* block_list;	// Linked list of used blocks, first one is used for allocations.
	skb_temp_alloc_block_t* free_list;	// Linked list of free blocks.
} skb_temp_alloc_t;

typedef struct skb_temp_alloc_header_t {
	int32_t restore_offset;
	int32_t top_offset;
} skb_temp_alloc_header_t;

#endif // SKB_COMMON_INTERNAL_H