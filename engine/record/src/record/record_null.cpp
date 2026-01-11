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

#include <stdint.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "record.h"
#include <dlib/log.h>

namespace dmRecord
{
    Result New(const NewParams* params, HRecorder* recorder)
    {
        return RESULT_RECORD_NOT_SUPPORTED;
    }

    Result Delete(HRecorder recorder)
    {
        return RESULT_OK;
    }

    Result RecordFrame(HRecorder recorder, const void* frame_buffer, uint32_t frame_buffer_size, BufferFormat format)
    {
        return RESULT_RECORD_NOT_SUPPORTED;
    }
}

