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

#include "automation_private.h"
#include "automation_artifact.h"


#include <errno.h>
#include <dmsdk/dlib/crypt.h>
#include <dmsdk/dlib/sys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#include <stb/stb_image_write.h>

#if defined(_WIN32)
#include <direct.h>
#endif

namespace dmAutomation
{
    bool IsScreenshotSupported()
    {
        if (!g_AutomationBridge.m_GraphicsContext)
        {
            return false;
        }

        dmGraphics::AdapterFamily family = dmGraphics::GetInstalledAdapterFamily();
        return family == dmGraphics::ADAPTER_FAMILY_OPENGL ||
               family == dmGraphics::ADAPTER_FAMILY_OPENGLES ||
               family == dmGraphics::ADAPTER_FAMILY_VULKAN ||
               family == dmGraphics::ADAPTER_FAMILY_METAL;
    }


    bool BuildScreenshotPath(uint64_t, char* path, uint32_t path_size)
    {
        return BuildCapturePath(".png", path, path_size);
    }

    bool ScheduleScreenshot(uint32_t after_frames, ScreenshotCapture* capture)
    {
        if (g_AutomationBridge.m_Screenshot.m_State == SCREENSHOT_PENDING)
        {
            return false;
        }
        if (g_AutomationBridge.m_Screenshot.m_State != SCREENSHOT_NONE)
        {
            if (g_AutomationBridge.m_ScreenshotHistory.Size() >= 16)
            {
                ArrayErase(&g_AutomationBridge.m_ScreenshotHistory, 0);
            }
            ArrayPush(&g_AutomationBridge.m_ScreenshotHistory, &g_AutomationBridge.m_Screenshot);
        }

        ScreenshotCapture next;
        memset(&next, 0, sizeof(next));
        next.m_Id = ++g_AutomationBridge.m_ScreenshotCounter;
        next.m_State = SCREENSHOT_PENDING;
        next.m_AfterFrames = after_frames;
        next.m_Frame = g_AutomationBridge.m_Frame;
        next.m_SceneSequence = g_AutomationBridge.m_Snapshot.m_Sequence;
        next.m_Width = g_AutomationBridge.m_Snapshot.m_BackbufferWidth;
        next.m_Height = g_AutomationBridge.m_Snapshot.m_BackbufferHeight;
        if (!BuildScreenshotPath(next.m_Id, next.m_Path, sizeof(next.m_Path)))
        {
            return false;
        }
        g_AutomationBridge.m_Screenshot = next;
        if (capture)
        {
            *capture = next;
        }
        return true;
    }

    const ScreenshotCapture* FindScreenshotCapture(uint64_t capture_id)
    {
        if (g_AutomationBridge.m_Screenshot.m_Id == capture_id &&
            g_AutomationBridge.m_Screenshot.m_State != SCREENSHOT_NONE)
        {
            return &g_AutomationBridge.m_Screenshot;
        }
        for (uint32_t i = 0; i < g_AutomationBridge.m_ScreenshotHistory.Size(); ++i)
        {
            const ScreenshotCapture* capture = &g_AutomationBridge.m_ScreenshotHistory.Begin()[i];
            if (capture->m_Id == capture_id)
            {
                return capture;
            }
        }
        return 0;
    }

    static bool ByteArrayPushBytes(ByteArray* array, const uint8_t* data, uint32_t count)
    {
        if (count == 0)
        {
            return true;
        }
        if (!ArrayReserve(array, array->Size() + count))
        {
            return false;
        }
        memcpy(array->Begin() + array->Size(), data, count);
        array->SetSize(array->Size() + count);
        return true;
    }

    static bool EncodePngRgba(const ByteArray* rgba, uint32_t width, uint32_t height, ByteArray* png)
    {
        uint64_t rgba_size = (uint64_t)width * height * 4;
        if (width == 0 || height == 0 || rgba_size != rgba->Size())
        {
            return false;
        }

        int length = 0;
        unsigned char* encoded = stbi_write_png_to_mem(rgba->Begin(), width * 4, width, height, 4, &length);
        if (!encoded) return false;
        png->SetSize(0);
        bool result = ByteArrayPushBytes(png, encoded, (uint32_t)length);
        free(encoded);
        return result;
    }

    static bool CaptureScreenshotPng(ByteArray* png)
    {
        if (!IsScreenshotSupported())
        {
            return false;
        }

        int32_t x = 0;
        int32_t y = 0;
        uint32_t width = dmGraphics::GetWindowWidth(g_AutomationBridge.m_GraphicsContext);
        uint32_t height = dmGraphics::GetWindowHeight(g_AutomationBridge.m_GraphicsContext);
        if (width == 0 || height == 0 || (uint64_t)width * height > UINT32_MAX / 4)
        {
            return false;
        }

        ByteArray pixels;
        ArrayInit(&pixels);
        if (!ArraySetCount(&pixels, width * height * 4))
        {
            return false;
        }

        dmGraphics::SetRenderTarget(g_AutomationBridge.m_GraphicsContext, 0, {});
        dmGraphics::ReadPixels(g_AutomationBridge.m_GraphicsContext, x, y, width, height, pixels.Begin(), pixels.Size());

        for (uint32_t i = 0; i < pixels.Size(); i += 4)
        {
            uint8_t b = pixels.Begin()[i + 0];
            pixels.Begin()[i + 0] = pixels.Begin()[i + 2];
            pixels.Begin()[i + 2] = b;
        }

        // Defold's graphics adapters normalize ReadPixels rows to top-left
        // display order. Preserve that order so PNG pixels use the same
        // coordinates as HID, scene bounds, and coordinate conversion.
        bool ok = EncodePngRgba(&pixels, width, height, png);
        ArrayFree(&pixels);
        return ok;
    }

    static bool WriteBytesToFile(const char* path, const ByteArray* bytes)
    {
        FILE* file = 0;
#if defined(_WIN32)
        if (fopen_s(&file, path, "wb") != 0)
        {
            file = 0;
        }
#else
        file = fopen(path, "wb");
#endif
        if (!file)
        {
            return false;
        }
        size_t written = fwrite(bytes->Begin(), 1, bytes->Size(), file);
        bool close_ok = fclose(file) == 0;
        return written == bytes->Size() && close_ok;
    }

    static void SetScreenshotFailure(ScreenshotCapture* capture, const char* failure)
    {
        capture->m_State = SCREENSHOT_FAILED;
        ReleaseCapturePath(capture->m_Path);
        dmSnPrintf(capture->m_Failure, sizeof(capture->m_Failure), "%s", failure ? failure : "capture failed");
    }

    static void FormatSha256(const ByteArray* bytes, char digest_string[65])
    {
        uint8_t digest[32];
        dmCrypt::HashSha256(bytes->Begin(), bytes->Size(), digest);
        for (uint32_t i = 0; i < sizeof(digest); ++i)
        {
            dmSnPrintf(digest_string + i * 2, 3, "%02x", digest[i]);
        }
        digest_string[64] = 0;
    }

    void AppendScreenshotJson(StringBuffer* out, const ScreenshotCapture* capture)
    {
        const char* state = "none";
        if (capture->m_State == SCREENSHOT_PENDING) state = "pending";
        else if (capture->m_State == SCREENSHOT_COMPLETE) state = "complete";
        else if (capture->m_State == SCREENSHOT_FAILED) state = "failed";

        StringBufferAppend(out, "{\"capture_id\":");
        AppendNumber(out, (double)capture->m_Id);
        StringBufferAppend(out, ",\"state\":");
        AppendJsonString(out, state);
        StringBufferAppend(out, ",\"pending\":");
        StringBufferAppend(out, capture->m_State == SCREENSHOT_PENDING ? "true" : "false");
        StringBufferAppend(out, ",\"artifact\":");
        AppendArtifactJson(out, capture->m_State == SCREENSHOT_COMPLETE ? capture->m_Path : 0);
        StringBufferAppend(out, ",\"format\":\"png\",\"engine_frame\":");
        AppendNumber(out, (double)capture->m_Frame);
        StringBufferAppend(out, ",\"scene_sequence\":");
        AppendNumber(out, (double)capture->m_SceneSequence);
        StringBufferAppend(out, ",\"width\":");
        AppendNumber(out, capture->m_Width);
        StringBufferAppend(out, ",\"height\":");
        AppendNumber(out, capture->m_Height);
        StringBufferAppend(out, ",\"sha256\":");
        if (capture->m_Sha256[0]) AppendJsonString(out, capture->m_Sha256); else StringBufferAppend(out, "null");
        StringBufferAppend(out, ",\"failure_reason\":");
        if (capture->m_Failure[0]) AppendJsonString(out, capture->m_Failure); else StringBufferAppend(out, "null");
        StringBufferAppendChar(out, '}');
    }

    void ProcessPendingScreenshot()
    {
        ScreenshotCapture* capture = &g_AutomationBridge.m_Screenshot;
        if (capture->m_State != SCREENSHOT_PENDING)
        {
            return;
        }
        if (capture->m_AfterFrames > 0)
        {
            --capture->m_AfterFrames;
            return;
        }

        if (g_AutomationBridge.m_SnapshotFrame != g_AutomationBridge.m_Frame)
        {
            UpdateSnapshot();
            g_AutomationBridge.m_SnapshotFrame = g_AutomationBridge.m_Frame;
        }
        capture->m_Frame = g_AutomationBridge.m_Frame;
        capture->m_SceneSequence = g_AutomationBridge.m_Snapshot.m_Sequence;
        capture->m_Width = g_AutomationBridge.m_Snapshot.m_BackbufferWidth;
        capture->m_Height = g_AutomationBridge.m_Snapshot.m_BackbufferHeight;

        ByteArray png;
        ArrayInit(&png);
        if (!CaptureScreenshotPng(&png))
        {
            dmLogError("Unable to capture Automation Bridge screenshot");
            SetScreenshotFailure(capture, "capture_failed");
            ArrayFree(&png);
            return;
        }
        char temporary_path[1100];
        int temporary_written = dmSnPrintf(temporary_path, sizeof(temporary_path), "%s.tmp", capture->m_Path);
        if (temporary_written < 0 || (uint32_t)temporary_written >= sizeof(temporary_path) || !WriteBytesToFile(temporary_path, &png))
        {
            dmLogError("Unable to write Automation Bridge screenshot '%s'", capture->m_Path);
            SetScreenshotFailure(capture, "write_failed");
            ArrayFree(&png);
            return;
        }
        FormatSha256(&png, capture->m_Sha256);
        if (dmSys::Rename(capture->m_Path, temporary_path) != dmSys::RESULT_OK)
        {
            dmLogError("Unable to publish Automation Bridge screenshot '%s'", capture->m_Path);
            SetScreenshotFailure(capture, "atomic_rename_failed");
            ArrayFree(&png);
            return;
        }
        capture->m_State = SCREENSHOT_COMPLETE;
        ArrayFree(&png);
    }


}
