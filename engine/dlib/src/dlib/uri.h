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

    const uint32_t MAX_SCHEME_LEN = 8;
    const uint32_t MAX_LOCATION_LEN = 64;
    const uint32_t MAX_PATH_LEN = 2048;
    // Maximum length of an URI
    // scheme :// location / path
    const uint32_t MAX_URI_LEN = MAX_SCHEME_LEN + 3 + MAX_LOCATION_LEN + 1 + MAX_PATH_LEN;


    /**
     * URI parsing result parts
     */
    struct Parts
    {
        /// Scheme parts, eg http
        char m_Scheme[MAX_SCHEME_LEN];

        /// Location part, eg foo.com:80
        char m_Location[MAX_LOCATION_LEN];

        /// Hostname part of location, eg foo.com
        char m_Hostname[MAX_LOCATION_LEN];

        /// Port part of location, eg 80. -1 if not present
        int m_Port;

        /// Path part, eg index.html
        // Increased from 512 to 2048 (DEF-3410). 2048 seems like a reasonable
        // number based on the following SO answer: https://stackoverflow.com/a/417184/1266551
        char m_Path[MAX_PATH_LEN];
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
     * Performs URL encoding of the supplied buffer
     * @param src String to encode
     * @param dst Encoded string
     * @param dst_size size of the provided out buffer
     */
    void Encode(const char* src, char* dst, uint32_t dst_size);

    /**
     * Undoes URL decoding on a buffer.
     * @note The output will never be larger than the input.
     * @param src Input
     * @param dst Decoded output
     */
    void Decode(const char* src, char* dst);
}

#endif // DM_URI_H
