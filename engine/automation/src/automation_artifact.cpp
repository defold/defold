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

#include "automation_artifact.h"
#include <dlib/sys.h>
#include <dmsdk/dlib/thread.h>
#include <dmsdk/dlib/array.h>
#include <mbedtls/md.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

namespace dmAutomation
{
    struct Artifact
    {
        char m_Id[96];
        char m_Path[1100];
        char m_Source[1024];
        char m_Sha256[65];
        uint64_t m_Size;
        dmMutex::HMutex m_Mutex;
        dmThread::Thread m_Worker;
        bool m_Ready;
        bool m_Failed;
    };
    static dmArray<Artifact*> g_Artifacts;
    static dmArray<char*> g_CapturePaths;
    static uint64_t g_FileSequence;

    bool BuildCapturePath(const char* suffix, char* path, uint32_t size)
    {
        if (g_CapturePaths.Size() >= 128) return false;
        char directory[1024];
        if (dmSys::GetApplicationSavePath("defold-automation", directory, sizeof(directory)) != dmSys::RESULT_OK) return false;
        char identity[64];
        dmStrlCpy(identity, g_AutomationBridge.m_EngineInstanceId, sizeof(identity));
        for (char* p = identity; *p; ++p) if (*p == ':') *p = '_';
        int n = dmSnPrintf(path, size, "%s/%s-%llu%s", directory, identity, (unsigned long long)++g_FileSequence, suffix);
        if (n <= 0 || (uint32_t)n >= size) return false;
        if (g_CapturePaths.Full()) g_CapturePaths.OffsetCapacity(16);
        g_CapturePaths.Push(DuplicateString(path));
        return true;
    }

    void ReleaseCapturePath(const char* path)
    {
        for (uint32_t i = 0; i < g_CapturePaths.Size(); ++i)
        {
            if (!StringsEqual(g_CapturePaths[i], path)) continue;
            if (dmSys::IsDir(path) == dmSys::RESULT_OK) dmSys::RmTree(path);
            else remove(path);
            char temporary[1100];
            dmSnPrintf(temporary, sizeof(temporary), "%s.tmp", path);
            remove(temporary);
            free(g_CapturePaths[i]);
            g_CapturePaths.EraseSwap(i);
            return;
        }
    }

    struct ArchiveContext { FILE* m_File; uint32_t m_Prefix; bool m_Failed; };
    static void ArchiveEntry(void* data, const char* path, bool is_directory)
    {
        ArchiveContext* context = (ArchiveContext*)data;
        if (context->m_Failed || is_directory) return;
        const char* name = path + context->m_Prefix;
        struct stat info;
        if (stat(path, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 || (uint64_t)info.st_size > 077777777777ULL) { context->m_Failed = true; return; }
        char header[512] = {};
        if (strlen(name) <= 100) memcpy(header, name, strlen(name));
        else
        {
            const char* separator = strrchr(name, '/');
            if (!separator || separator - name > 155 || strlen(separator + 1) > 100) { context->m_Failed = true; return; }
            memcpy(header, separator + 1, strlen(separator + 1));
            memcpy(header + 345, name, separator - name);
        }
        dmSnPrintf(header + 100, 8, "%07o", 0600);
        dmSnPrintf(header + 108, 8, "%07o", 0);
        dmSnPrintf(header + 116, 8, "%07o", 0);
        dmSnPrintf(header + 124, 12, "%011llo", (unsigned long long)info.st_size);
        dmSnPrintf(header + 136, 12, "%011llo", (unsigned long long)info.st_mtime);
        memset(header + 148, ' ', 8);
        header[156] = '0';
        memcpy(header + 257, "ustar", 5);
        memcpy(header + 263, "00", 2);
        uint32_t checksum = 0;
        for (uint32_t i = 0; i < sizeof(header); ++i) checksum += (uint8_t)header[i];
        dmSnPrintf(header + 148, 7, "%06o", checksum);
        header[155] = ' ';
        FILE* input = fopen(path, "rb");
        if (!input) { context->m_Failed = true; return; }
        bool ok = fwrite(header, 1, sizeof(header), context->m_File) == sizeof(header);
        char buffer[65536];
        size_t count;
        uint64_t bytes = 0;
        while (ok && (count = fread(buffer, 1, sizeof(buffer), input)) != 0)
        {
            bytes += count;
            ok = fwrite(buffer, 1, count, context->m_File) == count;
        }
        ok = ok && !ferror(input) && bytes == (uint64_t)info.st_size;
        fclose(input);
        uint32_t padding = (512 - bytes % 512) % 512;
        memset(buffer, 0, padding);
        context->m_Failed = !ok || fwrite(buffer, 1, padding, context->m_File) != padding;
    }

    static void PrepareArtifact(void* data)
    {
        Artifact* artifact = (Artifact*)data;
        bool failed = false;
        if (dmSys::IsDir(artifact->m_Source) == dmSys::RESULT_OK)
        {
            dmSnPrintf(artifact->m_Path, sizeof(artifact->m_Path), "%s.tar", artifact->m_Source);
            FILE* archive = fopen(artifact->m_Path, "wb");
            if (!archive) failed = true;
            else
            {
                const char* base = strrchr(artifact->m_Source, '/');
                ArchiveContext context = {archive, base ? (uint32_t)(base - artifact->m_Source + 1) : 0, false};
                failed = dmSys::IterateTree(artifact->m_Source, true, true, &context, ArchiveEntry) != dmSys::RESULT_OK || context.m_Failed;
                char end[1024] = {};
                failed = fwrite(end, 1, sizeof(end), archive) != sizeof(end) || failed;
                failed = fclose(archive) != 0 || failed;
            }
        }
        mbedtls_md_context_t hash;
        mbedtls_md_init(&hash);
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        failed = failed || !info || mbedtls_md_setup(&hash, info, 0) != 0 || mbedtls_md_starts(&hash) != 0;
        FILE* file = failed ? 0 : fopen(artifact->m_Path, "rb");
        uint64_t size = 0;
        if (!file) failed = true;
        else
        {
            uint8_t buffer[65536];
            size_t read;
            while ((read = fread(buffer, 1, sizeof(buffer), file)) != 0)
            {
                size += read;
                if (mbedtls_md_update(&hash, buffer, read) != 0) { failed = true; break; }
            }
            failed = failed || ferror(file);
            fclose(file);
        }
        uint8_t digest[32];
        if (!failed && mbedtls_md_finish(&hash, digest) != 0) failed = true;
        mbedtls_md_free(&hash);
        dmMutex::ScopedLock lock(artifact->m_Mutex);
        if (!failed)
            for (uint32_t i = 0; i < sizeof(digest); ++i) dmSnPrintf(artifact->m_Sha256 + i * 2, 3, "%02x", digest[i]);
        artifact->m_Size = size;
        artifact->m_Failed = failed;
        artifact->m_Ready = true;
    }

    void AppendArtifactJson(StringBuffer* out, const char* path)
    {
        if (IsEmpty(path)) { StringBufferAppend(out, "null"); return; }
        Artifact* artifact = 0;
        for (uint32_t i = 0; i < g_Artifacts.Size(); ++i)
            if (StringsEqual(g_Artifacts[i]->m_Source, path)) { artifact = g_Artifacts[i]; break; }
        if (!artifact)
        {
            bool retained = false;
            for (uint32_t i = 0; i < g_CapturePaths.Size(); ++i)
                retained = retained || StringsEqual(g_CapturePaths[i], path);
            // A receipt can outlive an explicitly deleted artifact. Never recreate it.
            if (!retained) { StringBufferAppend(out, "{\"state\":\"deleted\"}"); return; }
            if (g_Artifacts.Size() == 128) { StringBufferAppend(out, "{\"state\":\"failed\"}"); return; }
            artifact = new Artifact;
            memset(artifact, 0, sizeof(*artifact));
            dmStrlCpy(artifact->m_Source, path, sizeof(artifact->m_Source));
            dmStrlCpy(artifact->m_Path, path, sizeof(artifact->m_Path));
            dmSnPrintf(artifact->m_Id, sizeof(artifact->m_Id), "%s-a%llu", g_AutomationBridge.m_EngineInstanceId, (unsigned long long)++g_FileSequence);
            artifact->m_Mutex = dmMutex::New();
            if (g_Artifacts.Full()) g_Artifacts.OffsetCapacity(16);
            g_Artifacts.Push(artifact);
            artifact->m_Worker = dmThread::New(PrepareArtifact, 512 * 1024, artifact, "automation-artifact");
            if (!artifact->m_Worker) { artifact->m_Failed = true; artifact->m_Ready = true; }
        }
        dmMutex::ScopedLock lock(artifact->m_Mutex);
        StringBufferAppend(out, "{\"id\":"); AppendJsonString(out, artifact->m_Id);
        StringBufferAppend(out, ",\"state\":"); AppendJsonString(out, !artifact->m_Ready ? "pending" : (artifact->m_Failed ? "failed" : "complete"));
        StringBufferAppend(out, ",\"size\":"); AppendNumber(out, (double)artifact->m_Size);
        StringBufferAppend(out, ",\"sha256\":"); AppendJsonString(out, artifact->m_Sha256);
        StringBufferAppend(out, ",\"archive\":"); StringBufferAppend(out, dmSys::IsDir(path) == dmSys::RESULT_OK ? "true" : "false");
        StringBufferAppend(out, "}");
    }

    static void ArtifactError(dmWebServer::Request* request, int status, const char* code)
    {
        char body[256];
        int size = dmSnPrintf(body, sizeof(body), "{\"ok\":false,\"error\":{\"status\":%d,\"code\":\"%s\",\"message\":\"%s\"}}", status, code, code);
        dmWebServer::SetStatusCode(request, status);
        dmWebServer::SendAttribute(request, "Content-Type", "application/json");
        dmWebServer::Send(request, body, size);
    }

    static void DeleteArtifact(Artifact* artifact)
    {
        if (artifact->m_Worker) dmThread::Join(artifact->m_Worker);
        remove(artifact->m_Path);
        if (dmSys::IsDir(artifact->m_Source) == dmSys::RESULT_OK) dmSys::RmTree(artifact->m_Source);
        dmMutex::Delete(artifact->m_Mutex);
        for (uint32_t i = 0; i < g_CapturePaths.Size(); ++i)
        {
            if (!StringsEqual(g_CapturePaths[i], artifact->m_Source)) continue;
            free(g_CapturePaths[i]);
            g_CapturePaths.EraseSwap(i);
            break;
        }
        delete artifact;
    }

    void HandleArtifact(dmWebServer::Request* request, const char* id)
    {
        Artifact* artifact = 0;
        uint32_t index = 0;
        for (; index < g_Artifacts.Size(); ++index)
            if (StringsEqual(g_Artifacts[index]->m_Id, id)) { artifact = g_Artifacts[index]; break; }
        if (!artifact) { ArtifactError(request, 410, "stale_artifact"); return; }
        bool ready, failed;
        {
            dmMutex::ScopedLock lock(artifact->m_Mutex);
            ready = artifact->m_Ready;
            failed = artifact->m_Failed;
        }
        if (!ready) { ArtifactError(request, 409, "artifact_pending"); return; }
        if (StringsEqual(request->m_Method, "DELETE"))
        {
            DeleteArtifact(artifact);
            g_Artifacts.EraseSwap(index);
            dmWebServer::SetStatusCode(request, 204);
            return;
        }
        if (!StringsEqual(request->m_Method, "GET")) { ArtifactError(request, 405, "method_not_allowed"); return; }
        if (failed) { ArtifactError(request, 500, "artifact_failed"); return; }
        const char* range = dmWebServer::GetHeader(request, "Range");
        unsigned long long start = 0, end = 0;
        int consumed = 0;
        if (!range || sscanf(range, "bytes=%llu-%llu%n", &start, &end, &consumed) != 2 || range[consumed] ||
            end < start || start >= artifact->m_Size || end >= artifact->m_Size || end - start >= 1024 * 1024)
        { ArtifactError(request, 416, "invalid_range"); return; }
        FILE* file = fopen(artifact->m_Path, "rb");
#if defined(_WIN32)
        bool seek_ok = file && _fseeki64(file, start, SEEK_SET) == 0;
#else
        bool seek_ok = file && fseeko(file, (off_t)start, SEEK_SET) == 0;
#endif
        if (!seek_ok) { if (file) fclose(file); ArtifactError(request, 500, "artifact_unavailable"); return; }
        char content_range[100];
        dmSnPrintf(content_range, sizeof(content_range), "bytes %llu-%llu/%llu", start, end, (unsigned long long)artifact->m_Size);
        dmWebServer::SetStatusCode(request, 206);
        dmWebServer::SendAttribute(request, "Content-Type", "application/octet-stream");
        dmWebServer::SendAttribute(request, "Content-Range", content_range);
        dmWebServer::SendAttribute(request, "Accept-Ranges", "bytes");
        dmWebServer::SendAttribute(request, "ETag", artifact->m_Sha256);
        uint8_t buffer[65536];
        uint64_t remaining = end - start + 1;
        while (remaining)
        {
            uint32_t count = remaining < sizeof(buffer) ? (uint32_t)remaining : sizeof(buffer);
            size_t read = fread(buffer, 1, count, file);
            if (!read) break;
            if (dmWebServer::Send(request, buffer, read) != dmWebServer::RESULT_OK) break;
            remaining -= read;
        }
        fclose(file);
    }

    void FinalizeArtifacts()
    {
        for (uint32_t i = 0; i < g_Artifacts.Size(); ++i) DeleteArtifact(g_Artifacts[i]);
        g_Artifacts.SetSize(0);
        for (uint32_t i = 0; i < g_CapturePaths.Size(); ++i)
        {
            if (dmSys::IsDir(g_CapturePaths[i]) == dmSys::RESULT_OK) dmSys::RmTree(g_CapturePaths[i]);
            else remove(g_CapturePaths[i]);
            free(g_CapturePaths[i]);
        }
        g_CapturePaths.SetSize(0);
    }
}
