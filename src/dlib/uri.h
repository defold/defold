#ifndef DM_URI_H
#define DM_URI_H

namespace dmURI
{
    /**
     * URI parsing result
     */
    enum Result
    {
        RESULT_OK,//!< RESULT_OK
    };

    /**
     * URI parsing result parts
     */
    struct Parts
    {
        /// Scheme parts, eg http
        char m_Scheme[8];

        /// Location part, eg foo.com
        char m_Location[64];

        /// Path part, eg index.html
        char m_Path[128];
    };

    /**
     * Parse URI and split in three parts. (scheme, location, path)
     * @note This is a simplified URI parser and does not conform to rfc2396.
     *       Missing features are: parameters, query, fragment part of URI and support for escaped sequences
     * @param uri URI to parse
     * @param parts Result
     * @return RESULT_OK on success
     */
    Result Parse(const char* uri, Parts* parts);
}

#endif // DM_URI_H
