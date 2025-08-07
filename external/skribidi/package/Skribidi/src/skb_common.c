// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_common.h"
#include "skb_common_internal.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emoji_data.h"

#ifdef _WIN32
#include <windows.h>

void skb_debug_log(const char* format, ...)
{
	char s[1025];
	va_list args;
	va_start(args, format);
	vsnprintf(s, sizeof(s) - 1, format, args);
	va_end(args);
	OutputDebugStringA(s);
}

static LARGE_INTEGER g_freq = {0};

int64_t skb_perf_timer_get(void)
{
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return t.QuadPart;
}

int64_t skb_perf_timer_elapsed_us(int64_t start, int64_t end)
{
	if (g_freq.QuadPart == 0)
		QueryPerformanceFrequency(&g_freq);
	return (end - start) * 1000000 / g_freq.QuadPart; // us
}

#elif defined(SKB_PLATFORM_POSIX)

#include <time.h> // For clock_gettime

void skb_debug_log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

int64_t skb_perf_timer_get(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

int64_t skb_perf_timer_elapsed_us(int64_t start, int64_t end)
{
	return (end - start) / 1000;
}

#else

#warning "Unsupported platform some feature might be missing."

void skb_debug_log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

int64_t skb_perf_timer_get(void)
{
	return 0;
}

int64_t skb_perf_timer_elapsed_us(int64_t start, int64_t end)
{
	return 0;
}

#endif

void* skb_malloc(size_t size)
{
	void* ptr = malloc(size);
	assert(ptr);
	return ptr;
}

void* skb_realloc(void* ptr, size_t new_size)
{
	void* new_ptr = realloc(ptr, new_size);
	assert(new_ptr);
	return new_ptr;
}

void skb_free(void* ptr)
{
	free(ptr);
}


//
// Transform
//

skb_mat2_t skb_mat2_inverse(skb_mat2_t t)
{
	const double det = (double)t.xx * t.yy - (double)t.xy * t.yx;
	if (det > -1e-6 && det < 1e-6)
		return skb_mat2_make_identity();

	const double inv_det = 1. / det;
	skb_mat2_t r;

	r.xx = (float)(t.yy * inv_det);
	r.xy = (float)(-t.xy * inv_det);
	r.dx = (float)(((double)t.xy * t.dy - (double)t.yy * t.dx) * inv_det);

	r.yx = (float)(-t.yx * inv_det);
	r.yy = (float)(t.xx * inv_det);
	r.dy = (float)(((double)t.yx * t.dx - (double)t.xx * t.dy) * inv_det);

	return r;
}

//
// Temp allocator
//

#define SKB_TEMPALLOC_HEADER_SIZE ((int32_t)sizeof(skb_temp_alloc_header_t))

static skb_temp_alloc_block_t* skb_temp_alloc_create_page_(int32_t req_size)
{
	int32_t alloc_size = skb_align((int32_t)sizeof(skb_temp_alloc_block_t), SKB_TEMPALLOC_ALIGN) + skb_align(req_size, SKB_TEMPALLOC_ALIGN);
	uint8_t* memory = skb_malloc(alloc_size); // TODO: align?

	skb_temp_alloc_block_t* block = (skb_temp_alloc_block_t*)memory;
	block->memory = memory + skb_align(sizeof(skb_temp_alloc_block_t), SKB_TEMPALLOC_ALIGN);
	block->cap = req_size;
	block->offset = 0;
	block->next = NULL;

	return block;
}

skb_temp_alloc_t* skb_temp_alloc_create(int32_t default_block_size)
{
	skb_temp_alloc_t* alloc = skb_malloc(sizeof(skb_temp_alloc_t));
	memset(alloc, 0, sizeof(skb_temp_alloc_t));

	alloc->default_block_size = default_block_size <= 0 ? SKB_TEMPALLOC_DEFAULT_BLOCK_SIZE : default_block_size;

	return alloc;
}

void skb_temp_alloc_destroy(skb_temp_alloc_t* alloc)
{
	if (!alloc) return;

	skb_temp_alloc_reset(alloc);
	skb_temp_alloc_block_t* block = alloc->free_list;
	while (block) {
		skb_temp_alloc_block_t* next_block = block->next;
		skb_free(block);
		block = next_block;
	}

	memset(alloc, 0, sizeof(*alloc));

	skb_free(alloc);
}

skb_temp_alloc_stats_t skb_temp_alloc_stats(skb_temp_alloc_t* alloc)
{
	skb_temp_alloc_stats_t stats = {0};

	for (skb_temp_alloc_block_t* block = alloc->free_list; block; block = block->next)
		stats.allocated += block->cap + skb_align(sizeof(skb_temp_alloc_block_t), SKB_TEMPALLOC_ALIGN);

	for (skb_temp_alloc_block_t* block = alloc->block_list; block; block = block->next) {
		stats.allocated += block->cap + skb_align(sizeof(skb_temp_alloc_block_t), SKB_TEMPALLOC_ALIGN);
		stats.used += block->offset;
	}

	return stats;
}

void skb_temp_alloc_reset(skb_temp_alloc_t* alloc)
{
	assert(alloc);

	// Move used blocks to freelist
	skb_temp_alloc_block_t* block = alloc->block_list;
	while (block) {
		skb_temp_alloc_block_t* next_block = block->next;
		// Reset allocation count
		block->offset = 0;
		// Add to free blocks.
		block->next = alloc->free_list;
		alloc->free_list = block;
		// Advance to next block
		block = next_block;
	}
	alloc->block_list = NULL;
}

skb_temp_alloc_mark_t skb_temp_alloc_save(skb_temp_alloc_t* alloc)
{
	if (!alloc->block_list)
		return (skb_temp_alloc_mark_t) {0};

	return (skb_temp_alloc_mark_t) {
		.block_num = alloc->block_list->num,
		.offset = alloc->block_list->offset,
	};
}

void skb_temp_alloc_restore(skb_temp_alloc_t* alloc, skb_temp_alloc_mark_t mark)
{
	skb_temp_alloc_block_t* block = alloc->block_list;

	// Early out if we have freed past the mark.
	if (block && block->num < mark.block_num)
		return;

	// Restore allocations up to the mark.
	while (block) {
		if (block->num == mark.block_num) {
			block->offset = mark.offset;
			break;
		}
		skb_temp_alloc_block_t* next_block = block->next;
		// Reset allocation count
		block->offset = 0;
		// Add to free blocks.
		block->next = alloc->free_list;
		alloc->free_list = block;
		// Advance to next block
		block = next_block;
	}

	// Set the block list to start at  the rolled back block.
	alloc->block_list = block;
}

void* skb_temp_alloc_alloc(skb_temp_alloc_t* alloc, int32_t size)
{
	skb_temp_alloc_block_t* cur_block = alloc->block_list;
	int32_t offset = cur_block ? skb_align(cur_block->offset + SKB_TEMPALLOC_HEADER_SIZE, SKB_TEMPALLOC_ALIGN) : 0;

	if (!cur_block || (offset + size) > cur_block->cap) {
		// Not enough space for the allocation, try to find a fitting block from freelist, or allocate new.
		const int32_t req_size = skb_align(SKB_TEMPALLOC_HEADER_SIZE, SKB_TEMPALLOC_ALIGN) + size;
		cur_block = NULL;
		if (alloc->free_list) {
			// Try to find a free block that has enough space for our allocation.
			skb_temp_alloc_block_t* prev_block = NULL;
			cur_block = alloc->free_list;
			while (cur_block) {
				if (cur_block->cap >= req_size) {
					// Found match, remove from the linked list.
					if (prev_block)
						prev_block->next = cur_block->next;
					else
						alloc->free_list = cur_block->next;
					cur_block->offset = 0;
					cur_block->next = NULL;
					break;
				}
				prev_block = cur_block;
				cur_block = cur_block->next;
			}
		}
		if (!cur_block) {
			// No free block available, allocate one.
			cur_block = skb_temp_alloc_create_page_(skb_maxi(alloc->default_block_size, req_size));
		}
		assert(cur_block);

		// Increment block number.
		cur_block->num = alloc->block_list ? alloc->block_list->num + 1 : 0;

		// Insert the block to active block list
		assert(cur_block != alloc->block_list);
		cur_block->next = alloc->block_list;
		alloc->block_list = cur_block;

		offset = skb_align(cur_block->offset + SKB_TEMPALLOC_HEADER_SIZE, SKB_TEMPALLOC_ALIGN);
		assert((offset + size) <= cur_block->cap);
		assert(cur_block != cur_block->next);
	}

	skb_temp_alloc_header_t* header = (skb_temp_alloc_header_t*)&cur_block->memory[offset - SKB_TEMPALLOC_HEADER_SIZE];
	header->restore_offset = cur_block->offset;
	header->top_offset = offset + size;

	cur_block->offset = offset + size;

	return &cur_block->memory[offset];
}


static void skb_try_rollback_last_alloc_(skb_temp_alloc_block_t* block, void* ptr)
{
	// Rollback the alloc if it was the last block in the current active block.
	const int32_t offset_in_block = (int32_t)((intptr_t)ptr - (intptr_t)block->memory);
	if (offset_in_block >= 0 && offset_in_block < block->cap) {
		skb_temp_alloc_header_t* header = (skb_temp_alloc_header_t*)((char*)ptr - SKB_TEMPALLOC_HEADER_SIZE);
		if (header->top_offset == block->offset)
			block->offset = header->restore_offset;
	}
}

static bool skb_allocator_owns_ptr_(skb_temp_alloc_t* alloc, void* ptr)
{
	// Make sure in debug builds that the allocation comes from this allocator.
	skb_temp_alloc_block_t* block = alloc->block_list;
	while (block) {
		const int32_t offset_in_block = (int32_t)((intptr_t)ptr - (intptr_t)block->memory);
		if (offset_in_block >= 0 && offset_in_block < block->cap)
			return true;
		block = block->next;
	}
	return false;
}

void* skb_temp_alloc_realloc(skb_temp_alloc_t* alloc, void* ptr, int32_t new_size)
{
	assert(new_size > 0);

	if (!ptr || !alloc->block_list)
		return skb_temp_alloc_alloc(alloc, new_size);

#if !defined(NDEBUG)
	assert(skb_allocator_owns_ptr_(alloc, ptr));
#endif

	skb_temp_alloc_header_t* header = (skb_temp_alloc_header_t*)((uint8_t*)ptr - SKB_TEMPALLOC_HEADER_SIZE);
	const int32_t offset = skb_align(header->restore_offset + SKB_TEMPALLOC_HEADER_SIZE, SKB_TEMPALLOC_ALIGN);
	const int32_t old_size = header->top_offset - offset;

	skb_temp_alloc_block_t* cur_block = alloc->block_list;

	// If this is last alloc in current block, try to expand the size in place.
	const int32_t offset_in_block = (int32_t)((intptr_t)ptr - (intptr_t)cur_block->memory);
	// Check that the allocation belongs to the active block.
	if (offset_in_block >= 0 && offset_in_block < cur_block->cap) {
		assert(offset_in_block == offset);
		// Check that the allocation is the last one (offset at the time is same as current offset).
		if (header->top_offset == cur_block->offset) {
			const int32_t change = new_size - old_size;
			// Check that the new size fits into the block.
			if ((header->top_offset + change) < cur_block->cap) {
				header->top_offset += change;
				cur_block->offset = header->top_offset;
				return ptr;
			}
		}
	}

	// Alloc likely creates new page, keep the old one to allow trying to rollback the old allocation.
	skb_temp_alloc_block_t* old_block = cur_block;

	// Could not resize, alloc new and copy.
	void* mem = skb_temp_alloc_alloc(alloc, new_size);
	memcpy(mem, ptr, old_size);

	// Rollback the old alloc if it was the last block in the current active block.
	skb_try_rollback_last_alloc_(old_block, ptr);

	return mem;
}

void skb_temp_alloc_free(skb_temp_alloc_t* alloc, void* ptr)
{
	if (ptr == NULL)
		return;
#if !defined(NDEBUG)
	assert(skb_allocator_owns_ptr_(alloc, ptr));
#endif

	skb_temp_alloc_block_t* cur_block = alloc->block_list;
	skb_try_rollback_last_alloc_(cur_block, ptr);

	// If the rollback was successful and the block got empty, return it to the freelist.
	if (cur_block->offset == 0) {
		// Set the next block as head.
		alloc->block_list = cur_block->next;
		// Return the current block to freelist.
		cur_block->next = alloc->free_list;
		alloc->free_list = cur_block;
	}
}



//
// Hash table
//

skb_hash_table_t* skb_hash_table_create(void)
{
	skb_hash_table_t* ht = skb_malloc(sizeof(skb_hash_table_t));
	memset(ht, 0, sizeof(skb_hash_table_t));
	ht->freelist = SKB_INVALID_INDEX;
	return ht;
}

void skb_hash_table_destroy(skb_hash_table_t* ht)
{
	skb_free(ht->buckets);
	skb_free(ht->items);
	memset(ht, 0, sizeof(skb_hash_table_t));
	skb_free(ht);
}

bool skb_hash_table_find(skb_hash_table_t* ht, uint64_t hash, int32_t* value)
{
	if (!ht->buckets)
		return false;

	const int32_t hash_index = (int32_t)(hash & (uint64_t)(ht->bucket_count - 1));
	int32_t index = ht->buckets[hash_index];
	while (index != SKB_INVALID_INDEX) {
		skb_hashtable_item_t* item = &ht->items[index];
		if (item->hash == hash) {
			if (value)
				*value = item->value;
			return true;
		}
		index = item->next;
	}

	return false;
}


bool skb_hash_table_add(skb_hash_table_t* ht, uint64_t hash, int32_t value)
{
	// Check if the hash already exists, and update value.
	if (ht->buckets) {
		const int32_t hash_index = (int32_t)(hash & (uint64_t)(ht->bucket_count - 1));
		int32_t index = ht->buckets[hash_index];
		while (index != SKB_INVALID_INDEX) {
			skb_hashtable_item_t* item = &ht->items[index];
			if (item->hash == hash) {
				item->value = value;
				return true; // Has already exists.
			}
			index = item->next;
		}
	}

	// Did not exist, add.
	int32_t item_index = SKB_INVALID_INDEX;
	if (ht->freelist != SKB_INVALID_INDEX) {
		// Pop from freelist if available
		item_index = ht->freelist;
		ht->freelist = ht->items[item_index].next;
	} else {
		// Nothing in freelist, check to see if we need to grow the buffer.
		if (ht->items_count+1 > ht->items_cap) {
			ht->items_cap = ht->items_cap ? (int32_t)(ht->items_cap + ht->items_cap/2) : 8;
			ht->items = skb_realloc(ht->items, sizeof(skb_hashtable_item_t) * ht->items_cap);
			assert(ht->items);

			// Redistribute the buckets if exceeding the load factor.
			const int32_t desired_bucket_count = skb_next_pow2((int32_t)(ht->items_cap * (1. / 0.75)));	// load factor of 0.75
			if (desired_bucket_count > ht->bucket_count) {
				// Allocate bigger bucket array.
				ht->bucket_count = desired_bucket_count;
				ht->buckets = skb_realloc(ht->buckets, sizeof(uint32_t) * desired_bucket_count);
				assert(ht->buckets);
				// rehash
				for (int32_t i = 0; i < ht->bucket_count; i++)
					ht->buckets[i] = SKB_INVALID_INDEX;
				for (int32_t i = 0; i < ht->items_count; i++) {
					skb_hashtable_item_t* item = &ht->items[i];
					if (item->hash) {
						const int32_t hash_index = (int32_t)(item->hash & (uint64_t)(ht->bucket_count-1));
						item->next = ht->buckets[hash_index];
						ht->buckets[hash_index] = i;
					}
				}
			}
		}

		item_index = ht->items_count;
		ht->items_count++;
	}

	// Init item and copy value.
	skb_hashtable_item_t* item = &ht->items[item_index];
	memset(item, 0, sizeof(skb_hashtable_item_t));
	item->hash = hash;
	item->value = value;

	// Add to bucket
	const int32_t hash_index = (int32_t)(hash & (uint64_t)(ht->bucket_count-1));
	item->next = ht->buckets[hash_index];
	ht->buckets[hash_index] = item_index;

	return false; // Hash did not exist.
}

bool skb_hash_table_remove(skb_hash_table_t* ht, uint64_t hash)
{
	// Find in the hash index
	const int32_t hash_index = (int32_t)(hash & (uint64_t)(ht->bucket_count - 1));
	int32_t prev_index = SKB_INVALID_INDEX;
	int32_t index = ht->buckets[hash_index];
	while (index != SKB_INVALID_INDEX) {
		if (ht->items[index].hash == hash)
			break;
		prev_index = index;
		index = ht->items[index].next;
	}
	if (index == SKB_INVALID_INDEX)
		return false;

	// Found, remove from the linked list.
	skb_hashtable_item_t* item = &ht->items[index];
	if (prev_index == SKB_INVALID_INDEX)
		ht->buckets[hash_index] = item->next;
	else
		ht->items[prev_index].next = item->next;

	// Clear data
	memset(item, 0, sizeof(skb_hashtable_item_t));

	// Add to freelist
	item->next = ht->freelist;
	ht->freelist = index;

	return true;
}

//
// Unicode helpers
//

static bool skb_find_range_(uint32_t codepoint, const uint32_t* ranges, int32_t count)
{
	int32_t min = 0;
	int32_t max = count - 1;
	while (min <= max) {
		const int32_t mid = (min + max) / 2;
		const uint32_t* mid_range = &ranges[mid * 2];
		if (codepoint < mid_range[0]) // start
			max = mid - 1;
		else if (codepoint > mid_range[1]) // end
			min = mid + 1;
		else
			return true;
	}
	return false;
}

bool skb_is_emoji_modifier_base(uint32_t codepoint)
{
	const bool in_range = codepoint >= emoji_modifier_base_min && codepoint <= emoji_modifier_base_max;
	return	in_range && skb_find_range_(codepoint, emoji_modifier_base_ranges, emoji_modifier_base_count);
}

bool skb_is_regional_indicator_symbol(uint32_t codepoint)
{
	return codepoint >= 0x1F1E6 && codepoint <= 0x1F1FF;
}

bool skb_is_emoji_modifier(uint32_t codepoint)
{
	return codepoint >= 0x1F3FB && codepoint <= 0x1F3FF;
}

bool skb_is_variation_selector(uint32_t codepoint)
{
	return codepoint >= 0xFE00 && codepoint <= 0xFE0F;
}

bool skb_is_keycap_base(uint32_t codepoint)
{
	return (codepoint >= '0' && codepoint <= '9') || codepoint == '#' || codepoint == '*';
}

bool skb_is_tag_spec_char(uint32_t codepoint)
{
	return codepoint >= 0xE0020  && codepoint <= 0xE007E;
}

bool skb_is_paragraph_separator(uint32_t codepoint)
{
	return codepoint == SKB_CHAR_LINE_FEED
			|| codepoint == SKB_CHAR_VERTICAL_TAB
			|| codepoint == SKB_CHAR_FORM_FEED
			|| codepoint == SKB_CHAR_CARRIAGE_RETURN
			|| codepoint == SKB_CHAR_NEXT_LINE
			|| codepoint == SKB_CHAR_LINE_SEPARATOR
			|| codepoint == SKB_CHAR_PARAGRAPH_SEPARATOR;
}

bool skb_is_emoji_presentation(uint32_t codepoint)
{
	const bool in_range = codepoint >= emoji_presentation_min && codepoint <= emoji_presentation_max;
	return in_range && skb_find_range_(codepoint, emoji_presentation_ranges, emoji_presentation_count);
}

bool skb_is_emoji(uint32_t codepoint)
{
	const bool in_range = codepoint >= emoji_min && codepoint <= emoji_max;
	return in_range && skb_find_range_(codepoint, emoji_ranges, emoji_count);
}

enum skb_emoji_scanner_category {
	EMOJI = 0,
	EMOJI_TEXT_PRESENTATION = 1,
	EMOJI_EMOJI_PRESENTATION = 2,
	EMOJI_MODIFIER_BASE = 3,
	EMOJI_MODIFIER = 4,
	EMOJI_VS_BASE = 5,
	REGIONAL_INDICATOR = 6,
	KEYCAP_BASE = 7,
	COMBINING_ENCLOSING_KEYCAP = 8,
	COMBINING_ENCLOSING_CIRCLE_BACKSLASH = 9,
	ZWJ = 10,
	VS15 = 11,
	VS16 = 12,
	TAG_BASE = 13,
	TAG_SEQUENCE = 14,
	TAG_TERM = 15,
	MAX_EMOJI_CATEGORY = 16
};

static uint8_t skb__emoji_segmentation_category(uint32_t codepoint)
{
	// Specific ones first.
	switch (codepoint) {
		case SKB_CHAR_COMBINING_ENCLOSING_KEYCAP:
			return COMBINING_ENCLOSING_KEYCAP;
		case SKB_CHAR_COMBINING_ENCLOSING_CIRCLE_BACKSLASH:
			return COMBINING_ENCLOSING_CIRCLE_BACKSLASH;
		case SKB_CHAR_ZERO_WIDTH_JOINER:
			return ZWJ;
		case SKB_char_VARIATION_SELECTOR15:
			return VS15;
		case SKB_CHAR_VARIATION_SELECTOR16:
			return VS16;
		case SKB_CHAR_REGIONAL_INDICATOR_BASE:
			return TAG_BASE;
		case SKB_CHAR_CANCEL_TAG:
			return TAG_TERM;
		default:
			break;
	}

	if ((codepoint >= 0xE0030 && codepoint <= 0xE0039) ||
		(codepoint >= 0xE0061 && codepoint <= 0xE007A))
		return TAG_SEQUENCE;
	if (skb_is_emoji_modifier(codepoint))
		return EMOJI_MODIFIER;
	if (skb_is_regional_indicator_symbol(codepoint))
		return REGIONAL_INDICATOR;
	if (skb_is_keycap_base(codepoint))
		return KEYCAP_BASE;

	// More expensive binary search ones last.
	if (skb_is_emoji_modifier_base(codepoint))
		return EMOJI_MODIFIER_BASE;
	if (skb_is_emoji_presentation(codepoint))
		return EMOJI_EMOJI_PRESENTATION;
	if (skb_is_emoji(codepoint))
		return EMOJI_TEXT_PRESENTATION;
	// Note: Omitting EMOJI, since it's (EMOJI_EMOJI_PRESENTATION || EMOJI_TEXT_PRESENTATION), thus redundant.

	// Ragel state machine will interpret unknown category as "any".
	return MAX_EMOJI_CATEGORY;
}

// The scanner is genereated file, copied from: https://github.com/google/emoji-segmenter
typedef const uint8_t* emoji_text_iter_t;
#include "emoji_presentation_scanner.c"

skb_emoji_run_iterator_t skb_emoji_run_iterator_make(skb_range_t range, const uint32_t* text, uint8_t* emoji_category_buffer)
{
	const int32_t range_count = range.end - range.start;

	skb_emoji_run_iterator_t iter = {
		.offset = range.start,
		.count = range_count,
		.emoji_category = emoji_category_buffer,
	};

	// Parse categories
	for (int32_t i = 0; i < range_count; i++)
		iter.emoji_category[i] = skb__emoji_segmentation_category(text[range.start + i]);

	// Parse first item
	iter.pos = 0;
	iter.start = 0;
	bool is_emoji = false;
	bool has_vs = false;
	const uint8_t* next = scan_emoji_presentation(iter.emoji_category + iter.pos, iter.emoji_category + iter.count, &is_emoji, &has_vs);
	iter.pos = (int32_t)(next - iter.emoji_category);
	iter.has_emoji = is_emoji;

	return iter;
}

bool skb_emoji_run_iterator_next(skb_emoji_run_iterator_t* iter, skb_range_t* range, bool* range_has_emojis)
{
	if (iter->start == iter->count)
		return false;

	// Parse run of same type (emoji or not).
	bool is_emoji = false;
	bool has_vs = false;
	int32_t end = iter->count;
	while (iter->pos < iter->count) {
		int32_t cur_pos = iter->pos;
		const uint8_t* next = scan_emoji_presentation(iter->emoji_category + iter->pos, iter->emoji_category + iter->count, &is_emoji, &has_vs);
		iter->pos = (int32_t)(next - iter->emoji_category);
		if (iter->has_emoji != is_emoji) {
			end = cur_pos;
			break;
		}
	}

	range->start = iter->offset + iter->start;
	range->end = iter->offset + end;
	*range_has_emojis = iter->has_emoji;

	// Restart new run.
	iter->has_emoji = is_emoji;
	iter->start = end;

	return true;
}

//
// Character conversions
//

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

enum {
	SKB_UTF8_ACCEPT = 0,
	SKB_UTF8_REJECT = 12,
};

static uint32_t skb_decutf8_(uint32_t* state, uint32_t* codep, uint8_t byte)
{
	static const uint8_t utf8d[] = {
		// The first part of the table maps bytes to character classes that
		// to reduce the size of the transition table and create bitmasks.
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

		// The second part is a transition table that maps a combination
		// of a state of the automaton and a character class to a state.
		0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
		12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
		12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
		12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
		12,36,12,12,12,12,12,12,12,12,12,12,
	};

	uint32_t type = utf8d[byte];

	*codep = (*state != SKB_UTF8_ACCEPT) ?
		(byte & 0x3fu) | (*codep << 6) :
		(0xff >> type) & (byte);

	*state = utf8d[256 + *state + type];
	return *state;
}

int32_t skb_utf8_to_utf32(const char* utf8, int32_t utf8_len, uint32_t* utf32, int32_t utf32_cap)
{
	int32_t cp_count = 0;
	uint32_t state = 0;
	int32_t idx = 0;
	while (idx < utf8_len) {
		uint32_t cp;
		if (skb_decutf8_(&state, &cp, utf8[idx]) == SKB_UTF8_ACCEPT) {
			if (utf32 && cp_count < utf32_cap)
				utf32[cp_count] = cp;
			cp_count++;
		}
		idx++;
	}
	return cp_count;
}

int32_t skb_utf8_to_utf32_count(const char* utf8, int32_t utf8_len)
{
	int32_t cp_count = 0;
	uint32_t state = 0;
	int32_t idx = 0;
	while (idx < utf8_len) {
		uint32_t cp;
		if (skb_decutf8_(&state, &cp, utf8[idx]) == SKB_UTF8_ACCEPT)
			cp_count++;
		idx++;
	}
	return cp_count;
}


int32_t skb_utf8_codepoint_offset(const char* utf8, int32_t utf8_len, int32_t codepoint_offset)
{
	int32_t cp_count = 0;
	uint32_t state = 0;
	int32_t start_idx = 0;
	int32_t idx = 0;
	while (idx < utf8_len) {
		uint32_t cp;
		if (skb_decutf8_(&state, &cp, utf8[idx]) == SKB_UTF8_ACCEPT) {
			if (cp_count == codepoint_offset)
				return start_idx;
			start_idx = idx + 1;
			cp_count++;
		}
		idx++;
	}
	return start_idx;
}

int32_t skb_utf8_num_units(uint32_t cp)
{
	if (cp < 0x80) return 1;
	if (cp < 0x800) return 2;
	if (cp < 0x10000) return 3;
	if (cp < 0x200000) return 4;
	return 0;
}

int32_t skb_utf8_encode(uint32_t cp, char* utf8, int32_t utf8_cap)
{
	if (cp < 0x80 && utf8_cap >= 1) {
		utf8[0] = (char)cp;
		return 1;
	} else if (cp < 0x800 && utf8_cap >= 2) {
		utf8[0] = (char)(0xc0 | (cp >> 6));
		utf8[1] = (char)((0x80 | (cp & 0x3f)));
		return 2;
	} else if (cp < 0x10000 && utf8_cap >= 3) {
		utf8[0] = (char)(0xe0 | (cp >> 12));
		utf8[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
		utf8[2] = (char)(0x80 | (cp & 0x3f));
		return 3;
	} else if (cp < 0x200000 && utf8_cap >= 4) {
		utf8[0] = (char)(0xf0 | (cp >> 18));
		utf8[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
		utf8[2] = (char)(0x80 | ((cp >> 6)  & 0x3f));
		utf8[3] = (char)(0x80 | (cp & 0x3f));
		return 4;
	}
	return 0;
}

int32_t skb_utf32_to_utf8(const uint32_t* utf32, int32_t utf32_len, char* utf8, int32_t utf8_cap)
{
	if (!utf32) return 0;
	int32_t idx = 0;
	int32_t count = 0;
	while (idx < utf32_len) {
		if (utf8)
			count += skb_utf8_encode(utf32[idx], &utf8[count], utf8_cap - count);
		else
			count += skb_utf8_num_units(utf32[idx]);
		idx++;
	}

	return count;
}

int32_t skb_utf32_to_utf8_count(const uint32_t* utf32, int32_t utf32_len)
{
	if (!utf32) return 0;
	int32_t idx = 0;
	int32_t count = 0;
	while (idx < utf32_len) {
		count += skb_utf8_num_units(utf32[idx]);
		idx++;
	}

	return count;
}

int32_t skb_utf32_strlen(const uint32_t* utf32)
{
	if (!utf32) return 0;
	int32_t count = 0;
	while (utf32[count])
		count++;
	return count;
}
