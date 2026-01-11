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

#ifndef DM_RECORD_H
#define DM_RECORD_H

#include <stdint.h>

namespace dmRecord
{
    /**
     * Minimum dimension multiple. Height and width
     * must be a multiple of this value
     */
    const uint32_t MIN_DIMENSION_MULTIPLE = 8;

    typedef struct Recorder* HRecorder;

    enum BufferFormat
    {
        BUFFER_FORMAT_BGRA = 0,
    };

    enum ContainerFormat
    {
        CONTAINER_FORMAT_IVF = 0,
    };

    enum VideoCodec
    {
        VIDOE_CODEC_VP8 = 0,
    };

    enum Result
    {
        RESULT_OK = 0,
        RESULT_IO_ERROR = -1,
        RESULT_INVAL_ERROR = -2,
        RESULT_RECORD_NOT_SUPPORTED = -3,
        RESULT_UNKNOWN_ERROR = -1000,
    };

    struct NewParams
    {
        NewParams();
        uint32_t        m_Width;
        uint32_t        m_Height;
        ContainerFormat m_ContainerFormat;
        VideoCodec      m_VideoCodec;
        const char*     m_Filename;
        uint32_t        m_Fps;
    };

    Result New(const NewParams* params, HRecorder* recorder);
    Result Delete(HRecorder recorder);
    Result RecordFrame(HRecorder recorder, const void* frame_buffer, uint32_t frame_buffer_size, BufferFormat format);
}

#endif
