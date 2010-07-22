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

namespace dmConfigFile
{

    const int32_t CATEGORY_MAX_SIZE = 512;

    struct Entry
    {
        uint64_t m_Key;
        uint32_t m_ValueIndex;
        Entry(uint64_t k, uint32_t i) :
            m_Key(k), m_ValueIndex(i)
        {
        }
    };

    struct Config
    {
        dmArray<Entry> m_Entries;
        dmArray<char>  m_StringBuffer;
    };

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

        dmArray<Entry>  m_Entries;
        dmArray<char>   m_StringBuffer;
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
        do
        {
            c = GetChar(context);
            if (c == '\n')
                context->m_Line++;
        } while (isspace(c));

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

    static bool ContainsKey(const dmArray<Entry>& entries, uint64_t key_hash)
    {
        for (uint32_t i = 0; i < entries.Size(); ++i)
        {
            const Entry*e = &entries[i];
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
        context->m_Entries.Push(Entry(key_hash, i));
    }

    static void AddEntryWithHashedKey(Context* context, uint64_t key_hash, const char* value)
    {
        uint32_t i = AddString(context, value);
        if (context->m_Entries.Remaining() == 0)
        {
            context->m_Entries.OffsetCapacity(32);
        }
        context->m_Entries.Push(Entry(key_hash, i));
    }

    static void ParseLiteral(Context* context, char* buf, int buf_len)
    {
        int c = GetChar(context);
        int i = 0;
        while (!isspace(c))
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

    static void ParseEntry(Context* context)
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

        char value_buf[1024];

        ParseLiteral(context, value_buf, sizeof(value_buf));

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
        while (true)
        {
            EatSpace(context);
            if (BufferEof(context))
                return;

            if (PeekChar(context) == '[')
            {
                ParseSection(context);
            }
            else
            {
                ParseEntry(context);
            }
        }
    }

    void HttpHeader(dmHttpClient::HClient client, void* user_data, int status_code, const char* key, const char* value)
    {
        Context* context = (Context*) user_data;
        if (strcmp("Content-Length", key) == 0)
        {
            context->m_BufferSize = (int) strtol(value, 0, 10);
            context->m_Buffer = new char[context->m_BufferSize];
        }
    }

    void HttpContent(dmHttpClient::HClient client, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
    {
        Context* context = (Context*) user_data;
        if (status_code != 200 || context->m_BufferSize == -1)
            return;

        assert(context->m_BufferPos + (int) content_data_size <= context->m_BufferSize);
        memcpy(context->m_Buffer + context->m_BufferPos, content_data, content_data_size);
        context->m_BufferPos += (int) content_data_size;
    }

    Result Load(const char* file_name, int argc, const char** argv, HConfig* config)
    {
        assert(file_name);
        assert(config);

        *config = 0;

        dmURI::Parts uri_parts;
        dmURI::Result r = dmURI::Parse(file_name, &uri_parts);
        if (r != dmURI::RESULT_OK)
            return RESULT_INVALID_URI;

        Context context;

        if (strcmp(uri_parts.m_Scheme, "http") == 0)
        {
            dmHttpClient::NewParams params;
            params.m_Userdata = &context;
            params.m_HttpContent = &HttpContent;
            params.m_HttpHeader = &HttpHeader;
            dmHttpClient::HClient client = dmHttpClient::New(&params, "localhost", 7000);

            context.m_BufferSize = -1;
            dmHttpClient::Result http_result = dmHttpClient::Get(client, uri_parts.m_Path);
            if (http_result != dmHttpClient::RESULT_OK || context.m_BufferSize == -1)
            {
                dmHttpClient::Delete(client);
                return RESULT_FILE_NOT_FOUND;
            }
            dmHttpClient::Delete(client);
            context.m_BufferSize = context.m_BufferPos;
            context.m_BufferPos = 0;
        }
        else if (strcmp(uri_parts.m_Scheme, "file") == 0)
        {
            FILE* f = fopen(file_name, "rb");
            if (!f)
            {
                return RESULT_FILE_NOT_FOUND;
            }
            fseek(f, 0, SEEK_END);
            long file_size = ftell(f);
            fseek(f, 0, SEEK_SET);

            context.m_Buffer = new char[file_size];
            if (fread(context.m_Buffer, 1, file_size, f) != (size_t) file_size)
            {
                fclose(f);
                delete[] context.m_Buffer;
                return RESULT_UNEXPECTED_EOF;
            }
            fclose(f);
            context.m_BufferSize = (int) file_size;
        }

        context.m_Argc = argc;
        context.m_Argv = argv;
        context.m_URI = file_name;
        context.m_Entries.SetCapacity(128);
        context.m_StringBuffer.SetCapacity(0400);
        context.m_Line = 1;

        int ret = setjmp(context.m_JmpBuf);
        if (ret != RESULT_OK)
        {
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

            Config* c = new Config();

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

        return RESULT_OK;
    }

    void Delete(HConfig config)
    {
        delete config;
    }

    const char* GetString(HConfig config, const char* key, const char* default_value)
    {
        uint64_t key_hash = dmHashString64(key);
        for (uint32_t i = 0; i < config->m_Entries.Size(); ++i)
        {
            const Entry*e = &config->m_Entries[i];
            if (e->m_Key == key_hash)
            {
                return &config->m_StringBuffer[e->m_ValueIndex];
            }
        }

        return default_value;
    }

    int32_t GetInt(HConfig config, const char* key, int32_t default_value)
    {
        const char* tmp = GetString(config, key, 0);
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

    float GetFloat(HConfig config, const char* key, float default_value)
    {
        const char* tmp = GetString(config, key, 0);
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

}

