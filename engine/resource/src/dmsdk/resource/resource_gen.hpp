// Generated, do not edit!
// Generated with cwd=/Users/mawe/work/defold/engine/resource and cmd=/Users/mawe/work/defold/scripts/dmsdk/gen_sdk.py -i /Users/mawe/work/defold/engine/resource/sdk_gen.json

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

#ifndef DMSDK_RESOURCE_GEN_HPP
#define DMSDK_RESOURCE_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif


/*# Resource_gen
 *
 * description
 *
 * @document
 * @name Resource_gen
 * @namespace dmResource
 * @path dmsdk/resource/resource_gen.hpp
 */

#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/align.h>

#include <dmsdk/resource/resource.h>

namespace dmResource
{
    /*#
    * Generated from [ref:ResourceResult]
    */
    enum Result {
        RESULT_OK = 0,
        RESULT_INVALID_DATA = -1,
        RESULT_DDF_ERROR = -2,
        RESULT_RESOURCE_NOT_FOUND = -3,
        RESULT_MISSING_FILE_EXTENSION = -4,
        RESULT_ALREADY_REGISTERED = -5,
        RESULT_INVAL = -6,
        RESULT_UNKNOWN_RESOURCE_TYPE = -7,
        RESULT_OUT_OF_MEMORY = -8,
        RESULT_IO_ERROR = -9,
        RESULT_NOT_LOADED = -10,
        RESULT_OUT_OF_RESOURCES = -11,
        RESULT_STREAMBUFFER_TOO_SMALL = -12,
        RESULT_FORMAT_ERROR = -13,
        RESULT_CONSTANT_ERROR = -14,
        RESULT_NOT_SUPPORTED = -15,
        RESULT_RESOURCE_LOOP_ERROR = -16,
        RESULT_PENDING = -17,
        RESULT_INVALID_FILE_EXTENSION = -18,
        RESULT_VERSION_MISMATCH = -19,
        RESULT_SIGNATURE_MISMATCH = -20,
        RESULT_UNKNOWN_ERROR = -21,
    };

    /*#
    * Generated from [ref:HResourceFactory]
    */
    typedef HResourceFactory HFactory;

    /*#
    * Generated from [ref:HResourcePreloadHintInfo]
    */
    typedef HResourcePreloadHintInfo HPreloadHintInfo;

    /*#
    * Generated from [ref:HResourceDescriptor]
    */
    typedef HResourceDescriptor HDescriptor;

    /*#
    * Generated from [ref:ResourceReloadedParams]
    */
    typedef ResourceReloadedParams ResourceReloadedParams;

    /*#
    * Generated from [ref:FResourceReloadedCallback]
    */
    typedef FResourceReloadedCallback FReloadedCallback;

    /*#
    * Generated from [ref:ResourceGet]
    */
    Result Get(HFactory factory,const char * name,void ** resource);

    /*#
    * Generated from [ref:ResourceGetByHash]
    */
    Result GetByHash(HFactory factory,dmhash_t name,void ** resource);

    /*#
    * Generated from [ref:ResourceRelease]
    */
    void Release(HFactory factory,void * resource);

    /*#
    * Generated from [ref:ResourcePreloadHint]
    */
    bool PreloadHint(HPreloadHintInfo preloader,const char * path);

    /*#
    * Generated from [ref:ResourceGetPath]
    */
    Result GetPath(HFactory factory,const void * resource,dmhash_t * hash);

    /*#
    * Generated from [ref:ResourceAddFile]
    */
    Result AddFile(HFactory factory,const char * path,uint32_t size,const void * resource);

    /*#
    * Generated from [ref:ResourceRemoveFile]
    */
    Result RemoveFile(HFactory factory,const char * path);

    /*#
    * Generated from [ref:ResourceDescriptorGetNameHash]
    */
    dmhash_t GetNameHash(HDescriptor rd);

    /*#
    * Generated from [ref:ResourceDescriptorSetResource]
    */
    void SetResource(HDescriptor rd,void * resource);

    /*#
    * Generated from [ref:ResourceDescriptorGetResource]
    */
    void * GetResource(HDescriptor rd);

    /*#
    * Generated from [ref:ResourceDescriptorSetPrevResource]
    */
    void SetPrevResource(HDescriptor rd,void * resource);

    /*#
    * Generated from [ref:ResourceDescriptorGetPrevResource]
    */
    void * GetPrevResource(HDescriptor rd);

    /*#
    * Generated from [ref:ResourceDescriptorSetResourceSize]
    */
    void SetResourceSize(HDescriptor rd,uint32_t size);

    /*#
    * Generated from [ref:ResourceDescriptorGetResourceSize]
    */
    uint32_t GetResourceSize(HDescriptor rd);

    /*#
    * Generated from [ref:ResourceDescriptorGetType]
    */
    HResourceType GetType(HDescriptor rd);


} // namespace dmResource

#endif // #define DMSDK_RESOURCE_GEN_HPP

