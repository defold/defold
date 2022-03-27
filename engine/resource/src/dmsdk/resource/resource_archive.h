// Copyright 2020-2022 The Defold Foundation
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

#ifndef DMSDK_RESOURCE_ARCHIVE_H
#define DMSDK_RESOURCE_ARCHIVE_H

/*# Resource
 *
 * Functions for working with the resource archive.
 *
 * @document
 * @name ResourceArchive
 * @namespace dmResourceArchive
 * @path engine/resource/src/dmsdk/resource/resource_archive.h
 */


namespace dmResourceArchive
{

    enum Result
    {
        RESULT_OK = 0,
        RESULT_NOT_FOUND = 1,
        RESULT_VERSION_MISMATCH = -1,
        RESULT_IO_ERROR = -2,
        RESULT_MEM_ERROR = -3,
        RESULT_OUTBUFFER_TOO_SMALL = -4,
        RESULT_ALREADY_STORED = -5,
        RESULT_UNKNOWN = -1000,
    };

    typedef Result (*FDecryptResource)(void* buffer, uint32_t buffer_len);

    void RegisterResourceDecryption(FDecryptResource decrypt_resource);
}

#endif // DMSDK_RESOURCE_ARCHIVE_H
