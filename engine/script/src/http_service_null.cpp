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

#include <dlib/log.h>
#include <dlib/message.h>

#include "http_ddf.h"
#include "http_service.h"

namespace dmHttpService
{
    HHttpService New(const Params* params)
    {
        (void) params;
        dmLogError("HTTP service is not supported on HTML5.");
        return 0;
    }

    dmMessage::HSocket GetSocket(HHttpService http_service)
    {
        (void) http_service;
        return 0;
    }

    void Delete(HHttpService http_service)
    {
        (void) http_service;
    }
}
