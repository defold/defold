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

#include <stdio.h>
#include <stdint.h>

#include <dlib/log.h>
#include <dlib/testutil.h>
#include <dlib/socket.h>
#include <dlib/uri.h>

#include "../providers/provider.h"
#include "../providers/provider_private.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

// File generated with
// data = b"\x00\x01\x02\x03\x04\x05\x06\x07"
// with open('./src/test/files/src/test/files/somedata', 'wb') as f:
//     f.write(data)
const uint8_t SOMEDATA[8] = {0, 1, 2, 3, 4, 5, 6, 7};

typedef dmResourceProvider::ArchiveLoader ArchiveLoader;

TEST(HttpProviderBasic, Registered)
{
    dmResourceProvider::ArchiveLoader* loader;

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("http"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
    ASSERT_EQ((ArchiveLoader*)0, loader);

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
    ASSERT_EQ((ArchiveLoader*)0, loader);
}

TEST(HttpProviderBasic, CanMount)
{
    dmResourceProvider::ArchiveLoader* loader = dmResourceProvider::FindLoaderByName(dmHashString64("http"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    dmURI::Parts uri;
    dmURI::Parse(".", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));

    dmURI::Parse("file:some/folder", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));

    dmURI::Parse("dmanif:some/folder", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));

    dmURI::Parse("http://domain.com/path", &uri);
    ASSERT_TRUE(loader->m_CanMount(&uri));
}


class HttpProviderArchive : public jc_test_base_class
{
protected:
    virtual void SetUp()
    {
        m_Loader = dmResourceProvider::FindLoaderByName(dmHashString64("http"));
        ASSERT_NE((ArchiveLoader*)0, m_Loader);

        dmURI::Parts uri;
        dmURI::Parse("http://localhost:6123", &uri);

        dmResourceProvider::Result result = dmResourceProvider::CreateMount(m_Loader, &uri, 0, &m_Archive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    }

    virtual void TearDown()
    {
        dmResourceProvider::Result result = dmResourceProvider::Unmount(m_Archive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    }

    dmhash_t m_NameHashArchive;
    dmhash_t m_NameHashFile;

    dmResourceProvider::HArchive       m_Archive;
    dmResourceProvider::ArchiveLoader* m_Loader;
};


TEST_F(HttpProviderArchive, GetSize)
{
    dmResourceProvider::Result result;
    uint32_t file_size;

    result = dmResourceProvider::GetFileSize(m_Archive, 0, "/test.cont", &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(35U, file_size);

    result = dmResourceProvider::GetFileSize(m_Archive, 0, "/test_ref.cont", &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(25U, file_size);

    result = dmResourceProvider::GetFileSize(m_Archive, 0, "/not_exist", &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);
}

TEST_F(HttpProviderArchive, ReadFile)
{
    char path[1024];
    dmTestUtil::MakeHostPath(path, sizeof(path), "build/src/test/somedata");
    FILE* f = fopen(path, "wb");
    ASSERT_NE((FILE*)0, f);
    fwrite(SOMEDATA, sizeof(SOMEDATA), 1, f);
    fclose(f);

    dmResourceProvider::Result result;
    uint8_t short_buffer[4] = {0};
    uint8_t long_buffer[64] = {0};

    result = dmResourceProvider::ReadFile(m_Archive, 0, "/somedata", short_buffer, sizeof(short_buffer));
    ASSERT_EQ(dmResourceProvider::RESULT_IO_ERROR, result);

    result = dmResourceProvider::ReadFile(m_Archive, 0, "/somedata", long_buffer, sizeof(long_buffer));
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_ARRAY_EQ_LEN(SOMEDATA, long_buffer, sizeof(SOMEDATA));
}

#if defined(DM_TEST_HTTP_SUPPORTED)

int main(int argc, char **argv)
{
    dmHashEnableReverseHash(true);

    dmSocket::Initialize();
    dmLog::LogParams logparams;
    dmLog::LogInitialize(&logparams);

    jc_test_init(&argc, argv);
    int result = jc_test_run_all();

    dmLog::LogFinalize();
    dmSocket::Finalize();
    return result;
}

#else

int main(int argc, char **argv)
{
    return 0;
}

#endif
