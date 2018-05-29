#ifndef DM_MD5
#define DM_MD5

#include <stdint.h>

namespace dmMD5
{
    /// Number of bytes in md5 checksum
    #define DM_MD5_SIZE 16

    /**
     * MD5 hash state
     */
    struct State
    {
        uint32_t m_State[4];
        uint32_t m_Count[2];
        uint8_t  m_Buffer[64];
    };

    /**
     * MD5 digest
     */
    struct Digest
    {
        uint8_t m_Digest[DM_MD5_SIZE];
    };

    /**
     * Initialize hash-state for incremental hashing
     * @param state hash state
     */
    void Init(State* state);

    /**
     * Incremental hashing
     * @param state hash state
     * @param buffer buffer to hash
     * @param buffer_len buffer len
     */
    void Update(State *state, const void* buffer, uint32_t buffer_len);

    /**
     * Finalize hashing
     * @note The hash-state can't be updated after Final() is called.
     * @param state hash state
     * @param digest digest (out)
     */
    void Final(State* state, Digest* digest);
}

#endif // #ifndef DM_MD5
