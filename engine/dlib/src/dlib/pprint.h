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

#ifndef DM_PPRINT_H
#define DM_PPRINT_H

#include <stdint.h>

namespace dmPPrint
{
    /**
     * Simple pretty printer
     */
    struct Printer
    {
        /**
         * Constuctor
         * @param buf buffer to write to
         * @param buf_size buffer size
         */
        Printer(char* buf, int buf_size);

        /**
         * printf-style method
         * @param format
         */
        void Printf(const char* format, ...);

        /**
         * Set absolute indentation
         * @param indent indentation
         */
        void Indent(int indent);

        /**
         * Adjuest indentation (increase/decrease)
         * @param indent amount to change indent
         */
        void SetIndent(int indent);

        char*   m_Buffer;
        int     m_BufferSize;
        int     m_Cursor;
        int     m_Indent;
        bool    m_StartLine;
    };
}

#endif // #ifndef DM_PPRINT_H
