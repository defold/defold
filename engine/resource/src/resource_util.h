// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_RESOURCE_UTIL_H
#define DM_RESOURCE_UTIL_H

#include "resource.h"
#include <resource/liveupdate_ddf.h>

namespace dmResource
{
    typedef struct Manifest* HManifest;

    typedef Result (*FDecryptResource)(void* buffer, uint32_t buffer_len);
    void RegisterResourceDecryption(FDecryptResource decrypt_resource);

    // Decrypts a buffer, using the built in function, or the custom one set by RegisterResourceDecryption
    Result DecryptBuffer(void* buffer, uint32_t buffer_len);

    /**
     * The manifest has a signature embedded. This signature is created when bundling by hashing the manifest content
     * and encrypting the hash with the private part of a public-private key pair. To verify a manifest this procedure
     * is performed in reverse; first decrypting the signature using the public key (bundled with the engine) to
     * retreive the content hash then hashing the actual manifest content and comparing the two.
     * This method handles the signature decryption part.
     */
    Result DecryptSignatureHash(const dmResource::HManifest manifest, const uint8_t* pub_key_buf, uint32_t pub_key_len, uint8_t** out_digest, uint32_t* out_digest_len);


    /*#
     * Returns the length in bytes of the supplied hash algorithm
     * @name HashLength
     * @param algorithm [type: dmLiveUpdateDDF::HashAlgorithm] The algorithm
     * @return size [type: uint32_t] the size required to store a hash using the algorithm (in bytes)
     */
    uint32_t HashLength(dmLiveUpdateDDF::HashAlgorithm algorithm);

    /*#
     * Create a hash of a byte buffer representing a resource
     * @name CreateResourceHash
     * @param algorithm [type: dmLiveUpdateDDF::HashAlgorithm] The algorithm (MD5 or SHA1)
     * @param buffer [type: uint8_t*] the buffer
     * @param buffer_size [type: size_t] the size of the buffer (in bytes)
     * @param digest [type: uint8_t*] the output digest. Expected to be at least the size needed for this algorithm (See HashLength)
     */
    Result CreateResourceHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* buffer, size_t buffer_size, uint8_t* digest);

    /*#
     * Create a hash of a manifest
     * @name CreateManifestHash
     * @param algorithm [type: dmLiveUpdateDDF::HashAlgorithm] The algorithm (SHA1, SHA256 or SHA512)
     * @param buffer [type: uint8_t*] the buffer
     * @param buffer_size [type: size_t] the size of the buffer (in bytes)
     * @param digest [type: uint8_t*] the output digest. Expected to be at least the size needed for this algorithm (See HashLength)
     */
    Result CreateManifestHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* buffer, size_t buffer_size, uint8_t* digest);

    /*#
     * Converts the supplied byte buffer to hexadecimal string representation
     * @name BytesToHexString
     * @param byte_buf [type: uint8_t*] The byte buffer
     * @param byte_buf_len [type: uint32_t] The byte buffer length
     * @param out_buf [type: uint8_t*] The output string buffer
     * @param out_len [type: uint32_t] The output buffer length
     */
    void BytesToHexString(const uint8_t* byte_buf, uint32_t byte_buf_len, char* out_buf, uint32_t out_len);

    /**
     * Byte-wise comparison of two buffers
     * @name MemCompare
     * @param digest The hash digest to compare
     * @param len The hash digest length
     * @param buf The expected hash digest
     * @param buflen The expected hash digest length
     * @return RESULT_OK if the hashes are equal in length and content
     */
    Result MemCompare(const uint8_t* digest, uint32_t len, const uint8_t* expected_digest, uint32_t expected_len);

    /**
     * Gets the normalized resource path: "/my//icon.texturec" -> "/my/icon.texturec". "my/icon.texturec" -> "/my/icon.texturec".
     * @param relative_dir the relative dir of the resource
     * @param buf the result of the operation
     * @return the length of the buffer
     */
    uint32_t GetCanonicalPath(const char* relative_dir, char* buf);

    /**
     * Gets the actual resource path e.g. "base_dir/my/icon.texturec".
     * @param relative_dir the relative dir of the resource
     * @param buf the result of the operation
     * @return the length of the buffer
     */
    uint32_t GetCanonicalPathFromBase(const char* base_dir, const char* relative_dir, char* buf);

    // For debugging
    void PrintHash(const uint8_t* hash, uint32_t len);
}

#endif // DM_RESOURCE_UTIL_H
