#ifndef DM_UUID
#define DM_UUID

#include <stdint.h>

namespace dmUUID
{
    union UUID
    {
        uint8_t m_UUID[16];
        struct
        {
            uint32_t m_TimeLow;
            uint16_t m_TimeMid;
            uint16_t m_TimeHiAndVersion;
            uint16_t m_ClockSeq;
            uint8_t  m_Node[6];
        };
    };

    /**
     * Generate UUID
     * @param uuid pointer to UUID struct
     */
    void Generate(UUID* uuid);
}

#endif // DM_UUID
