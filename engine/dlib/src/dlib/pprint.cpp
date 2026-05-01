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

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "math.h"
#include "pprint.h"

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
        vsnprintf(m_Buffer + m_Cursor, c, format, argp);

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
