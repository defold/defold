// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DMSDK_DLIB_CONTEXT_REGISTRY_H
#define DMSDK_DLIB_CONTEXT_REGISTRY_H

#include <dmsdk/dlib/hash.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*# ContextRegistry API documentation
 *
 * A named registry for engine-owned context pointers.
 *
 * @document
 * @name ContextRegistry
 * @language C
 */

/*#
 * Context registry handle.
 * @typedef
 * @name HContextRegistry
 */
typedef struct ContextRegistry* HContextRegistry;

#define CONTEXT_REGISTRY_CONTEXT_NAME "context_registry"

/*#
 * Sets a context in the context registry using a specified name.
 * @name ContextRegistrySet
 * @param registry [type:HContextRegistry] the context registry
 * @param name [type:const char*] the context name
 * @param context [type:void*] the context, or 0 to remove the context
 * @return result [type:int] 0 if successful
 */
int ContextRegistrySet(HContextRegistry registry, const char* name, void* context);

/*#
 * Sets a context in the context registry using a specified name hash.
 * @name ContextRegistrySetByHash
 * @param registry [type:HContextRegistry] the context registry
 * @param name_hash [type:dmhash_t] the context name hash
 * @param context [type:void*] the context, or 0 to remove the context
 * @return result [type:int] 0 if successful
 */
int ContextRegistrySetByHash(HContextRegistry registry, dmhash_t name_hash, void* context);

/*#
 * Gets a context from the context registry using a specified name.
 * @name ContextRegistryGet
 * @param registry [type:HContextRegistry] the context registry
 * @param name [type:const char*] the context name
 * @return context [type:void*] The context, if it exists
 */
void* ContextRegistryGet(HContextRegistry registry, const char* name);

/*#
 * Gets a context from the context registry using a specified name hash.
 * @name ContextRegistryGetByHash
 * @param registry [type:HContextRegistry] the context registry
 * @param name_hash [type:dmhash_t] the context name hash
 * @return context [type:void*] The context, if it exists
 */
void* ContextRegistryGetByHash(HContextRegistry registry, dmhash_t name_hash);

#if defined(__cplusplus)
}
#endif

#endif // DMSDK_DLIB_CONTEXT_REGISTRY_H
