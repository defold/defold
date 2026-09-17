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

#ifndef DM_CONFIGFILE_HTTP_H
#define DM_CONFIGFILE_HTTP_H

#include <stdint.h>

#include "configfile.h"
#include "uri.h"

namespace dmConfigFile
{
    typedef Result (*FLoadFromBufferInternal)(const char* url, const char* buffer, uint32_t buffer_size, int argc, const char** argv, HConfig* config);

    Result LoadFromHttpInternal(const char* url, const dmURI::Parts& uri_parts, int argc, const char** argv, HConfig* config, FLoadFromBufferInternal load_from_buffer);
}

#endif // DM_CONFIGFILE_HTTP_H
