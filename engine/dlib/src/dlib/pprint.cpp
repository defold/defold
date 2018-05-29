#include "pprint.h"

#include "math.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace dmPPrint
{
    Printer::Printer(char* buf, int buf_size)
    {
        assert(buf_size > 0);
        m_Buffer = buf;
        m_BufferSize = buf_size;
        m_Cursor = 0;
        m_Indent = 0;
        m_StartLine = true;
        m_Buffer[0] = '\0';
    }

    void Printer::Printf(const char* format, ...)
    {
        va_list argp;
        va_start(argp, format);

        if (m_StartLine) {
            int n = dmMath::Min(m_Indent, m_BufferSize - m_Cursor - 1);
            for (int i = 0; i < n; ++i) {
                m_Buffer[i + m_Cursor] = ' ';
            }
            m_Cursor += n;
            m_StartLine = false;
        }

        int c = m_BufferSize - m_Cursor;
    #if defined(_WIN32)
        _vsnprintf_s(m_Buffer + m_Cursor, c, _TRUNCATE, format, argp);
    #else
        vsnprintf(m_Buffer + m_Cursor, c, format, argp);
    #endif

        m_Buffer[m_BufferSize-1] = '\0';
        m_Cursor = strlen(m_Buffer);

        if (strchr(format, '\n')) {
            m_StartLine = true;
        }
        va_end(argp);
        assert(m_Cursor <= m_BufferSize);
    }

    void Printer::SetIndent(int indent)
    {
        m_Indent = indent;
    }

    void Printer::Indent(int indent)
    {
        m_Indent += indent;
        m_Indent = dmMath::Max(0, m_Indent);
    }

}
