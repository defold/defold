#include "record.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace dmRecord
{
    NewParams::NewParams()
    {
        memset(this, 0, sizeof(*this));
        m_ContainerFormat = CONTAINER_FORMAT_IVF;
        m_VideoCodec = VIDOE_CODEC_VP8;
        m_Fps = 30;
    }
}
