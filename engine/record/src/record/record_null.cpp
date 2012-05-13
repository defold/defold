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

