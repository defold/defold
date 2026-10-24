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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <record/record_file.h>
#include <record/record_private.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// Host paths must be converted from the active Windows code page before capture.
TEST(RecordFile, HostPathEncoding)
{
    char host[128];
    char expected[128];
#if defined(_WIN32)
    // Use the actual roundtrip value: characters outside the active code page
    // may be substituted, while UTF-8 system locales preserve all of them.
    const wchar_t path[] = L"record-\x00e9-\x0416-\x65e5";
    wchar_t wide[128];
    ASSERT_GT(WideCharToMultiByte(CP_ACP, 0, path, -1, host, sizeof(host), 0, 0), 0);
    ASSERT_GT(MultiByteToWideChar(CP_ACP, 0, host, -1, wide, sizeof(wide) / sizeof(wide[0])), 0);
    ASSERT_GT(WideCharToMultiByte(CP_UTF8, 0, wide, -1, expected, sizeof(expected), 0, 0), 0);
#else
    strcpy(host, "record-\xc3\xa9-\xe6\x97\xa5");
    strcpy(expected, host);
#endif
    char output[128];
    ASSERT_TRUE(dmRecord::HostPathToUtf8(host, output, sizeof(output)));
    ASSERT_STREQ(expected, output);
    ASSERT_FALSE(dmRecord::HostPathToUtf8(host, output, (uint32_t)strlen(expected)));
    ASSERT_EQ(0, output[0]);
}

// File preparation, download and deletion must address the same UTF-8 file as capture.
TEST(RecordFile, UnicodeFileRoundtrip)
{
    const char path[] = "record-file-\xc3\xa9-\xe5\xbd\x95\xe5\x83\x8f.tmp";
    const char contents[] = "recording data";
    FILE* file = dmRecord::FileOpen(path, "wb");
    ASSERT_NE((FILE*)0, file);
    ASSERT_EQ(sizeof(contents), fwrite(contents, 1, sizeof(contents), file));
    ASSERT_EQ(0, fclose(file));
#if defined(_WIN32)
    const wchar_t wide_path[] = L"record-file-\x00e9-\x5f55\x50cf.tmp";
    ASSERT_NE(INVALID_FILE_ATTRIBUTES, GetFileAttributesW(wide_path));
#endif
    file = dmRecord::FileOpen(path, "rb");
    ASSERT_NE((FILE*)0, file);
    char buffer[sizeof(contents)];
    ASSERT_EQ(sizeof(contents), fread(buffer, 1, sizeof(buffer), file));
    ASSERT_EQ(0, fclose(file));
    ASSERT_STREQ(contents, buffer);
    ASSERT_EQ(0, dmRecord::FileRemove(path));
    ASSERT_EQ((FILE*)0, dmRecord::FileOpen(path, "rb"));
#if defined(_WIN32)
    ASSERT_EQ(INVALID_FILE_ATTRIBUTES, GetFileAttributesW(wide_path));
#endif
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
