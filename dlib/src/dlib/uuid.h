#ifndef DM_UUID
#define DM_UUID

namespace dmUUID
{
    struct UUID
    {
        uint8_t m_UUID[16];
    };

    /**
     * Generate UUID
     * @param uuid pointer to UUID struct
     */
    void Generate(UUID* uuid);
}

#endif // DM_UUID
