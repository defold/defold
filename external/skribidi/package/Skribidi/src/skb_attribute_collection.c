// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_attribute_collection.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "skb_attributes.h"

typedef struct skb__attribute_set_t {
	char* name;
	char* group_name;
	skb_attribute_t* attributes;
	int32_t attributes_count;
	int32_t attributes_cap;
	skb_attribute_set_handle_t handle;
} skb__attribute_set_t;

typedef struct skb_attribute_collection_t {
	uint32_t id; // Unique id of the icon collection.

	skb_hash_table_t* attribute_name_lookup;
	skb_hash_table_t* attribute_group_lookup;
	int32_t group_id_gen;

	skb__attribute_set_t* attribute_sets;
	int32_t attribute_sets_count;
	int32_t attribute_sets_cap;

} skb_attribute_collection_t;

skb_attribute_collection_t* skb_attribute_collection_create(void)
{
	static uint32_t id = 0;

	skb_attribute_collection_t* result = skb_malloc(sizeof(skb_attribute_collection_t));
	memset(result, 0, sizeof(skb_attribute_collection_t));

	result->attribute_name_lookup = skb_hash_table_create();
	result->attribute_group_lookup = skb_hash_table_create();

	result->id = ++id;

	return result;
}

	void skb_attribute_collection_destroy(skb_attribute_collection_t* attribute_collection)
{
	if (!attribute_collection) return;

	for (int32_t i = 0; i < attribute_collection->attribute_sets_count; i++) {
		skb__attribute_set_t* set = &attribute_collection->attribute_sets[i];
		skb_free(set->name);
		skb_free(set->group_name);
		skb_free(set->attributes);
	}
	skb_free(attribute_collection->attribute_sets);

	skb_hash_table_destroy(attribute_collection->attribute_name_lookup);
	skb_hash_table_destroy(attribute_collection->attribute_group_lookup);
	skb_free(attribute_collection);
}

uint32_t skb_attribute_collection_get_id(const skb_attribute_collection_t* attribute_collection)
{
	assert(attribute_collection);
	return attribute_collection->id;
}

skb_attribute_set_handle_t skb_attribute_collection_add_set(skb_attribute_collection_t* attribute_collection, const char* name, skb_attribute_set_t attributes)
{
	return skb_attribute_collection_add_set_with_group(attribute_collection, name, name, attributes);
}

static uint64_t skb__make_handle(int32_t name_id, int32_t group_id)
{
	return (uint64_t)group_id | ((uint64_t)name_id << 32);
}


skb_attribute_set_handle_t skb_attribute_collection_add_set_with_group(skb_attribute_collection_t* attribute_collection, const char* name, const char* group_name, skb_attribute_set_t attributes)
{
	assert(attribute_collection);
	assert(name);
	assert(group_name);

	int32_t set_idx = attribute_collection->attribute_sets_count;
	SKB_ARRAY_RESERVE(attribute_collection->attribute_sets, attribute_collection->attribute_sets_count + 1);
	skb__attribute_set_t* attribute_set = &attribute_collection->attribute_sets[attribute_collection->attribute_sets_count++];
	memset(attribute_set, 0, sizeof(skb__attribute_set_t));

	const int32_t name_len = strlen(name);
	attribute_set->name = skb_malloc(name_len + 1);
	memcpy(attribute_set->name, name, name_len + 1);

	int32_t group_len = strlen(group_name);
	attribute_set->group_name = skb_malloc(group_len + 1);
	memcpy(attribute_set->group_name, group_name, group_len + 1);

	int32_t attribute_count = skb_attributes_get_copy_flat_count(attributes);
	if (attribute_count > 0) {
		SKB_ARRAY_RESERVE(attribute_set->attributes, attribute_count);
		skb_attributes_copy_flat(attributes, attribute_set->attributes, attribute_count);
		attribute_set->attributes_count = attribute_count;
	}

	const uint64_t name_hash = skb_hash64_append_str(skb_hash64_empty(), name);
	skb_hash_table_add(attribute_collection->attribute_name_lookup, name_hash, attribute_collection->attribute_sets_count - 1);

	const uint64_t group_name_hash = skb_hash64_append_str(skb_hash64_empty(), group_name);
	int32_t group_id = 0;
	if (!skb_hash_table_find(attribute_collection->attribute_group_lookup, group_name_hash, &group_id)) {
		group_id = ++attribute_collection->group_id_gen;
		skb_hash_table_add(attribute_collection->attribute_group_lookup, group_name_hash, group_id);
	}

	attribute_set->handle = skb__make_handle(set_idx, group_id);

	return attribute_set->handle;
}

static skb__attribute_set_t* skb__get_set_by_name(const skb_attribute_collection_t* attribute_collection, const char* name)
{
	if (attribute_collection) {
		int32_t set_idx = SKB_INVALID_INDEX;
		const uint64_t name_hash = skb_hash64_append_str(skb_hash64_empty(), name);
		if (skb_hash_table_find(attribute_collection->attribute_name_lookup, name_hash, &set_idx)) {
			return &attribute_collection->attribute_sets[set_idx];
		}
	}
	return NULL;
}

static skb__attribute_set_t* skb__get_set_by_handle(const skb_attribute_collection_t* attribute_collection, skb_attribute_set_handle_t handle)
{
	if (attribute_collection) {
		const int32_t idx = (int32_t)(handle >> 32);
		if (idx >= 0 && idx < attribute_collection->attribute_sets_count)
			return &attribute_collection->attribute_sets[idx];
	}
	return NULL;
}

skb_attribute_set_handle_t skb_attribute_collection_find_set_by_name(const skb_attribute_collection_t* attribute_collection, const char* name)
{
	skb__attribute_set_t* set = skb__get_set_by_name(attribute_collection, name);
	return set ? set->handle : 0;
}

skb_attribute_set_t skb_attribute_collection_get_set(const skb_attribute_collection_t* attribute_collection, skb_attribute_set_handle_t handle)
{
	skb__attribute_set_t* set = skb__get_set_by_handle(attribute_collection, handle);
	if (set) {
		return (skb_attribute_set_t) {
			.attributes = set->attributes,
			.attributes_count = set->attributes_count
		};
	}
	return (skb_attribute_set_t) {0};
}

skb_attribute_set_t skb_attribute_collection_get_set_by_name(const skb_attribute_collection_t* attribute_collection, const char* name)
{
	skb__attribute_set_t* set = skb__get_set_by_name(attribute_collection, name);
	if (set) {
		return (skb_attribute_set_t) {
			.attributes = set->attributes,
			.attributes_count = set->attributes_count
		};
	}
	return (skb_attribute_set_t) {0};
}
