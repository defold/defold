#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include "hash.h"
#include "log.h"

#include "configfile.h"
#include "array.h"

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

        FILE*           m_File;
        const char*     m_Filename;
        jmp_buf         m_JmpBuf;
        char            m_CategoryBuffer[CATEGORY_MAX_SIZE];
        uint32_t        m_Line;

        dmArray<Entry>  m_Entries;
        dmArray<char>   m_StringBuffer;
    };

    void ParseError(Context* context, Result result)
    {
        dmLogWarning("Config file parse error in file '%s' at line: %d", context->m_Filename, context->m_Line);
        longjmp(context->m_JmpBuf, (int) result);
    }

    static bool IsBlank(char c)
    {
        return c == ' ' || c == '\t';
    }

    static int GetChar(Context* context)
    {
        int c = fgetc(context->m_File);
        // Ignore '\r' in order to accept DOS-lineendings
        while (c == '\r')
        {
            c = fgetc(context->m_File);
        }
        return c;
    }

    static int SafeGetChar(Context* context)
    {
        int c = fgetc(context->m_File);
        // Ignore '\r' in order to accept DOS-lineendings
        while (c == '\r')
        {
            c = fgetc(context->m_File);
        }
        if (c == EOF)
            ParseError(context, RESULT_UNEXPECTED_EOF);
        return c;
    }

    static int PeekChar(Context* context)
    {
        int c = fgetc(context->m_File);
        ungetc(c, context->m_File);
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

        ungetc(c, context->m_File);
    }

    static void EatBlank(Context* context)
    {
        int c;
        do
        {
            c = GetChar(context);
        } while (IsBlank(c));

        ungetc(c, context->m_File);
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

    static void AddEntry(Context* context, const char* key, const char* value)
    {
        uint64_t key_hash = dmHashString64(key);
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
        ungetc(c, context->m_File);
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
        ungetc(c, context->m_File);
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
            if (feof(context->m_File))
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

    Result Load(const char* file_name, HConfig* config)
    {
        assert(file_name);
        assert(config);

        *config = 0;

        FILE* f = fopen(file_name, "rb");
        if (!f)
        {
            return RESULT_FILE_NOT_FOUND;
        }

        Context context;
        context.m_File = f;
        context.m_Filename = file_name;
        context.m_Entries.SetCapacity(128);
        context.m_StringBuffer.SetCapacity(0400);
        context.m_Line = 1;

        int ret = setjmp(context.m_JmpBuf);
        if (ret != RESULT_OK)
        {
            fclose(f);
            return (Result) ret;
        }
        else
        {
            Parse(&context);

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
        fclose(f);

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
}

