// Generated, do not edit!
// Generated with cwd=/Users/mathiaswesterdahl/work/defold/engine/resource and cmd=/Users/mathiaswesterdahl/work/defold/scripts/dmsdk/gen_sdk.py -i /Users/mathiaswesterdahl/work/defold/engine/resource/sdk_gen.json

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

#ifndef DMSDK_RESOURCE_DESC_GEN_HPP
#define DMSDK_RESOURCE_DESC_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif

#include <dmsdk/dlib/hash.h>

#include <dmsdk/resource/resource_desc.h>

namespace dmResource {
    /*Descriptor function

     */
    dmhash_t GetNameHash(HResourceDescriptor rd);

    // no documentation found
    void SetResource(HResourceDescriptor rd,void * resource);

    // no documentation found
    void * GetResource(HResourceDescriptor rd);

    // no documentation found
    void SetPrevResource(HResourceDescriptor rd,void * resource);

    // no documentation found
    void * GetPrevResource(HResourceDescriptor rd);

    // no documentation found
    void SetResourceSize(HResourceDescriptor rd,uint32_t size);

    // no documentation found
    uint32_t GetResourceSize(HResourceDescriptor rd);

    // no documentation found
    HResourceType GetType(HResourceDescriptor rd);


} // namespace dmResource

#endif // #define DMSDK_RESOURCE_DESC_GEN_HPP

