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

#include "window_state.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlib/configfile.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/sys.h>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace dmWindowState
{
    static const int MAX_COORDINATE = 1 << 24;
    static const int MAX_SIZE = 65536;
    static const int SCALE_PRECISION = 1000000;

    static bool ReadInt(dmConfigFile::HConfig config, const char* key, int minimum, int maximum, int32_t* value)
    {
        const char* text = dmConfigFile::GetString(config, key, 0);
        if (!text || !text[0])
            return false;
        char* end;
        errno = 0;
        long parsed = strtol(text, &end, 10);
        if (errno || *end || parsed < minimum || parsed > maximum)
            return false;
        *value = (int32_t) parsed;
        return true;
    }

    static bool ReadScale(dmConfigFile::HConfig config, const char* key, float* value)
    {
        const char* text = dmConfigFile::GetString(config, key, 0);
        if (!text || !text[0])
            return false;

        // The file uses a dot and up to six decimal places, regardless of a
        // game's LC_NUMERIC setting. Do not use locale-sensitive strtod here.
        const char* end = text;
        int32_t whole = 0;
        while (*end >= '0' && *end <= '9')
        {
            whole = whole * 10 + (*end++ - '0');
            if (whole > 16)
                return false;
        }
        if (end == text)
            return false;
        int32_t fraction = 0, divisor = 1;
        if (*end == '.')
        {
            const char* digits = ++end;
            while (*end >= '0' && *end <= '9' && divisor < SCALE_PRECISION)
            {
                fraction = fraction * 10 + (*end++ - '0');
                divisor *= 10;
            }
            if (end == digits)
                return false;
        }
        double parsed = whole + (double) fraction / divisor;
        if (*end || parsed < 0.25 || parsed > 16.0)
            return false;
        *value = (float) parsed;
        return true;
    }

    static bool ValidToken(const char* text)
    {
        if (strlen(text) >= sizeof(((State*)0)->m_ResetToken))
            return false;
        for (const char* p = text; *p; ++p)
        {
            if (!(*p >= '0' && *p <= '9') && !(*p >= 'a' && *p <= 'f') && *p != '-')
                return false;
        }
        return true;
    }

    void ReadResetToken(const char* path, char* token, uint32_t capacity)
    {
        FILE* file = fopen(path, "rb");
        if (!file)
            return;
        char buffer[40];
        size_t length = fread(buffer, 1, sizeof(buffer), file);
        bool ok = !ferror(file) && length < sizeof(buffer);
        fclose(file);
        if (ok)
        {
            buffer[length] = 0;
            if (ValidToken(buffer))
                dmStrlCpy(token, buffer, capacity);
        }
    }

    LoadResult Load(const char* path, State* state)
    {
        FILE* file = fopen(path, "rb");
        if (!file)
            return errno == ENOENT ? LOAD_MISSING : LOAD_INVALID;
        char buffer[4096];
        size_t length = fread(buffer, 1, sizeof(buffer), file);
        bool ok = !ferror(file) && length < sizeof(buffer);
        fclose(file);
        if (!ok)
            return LOAD_INVALID;
        dmConfigFile::HConfig config;
        if (dmConfigFile::Parse(buffer, (uint32_t) length, &config) != dmConfigFile::RESULT_OK)
            return LOAD_INVALID;

        int32_t version = 0;
        bool has_version = ReadInt(config, "window.version", 1, 0x7fffffff, &version);
        if (has_version && version != 1)
        {
            dmConfigFile::Delete(config);
            return LOAD_UNSUPPORTED_VERSION;
        }
        State result = {};
        int32_t has_position = 0;
        ok = has_version
            && ReadInt(config, "window.width", 1, MAX_SIZE, &result.m_Rect.m_Width)
            && ReadInt(config, "window.height", 1, MAX_SIZE, &result.m_Rect.m_Height)
            && ReadInt(config, "window.config_width", 1, MAX_SIZE, &result.m_ConfigWidth)
            && ReadInt(config, "window.config_height", 1, MAX_SIZE, &result.m_ConfigHeight)
            && ReadInt(config, "window.has_position", 0, 1, &has_position)
            && ReadScale(config, "monitor.scale_x", &result.m_Monitor.m_ScaleX)
            && ReadScale(config, "monitor.scale_y", &result.m_Monitor.m_ScaleY);
        const char* reset_token = dmConfigFile::GetString(config, "window.reset_token", "");
        ok = ok && ValidToken(reset_token);
        if (ok && has_position)
        {
            Rect& work = result.m_Monitor.m_WorkArea;
            ok = ReadInt(config, "window.x", -MAX_COORDINATE, MAX_COORDINATE, &result.m_Rect.m_X)
                && ReadInt(config, "window.y", -MAX_COORDINATE, MAX_COORDINATE, &result.m_Rect.m_Y)
                && ReadInt(config, "monitor.x", -MAX_COORDINATE, MAX_COORDINATE, &work.m_X)
                && ReadInt(config, "monitor.y", -MAX_COORDINATE, MAX_COORDINATE, &work.m_Y)
                && ReadInt(config, "monitor.width", 1, MAX_SIZE, &work.m_Width)
                && ReadInt(config, "monitor.height", 1, MAX_SIZE, &work.m_Height);
            const char* hash = dmConfigFile::GetString(config, "monitor.name_hash", "");
            char* end;
            errno = 0;
            result.m_Monitor.m_NameHash = strtoull(hash, &end, 16);
            ok = ok && strlen(hash) == 16 && !errno && !*end;
            result.m_Rect.m_X += work.m_X;
            result.m_Rect.m_Y += work.m_Y;
            ok = ok && abs(result.m_Rect.m_X) <= MAX_COORDINATE && abs(result.m_Rect.m_Y) <= MAX_COORDINATE;
        }
        if (ok)
        {
            result.m_HasPosition = has_position != 0;
            dmStrlCpy(result.m_ResetToken, reset_token, sizeof(result.m_ResetToken));
            *state = result;
        }
        dmConfigFile::Delete(config);
        return ok ? LOAD_OK : LOAD_INVALID;
    }

    bool Save(const char* path, const State& state)
    {
        // A sibling temp file permits replacement on the same filesystem. The
        // process id also isolates two editors accidentally launching the same slot.
        char temporary[1024];
#if defined(_WIN32)
        int pid = _getpid();
#else
        int pid = getpid();
#endif
        int length = dmSnPrintf(temporary, sizeof(temporary), "%s.%d.tmp", path, pid);
        if (length < 0 || (uint32_t) length >= sizeof(temporary))
            return false;
        FILE* file = fopen(temporary, "wb");
        if (!file)
            return false;
        const Rect& rect = state.m_Rect;
        const Rect& work = state.m_Monitor.m_WorkArea;
        // Format fixed-point components separately so printf cannot substitute
        // a locale-specific decimal separator in the persisted state.
        int32_t scale_x = (int32_t) round((double) state.m_Monitor.m_ScaleX * SCALE_PRECISION);
        int32_t scale_y = (int32_t) round((double) state.m_Monitor.m_ScaleY * SCALE_PRECISION);
        bool ok = fprintf(file,
            "[window]\nversion = 1\nwidth = %d\nheight = %d\nconfig_width = %d\nconfig_height = %d\n"
            "reset_token = %s\nhas_position = %d\nx = %d\ny = %d\n"
            "[monitor]\nname_hash = %016" PRIx64 "\nx = %d\ny = %d\nwidth = %d\nheight = %d\nscale_x = %d.%06d\nscale_y = %d.%06d\n",
            rect.m_Width, rect.m_Height, state.m_ConfigWidth, state.m_ConfigHeight,
            state.m_ResetToken, state.m_HasPosition ? 1 : 0, rect.m_X - work.m_X, rect.m_Y - work.m_Y,
            state.m_Monitor.m_NameHash, work.m_X, work.m_Y, work.m_Width, work.m_Height,
            scale_x / SCALE_PRECISION, scale_x % SCALE_PRECISION, scale_y / SCALE_PRECISION, scale_y % SCALE_PRECISION) > 0;
        ok = fclose(file) == 0 && ok;
        if (ok)
            ok = dmSys::Rename(path, temporary) == dmSys::RESULT_OK;
        if (!ok)
            remove(temporary);
        return ok;
    }

    bool RestoreSize(const State& state, int32_t width, int32_t height, const char* reset_token)
    {
        return state.m_ConfigWidth == width && state.m_ConfigHeight == height && !strcmp(state.m_ResetToken, reset_token);
    }

    uint32_t SelectMonitor(const State& state, const Monitor* monitors, uint32_t count)
    {
        uint32_t matching_count = 0, match = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (monitors[i].m_NameHash == state.m_Monitor.m_NameHash)
            {
                match = i;
                ++matching_count;
            }
        }
        if (matching_count == 1)
            return match;
        int64_t largest_overlap = 0;
        uint32_t best = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            const Rect& work = monitors[i].m_WorkArea;
            const Rect& rect = state.m_Rect;
            int64_t width = dmMath::Max(0, dmMath::Min(rect.m_X + rect.m_Width, work.m_X + work.m_Width) - dmMath::Max(rect.m_X, work.m_X));
            int64_t height = dmMath::Max(0, dmMath::Min(rect.m_Y + rect.m_Height, work.m_Y + work.m_Height) - dmMath::Max(rect.m_Y, work.m_Y));
            if (width * height > largest_overlap)
            {
                largest_overlap = width * height;
                best = i;
            }
        }
        return best;
    }

    Rect RestoreRect(const State& state, const Monitor& monitor, bool scale_screen_coordinates)
    {
        float sx = scale_screen_coordinates ? monitor.m_ScaleX / state.m_Monitor.m_ScaleX : 1.0f;
        float sy = scale_screen_coordinates ? monitor.m_ScaleY / state.m_Monitor.m_ScaleY : 1.0f;
        Rect rect;
        rect.m_X = monitor.m_WorkArea.m_X + (int32_t) roundf((state.m_Rect.m_X - state.m_Monitor.m_WorkArea.m_X) * sx);
        rect.m_Y = monitor.m_WorkArea.m_Y + (int32_t) roundf((state.m_Rect.m_Y - state.m_Monitor.m_WorkArea.m_Y) * sy);
        rect.m_Width = dmMath::Clamp((int32_t) roundf(state.m_Rect.m_Width * sx), 1, MAX_SIZE);
        rect.m_Height = dmMath::Clamp((int32_t) roundf(state.m_Rect.m_Height * sy), 1, MAX_SIZE);
        return rect;
    }

    void ClampPosition(Rect* rect, const Rect& work, int left, int top, int right, int bottom)
    {
        int min_x = work.m_X + left;
        int min_y = work.m_Y + top;
        int max_x = dmMath::Max(min_x, work.m_X + work.m_Width - rect->m_Width - right);
        int max_y = dmMath::Max(min_y, work.m_Y + work.m_Height - rect->m_Height - bottom);
        rect->m_X = dmMath::Clamp(rect->m_X, min_x, max_x);
        rect->m_Y = dmMath::Clamp(rect->m_Y, min_y, max_y);
    }
}
