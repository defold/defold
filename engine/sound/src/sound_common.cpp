#include "sound.h"
#ifdef __EMSCRIPTEN__
    #define BUFFER_SIZE (12 * 4096)
#else
    #define BUFFER_SIZE (4 * 4096)
#endif

namespace dmSound
{
    void SetDefaultInitializeParams(InitializeParams* params)
    {
        memset(params, 0, sizeof(InitializeParams));
        params->m_OutputDevice = "default";
        params->m_MasterGain = 1.0f;
        params->m_MaxSoundData = 128;
        params->m_MaxSources = 16;
        params->m_MaxBuffers = 32;
        params->m_BufferSize = BUFFER_SIZE;
        params->m_FrameCount = 2048;
        params->m_MaxInstances = 256;
    }
}
