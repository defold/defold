#ifndef DM_PPRINT_H
#define DM_PPRINT_H

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
