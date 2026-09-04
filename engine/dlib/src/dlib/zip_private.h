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

#ifndef DM_ZIP_PRIVATE_H
#define DM_ZIP_PRIVATE_H

#include "zip.h"
#include "zip/zip.h"

namespace dmZip
{
    typedef void (*FCloseCallback)(void* context);

    struct ZipArchive
    {
        zip_t*          m_Archive;
        void*           m_CloseContext;
        FCloseCallback  m_CloseCallback;
    };

    Result OpenArchive(zip_t* archive, void* close_context, FCloseCallback close_callback, HZip* zip);
    Result OpenFileRangeInternal(FILE* file, uint64_t offset, uint64_t size,
                                 zip_cstream_read_callback read_callback,
                                 void* close_context, FCloseCallback close_callback,
                                 HZip* zip);
    Result OpenResourcePlatform(const char* path, HZip* zip);
}

#endif // DM_ZIP_PRIVATE_H
