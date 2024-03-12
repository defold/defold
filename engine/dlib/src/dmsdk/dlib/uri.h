// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_URI_H
#define DMSDK_URI_H

#include <stdint.h>

/*# SDK URI API documentation
 *
 * URI functions.
 *
 * @document
 * @name URI
 * @namespace dmURI
 * @path engine/dlib/src/dmsdk/dlib/uri.h
 */

namespace dmURI
{
    /*#
     * URI parsing result
     * @name dmURI::Result
     */
    enum Result
    {
        RESULT_OK,                          //!< RESULT_OK
        RESULT_TOO_SMALL_BUFFER,            //!< RESULT_TOO_SMALL_BUFFER
    };

    const uint32_t MAX_SCHEME_LEN = 8;      //!< MAX_SCHEME_LEN
    const uint32_t MAX_LOCATION_LEN = 64;   //!< MAX_LOCATION_LEN
    const uint32_t MAX_PATH_LEN = 2048;     //!< MAX_PATH_LEN

    /// Maximum length of an URI: scheme :// location / path
    const uint32_t MAX_URI_LEN = MAX_SCHEME_LEN + 3 + MAX_LOCATION_LEN + 1 + MAX_PATH_LEN;


    /*#
     * URI parsing result parts
     * @struct
     * @name dmURI::Parts
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

    /*#
     * Parse URI and split in three parts. (scheme, location, path)
     * @name dmURI::Parse
     * @note This is a simplified URI parser and does not conform to rfc2396.
     *       Missing features are: parameters, query, fragment part of URI and support for escaped sequences
     * @note For http m_Port is set to 80 if not specified in uri.
     * @param uri [type:const char*] URI to parse
     * @param parts [type:dmURI::Parts] Result
     * @return RESULT_OK on success
     */
    Result Parse(const char* uri, Parts* parts);

    /*#
     * Performs URL encoding of the supplied buffer
     * @name dmURI::Encode
     * @param src [type:const char*] string to encode
     * @param dst [type:char*] the destination buffer
     * @param dst_size [type:uint32_t] size of the provided out buffer
     * @param bytes_written[out] [type:uint32_t] number of bytes written
     * @note If dst=0 the bytes_written will return the number of required bytes (including null character)
     */
    Result Encode(const char* src, char* dst, uint32_t dst_size, uint32_t* bytes_written);

    /*#
     * Decodes an URL encoded buffer
     * @name dmURI::Decode
     * @note The output will never be larger than the input.
     * @param src [type:const char*] Input
     * @param dst [type:char*] Decoded output
     */
    void Decode(const char* src, char* dst);
}

#endif // DMSDK_URI_H
