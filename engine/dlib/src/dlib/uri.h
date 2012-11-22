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

        /// Location part, eg foo.com:80
        char m_Location[64];

        /// Hostname part of location, eg foo.com
        char m_Hostname[64];

        /// Port part of location, eg 80. -1 if not present
        int m_Port;

        /// Path part, eg index.html
        char m_Path[512];
    };

    /**
     * Parse URI and split in three parts. (scheme, location, path)
     * @note This is a simplified URI parser and does not conform to rfc2396.
     *       Missing features are: parameters, query, fragment part of URI and support for escaped sequences
     * @note For http m_Port is set to 80 if not specified in uri.
     * @param uri URI to parse
     * @param parts Result
     * @return RESULT_OK on success
     */
    Result Parse(const char* uri, Parts* parts);

    /**
     * Encodes the provided URI into escaped format.
     * @note This function currently only replaces spaces with '+'
     * @param uri URI to encode
     * @param out Decoded URI
     * @param out_size size of the provided out buffer
     */
    void Encode(const char* uri, char* out, uint32_t out_size);

    /**
     * Decodes the provided URI into escaped format.
     * @note This function currently only replaces '+' with ' '
     * @param uri URI to decode
     * @param out Decoded URI
     * @param out_size size of the provided out buffer
     */
    void Decode(const char* uri, char* out, uint32_t out_size);
}

#endif // DM_URI_H
