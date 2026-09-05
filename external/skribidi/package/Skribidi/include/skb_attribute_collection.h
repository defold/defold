// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_ATTRIBUTE_COLLECTION_H
#define SKB_ATTRIBUTE_COLLECTION_H

#include "skb_common.h"
#include "skb_attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup attribute_collection Attribute Collection
 *
 * @{
 */

/** Opaque type for the attribute collection. Use skb_attribute_collection_create() to create. */
typedef struct skb_attribute_collection_t skb_attribute_collection_t;

typedef uint64_t skb_attribute_set_handle_t;

static inline int32_t skb_attribute_set_handle_get_group(skb_attribute_set_handle_t handle)
{
	return (int32_t)(handle & 0xffffffff);
}

/**
 * Create new attribute collection.
 * @return create attribute collection.
 */
SKB_API skb_attribute_collection_t* skb_attribute_collection_create(void);

/**
 * Destroy attribute collection.
 * @param attribute_collection attribute collection to destroy.
 */
SKB_API void skb_attribute_collection_destroy(skb_attribute_collection_t* attribute_collection);

SKB_API uint32_t skb_attribute_collection_get_id(const skb_attribute_collection_t* attribute_collection);

SKB_API skb_attribute_set_handle_t skb_attribute_collection_add_set(skb_attribute_collection_t* attribute_collection, const char* name, skb_attribute_set_t attributes);
SKB_API skb_attribute_set_handle_t skb_attribute_collection_add_set_with_group(skb_attribute_collection_t* attribute_collection, const char* name, const char* group_name, skb_attribute_set_t attributes);
SKB_API skb_attribute_set_handle_t skb_attribute_collection_find_set_by_name(const skb_attribute_collection_t* attribute_collection, const char* name);
SKB_API skb_attribute_set_t skb_attribute_collection_get_set(const skb_attribute_collection_t* attribute_collection, skb_attribute_set_handle_t handle);
SKB_API skb_attribute_set_t skb_attribute_collection_get_set_by_name(const skb_attribute_collection_t* attribute_collection, const char* name);

/** @} */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SKB_ATTRIBUTE_COLLECTION_H
