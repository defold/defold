// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_LIVEUPDATE_VERIFY_H
#define DM_LIVEUPDATE_VERIFY_H

#include "liveupdate.h"
#include <resource/resource.h>

namespace dmLiveUpdate
{
    dmResource::Result VerifyZipArchive(const char* path, const char* public_key_path);
}

#endif // DM_LIVEUPDATE_VERIFY_H
