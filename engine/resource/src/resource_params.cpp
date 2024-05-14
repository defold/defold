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

#include <dmsdk/resource/resource_params.h>
#include "resource_private.h"

// // Preloader functions

// HResourceFactory ResourcePreloadGetFactory(HResourcePreloadParams params)
// {
//     return params->m_Factory;
// }

// HResourceType ResourcePreloadGetType(HResourcePreloadParams params)
// {
//     return params->m_Type;
// }

// const char* ResourcePreloadGetFilename(HResourcePreloadParams params)
// {
//     return params->m_Filename;
// }

// const void* ResourcePreloadGetFileData(HResourcePreloadParams params)
// {
//     return params->m_Buffer;
// }

// uint32_t ResourcePreloadGetFileDataSize(HResourcePreloadParams params)
// {
//     return params->m_BufferSize;
// }

// HResourcePreloadInfo ResourcePreloadGetHintInfo(HResourcePreloadParams params)
// {
//     return params->m_HintInfo;
// }

// void ResourcePreloadSetPreloadData(HResourcePreloadParams params, void* data)
// {
//     *params->m_PreloadData = data;
// }


// // Create functions

// HResourceFactory ResourceCreateGetFactory(HResourceCreateParams params)
// {
//     return params->m_Factory;
// }

// const char* ResourceCreateGetFilename(HResourceCreateParams params)
// {
//     return params->m_Filename;
// }

// const void* ResourceCreateGetFileData(HResourceCreateParams params)
// {
//     return params->m_Buffer;
// }

// uint32_t ResourceCreateGetFileDataSize(HResourceCreateParams params)
// {
//     return params->m_BufferSize;
// }

// const void* ResourceCreateGetPreloadData(HResourceCreateParams params)
// {
//     return params->m_PreloadData;
// }

// HResourceDescriptor ResourceCreateGetDescriptor(HResourceCreateParams params)
// {
//     return params->m_Resource;
// }

// HResourceType ResourceCreateGetType(HResourceCreateParams params)
// {
//     return params->m_Type;
// }


// // Post create functions

// HResourceFactory ResourcePostCreateGetFactory(HResourcePostCreateParams params)
// {
//     return params->m_Factory;
// }

// HResourceType ResourcePostCreateGetType(HResourcePostCreateParams params)
// {
//     return params->m_Type;
// }

// const char* ResourcePostCreateGetFilename(HResourcePostCreateParams params)
// {
//     return params->m_Filename;
// }

// const void* ResourcePostCreateGetPreloadData(HResourcePostCreateParams params)
// {
//     return params->m_PreloadData;
// }

// HResourceDescriptor ResourcePostCreateGetDescriptor(HResourcePostCreateParams params)
// {
//     return params->m_Resource;
// }

// // Recreate functions

// HResourceFactory ResourceRecreateGetFactory(HResourceRecreateParams params)
// {
//     return params->m_Factory;
// }

// HResourceType    ResourceRecreateGetType(HResourceRecreateParams params)
// {
//     return params->m_Type;
// }

// const char*      ResourceRecreateGetFilename(HResourceRecreateParams params)
// {
//     return params->m_Filename;
// }

// const void*      ResourceRecreateGetFileData(HResourceRecreateParams params)
// {
//     return params->m_Buffer;
// }

// uint32_t         ResourceRecreateGetFileDataSize(HResourceRecreateParams params)
// {
//     return params->m_BufferSize;
// }

// HResourceDescriptor ResourceRecreateGetDescriptor(HResourceRecreateParams params)
// {
//     return params->m_Resource;
// }

// // Destroy functions

// HResourceFactory ResourceDestroyGetFactory(HResourceDestroyParams params)
// {
//     return params->m_Factory;
// }

// HResourceType ResourceDestroyGetType(HResourceDestroyParams params)
// {
//     return params->m_Type;
// }

// HResourceDescriptor ResourceDestroyGetDescriptor(HResourceDestroyParams params)
// {
//     return params->m_Resource;
// }


// // Reloaded functions

// HResourceType ResourceReloadedGetType(HResourceReloadedParams params)
// {
//     return params->m_Type;
// }

// HResourceDescriptor ResourceReloadedGetDescriptor(HResourceReloadedParams params)
// {
//     return params->m_Resource;
// }

// void* ResourceReloadedGetUserData(HResourceReloadedParams params)
// {
//     return params->m_UserData;
// }
