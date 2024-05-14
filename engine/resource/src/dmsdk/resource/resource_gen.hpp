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

#ifndef DMSDK_RESOURCE_GEN_HPP
#define DMSDK_RESOURCE_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif

#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/resource/resource_desc.h>

#include <dmsdk/resource/resource.h>

namespace dmResource {
    /*Result
     * @enum
     * @name Result
     * @member RESULT_OK
     * @member RESULT_INVALID_DATA
     * @member RESULT_DDF_ERROR
     * @member RESULT_RESOURCE_NOT_FOUND
     * @member RESULT_MISSING_FILE_EXTENSION
     * @member RESULT_ALREADY_REGISTERED
     * @member RESULT_INVAL
     * @member RESULT_UNKNOWN_RESOURCE_TYPE
     * @member RESULT_OUT_OF_MEMORY
     * @member RESULT_IO_ERROR
     * @member RESULT_NOT_LOADED
     * @member RESULT_OUT_OF_RESOURCES
     * @member RESULT_STREAMBUFFER_TOO_SMALL
     * @member RESULT_FORMAT_ERROR
     * @member RESULT_CONSTANT_ERROR
     * @member RESULT_NOT_SUPPORTED
     * @member RESULT_RESOURCE_LOOP_ERROR
     * @member RESULT_PENDING
     * @member RESULT_INVALID_FILE_EXTENSION
     * @member RESULT_VERSION_MISMATCH
     * @member RESULT_SIGNATURE_MISMATCH
     * @member RESULT_UNKNOWN_ERRO
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

    // no documentation found
    typedef HResourceFactory HFactory;

    // no documentation found
    typedef HResourcePreloadHintInfo HPreloadHintInfo;

    /*#
     * Function called when a resource has been reloaded.
     * @param params Parameters
     * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT
     * @see RegisterReloadedCallbac
     */
    typedef FResourceReloadedCallback FReloadedCallback;

    /*#
     * Get a resource from factory
     * @name Get
     * @param factory [type: HFactory] Factory handle
     * @param name [type: const char*] Resource name
     * @param resource [type: void**] Created resource
     * @return result [type: Result]  RESULT_OK on succes
     */
    Result Get(HFactory factory,const char * name,void ** resource);

    /*#
     * Get a resource from factory
     * @name Get
     * @param factory [type: HFactory] Factory handle
     * @param name [type: dmhash_t] Resource name
     * @param resource [type: void**] Created resource
     * @return result [type: Result]  RESULT_OK on succes
     */
    Result GetByHash(HFactory factory,dmhash_t name,void ** resource);

    /*#
     * Release resource
     * @name Release
     * @param factory [type: HFactory] Factory handle
     * @param resource [type: void*] Resource pointe
     */
    void Release(HFactory factory,void * resource);

    /*#
     * Hint the preloader what to load before Create is called on the resource.
     * The resources are not guaranteed to be loaded before Create is called.
     * This function can be called from a worker thread.
     * @name PreloadHint
     * @param factory [type: dmResource::HPreloadHintInfo] Preloader handle
     * @param path [type: const char*] Resource path
     * @return result [type: bool] if successfully invoking preloader
     */
    bool PreloadHint(HPreloadHintInfo preloader,const char * path);

    /*#
     * Returns the canonical path hash of a resource
     * @param factory [type: HFactory] Factory handle
     * @param resource [type: void*] The resource pointer
     * @param hash [type: dmhash_t] The path hash of the resource
     * @return result [type: Result] RESULT_OK on succes
     */
    Result GetPath(HFactory factory,const void * resource,dmhash_t * hash);

    /*#
     * Adds a file to the resource system
     * Any request for this path will go through any existing mounts first.
     * If you wish to provide file overrides, please use the LiveUpdate feature for that.
     * The file isn't persisted between sessions.
     *
     * @name AddFile
     * @param factory [type: HFactory] Factory handle
     * @param path [type: const char*] The path of the resource
     * @param size [type: uint32_t] The size of the resource (in bytes)
     * @param resource [type: const void*] The resource payload
     * @return result [type: Result] RESULT_OK on succes
     */
    Result AddFile(HFactory factory,const char * path,uint32_t size,const void * resource);

    /*#
     * Removes a previously registered file from the resource system
     * @name RemoveFile
     * @param factory [type: HFactory] Factory handle
     * @param path [type: const char*] The path of the resource
     * @return result [type: Result] RESULT_OK on succes
     */
    Result RemoveFile(HFactory factory,const char * path);


} // namespace dmResource

#endif // #define DMSDK_RESOURCE_GEN_HPP

