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
#include <testmain/testmain.h>
#include <dlib/dlib.h>
#include <dlib/dstrings.h>
#include <dlib/profile.h>
#include <dlib/testutil.h>
#include "../engine.h"
#include "../engine_private.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

extern "C" void dmExportedSymbols();

DM_PROPERTY_U32(rmtp_ReportCount, 0, PROFILE_PROPERTY_FRAME_RESET, "# report test", 0);
DM_PROPERTY_U64(rmtp_ReportU64, 0, PROFILE_PROPERTY_NONE, "large integer", 0);
DM_PROPERTY_S32(rmtp_ReportNegative, -10, PROFILE_PROPERTY_NONE, "negative default", 0);

class ProfileReportTest : public jc_test_base_class
{
protected:
    dmEngine::HEngine m_Engine;
    char m_ReportPath[512];
    char m_LogPath[512];

    void SetUp()
    {
        m_Engine = 0;
        dmTestUtil::MakeHostPath(m_ReportPath, sizeof(m_ReportPath), "build/src/test/instance_11229_profile.txt");
        dmTestUtil::MakeHostPath(m_LogPath, sizeof(m_LogPath), "build/src/test/instance_11229_log.txt");
        remove(m_ReportPath);
        remove(m_LogPath);
        dmEngineInitialize();
    }

    void TearDown()
    {
        if (m_Engine)
            dmEngine::Delete(m_Engine);
        dmEngineFinalize();
        dLib::SetDebugMode(true);
        remove(m_ReportPath);
        remove(m_LogPath);
    }

    bool Start()
    {
        char project_path[512];
        dmTestUtil::MakeHostPath(project_path, sizeof(project_path), CONTENT_ROOT "/game.projectc");
        char log_dir[512];
        dmTestUtil::MakeHostPath(log_dir, sizeof(log_dir), "build/src/test");
        char log_config[600];
        dmSnPrintf(log_config, sizeof(log_config), "--config=project.log_dir=%s", log_dir);
        const char* argv[] = {"test_profile_report", "--config=dmengine.unload_builtins=0",
                             "--config=project.instance_index=11229", "--config=project.write_log_file=1",
                             log_config, project_path};
        m_Engine = dmEngine::New(0);
        return dmEngine::Init(m_Engine, DM_ARRAY_SIZE(argv), (char**)argv);
    }

    void Stop()
    {
        dmEngine::Delete(m_Engine);
        m_Engine = 0;
    }

    void CheckReport(const char* path, uint32_t peak)
    {
        FILE* file = fopen(path, "r");
        ASSERT_NE((FILE*)0, file);
        char buffer[65536];
        size_t size = fread(buffer, 1, sizeof(buffer) - 1, file);
        buffer[size] = 0;
        fclose(file);
        ASSERT_NE((char*)0, strstr(buffer, "Profile property peaks:"));
        char row[128];
        dmSnPrintf(row, sizeof(row), "%-32s %20u  # report test", "ReportCount", peak);
        ASSERT_NE((char*)0, strstr(buffer, row));
        dmSnPrintf(row, sizeof(row), "%-32s %20s  large integer", "ReportU64", "9223372036854775815");
        ASSERT_NE((char*)0, strstr(buffer, row));
        dmSnPrintf(row, sizeof(row), "%-32s %20s  negative default", "ReportNegative", "-10");
        ASSERT_NE((char*)0, strstr(buffer, row));
    }
};

TEST_F(ProfileReportTest, ShutdownAndReboot)
{
    // Reboot creates a new engine without reinitializing the shared profiler.
    const uint32_t peaks[] = {52, 7};
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(peaks); ++i)
    {
        ASSERT_TRUE(Start());
        dmEngine::Step(m_Engine);
        HProfile frame = ProfileFrameBegin();
        DM_PROPERTY_ADD_U32(rmtp_ReportCount, peaks[i]);
        DM_PROPERTY_SET_U32(rmtp_ReportCount, 2);
        DM_PROPERTY_SET_U64(rmtp_ReportU64, ((uint64_t)1 << 63) + 7);
        ProfileFrameEnd(frame);
        Stop();
        CheckReport(m_ReportPath, peaks[i]);
        CheckReport(m_LogPath, peaks[i]);
    }
}

TEST_F(ProfileReportTest, DisabledInReleaseMode)
{
    ASSERT_TRUE(Start());
    dLib::SetDebugMode(false);
    Stop();
    FILE* file = fopen(m_ReportPath, "r");
    EXPECT_EQ((FILE*)0, file);
    if (file)
        fclose(file);
}

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE);
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
