#ifndef DM_CRYPT_H
#define DM_CRYPT_H

#include <stdint.h>

namespace dmCrypt
{
    enum Algorithm
    {
        ALGORITHM_XTEA,
    };

    enum Result
    {
        RESULT_OK = 0,
    };

    /**
     *  Encrypt data in place
     *  @param algo algorithm
     *  @param data data
     *  @param datalen data lengt in bytes
     *  @param key key
     *  @param keylen key length
     */
    Result Encrypt(Algorithm algo, uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen);

    /**
     *  Decrypt data in place
     *  @param algo algorithm
     *  @param data data
     *  @param datalen data lengt in bytes
     *  @param key key
     *  @param keylen key length
     */
    Result Decrypt(Algorithm algo, uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen);
}

#endif /* DM_CRYPT_H */
