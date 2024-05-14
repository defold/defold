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

#ifndef DMSDK_RESOURCE_TYPE_GEN_HPP
#define DMSDK_RESOURCE_TYPE_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif

#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/align.h>
#include <dmsdk/resource/resource.h>
#include <dmsdk/resource/resource_params.h>

#include <dmsdk/resource/resource_type.h>

namespace dmResource {
    // no documentation found
    typedef HResourceType HResourceType;

    /*# get context from type
     * @name GetContext
     * @param type [type: HResourceType] The type
     * @return context [type: void*] 0 if no context was registere
     */
    void * GetContext(HResourceType type);

    /*# set context from type
     * @name SetContext
     * @param type [type: HResourceType] The type
     * @param context [type: void*] The context to associate with the typ
     */
    void SetContext(HResourceType type,void * context);

    /*# get registered extension name of the type
     * @name GetName
     * @param type [type: HResourceType] The type
     * @return name [type: const char*] The name of the type (e.g. "collectionc"
     */
    const char * GetName(HResourceType type);

    /*# get registered extension name hash of the type
     * @name GetNameHash
     * @param type [type: HResourceType] The type
     * @return hash [type: dmhash_t] The name has
     */
    dmhash_t GetNameHash(HResourceType type);


} // namespace dmResource

#endif // #define DMSDK_RESOURCE_TYPE_GEN_HPP

