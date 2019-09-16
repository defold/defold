#ifndef DM_CRYPT_H
#define DM_CRYPT_H

#include <stdint.h>

namespace dmCrypt
{
    enum Algorithm
    {
        ALGORITHM_XTEA
    };

    enum Result
    {
        RESULT_OK = 0,
        RESULT_ERROR = 1,
    };

    /**
     *  Encrypt data in place
     *  @param algo algorithm
     *  @param data data
     *  @param datalen data length in bytes
     *  @param key key
     *  @param keylen key length
     */
    Result Encrypt(Algorithm algo, uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen);

    /**
     *  Decrypt data in place
     *  @param algo algorithm
     *  @param data data
     *  @param datalen data length in bytes
     *  @param key key
     *  @param keylen key length
     */
    Result Decrypt(Algorithm algo, uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen);

    /**
     * Decrypt data with a public hash
     * @param key The public key
     * @param keylen
     * @param data The input data
     * @param datalen
     * @param output [out] The output. If the call was successful, the caller needs to free() this memory
     * @param outputlen [out]
     * @return RESULT_OK if decrypting went ok.
     */
    Result Decrypt(const uint8_t* key, uint32_t keylen, const uint8_t* data, uint32_t datalen, uint8_t** output, uint32_t* outputlen);

    void HashSha1(const uint8_t* buf, uint32_t buflen, uint8_t* digest);      // output is 20 bytes
    void HashSha256(const uint8_t* buf, uint32_t buflen, uint8_t* digest);    // output is 32 bytes
    void HashSha512(const uint8_t* buf, uint32_t buflen, uint8_t* digest);    // output is 64 bytes
    void HashMd5(const uint8_t* buf, uint32_t buflen, uint8_t* digest);       // output is 16 bytes
}

#endif /* DM_CRYPT_H */
