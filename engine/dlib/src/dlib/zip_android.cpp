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

#include "zip_private.h"
#include "sys.h"

#include <dmsdk/dlib/android.h>

#include <android/asset_manager.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

namespace dmZip
{

static const char ANDROID_ASSET_PATH[] = "/android_asset/";
static const uint32_t ANDROID_ASSET_PATH_LENGTH = sizeof(ANDROID_ASSET_PATH) - 1;

static AAssetManager* GetAndroidAssetManager()
{
    struct android_app* app = dmAndroid::GetAndroidApp();
    if (!app || !app->activity)
        return 0;
    return app->activity->assetManager;
}

static size_t ReadFileAt(FILE* file, uint64_t offset, void* buffer, size_t size)
{
    size_t total_read = 0;
    int fd = fileno(file);
    if (fd < 0)
        return 0;

    while (total_read < size)
    {
        uint64_t read_offset = offset + total_read;
        if (read_offset < offset || read_offset > INT64_MAX)
            break;

        size_t read_size = size - total_read;
        if (read_size > SSIZE_MAX)
            read_size = SSIZE_MAX;

        ssize_t nread;
        do
        {
            nread = pread64(fd, (uint8_t*)buffer + total_read, read_size, (off64_t)read_offset);
        } while (nread < 0 && errno == EINTR);

        if (nread <= 0)
            break;
        total_read += (size_t)nread;
    }

    return total_read;
}

static void CloseFile(void* context)
{
    fclose((FILE*)context);
}

static void CloseAsset(void* context)
{
    AAsset_close((AAsset*)context);
}

Result OpenFileRange(FILE* file, uint64_t offset, uint64_t size, HZip* zip)
{
    return OpenFileRangeInternal(file, offset, size, ReadFileAt, 0, 0, zip);
}

static Result OpenAndroidAsset(const char* path, HZip* zip)
{
    *zip = 0;

    AAssetManager* asset_manager = GetAndroidAssetManager();
    if (!asset_manager)
        return RESULT_IO_ERROR;

    AAsset* asset = AAssetManager_open(asset_manager, path, AASSET_MODE_RANDOM);
    if (!asset)
        return RESULT_NO_SUCH_ENTRY;

    off64_t offset = 0;
    off64_t size = 0;
    int fd = AAsset_openFileDescriptor64(asset, &offset, &size);
    if (fd >= 0)
    {
        AAsset_close(asset);
        if (offset < 0 || size < 0)
        {
            close(fd);
            return RESULT_IO_ERROR;
        }

        FILE* file = fdopen(fd, "rb");
        if (!file)
        {
            close(fd);
            return RESULT_IO_ERROR;
        }

        return OpenFileRangeInternal(file, (uint64_t)offset, (uint64_t)size,
                                     ReadFileAt, file, CloseFile, zip);
    }

    off64_t asset_size = AAsset_getLength64(asset);
    const void* asset_buffer = AAsset_getBuffer(asset);
    if (asset_size < 0 || (uint64_t)asset_size > UINT32_MAX || !asset_buffer)
    {
        AAsset_close(asset);
        return RESULT_IO_ERROR;
    }

    zip_t* archive = zip_stream_open((const char*)asset_buffer, (size_t)asset_size, 9, 'r');
    return OpenArchive(archive, asset, CloseAsset, zip);
}

Result OpenResourcePlatform(const char* path, HZip* zip)
{
    if (strncmp(path, ANDROID_ASSET_PATH, ANDROID_ASSET_PATH_LENGTH) == 0)
        return OpenAndroidAsset(path + ANDROID_ASSET_PATH_LENGTH, zip);

    char mount_path[1024];
    if (dmSys::ResolveMountFileName(mount_path, sizeof(mount_path), path) != dmSys::RESULT_OK)
    {
        *zip = 0;
        return RESULT_NO_SUCH_ENTRY;
    }
    return Open(mount_path, zip);
}

} // namespace dmZip
