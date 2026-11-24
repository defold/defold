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

#include <jc_test/jc_test.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlib/align.h>
#include <dlib/configfile.h>
#include "../window_state.h"

using namespace dmWindowState;

static State MakeState()
{
    State state = {};
    Rect rect = {-1800, 100, 960, 640};
    Rect work = {-1920, 20, 1920, 1040};
    state.m_Rect = rect;
    state.m_Monitor.m_WorkArea = work;
    state.m_Monitor.m_NameHash = 1234;
    state.m_Monitor.m_ScaleX = 1.5f;
    state.m_Monitor.m_ScaleY = 1.5f;
    state.m_ConfigWidth = 640;
    state.m_ConfigHeight = 480;
    state.m_HasPosition = true;
    return state;
}

class WindowStateTest : public jc_test_base_class
{
protected:
    const char* m_Path;
    char* m_NumericLocale;
    void SetUp()
    {
        m_Path = "test-window-state.ini";
        m_NumericLocale = strdup(setlocale(LC_NUMERIC, 0));
        remove(m_Path);
    }
    void TearDown()
    {
        setlocale(LC_NUMERIC, m_NumericLocale);
        free(m_NumericLocale);
        remove(m_Path);
    }
};

static bool UseCommaDecimalLocale()
{
    const char* names[] = {"sv_SE.UTF-8", "sv_SE.utf8", "Swedish_Sweden.1252", "de_DE.UTF-8"};
    for (uint32_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        if (setlocale(LC_NUMERIC, names[i]) && !strcmp(localeconv()->decimal_point, ","))
            return true;
    }
    return false;
}

// Verifies a game's numeric locale cannot make saved placement unreadable on the next launch.
TEST_F(WindowStateTest, SaveAcrossNumericLocales)
{
    if (!UseCommaDecimalLocale())
        SKIP();
    State state = MakeState(), loaded = {};
    state.m_Monitor.m_ScaleY = 1.25f;
    ASSERT_TRUE(Save(m_Path, state));
    ASSERT_STREQ(",", localeconv()->decimal_point);
    ASSERT_NE((const char*)0, setlocale(LC_NUMERIC, "C"));
    ASSERT_EQ(LOAD_OK, Load(m_Path, &loaded));
    ASSERT_EQ(1.5f, loaded.m_Monitor.m_ScaleX);
    ASSERT_EQ(1.25f, loaded.m_Monitor.m_ScaleY);
    ASSERT_EQ(state.m_Rect.m_X, loaded.m_Rect.m_X);
}

// Verifies loading existing decimal scale values also ignores the process's numeric locale.
TEST_F(WindowStateTest, LoadAcrossNumericLocales)
{
    ASSERT_NE((const char*)0, setlocale(LC_NUMERIC, "C"));
    State state = MakeState(), loaded = {};
    state.m_Monitor.m_ScaleY = 1.25f;
    ASSERT_TRUE(Save(m_Path, state));
    if (!UseCommaDecimalLocale())
        SKIP();
    ASSERT_EQ(LOAD_OK, Load(m_Path, &loaded));
    ASSERT_STREQ(",", localeconv()->decimal_point);
    ASSERT_EQ(1.5f, loaded.m_Monitor.m_ScaleX);
    ASSERT_EQ(1.25f, loaded.m_Monitor.m_ScaleY);
}

// Verifies scale parsing accepts the persisted decimal format and rejects corrupt or out-of-range values.
TEST_F(WindowStateTest, ScaleSyntaxAndBounds)
{
    struct Case
    {
        const char* m_Text;
        bool m_Valid;
    };
    const Case cases[] = {
        {"0.25", true}, {"1", true}, {"1.500000", true}, {"16.000000", true},
        {"0.249999", false}, {"16.000001", false}, {"1000000000000", false},
        {"nan", false}, {"inf", false}, {"1,500000", false}, {"1.5garbage", false},
        {"1.", false}, {"1.-5", false}, {"-1.5", false}, {"1.0000001", false}
    };
    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        FILE* file = fopen(m_Path, "wb");
        ASSERT_NE((FILE*)0, file);
        fprintf(file, "[window]\nversion = 1\nwidth = 400\nheight = 300\n"
            "config_width = 400\nconfig_height = 300\nhas_position = 0\n"
            "[monitor]\nscale_x = %s\nscale_y = %s\n", cases[i].m_Text, cases[i].m_Text);
        fclose(file);
        State loaded;
        ASSERT_EQ(cases[i].m_Valid ? LOAD_OK : LOAD_INVALID, Load(m_Path, &loaded));
    }
}

// Verifies replacement and round-tripping preserve signed positions, DPI, and reset identity.
TEST_F(WindowStateTest, RoundTripAndReplace)
{
    State state = MakeState(), loaded = {};
    ASSERT_EQ(LOAD_MISSING, Load(m_Path, &loaded));
    ASSERT_TRUE(Save(m_Path, state));
    ASSERT_EQ(LOAD_OK, Load(m_Path, &loaded));
    ASSERT_EQ(state.m_Rect.m_X, loaded.m_Rect.m_X);
    ASSERT_EQ(state.m_Rect.m_Y, loaded.m_Rect.m_Y);
    ASSERT_EQ(state.m_Monitor.m_NameHash, loaded.m_Monitor.m_NameHash);
    ASSERT_EQ(1.5f, loaded.m_Monitor.m_ScaleX);
    ASSERT_STREQ("", loaded.m_ResetToken);
    state.m_Rect.m_Width = 777;
    strcpy(state.m_ResetToken, "deadbeef-1234");
    ASSERT_TRUE(Save(m_Path, state));
    ASSERT_EQ(LOAD_OK, Load(m_Path, &loaded));
    ASSERT_EQ(777, loaded.m_Rect.m_Width);
    ASSERT_STREQ(state.m_ResetToken, loaded.m_ResetToken);
    // An unwritable destination must leave the last completed record intact.
    ASSERT_FALSE(Save("missing-window-state-directory/state.ini", state));
    ASSERT_EQ(LOAD_OK, Load(m_Path, &loaded));
    ASSERT_EQ(777, loaded.m_Rect.m_Width);
}

// Verifies size-only state never invents a valid (0,0) placement on Wayland.
TEST_F(WindowStateTest, SizeOnly)
{
    State state = MakeState(), loaded = {};
    Rect empty_work_area = {};
    state.m_HasPosition = false;
    state.m_Monitor.m_WorkArea = empty_work_area;
    ASSERT_TRUE(Save(m_Path, state));
    ASSERT_EQ(LOAD_OK, Load(m_Path, &loaded));
    ASSERT_FALSE(loaded.m_HasPosition);
    ASSERT_EQ(960, loaded.m_Rect.m_Width);
}

// Verifies corrupt records are rejected and future versions are distinguished for write protection.
TEST_F(WindowStateTest, InvalidAndFutureRecords)
{
    const char* records[] = {
        "[window]\nversion = 2\n",
        "[window]\nversion = 1\nwidth = 0\n",
        "[window]\nversion = 1\nwidth = 99999999999999999999\n",
        "not an ini file"
    };
    for (uint32_t i = 0; i < sizeof(records) / sizeof(records[0]); ++i)
    {
        FILE* file = fopen(m_Path, "wb");
        ASSERT_NE((FILE*)0, file);
        fputs(records[i], file);
        fclose(file);
        State state = MakeState();
        ASSERT_EQ(i == 0 ? LOAD_UNSUPPORTED_VERSION : LOAD_INVALID, Load(m_Path, &state));
        ASSERT_EQ(-1800, state.m_Rect.m_X);
    }
}

// Verifies a reset made while stopped invalidates size independently of the remembered position.
TEST_F(WindowStateTest, ResetTokenAndConfiguration)
{
    State state = MakeState();
    ASSERT_TRUE(RestoreSize(state, 640, 480, ""));
    ASSERT_FALSE(RestoreSize(state, 800, 480, ""));
    ASSERT_FALSE(RestoreSize(state, 640, 600, ""));
    char token[40] = "";
    FILE* file = fopen(m_Path, "wb");
    ASSERT_NE((FILE*)0, file);
    fputs("deadbeef-1234", file);
    fclose(file);
    ReadResetToken(m_Path, token, sizeof(token));
    ASSERT_STREQ("deadbeef-1234", token);
    ASSERT_FALSE(RestoreSize(state, 640, 480, token));
    ASSERT_EQ(-1800, state.m_Rect.m_X);
}

// Verifies duplicate monitor names use geometry, reordered monitors match, and disconnected ones fall back.
TEST(WindowState, MonitorSelection)
{
    State state = MakeState();
    Monitor monitors[] = {
        {{0, 20, 1920, 1040}, 5678, 1, 1},
        {{-1920, 20, 1920, 1040}, 1234, 1.5f, 1.5f}
    };
    ASSERT_EQ(1u, SelectMonitor(state, monitors, 2));
    monitors[0].m_NameHash = 1234;
    ASSERT_EQ(1u, SelectMonitor(state, monitors, 2));
    monitors[0].m_NameHash = 5678;
    ASSERT_EQ(0u, SelectMonitor(state, monitors, 1));
    monitors[1].m_WorkArea.m_X = 1920;
    ASSERT_EQ(1u, SelectMonitor(state, monitors, 2));
}

// Verifies screen units do not grow on repeat restoration and Cocoa points are not scaled like Windows pixels.
TEST(WindowState, DpiAndRelativePosition)
{
    State state = MakeState();
    Monitor monitor = state.m_Monitor;
    Rect rect = RestoreRect(state, monitor, true);
    ASSERT_EQ(960, rect.m_Width);
    ASSERT_EQ(-1800, rect.m_X);
    monitor.m_ScaleX = monitor.m_ScaleY = 2;
    monitor.m_WorkArea.m_X = 0;
    rect = RestoreRect(state, monitor, true);
    ASSERT_EQ(1280, rect.m_Width);
    ASSERT_EQ(160, rect.m_X);
    rect = RestoreRect(state, monitor, false);
    ASSERT_EQ(960, rect.m_Width);
    ASSERT_EQ(120, rect.m_X);
}

// Verifies decorations and oversized windows cannot leave the title bar outside the work area.
TEST(WindowState, ClampDecoratedAndOversized)
{
    Rect work = {-1920, 30, 1920, 1000};
    Rect rect = {-2500, -100, 960, 640};
    ClampPosition(&rect, work, 5, 28, 5, 5);
    ASSERT_EQ(-1915, rect.m_X);
    ASSERT_EQ(58, rect.m_Y);
    Rect oversized = {5000, 5000, 4000, 3000};
    ClampPosition(&oversized, work, 5, 28, 5, 5);
    ASSERT_EQ(-1915, oversized.m_X);
    ASSERT_EQ(58, oversized.m_Y);
}

static int g_ConfigCallbacks = 0;

static void ConfigLifecycle(HConfigFile)
{
    ++g_ConfigCallbacks;
}

static bool ConfigOverride(HConfigFile, const char*, const char*, const char** value)
{
    ++g_ConfigCallbacks;
    *value = "999";
    return true;
}

// Verifies reading player state cannot invoke or be overridden by game config extensions.
TEST_F(WindowStateTest, IgnoresGameConfigExtensions)
{
    static uint8_t DM_ALIGNED(16) descriptor[ConfigFileExtensionDescBufferSize];
    ConfigFileRegisterExtension(descriptor, sizeof(descriptor), "window-state-test", ConfigLifecycle, ConfigLifecycle, ConfigOverride, 0, 0);
    dmConfigFile::HConfig config;
    const char* text = "[window]\nwidth = 640\n";
    ASSERT_EQ(dmConfigFile::RESULT_OK, dmConfigFile::LoadFromBuffer(text, strlen(text), 0, 0, &config));
    ASSERT_STREQ("999", dmConfigFile::GetString(config, "window.width", ""));
    dmConfigFile::Delete(config);
    ASSERT_EQ(3, g_ConfigCallbacks);

    State state = MakeState(), loaded = {};
    ASSERT_TRUE(Save(m_Path, state));
    ASSERT_EQ(LOAD_OK, Load(m_Path, &loaded));
    ASSERT_EQ(960, loaded.m_Rect.m_Width);
    ASSERT_EQ(3, g_ConfigCallbacks);
    // Delete has always accepted a null handle, including for extension callbacks.
    dmConfigFile::Delete(0);
    ASSERT_EQ(4, g_ConfigCallbacks);
}
