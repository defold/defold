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

#include "record_private.h"
#include "record_file.h"
#include "record_service.h"
#include "record_service_openapi.h"
#include <dlib/array.h>
#include <dlib/crypt.h>
#include <dlib/dstrings.h>
#include <dlib/mutex.h>
#include <dlib/sys.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <dlib/webserver.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace dmRecord
{
    enum FileState
    {
        FILE_RECORDING,
        FILE_PREPARING,
        FILE_READY,
        FILE_FAILED
    };

    struct RecordingFile
    {
        char             m_Id[96];
        char             m_Path[1024];
        char             m_Sha256[65];
        uint64_t         m_Size;
        uint64_t         m_OperationId;
        dmMutex::HMutex  m_Mutex;
        dmThread::Thread m_Worker;
        FileState        m_State;
        uint32_t         m_References;
    };

    struct FileDownload
    {
        FILE*          m_Input;
        RecordingFile* m_File;
    };

    static dmWebServer::HServer    g_Server;
    static GetNativeWindow         g_GetWindow;
    static char                    g_Directory[1024];
    static dmArray<RecordingFile*> g_Files;
    static RecordingFile*          g_CurrentFile;
    static uint64_t                g_Sequence;
    static char                    g_InstanceId[96];
    static uint64_t                g_Generation;

    static void JoinWorker(dmThread::Thread worker)
    {
        if (!worker)
        {
            return;
        }
        dmThread::Join(worker);
#if defined(_WIN32)
        dmThread::Detach(worker);
#endif
    }

    static void ReleaseFile(RecordingFile* file)
    {
        bool released;
        {
            DM_MUTEX_SCOPED_LOCK(file->m_Mutex);
            released = --file->m_References == 0;
        }
        if (released)
        {
            FileRemove(file->m_Path);
            dmMutex::Delete(file->m_Mutex);
            delete file;
        }
    }

    static uint32_t ReadDownload(void* context, void* buffer, uint32_t size)
    {
        FileDownload* download = (FileDownload*)context;
        return (uint32_t)fread(buffer, 1, size, download->m_Input);
    }

    static void CloseDownload(void* context)
    {
        FileDownload* download = (FileDownload*)context;
        fclose(download->m_Input);
        ReleaseFile(download->m_File);
        delete download;
    }

    static void DeleteFile(uint32_t index)
    {
        RecordingFile* file = g_Files[index];
        JoinWorker(file->m_Worker);
        if (g_CurrentFile == file)
        {
            g_CurrentFile = 0;
        }
        g_Files.EraseSwap(index);
        // Hide the receipt immediately. An active download keeps the file alive
        // until its worker closes it, including on Windows and during reboot.
        ReleaseFile(file);
    }

    static void PrepareFile(void* arg)
    {
        RecordingFile* recording = (RecordingFile*)arg;
        dmCrypt::HHashSha256 hash = dmCrypt::NewHashSha256();
        bool failed = hash == 0;
        FILE* file = failed ? 0 : FileOpen(recording->m_Path, "rb");
        uint64_t size = 0;
        if (!file)
        {
            failed = true;
        }
        else
        {
            uint8_t buffer[65536];
            size_t count;
            while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0)
            {
                size += count;
                if (dmCrypt::UpdateHashSha256(hash, buffer, (uint32_t)count) != dmCrypt::RESULT_OK)
                {
                    failed = true;
                    break;
                }
            }
            failed = failed || ferror(file) || size == 0;
            fclose(file);
        }
        uint8_t digest[32];
        if (!failed && dmCrypt::FinalHashSha256(hash, digest) != dmCrypt::RESULT_OK)
        {
            failed = true;
        }
        dmCrypt::DeleteHashSha256(hash);
        DM_MUTEX_SCOPED_LOCK(recording->m_Mutex);
        if (!failed)
        {
            for (uint32_t i = 0; i < sizeof(digest); ++i)
            {
                dmSnPrintf(recording->m_Sha256 + 2 * i, 3, "%02x", digest[i]);
            }
        }
        recording->m_Size = size;
        recording->m_State = failed ? FILE_FAILED : FILE_READY;
    }

    static void OnStatus(const Status* status)
    {
        if (!g_CurrentFile || g_CurrentFile->m_OperationId != status->m_Id || status->m_Pending || status->m_Active)
        {
            return;
        }
        RecordingFile* file = g_CurrentFile;
        if (!status->m_Finalized)
        {
            for (uint32_t i = 0; i < g_Files.Size(); ++i)
            {
                if (g_Files[i] == file)
                {
                    DeleteFile(i);
                    break;
                }
            }
            return;
        }
        DM_MUTEX_SCOPED_LOCK(file->m_Mutex);
        if (file->m_State != FILE_RECORDING)
        {
            return;
        }
        file->m_State = FILE_PREPARING;
        file->m_Worker = dmThread::New(PrepareFile, 128 * 1024, file, "record-file");
        if (!file->m_Worker)
        {
            file->m_State = FILE_FAILED;
        }
    }

    static StatusListener g_StatusListener = { OnStatus, 0 };

    // All response strings originate in bounded status/platform buffers.
    static void EscapeJson(const char* source, char* output)
    {
        while (*source)
        {
            unsigned char c = (unsigned char)*source++;
            if (c == '"' || c == '\\')
            {
                *output++ = '\\';
                *output++ = c;
            }
            else if (c < 32)
            {
                dmSnPrintf(output, 7, "\\u%04x", c);
                output += 6;
            }
            else
            {
                *output++ = c;
            }
        }
        *output = 0;
    }

    static void SendJson(dmWebServer::Request* request, int status, const char* body)
    {
        dmWebServer::SetStatusCode(request, status);
        dmWebServer::SendAttribute(request, "Content-Type", "application/json");
        dmWebServer::SendAttribute(request, "Cache-Control", "no-store");
        dmWebServer::Send(request, body, (uint32_t)strlen(body));
    }

    static void Error(dmWebServer::Request* request, int status, const char* code, const char* message)
    {
        char escaped[512 * 6 + 1];
        EscapeJson(message, escaped);
        char body[4096];
        dmSnPrintf(body, sizeof(body), "{\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}", code, escaped);
        SendJson(request, status, body);
    }

    static void SendStatus(dmWebServer::Request* request, int code, const Status* status)
    {
        char file_json[512] = "null";
        if (g_CurrentFile && g_CurrentFile->m_OperationId == status->m_Id)
        {
            RecordingFile* file = g_CurrentFile;
            DM_MUTEX_SCOPED_LOCK(file->m_Mutex);
            const char* state = file->m_State == FILE_READY ? "complete" : (file->m_State == FILE_FAILED ? "failed" : "pending");
            dmSnPrintf(file_json, sizeof(file_json), "{\"id\":\"%s\",\"url\":\"/recording/files/%s\",\"state\":\"%s\",\"size\":%llu,\"sha256\":\"%s\"}", file->m_Id, file->m_Id, state, (unsigned long long)file->m_Size, file->m_Sha256);
        }
        const char* state = status->m_Pending ? (status->m_Stopping ? "stopping" : "starting") :
                                                (status->m_Failure[0] ? "failed" : (status->m_Active ? "recording" : (status->m_Finalized ? "complete" : "idle")));
        char failure[sizeof(status->m_Failure) * 6 + 1];
        EscapeJson(status->m_Failure, failure);
        char body[8192];
        dmSnPrintf(body, sizeof(body), "{\"instance_id\":\"%s\",\"operation_id\":%llu,\"state\":\"%s\",\"pending\":%s,\"supported\":%s,\"active\":%s,\"finalized\":%s,"
                                       "\"backend\":\"%s\",\"format\":\"mp4\",\"codec\":\"h264\",\"audio\":%s,\"width\":%u,\"height\":%u,\"fps\":%u,"
                                       "\"started_wall_time_us\":%llu,\"started_monotonic_time_us\":%llu,\"stopped_wall_time_us\":%llu,"
                                       "\"stopped_monotonic_time_us\":%llu,\"failure\":\"%s\",\"file\":%s}",
                   g_InstanceId,
                   (unsigned long long)status->m_Id,
                   state,
                   status->m_Pending ? "true" : "false",
                   status->m_Supported ? "true" : "false",
                   status->m_Active ? "true" : "false",
                   status->m_Finalized ? "true" : "false",
                   GetBackendName(),
                   status->m_Audio ? "true" : "false",
                   status->m_Width,
                   status->m_Height,
                   status->m_Fps,
                   (unsigned long long)status->m_StartedWallTime,
                   (unsigned long long)status->m_StartedMonotonicTime,
                   (unsigned long long)status->m_StoppedWallTime,
                   (unsigned long long)status->m_StoppedMonotonicTime,
                   failure,
                   file_json);
        SendJson(request, code, body);
    }

    static void SkipWhitespace(const char** p)
    {
        while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n')
        {
            ++*p;
        }
    }

    // The request schema contains only known ASCII field names, unsigned
    // integers and booleans. Reject duplicates, unknown fields and nested data.
    static bool ParseName(const char** p, char* name, uint32_t size)
    {
        if (*(*p)++ != '"')
        {
            return false;
        }
        uint32_t length = 0;
        while (**p && **p != '"')
        {
            unsigned char c = (unsigned char)*(*p)++;
            if (c == '\\')
            {
                if (*(*p)++ != 'u')
                {
                    return false;
                }
                uint32_t value = 0;
                for (uint32_t i = 0; i < 4; ++i)
                {
                    c = (unsigned char)**p;
                    if (!c)
                    {
                        return false;
                    }
                    ++*p;
                    uint32_t digit = c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : (c >= 'A' && c <= 'F' ? c - 'A' + 10 : 16));
                    if (digit == 16)
                    {
                        return false;
                    }
                    value = value * 16 + digit;
                }
                if (value > 127)
                {
                    return false;
                }
                c = (unsigned char)value;
            }
            if (c < 32 || c > 127 || length + 1 >= size)
            {
                return false;
            }
            name[length++] = c;
        }
        if (**p != '"')
        {
            return false;
        }
        ++*p;
        name[length] = 0;
        return true;
    }

    static bool ParseUInt(const char** p, uint64_t* value)
    {
        if (**p < '0' || **p > '9')
        {
            return false;
        }
        bool zero = **p == '0';
        *value = 0;
        uint32_t digits = 0;
        do
        {
            uint32_t digit = *(*p)++ - '0';
            if (*value > (UINT64_MAX - digit) / 10)
            {
                return false;
            }
            *value = *value * 10 + digit;
            ++digits;
        } while (**p >= '0' && **p <= '9');
        return !zero || digits == 1;
    }

    static bool ParseOptions(const char* body, bool stopping, StartParams* params, uint64_t* id)
    {
        const char* p = body;
        SkipWhitespace(&p);
        if (*p++ != '{')
        {
            return false;
        }
        SkipWhitespace(&p);
        uint32_t seen = 0;
        while (*p != '}')
        {
            char name[32];
            if (!ParseName(&p, name, sizeof(name)))
            {
                return false;
            }
            uint32_t field = !strcmp(name, "fps") ? 1 : (!strcmp(name, "audio") ? 2 : (!strcmp(name, "operation_id") ? 4 : 0));
            if (!field || (seen & field) || (stopping ? field != 4 : field == 4))
            {
                return false;
            }
            seen |= field;
            SkipWhitespace(&p);
            if (*p++ != ':')
            {
                return false;
            }
            SkipWhitespace(&p);
            if (field == 2)
            {
                if (!strncmp(p, "true", 4))
                {
                    params->m_Audio = true;
                    p += 4;
                }
                else if (!strncmp(p, "false", 5))
                {
                    params->m_Audio = false;
                    p += 5;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                uint64_t value;
                if (!ParseUInt(&p, &value) || !value)
                {
                    return false;
                }
                if (field == 1 && value > 60)
                {
                    return false;
                }
                if (field == 1)
                {
                    params->m_Fps = (uint32_t)value;
                }
                else
                {
                    *id = value;
                }
            }
            SkipWhitespace(&p);
            if (*p == '}')
            {
                break;
            }
            if (*p++ != ',')
            {
                return false;
            }
            SkipWhitespace(&p);
            if (*p == '}')
            {
                return false;
            }
        }
        ++p;
        SkipWhitespace(&p);
        return *p == 0 && (!stopping || seen == 4);
    }

    static void FileRequest(dmWebServer::Request* request, const char* id)
    {
        uint32_t index = 0;
        while (index < g_Files.Size() && strcmp(g_Files[index]->m_Id, id))
        {
            ++index;
        }
        if (index == g_Files.Size())
        {
            Error(request, 410, "stale_file", "recording file no longer exists");
            return;
        }
        RecordingFile* file = g_Files[index];
        FileState state;
        {
            DM_MUTEX_SCOPED_LOCK(file->m_Mutex);
            state = file->m_State;
        }
        if (state == FILE_RECORDING || state == FILE_PREPARING)
        {
            Error(request, 409, "file_pending", "recording file is not ready");
            return;
        }
        if (!strcmp(request->m_Method, "DELETE"))
        {
            DeleteFile(index);
            dmWebServer::SetStatusCode(request, 204);
            return;
        }
        if (state == FILE_FAILED)
        {
            Error(request, 500, "file_failed", "recording file could not be prepared");
            return;
        }
        const char* range = dmWebServer::GetHeader(request, "Range");
        unsigned long long start = 0, end = file->m_Size - 1;
        int consumed = 0;
        if (range && (sscanf(range, "bytes=%llu-%llu%n", &start, &end, &consumed) != 2 || range[consumed] || end < start || start >= file->m_Size || end >= file->m_Size))
        {
            Error(request, 416, "invalid_range", "provide a closed byte range within the file");
            return;
        }
        FILE* input = FileOpen(file->m_Path, "rb");
#if defined(_WIN32)
        bool seek_ok = input && _fseeki64(input, start, SEEK_SET) == 0;
#else
        bool seek_ok = input && fseeko(input, (off_t)start, SEEK_SET) == 0;
#endif
        if (!seek_ok)
        {
            if (input)
            {
                fclose(input);
            }
            Error(request, 500, "file_unavailable", "cannot read recording file");
            return;
        }
        dmWebServer::SetStatusCode(request, range ? 206 : 200);
        dmWebServer::SendAttribute(request, "Content-Type", "video/mp4");
        if (range)
        {
            char content_range[100];
            dmSnPrintf(content_range, sizeof(content_range), "bytes %llu-%llu/%llu", start, end, (unsigned long long)file->m_Size);
            dmWebServer::SendAttribute(request, "Content-Range", content_range);
        }
        dmWebServer::SendAttribute(request, "Accept-Ranges", "bytes");
        dmWebServer::SendAttribute(request, "ETag", file->m_Sha256);
        FileDownload* download = new FileDownload;
        download->m_Input = input;
        download->m_File = file;
        {
            DM_MUTEX_SCOPED_LOCK(file->m_Mutex);
            ++file->m_References;
        }
        if (dmWebServer::SendStream(request, end - start + 1, ReadDownload, CloseDownload, download) != dmWebServer::RESULT_OK)
        {
            CloseDownload(download);
        }
    }

    static void Handler(void*, dmWebServer::Request* request)
    {
        char body[1024];
        if (request->m_ContentLength >= sizeof(body))
        {
            dmWebServer::CloseConnection(request);
            Error(request, 413, "body_too_large", "recording requests must be smaller than 1024 bytes");
            return;
        }
        dmWebServer::Result received = dmWebServer::ReceiveBuffered(request, body, sizeof(body));
        if (received == dmWebServer::RESULT_TRY_AGAIN)
        {
            return;
        }
        if (received != dmWebServer::RESULT_OK || memchr(body, 0, request->m_ContentLength))
        {
            dmWebServer::CloseConnection(request);
            Error(request, 400, "invalid_body", "cannot read request body");
            return;
        }
        body[request->m_ContentLength] = 0;
        const char* path = request->m_Resource;
        if (strchr(path, '?'))
        {
            Error(request, 400, "invalid_query", "use JSON request bodies, not query parameters");
            return;
        }
        const bool capabilities = !strcmp(path, "/recording/capabilities");
        const bool status_route = !strcmp(path, "/recording/status");
        const bool start = !strcmp(path, "/recording/start");
        const bool stop = !strcmp(path, "/recording/stop");
        const bool file = !strncmp(path, "/recording/files/", 17);
        if (!capabilities && !status_route && !start && !stop && !file)
        {
            Error(request, 404, "not_found", "recording route does not exist");
            return;
        }
        const char* allowed = file ? "GET, DELETE" : ((start || stop) ? "POST" : "GET");
        bool method_ok = file ? (!strcmp(request->m_Method, "GET") || !strcmp(request->m_Method, "DELETE")) : !strcmp(request->m_Method, allowed);
        if (!method_ok)
        {
            dmWebServer::SetStatusCode(request, 405);
            dmWebServer::SendAttribute(request, "Allow", allowed);
            Error(request, 405, "method_not_allowed", "method is not supported by this recording route");
            return;
        }
        if (!(start || stop) && request->m_ContentLength)
        {
            Error(request, 400, "unexpected_body", "this route does not accept a body");
            return;
        }
        const char* instance = dmWebServer::GetHeader(request, "X-Recording-Instance");
        const bool mutation = start || stop || (file && !strcmp(request->m_Method, "DELETE"));
        if (mutation && (!instance || !instance[0]))
        {
            Error(request, 400, "missing_instance", "X-Recording-Instance from /recording/capabilities is required");
            return;
        }
        if (!capabilities && instance && strcmp(instance, g_InstanceId))
        {
            Error(request, 410, "stale_instance", "recording service restarted; obtain capabilities and reacquire recording receipts");
            return;
        }
        if (file)
        {
            FileRequest(request, path + 17);
            return;
        }
        if (capabilities)
        {
            char response[2048];
            bool supported = IsSupported() && g_GetWindow();
            dmSnPrintf(response, sizeof(response), "{\"version\":1,\"instance_id\":\"%s\",\"supported\":%s,\"audio\":%s,\"backend\":\"%s\",\"format\":\"mp4\",\"codec\":\"h264\","
                                                   "\"minimum_platform_version\":\"%s\",\"unsupported_reason\":\"%s\",\"max_fps\":60}",
                       g_InstanceId,
                       supported ? "true" : "false",
                       supported && IsAudioSupported() ? "true" : "false",
                       GetBackendName(),
                       GetMinimumPlatformVersion(),
                       supported ? "" : GetUnsupportedReason());
            SendJson(request, 200, response);
            return;
        }
        Status status;
        GetStatus(&status);
        if (status_route)
        {
            SendStatus(request, 200, &status);
            return;
        }
        if (request->m_ContentLength)
        {
            const char* type = dmWebServer::GetHeader(request, "Content-Type");
            if (!type || strncmp(type, "application/json", 16) || (type[16] && type[16] != ';'))
            {
                Error(request, 415, "unsupported_media_type", "use application/json");
                return;
            }
        }
        StartParams params;
        params.m_Audio = IsAudioSupported();
        uint64_t id = 0;
        if (!ParseOptions(request->m_ContentLength ? body : "{}", stop, &params, &id))
        {
            Error(request, 400, "invalid_options", "request does not match the recording schema in /openapi.json");
            return;
        }
        if (stop)
        {
            if (id != status.m_Id)
            {
                Error(request, 410, "stale_operation", "recording operation was replaced");
                return;
            }
            if (!Stop(&status))
            {
                Error(request, 409, "recording_inactive", status.m_Failure);
                return;
            }
            SendStatus(request, 202, &status);
            return;
        }
        if (!IsSupported())
        {
            Error(request, 501, "recording_unsupported", GetUnsupportedReason());
            return;
        }
        if (params.m_Audio && !IsAudioSupported())
        {
            Error(request, 501, "audio_unsupported", "application audio is unavailable; use audio=false");
            return;
        }
        if (status.m_Pending || status.m_Active)
        {
            Error(request, 409, "recording_active", "a recording operation is already active");
            return;
        }
        if (g_Files.Size() >= 128)
        {
            Error(request, 507, "storage_full", "delete retained recording files before starting another recording");
            return;
        }
        if (!g_Directory[0])
        {
            char directory[sizeof(g_Directory)];
            if (dmSys::GetApplicationSavePath("defold-record", directory, sizeof(directory)) != dmSys::RESULT_OK ||
                !HostPathToUtf8(directory, g_Directory, sizeof(g_Directory)))
            {
                Error(request, 500, "storage_unavailable", "cannot create recording storage");
                return;
            }
        }
        RecordingFile* recording = new RecordingFile;
        memset(recording, 0, sizeof(*recording));
        dmSnPrintf(recording->m_Id, sizeof(recording->m_Id), "r%llx-%llx-%llu", (unsigned long long)dmTime::GetTime(), (unsigned long long)dmTime::GetMonotonicTime(), (unsigned long long)++g_Sequence);
        int count = dmSnPrintf(recording->m_Path, sizeof(recording->m_Path), "%s/%s.mp4", g_Directory, recording->m_Id);
        if (count < 0 || (uint32_t)count >= sizeof(recording->m_Path))
        {
            delete recording;
            Error(request, 500, "storage_unavailable", "recording storage path is too long");
            return;
        }
        params.m_Window = g_GetWindow();
        params.m_Path = recording->m_Path;
        if (!Start(&params, &status))
        {
            delete recording;
            Error(request, 500, "start_failed", status.m_Failure);
            return;
        }
        recording->m_Mutex = dmMutex::New();
        recording->m_References = 1;
        recording->m_OperationId = status.m_Id;
        recording->m_State = FILE_RECORDING;
        if (g_Files.Full())
        {
            g_Files.OffsetCapacity(16);
        }
        g_Files.Push(recording);
        g_CurrentFile = recording;
        SendStatus(request, 202, &status);
    }

    bool RegisterService(dmWebServer::HServer server, GetNativeWindow get_window, const char* directory)
    {
        if (g_Server)
        {
            return g_Server == server;
        }
        dmWebServer::HandlerParams handler;
        handler.m_Userdata = 0;
        handler.m_Handler = Handler;
        if (dmWebServer::AddHandler(server, "/recording", &handler, RECORD_OPENAPI) != dmWebServer::RESULT_OK)
        {
            return false;
        }
        g_Server = server;
        AddStatusListener(&g_StatusListener);
        g_GetWindow = get_window;
        dmStrlCpy(g_Directory, directory ? directory : "", sizeof(g_Directory));
#if defined(_WIN32)
        uint32_t process_id = (uint32_t)_getpid();
#else
        uint32_t process_id = (uint32_t)getpid();
#endif
        // The generation distinguishes reboots even within one clock tick; the
        // process and clocks distinguish replacement engines at the same URL.
        dmSnPrintf(g_InstanceId, sizeof(g_InstanceId), "%llx-%llx-%x-%llu", (unsigned long long)dmTime::GetTime(), (unsigned long long)dmTime::GetMonotonicTime(), process_id, (unsigned long long)++g_Generation);
        return true;
    }

    void FinalizeService()
    {
        if (!g_Server)
        {
            return;
        }
        dmWebServer::RemoveHandler(g_Server, "/recording");
        RemoveStatusListener(&g_StatusListener);
        Finalize();
        while (!g_Files.Empty())
        {
            DeleteFile(g_Files.Size() - 1);
        }
        g_Files.SetCapacity(0);
        g_Server = 0;
        g_GetWindow = 0;
        g_Directory[0] = 0;
        g_InstanceId[0] = 0;
    }
} // namespace dmRecord
