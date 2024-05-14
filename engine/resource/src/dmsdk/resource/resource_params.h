// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_RESOURCE_PARAMS_H
#define DMSDK_RESOURCE_PARAMS_H

#include <stdint.h>
#include <dmsdk/dlib/hash.h>

// resource.h
typedef struct ResourceFactory* HResourceFactory;
typedef struct ResourcePreloadHintInfo* HResourcePreloadHintInfo;
typedef struct ResourceDescriptor* HResourceDescriptor;
// resource_type.h
typedef struct ResourceType* HResourceType;

/**
 * Parameters to ResourcePreload callback.
 */
struct ResourcePreloadParams
{
    /// Factory handle
    HResourceFactory m_Factory;
    /// Resource context
    void* m_Context;
    /// File name of the loaded file
    const char* m_Filename;
    /// Buffer containing the loaded file
    const void* m_Buffer;
    /// Size of data buffer
    uint32_t m_BufferSize;
    /// Hinter info. Use this when calling PreloadHint
    HResourcePreloadHintInfo m_HintInfo;
    /// User data from the preload step, that will be passed on to ResourceCreate function
    void** m_PreloadData;
    /// the resource type
    HResourceType m_Type;
};

/**
 * Parameters to ResourceCreate callback.
 */
struct ResourceCreateParams
{
    /// Factory handle
    HResourceFactory m_Factory;
    /// Resource context
    void* m_Context;
    /// File name of the loaded file
    const char* m_Filename;
    /// Buffer containing the loaded file
    const void* m_Buffer;
    /// Size of the data buffer
    uint32_t m_BufferSize;
    /// Preloaded data from Preload phase
    void* m_PreloadData;
    /// Resource descriptor to fill in
    HResourceDescriptor m_Resource;
    /// the resource type
    HResourceType m_Type;
};


/**
 * Parameters to ResourcePostCreate callback.
 */
struct ResourcePostCreateParams
{
    /// Factory handle
    HResourceFactory m_Factory;
    /// Resource context
    void* m_Context;
    /// Preloaded data from Preload phase
    void* m_PreloadData;
    /// Resource descriptor passed from create function
    HResourceDescriptor m_Resource;
    /// the resource type
    HResourceType m_Type;
    /// File name of the loaded file
    const char* m_Filename;
};

/**
 * Parameters to ResourceRecreate callback.
 */
struct ResourceRecreateParams
{
    /// Factory handle
    HResourceFactory m_Factory;
    /// Resource context
    void* m_Context;
    /// File name hash of the data
    uint64_t m_NameHash;
    /// File name of the loaded file
    const char* m_Filename;
    /// Data buffer containing the loaded file
    const void* m_Buffer;
    /// Size of data buffer
    uint32_t m_BufferSize;
    /// Pointer holding a precreated message
    const void* m_Message;
    /// Resource descriptor to write into
    HResourceDescriptor m_Resource;
    /// the resource type
    HResourceType m_Type;
};

/**
 * Parameters to ResourceDestroy callback.
 */
struct ResourceDestroyParams
{
    /// Factory handle
    HResourceFactory m_Factory;
    /// Resource context
    void* m_Context;
    /// Resource descriptor for resource to destroy
    HResourceDescriptor m_Resource;
    /// the resource type
    HResourceType m_Type;
};

/**
 * Parameters to ResourceReloaded callback.
 */
typedef struct ResourceReloadedParams
{
    /// User data supplied when the callback was registered
    void* m_UserData;
    /// Descriptor of the reloaded resource
    HResourceDescriptor m_Resource;
    /// Name of the resource, same as provided to Get() when the resource was obtained
    const char* m_Name;
    /// Hashed name of the resource
    uint64_t m_NameHash;
    /// the resource type
    HResourceType m_Type;
} ResourceReloadedParams;

#if defined(__cplusplus)

namespace dmResource
{
    typedef ::ResourceCreateParams ResourceCreateParams;
    typedef ::ResourceDestroyParams ResourceDestroyParams;
    typedef ::ResourcePreloadParams ResourcePreloadParams;
    typedef ::ResourcePostCreateParams ResourcePostCreateParams;
    typedef ::ResourceRecreateParams ResourceRecreateParams;
    typedef ::ResourceReloadedParams ResourceReloadedParams;
}

#endif // __cplusplus

#endif // DMSDK_RESOURCE_PARAMS_H
