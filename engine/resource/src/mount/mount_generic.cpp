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


#include <dlib/log.h>

#include "resource.h"
#include "resource_archive.h"
#include "resource_private.h"

namespace dmResource
{
    Result MapFile(const char* filename, void*& map, uint32_t& size)
    {
        // Not used
        return RESULT_OK;
    }

    Result UnmapFile(void*& map, uint32_t size)
    {
        // Not used
        return RESULT_OK;
    }

    Result MountManifest(const char* manifest_filename, void*& out_map, uint32_t& out_size)
    {
        // Not used
        return RESULT_OK;
    }

    Result UnmountManifest(void *& map, uint32_t size)
    {
        // Not used
        return RESULT_OK;
    }

    Result MountArchiveInternal(const char* index_path, const char* data_path, dmResourceArchive::HArchiveIndexContainer* archive, void** mount_info)
    {
        *mount_info = 0;
        dmResourceArchive::Result r = LoadArchiveFromFile(index_path, data_path, archive);
        if (r != dmResourceArchive::RESULT_OK)
        {
            if (r == dmResourceArchive::RESULT_VERSION_MISMATCH)
                return RESULT_VERSION_MISMATCH;
            return RESULT_RESOURCE_NOT_FOUND;
        }
        return RESULT_OK;
    }

    void UnmountArchiveInternal(dmResourceArchive::HArchiveIndexContainer& archive, void* mount_info)
    {
        dmResourceArchive::Delete(archive);
    }
}
