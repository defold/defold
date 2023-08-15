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

#ifndef DM_ZIP_H
#define DM_ZIP_H

#include <stdint.h>

struct zip_t; // internal, don't use

namespace dmZip
{
    typedef zip_t* HZip;

    enum Result
    {
        RESULT_OK,
        RESULT_NO_SUCH_ENTRY,
        RESULT_BUFFER_NOT_LARGE_ENOUGH,
    };

    /*# Opens a read only zip archive
     *
     * @param path [type: const char*] path to the zip archive
     * @param path [type: HZip*] pointer to zip handle
     * @return [type:Result] path to the zip archive
     */
    Result Open(const char* path, HZip* zip);

    /*# Closes the zip archive
     *
     * @param zip zip archive
     */
    void Close(HZip zip);

    /*# Get number of entries in archive
     */
    uint32_t GetNumEntries(HZip zip);

    /*# Open an entry
     * Must be paired with a call to CloseEntry()
     */
    Result OpenEntry(HZip zip, const char* name);

    /*# Open an entry by index
     * Must be paired with a call to CloseEntry()
     */
    Result OpenEntry(HZip zip, uint32_t index);

    /*# Close an entry
     */
    Result CloseEntry(HZip zip);

    /*# Returns true if the entry is a directory
     */
    bool IsEntryDir(HZip zip);

    /*# Gets the name of the current zip entry
     */
    const char* GetEntryName(HZip zip);

    /*# get uncompressed size
     *
     */
    Result GetEntrySize(HZip zip, uint32_t* size);

    /*# get the index of the currently open entry, or 0xFFFFFFFF
     *
     */
    Result GetEntryIndex(HZip zip, uint32_t* index);

    /*# gets the data for the currently open entry
     *
     */
    Result GetEntryData(HZip zip, void* buffer, uint32_t buffer_size);
}

#endif // DM_ZIP_H