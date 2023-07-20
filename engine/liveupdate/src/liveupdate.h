// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_LIVEUPDATE_H
#define DM_LIVEUPDATE_H

#include <stdint.h>
#include <dlib/hash.h>
#include <resource/resource.h>

namespace dmResourceArchive
{
    struct LiveUpdateResource;
}

namespace dmResourceMounts
{
    typedef struct ResourceMountsContext* HContext;
}

namespace dmLiveUpdate
{
    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK                        =  0,
        RESULT_INVALID_HEADER            = -1,
        RESULT_MEM_ERROR                 = -2,
        RESULT_INVALID_RESOURCE          = -3,
        RESULT_VERSION_MISMATCH          = -4,
        RESULT_ENGINE_VERSION_MISMATCH   = -5,
        RESULT_SIGNATURE_MISMATCH        = -6,
        RESULT_SCHEME_MISMATCH           = -7,
        RESULT_BUNDLED_RESOURCE_MISMATCH = -8,
        RESULT_FORMAT_ERROR              = -9,
        RESULT_IO_ERROR                  = -10,
        RESULT_INVAL                     = -11,
        RESULT_UNKNOWN                   = -1000,
    };

    const char* ResultToString(Result result);

    // Scripting
    bool HasLiveUpdateMount();

    Result ResourceResultToLiveupdateResult(dmResource::Result r);

    // For .arci/.arcd storage using the "archive" provider
    Result StoreResourceAsync(const char* expected_digest, uint32_t expected_digest_length,
                                    const dmResourceArchive::LiveUpdateResource* resource, void (*callback)(bool, void*), void* callback_data);

    Result StoreManifestAsync(const uint8_t* manifest_data, uint32_t manifest_len, void (*callback)(int, void*), void* callback_data);


    // For .zip storage using the "zip" provider
    // Registers an archive (.zip) on disc
    Result StoreArchiveAsync(const char* path, void (*callback)(const char*, int, void*), void* callback_data, const char* mountname, int priority, bool verify_archive);


    // The new api
    Result AddMountAsync(const char* name, const char* uri, int priority, void (*callback)(const char*, const char*, int, void*), void* cbk_ctx);
};

#endif // DM_LIVEUPDATE_H
