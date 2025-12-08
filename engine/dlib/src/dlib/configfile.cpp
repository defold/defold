// Copyright 2020-2025 The Defold Foundation
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
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "hash.h"
#include "log.h"
#include "http_client.h"
#include "uri.h"

#include "configfile.h"
#include "array.h"
#include "dstrings.h"
#include "math.h"
#include "sys.h"
#include "static_assert.h"

struct ConfigFileEntry
{
    uint64_t m_Key;
    uint32_t m_ValueIndex;
    ConfigFileEntry(uint64_t k, uint32_t i) :
        m_Key(k), m_ValueIndex(i)
    {
    }
};

struct ConfigFile
{
    dmArray<ConfigFileEntry>  m_Entries;
    dmArray<char>               m_StringBuffer;
};

struct ConfigFilePluginDesc
{
    const char* m_Name;
    FConfigFileCreate m_Create;
    FConfigFileDestroy m_Destroy;
    FConfigFileGetString m_GetString;
    FConfigFileGetInt m_GetInt;
    FConfigFileGetFloat m_GetFloat;

    const ConfigFilePluginDesc* m_Next;
};

ConfigFilePluginDesc* g_FirstExtension = 0;

static const ConfigFilePluginDesc* GetFirstExtension()
{
    return g_FirstExtension;
}

void ConfigFileRegisterExtension(void* _desc,
    uint32_t desc_size,
    const char* name,
    FConfigFileCreate create,
    FConfigFileDestroy destroy,
    FConfigFileGetString get_string,
    FConfigFileGetInt get_int,
    FConfigFileGetFloat get_float)
{
    struct ConfigFilePluginDesc* desc = (struct ConfigFilePluginDesc*)_desc;
    DM_STATIC_ASSERT(ConfigFileExtensionDescBufferSize >= sizeof(struct ConfigFilePluginDesc), Invalid_Struct_Size);
    desc->m_Name = name;
    desc->m_Create = create;
    desc->m_Destroy = destroy;
    desc->m_GetString = get_string;
    desc->m_GetInt = get_int;
    desc->m_GetFloat = get_float;

    desc->m_Next = g_FirstExtension;
    g_FirstExtension = desc;
}


static const char* GetBaseString(HConfigFile config, const char* key, const char* default_value)
{
    uint64_t key_hash = dmHashString64(key);
    for (uint32_t i = 0; i < config->m_Entries.Size(); ++i)
    {
        const ConfigFileEntry*e = &config->m_Entries[i];
        if (e->m_Key == key_hash)
        {
            return &config->m_StringBuffer[e->m_ValueIndex];
        }
    }

    return default_value;
}

static bool GetStringOverride(HConfigFile config, const char* key, const char* default_value, const char** out_value)
{
    const ConfigFilePluginDesc* plugin = GetFirstExtension();
    while (plugin != 0)
    {
        if (plugin->m_GetString && plugin->m_GetString(config, key, default_value, out_value))
            return true;
        plugin = plugin->m_Next;
    }
    return false;
}

static int32_t GetBaseInt(HConfigFile config, const char* key, int32_t default_value)
{
    const char* tmp = GetBaseString(config, key, 0);
    if (tmp == 0)
        return default_value;
    int l = strlen(tmp);
    char* end = 0;
    int32_t ret = strtol(tmp, &end, 10);
    if (end != (tmp + l) || end == tmp)
    {
        dmLogWarning("Unable to convert '%s' to int", tmp);
        return default_value;
    }
    return ret;
}

static bool GetIntOverride(HConfigFile config, const char* key, int32_t default_value, int32_t* out_value)
{
    const ConfigFilePluginDesc* plugin = GetFirstExtension();
    while (plugin != 0)
    {
        if (plugin->m_GetInt && plugin->m_GetInt(config, key, default_value, out_value))
            return true;
        plugin = plugin->m_Next;
    }
    return false;
}

static float GetBaseFloat(HConfigFile config, const char* key, float default_value)
{
    const char* tmp = GetBaseString(config, key, 0);
    if (tmp == 0)
        return default_value;
    int l = strlen(tmp);
    char* end = 0;
    float ret = (float) strtod(tmp, &end);
    if (end != (tmp + l) || end == tmp)
    {
        dmLogWarning("Unable to convert '%s' to float", tmp);
        return default_value;
    }
    return ret;
}

static bool GetFloatOverride(HConfigFile config, const char* key, float default_value, float* out_value)
{
    const ConfigFilePluginDesc* plugin = GetFirstExtension();
    while (plugin != 0)
    {
        if (plugin->m_GetFloat && plugin->m_GetFloat(config, key, default_value, out_value))
            return true;
        plugin = plugin->m_Next;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// C api

const char* ConfigFileGetString(HConfigFile config, const char* key, const char* default_value)
{
    const char* base_value = GetBaseString(config, key, default_value);
    const char* out_value = 0;
    if (GetStringOverride(config, key, base_value, &out_value))
        return out_value;
    return base_value;
}

int32_t ConfigFileGetInt(HConfigFile config, const char* key, int32_t default_value)
{
    int32_t base_value = GetBaseInt(config, key, default_value);
    int32_t out_value = 0;
    if (GetIntOverride(config, key, base_value, &out_value))
        return out_value;
    return base_value;
}

float ConfigFileGetFloat(HConfigFile config, const char* key, float default_value)
{
    float base_value = GetBaseFloat(config, key, default_value);
    float out_value = 0;
    if (GetFloatOverride(config, key, base_value, &out_value))
        return out_value;
    return base_value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace dmConfigFile
{
    const int32_t CATEGORY_MAX_SIZE = 512;

    struct Context
    {
        Context()
        {
            memset(this, 0, sizeof(*this));
        }
        int             m_Argc;
        const char**    m_Argv;
        char*           m_Buffer;
        int             m_BufferPos;
        int             m_BufferSize;
        const char*     m_URI;
        jmp_buf         m_JmpBuf;
        char            m_CategoryBuffer[CATEGORY_MAX_SIZE];
        uint32_t        m_Line;

        dmArray<ConfigFileEntry>  m_Entries;
        dmArray<char>               m_StringBuffer;
    };

    void ParseError(Context* context, Result result)
    {
        dmLogWarning("Config file parse error in file '%s' at line: %d", context->m_URI, context->m_Line);
        longjmp(context->m_JmpBuf, (int) result);
    }

    static bool IsBlank(char c)
    {
        return c == ' ' || c == '\t';
    }

    static int BufferGetChar(Context* context)
    {
        if (context->m_BufferPos >= context->m_BufferSize)
            return 0;
        return context->m_Buffer[context->m_BufferPos++];
    }

    static bool BufferEof(Context* context)
    {
        return context->m_BufferPos >= context->m_BufferSize;
    }

    static void BufferUngetChar(char c, Context* context)
    {
        if (context->m_BufferPos <= 0 || c == 0)
        {
            return;
        }
        context->m_Buffer[--context->m_BufferPos] = c;
    }

    static int GetChar(Context* context)
    {
        int c = BufferGetChar(context);
        // Ignore '\r' in order to accept DOS-lineendings
        while (c == '\r')
        {
            c = BufferGetChar(context);
        }
        return c;
    }

    static int SafeGetChar(Context* context)
    {
        int c = BufferGetChar(context);
        // Ignore '\r' in order to accept DOS-lineendings
        while (c == '\r')
        {
            c = BufferGetChar(context);
        }
        if (c == EOF)
            ParseError(context, RESULT_UNEXPECTED_EOF);
        return c;
    }

    static int PeekChar(Context* context)
    {
        int c = BufferGetChar(context);
        BufferUngetChar(c, context);
        return c;
    }

    static void Expect(Context* context, char ch)
    {
        int c = SafeGetChar(context);
        if (c != ch)
        {
            ParseError(context, RESULT_SYNTAX_ERROR);
        }
    }

    static void EatSpace(Context* context)
    {
        int c;
        bool comment = false;
        bool line_start = true;
        do
        {
            c = GetChar(context);
            if (line_start && (c == '#' || c == ';'))
            {
                comment = true;
            }
            if (c == '\n')
            {
                context->m_Line++;
                comment = false;
                line_start = true;
            }
            else
            {
                line_start = false;
            }
        } while (comment || isspace(c));

        BufferUngetChar(c, context);
    }

    static void EatBlank(Context* context)
    {
        int c;
        do
        {
            c = GetChar(context);
        } while (IsBlank(c));

        BufferUngetChar(c, context);
    }

    static uint32_t AddString(Context* context, const char* string)
    {
        uint32_t l = strlen(string) + 1;
        if (context->m_StringBuffer.Remaining() < l)
        {
            uint32_t off = 0x400;
            if (l > off)
                off = l;
            context->m_StringBuffer.OffsetCapacity(off);
        }
        uint32_t s = context->m_StringBuffer.Size();
        context->m_StringBuffer.SetSize(s + l);
        memcpy(&context->m_StringBuffer[s], string, l);
        return s;
    }

    static bool ContainsKey(const dmArray<ConfigFileEntry>& entries, uint64_t key_hash)
    {
        for (uint32_t i = 0; i < entries.Size(); ++i)
        {
            const ConfigFileEntry*e = &entries[i];
            if (e->m_Key == key_hash)
            {
                return true;
            }
        }

        return false;
    }

    static void AddEntry(Context* context, const char* key, const char* value)
    {
        uint64_t key_hash = dmHashString64(key);
        if (ContainsKey(context->m_Entries, key_hash))
        {
            dmLogWarning("Config value '%s' specified twice. First value will be used.", key);
            return;
        }

        uint32_t i = AddString(context, value);
        if (context->m_Entries.Remaining() == 0)
        {
            context->m_Entries.OffsetCapacity(32);
        }
        context->m_Entries.Push(ConfigFileEntry(key_hash, i));
    }

    static void AddEntryWithHashedKey(Context* context, uint64_t key_hash, const char* value)
    {
        uint32_t i = AddString(context, value);
        if (context->m_Entries.Remaining() == 0)
        {
            context->m_Entries.OffsetCapacity(32);
        }
        context->m_Entries.Push(ConfigFileEntry(key_hash, i));
    }

    static void ParseLiteral(Context* context, char* buf, int buf_len)
    {
        int c = GetChar(context);
        int i = 0;
        while (c != '\n' && c != '\r')
        {
            buf[i] = c;
            if (i >= (int) buf_len - 1)
                ParseError(context, RESULT_LITERAL_TOO_LONG);

            c = GetChar(context);
            ++i;
        }
        BufferUngetChar(c, context);
        buf[i] = '\0';
    }

    static void ParseKey(Context* context, char* buf, int buf_len)
    {
        int c = GetChar(context);
        int i = 0;
        while (isalnum(c) || c == '_')
        {
            buf[i] = c;
            if (i >= (int) buf_len - 1)
                ParseError(context, RESULT_LITERAL_TOO_LONG);

            c = GetChar(context);
            ++i;
        }
        BufferUngetChar(c, context);
        buf[i] = '\0';
    }

    static void ParseEntry(Context* context, char* value_buf, int value_len)
    {
        char key_buf[CATEGORY_MAX_SIZE + 512];
        int category_len = strlen(context->m_CategoryBuffer);
        memcpy(key_buf, context->m_CategoryBuffer, category_len);
        key_buf[category_len] = '.'; // We have enough space here.
        category_len++;

        key_buf[category_len] = '\0'; // TODO: TMP REMOVE ME

        ParseKey(context, key_buf + category_len, sizeof(key_buf) - category_len);
        EatBlank(context);
        Expect(context, '=');
        EatBlank(context);

        ParseLiteral(context, value_buf, value_len);

        for (int i = 0; i < context->m_Argc; ++i)
        {
            const char* arg = context->m_Argv[i];
            if (strncmp("--config=", arg, sizeof("--config=")-1) == 0)
            {
                const char* eq = strchr(arg, '=');
                const char* eq2 = strchr(eq+1, '=');
                if (!eq2)
                {
                    dmLogWarning("Invalid config option: %s", arg);
                    continue;
                }

                if (strncmp(key_buf, eq + 1, eq2 - (eq+1)) == 0)
                {
                    AddEntry(context, key_buf, eq2 + 1);
                    return;
                }
            }
        }

        AddEntry(context, key_buf, value_buf);
    }

    void ParseSection(Context* context)
    {
        Expect(context, '[');
        ParseKey(context, context->m_CategoryBuffer, CATEGORY_MAX_SIZE);
        Expect(context, ']');
    }

    static void Parse(Context* context)
    {
        int value_len = context->m_BufferSize;
        char* value_buf = new char[value_len];

        while (true)
        {
            EatSpace(context);
            if (BufferEof(context))
                break;

            if (PeekChar(context) == '[')
            {
                ParseSection(context);
            }
            else
            {
                ParseEntry(context, value_buf, value_len);
            }
        }
        delete[] value_buf;
    }

    struct HttpContext
    {
        HttpContext()
        {
        }
        dmArray<char> m_Buffer;
    };

    static void HttpHeader(dmHttpClient::HResponse response, void* user_data, int status_code, const char* key, const char* value)
    {
        (void) response;
        (void) user_data;
        (void) status_code;
        (void) key;
        (void) value;
    }

    static void HttpContent(dmHttpClient::HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size, int32_t content_length,
                            uint32_t range_start, uint32_t range_end, uint32_t document_size,
                            const char* method)
    {
        (void) method; // unused
        HttpContext* context = (HttpContext*) user_data;
        if (status_code != 200)
            return;

        if (!content_data && !content_data_size)
        {
            context->m_Buffer.SetSize(0);
            return;
        }

        dmArray<char>& buffer = context->m_Buffer;
        if (buffer.Remaining() < content_data_size)
        {
            uint32_t new_capacity = buffer.Capacity() + dmMath::Max(4U * 1024U, content_data_size);
            context->m_Buffer.SetCapacity(new_capacity);
        }

        assert(content_data);
        buffer.PushArray((const char*) content_data, content_data_size);
    }

    static Result LoadFromBufferInternal(const char* url, const char* buffer, uint32_t buffer_size, int argc, const char** argv, HConfig* config)
    {
        Context context;
        // NOTE: We allocate n + 1 bytes in order to ensure that
        // the file has a terminating newline
        context.m_Buffer = new char[buffer_size + 1];
        memcpy(context.m_Buffer, buffer, buffer_size);
        // Ensure terminating newline.
        context.m_Buffer[buffer_size] = '\n';
        context.m_BufferSize = buffer_size + 1;
        context.m_BufferPos = 0;

        context.m_Argc = argc;
        context.m_Argv = argv;
        context.m_URI = url;
        context.m_Entries.SetCapacity(128);
        context.m_StringBuffer.SetCapacity(0400);
        context.m_Line = 1;

        int ret = setjmp(context.m_JmpBuf);
        if (ret != RESULT_OK)
        {
            delete[] context.m_Buffer;
            return (Result) ret;
        }
        else
        {
            Parse(&context);

            for (int i = 0; i < context.m_Argc; ++i)
            {
                const char* arg = context.m_Argv[i];
                if (strncmp("--config=", arg, sizeof("--config=")-1) == 0)
                {
                    const char* eq = strchr(arg, '=');
                    const char* eq2 = strchr(eq+1, '=');

                    if (!eq2)
                    {
                        dmLogWarning("Invalid config option: %s", arg);
                        continue;
                    }

                    uint64_t key_hash =  dmHashBuffer64(eq+1, eq2 - (eq+1));
                    if (!ContainsKey(context.m_Entries, key_hash))
                    {
                        AddEntryWithHashedKey(&context, key_hash, eq2 + 1);
                    }
                }
            }

            ConfigFile* c = new ConfigFile();

            if (context.m_Entries.Size() > 0)
            {
                c->m_Entries.SetCapacity(context.m_Entries.Size());
                c->m_Entries.SetSize(context.m_Entries.Size());
                memcpy(&c->m_Entries[0], &context.m_Entries[0], sizeof(context.m_Entries[0]) * context.m_Entries.Size());
            }

            if (context.m_StringBuffer.Size() > 0)
            {
                c->m_StringBuffer.SetCapacity(context.m_StringBuffer.Size());
                c->m_StringBuffer.SetSize(context.m_StringBuffer.Size());
                memcpy(&c->m_StringBuffer[0], &context.m_StringBuffer[0], sizeof(context.m_StringBuffer[0])
                        * context.m_StringBuffer.Size());
            }

            *config = c;
        }

        delete[] context.m_Buffer;
        return RESULT_OK;

    }

    static void PluginsCreate(HConfig config)
    {
        const ConfigFilePluginDesc* plugin = GetFirstExtension();
        while (plugin != 0)
        {
            if (plugin->m_Create)
                plugin->m_Create(config);
            plugin = plugin->m_Next;
        }
    }

    static void PluginsDestroy(HConfig config)
    {
        const ConfigFilePluginDesc* plugin = GetFirstExtension();
        while (plugin != 0)
        {
            if (plugin->m_Destroy)
                plugin->m_Destroy(config);
            plugin = plugin->m_Next;
        }
    }

    static Result LoadFromFileInternal(const char* url, int argc, const char** argv, HConfig* config)
    {
#ifdef __ANDROID__
        // TODO:
        // This is not pretty but we wan't use dmSys::LoadResource()
        // on all platforms as we must be able
        // to load from any path (file:///...)
        // We should probably add a new scheme resource: or similar
        // to solve this situation.
        uint32_t buffer_size = 1024 * 256;
        void* buffer = malloc(buffer_size);

        uint32_t project_size;
        dmSys::Result sysr = dmSys::LoadResource(url, buffer, buffer_size, &project_size);
        if (sysr != dmSys::RESULT_OK) {
            return RESULT_FILE_NOT_FOUND;
        }

        Result r = LoadFromBufferInternal(url, (const char*) buffer, project_size, argc, argv, config);
        free(buffer);
        return r;
#else
        FILE* f = fopen(url, "rb");
        if (!f)
        {
            return RESULT_FILE_NOT_FOUND;
        }
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        char* buffer = new char[file_size];
        if (fread(buffer, 1, file_size, f) != (size_t) file_size)
        {
            fclose(f);
            delete[] buffer;
            return RESULT_UNEXPECTED_EOF;
        }
        fclose(f);

        Result r = LoadFromBufferInternal(url, buffer, file_size, argc, argv, config);
        delete[] buffer;

        return r;
#endif
    }

    static Result LoadFromHttpInternal(const char* url, const dmURI::Parts& uri_parts, int argc, const char** argv, HConfig* config)
    {
        HttpContext context;

        dmHttpClient::NewParams params;
        params.m_Userdata = &context;
        params.m_HttpContent = &HttpContent;
        params.m_HttpHeader = &HttpHeader;
        dmHttpClient::HClient client = dmHttpClient::New(&params, &uri_parts, 0);
        if (client == 0x0)
        {
            return RESULT_FILE_NOT_FOUND;
        }

        dmHttpClient::Result http_result = dmHttpClient::Get(client, uri_parts.m_Path);
        dmHttpClient::Delete(client);

        if (http_result != dmHttpClient::RESULT_OK)
        {
            return RESULT_FILE_NOT_FOUND;
        }

        Result r = LoadFromBufferInternal(url, (const char*) &context.m_Buffer.Front(), context.m_Buffer.Size(), argc, argv, config);
        return r;
    }

    Result LoadFromBuffer(const char* buffer, uint32_t buffer_size, int argc, const char** argv, HConfig* config)
    {
        Result r = LoadFromBufferInternal("<buffer>", buffer, buffer_size, argc, argv, config);
        if (r == RESULT_OK)
        {
            PluginsCreate(*config);
        }
        return r;
    }

    static Result DoLoad(const char* url, int argc, const char** argv, HConfig* config)
    {
        assert(url);
        assert(config);

        *config = 0;

        dmURI::Parts uri_parts;
        dmURI::Result r = dmURI::Parse(url, &uri_parts);
        if (r == dmURI::RESULT_OK)
        {
            if (strcmp(uri_parts.m_Scheme, "http") == 0 || strcmp(uri_parts.m_Scheme, "https") == 0)
            {
                return LoadFromHttpInternal(url, uri_parts, argc, argv, config);
            }
            else if (strcmp(uri_parts.m_Scheme, "file") == 0)
            {
                return LoadFromFileInternal(uri_parts.m_Path, argc, argv, config);
            }
            else if (strcmp(uri_parts.m_Scheme, "data") == 0 || strcmp(uri_parts.m_Scheme, "host") == 0)
            {
                return LoadFromFileInternal(url, argc, argv, config);
            }
            else
            {
#if defined(_WIN32)
                // any drive letter is ok
                if (strlen(uri_parts.m_Scheme) == 1)
                {
                    if (dmSys::Exists(url))
                    {
                        // In order to support windows paths with drive letter we
                        // first test if the url is a valid file
                        return LoadFromFileInternal(url, argc, argv, config);
                    }
                }
#endif
                return RESULT_INVALID_URI;
            }
        }

        if (dmSys::Exists(url))
        {
            // In order to support windows paths with drive letter we
            // first test if the url is a valid file
            return LoadFromFileInternal(url, argc, argv, config);
        }

        return RESULT_INVALID_URI;
    }

    Result Load(const char* url, int argc, const char** argv, HConfig* config)
    {
        Result result = DoLoad(url, argc, argv, config);
        if (result == RESULT_OK)
        {
            PluginsCreate(*config);
        }
        return result;
    }

    void Delete(HConfig config)
    {
        PluginsDestroy(config);
        delete config;
    }

    const char* GetString(HConfig config, const char* key, const char* default_value)
    {
        return ConfigFileGetString(config, key, default_value);
    }

    int32_t GetInt(HConfig config, const char* key, int32_t default_value)
    {
        return ConfigFileGetInt(config, key, default_value);
    }

    float GetFloat(HConfig config, const char* key, float default_value)
    {
        return ConfigFileGetFloat(config, key, default_value);
    }
}
