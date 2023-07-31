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

#ifndef DM_RESOURCE_MOUNTS_PRIVATE_H
#define DM_RESOURCE_MOUNTS_PRIVATE_H

#include "resource.h"
#include "resource_mounts.h"
#include <dlib/array.h>

namespace dmResourceMounts
{
    extern const int MOUNTSFILE_HEADER_VERSION;
    extern const char* MOUNTSFILE_CSV_SEPARATOR;

    struct MountFileEntry
    {
        const char*  m_Name;
        const char*  m_Uri;
        int          m_Priority;
    };

    dmResource::Result WriteMountsFile(const char* path, const dmArray<MountFileEntry>& entries);
    dmResource::Result ReadMountsFile(const char* path, dmArray<MountFileEntry>& entries);
    void               FreeMountsFile(dmArray<MountFileEntry>& entries);
}

#endif // DM_RESOURCE_MOUNTS_PRIVATE_H
