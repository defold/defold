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

#ifndef DM_RESOURCE_PROVIDER_PRIVATE_H
#define DM_RESOURCE_PROVIDER_PRIVATE_H

#include "provider.h"

namespace dmResourceProvider
{
    struct Archive
    {
        const ArchiveLoader*    m_Loader;
        void*                   m_Internal; // Each provider may have its own type to handle the code efficiently
    };

    struct ArchiveLoader
    {
        dmhash_t                m_NameHash;             // E.g. "http", "archive", "file", "zip"
        FCanMount               m_CanMount;
        FMount                  m_Mount;
        FUnmount                m_Unmount;
        FGetFileSize            m_GetFileSize;
        FReadFile               m_ReadFile;
        FGetManifest            m_GetManifest;

        // FManifestLoad           m_LoadManifest;
        // FArchiveLoad            m_Load;
        // FArchiveUnload          m_Unload;
        // FArchiveFindEntry       m_FindEntry;
        // FArchiveRead            m_Read;

        void Verify();

        // private
        ArchiveLoader*          m_Next;
    };


} // namespace

#endif
