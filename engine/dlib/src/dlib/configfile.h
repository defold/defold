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

#ifndef DM_CONFIGFILE_H
#define DM_CONFIGFILE_H

#include <dmsdk/dlib/configfile_gen.hpp>

namespace dmConfigFile
{
    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK               = 0, //!< RESULT_OK
        RESULT_FILE_NOT_FOUND   = -1,//!< RESULT_FILE_NOT_FOUND
        RESULT_LITERAL_TOO_LONG = -2,//!< RESULT_LITERAL_TOO_LONG
        RESULT_SYNTAX_ERROR     = -3,//!< RESULT_SYNTAX_ERROR
        RESULT_UNEXPECTED_EOF   = -4,//!< RESULT_UNEXPECTED_EOF
        RESULT_INVALID_URI      = -5,//!< RESULT_INVALID_URI
    };

    /**
     * Load config file
     * @note Config values can be specified or overridden by command line arguments with the following syntax:
     *       --config=section.key=value
     * @param url URL to load the config-file from. Local path names are also supported.
     * @param argc Command line argument count (typically from main(...))
     * @param argv Command line arguments (typically from main(...))
     * @param config Config file handle (out)
     * @return RESULT_OK on success
     */
    Result Load(const char* url, int argc, const char** argv, HConfig* config);

    /**
     * Load config from buffer
     * @note Config values can be specified or overridden by command line arguments with the following syntax:
     *       --config=section.key=value
     * @param buffer Buffer to load from
     * @param buffer_size Buffer size
     * @param argc Command line argument count (typically from main(...))
     * @param argv Command line arguments (typically from main(...))
     * @param config Config file handle (out)
     * @return RESULT_OK on success
     */
    Result LoadFromBuffer(const char* buffer, uint32_t buffer_size, int argc, const char** argv, HConfig* config);

    /**
     * Delete config file
     * @param config Config file handle
     */
    void Delete(HConfig config);

}

#endif // DM_CONFIGFILE_H

