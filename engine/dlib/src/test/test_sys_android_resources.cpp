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

#include <stdio.h>
#include <string.h>

#include <android/asset_manager.h>
#include <dmsdk/dlib/android.h>
#include <dlib/dstrings.h>
#include <dlib/path.h>
#include <dlib/sys.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static const uint8_t FILE_DATA[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static const char ASSET_DATA[] = "WRONG";
static char g_AssetName[DMPATH_MAX_PATH];
static uint64_t g_AssetManager;
static ANativeActivity g_Activity;
static android_app g_App;

// adb runs these tests as executables, without a NativeActivity. Supply asset-manager
// responses while exercising the production dmSys Android resource functions.
struct AAsset
{
    uint32_t m_Offset;
};

extern "C" AAsset* AAssetManager_open(AAssetManager* manager, const char* filename, int mode)
{
    (void)mode;
    if (manager != (AAssetManager*)&g_AssetManager || strcmp(filename, g_AssetName) != 0)
    {
        return 0;
    }
    AAsset* asset = new AAsset;
    asset->m_Offset = 0;
    return asset;
}

extern "C" void AAsset_close(AAsset* asset)
{
    delete asset;
}

extern "C" off_t AAsset_getLength(AAsset* asset)
{
    (void)asset;
    return sizeof(ASSET_DATA) - 1;
}

extern "C" int AAsset_read(AAsset* asset, void* buffer, size_t count)
{
    size_t remaining = sizeof(ASSET_DATA) - 1 - asset->m_Offset;
    if (count > remaining)
    {
        count = remaining;
    }
    memcpy(buffer, ASSET_DATA + asset->m_Offset, count);
    asset->m_Offset += (uint32_t)count;
    return (int)count;
}

extern "C" off_t AAsset_seek(AAsset* asset, off_t offset, int whence)
{
    if (whence != SEEK_SET || offset < 0 || offset > (off_t)(sizeof(ASSET_DATA) - 1))
    {
        return -1;
    }
    asset->m_Offset = (uint32_t)offset;
    return offset;
}

class AndroidResourcePaths : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_PreviousApp = dmAndroid::GetAndroidApp();
        ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetAbsolutePath("android-resource-path-test.bin", m_FilePath, sizeof(m_FilePath)));
        ASSERT_EQ('/', m_FilePath[0]);
        dmStrlCpy(g_AssetName, m_FilePath + 1, sizeof(g_AssetName));

        FILE* file = fopen(m_FilePath, "wb");
        ASSERT_NE((FILE*)0, file);
        size_t written = fwrite(FILE_DATA, 1, sizeof(FILE_DATA), file);
        int closed = fclose(file);
        ASSERT_EQ(sizeof(FILE_DATA), written);
        ASSERT_EQ(0, closed);

        g_Activity.assetManager = (AAssetManager*)&g_AssetManager;
        g_App.activity = &g_Activity;
        dmAndroid::SetAndroidApp(&g_App);
    }

    void TearDown() override
    {
        dmAndroid::SetAndroidApp(m_PreviousApp);
        EXPECT_EQ(dmSys::RESULT_OK, dmSys::Unlink(m_FilePath));
    }

    android_app* m_PreviousApp;
    char         m_FilePath[DMPATH_MAX_PATH];
};

// Absolute file reads must not select an APK entry matching the path without its leading slash.
TEST_F(AndroidResourcePaths, AbsolutePathReadsFilesystem)
{
    ASSERT_TRUE(dmSys::ResourceExistsInPackage(g_AssetName));
    EXPECT_FALSE(dmSys::ResourceExistsInPackage(m_FilePath));
    EXPECT_TRUE(dmSys::ResourceExists(m_FilePath));

    uint32_t size = 0;
    EXPECT_EQ(dmSys::RESULT_OK, dmSys::ResourceSize(m_FilePath, &size));
    EXPECT_EQ((uint32_t)sizeof(FILE_DATA), size);
    uint8_t buffer[sizeof(FILE_DATA)] = { 0 };
    EXPECT_EQ(dmSys::RESULT_OK, dmSys::LoadResource(m_FilePath, buffer, sizeof(buffer), &size));
    EXPECT_EQ((uint32_t)sizeof(FILE_DATA), size);
    EXPECT_ARRAY_EQ_LEN(FILE_DATA, buffer, sizeof(FILE_DATA));

    uint32_t nread = 0;
    EXPECT_EQ(dmSys::RESULT_OK, dmSys::LoadResourcePartial(m_FilePath, 2, 3, buffer, &nread));
    EXPECT_EQ(3U, nread);
    EXPECT_ARRAY_EQ_LEN(FILE_DATA + 2, buffer, 3);
}

// The same resource name must still select the APK entry when supplied as a relative path.
TEST_F(AndroidResourcePaths, RelativePathReadsPackage)
{
    EXPECT_TRUE(dmSys::ResourceExists(g_AssetName));
    uint32_t size = 0;
    EXPECT_EQ(dmSys::RESULT_OK, dmSys::ResourceSize(g_AssetName, &size));
    EXPECT_EQ((uint32_t)(sizeof(ASSET_DATA) - 1), size);
    char buffer[sizeof(ASSET_DATA)] = { 0 };
    EXPECT_EQ(dmSys::RESULT_OK, dmSys::LoadResource(g_AssetName, buffer, sizeof(buffer), &size));
    EXPECT_STREQ(ASSET_DATA, buffer);

    uint32_t nread = 0;
    EXPECT_EQ(dmSys::RESULT_OK, dmSys::LoadResourcePartial(g_AssetName, 2, 3, buffer, &nread));
    EXPECT_EQ(3U, nread);
    EXPECT_ARRAY_EQ_LEN(ASSET_DATA + 2, buffer, 3);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
