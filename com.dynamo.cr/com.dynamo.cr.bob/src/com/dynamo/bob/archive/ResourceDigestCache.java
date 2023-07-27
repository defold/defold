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

package com.dynamo.bob.archive;

import java.util.Map;
import java.util.HashMap;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.util.CryptographicOperations;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;

public class ResourceDigestCache {

    public static class ResourceDigest {

        private String path;
        private MessageDigest messageDigest;
        private byte[] hash;

        public ResourceDigest(String path, MessageDigest messageDigest) {
            this.path = path;
            this.messageDigest = messageDigest;
        }

        /**
         * Update this digest with additional bytes. Calling this function has
         * no effect if digest() has been called.
         * @param input Bytes to add
         * @return This instance
         */
        public ResourceDigest update(byte[] input) {
            if (hash == null) {
                messageDigest.update(input);
            }
            return this;
        }

        /**
         * Calculate the final digest. This function can be called multiple
         * times. Once thid function has been called any subsequent call to
         * update() will be ignored.
         * @return The digest
         */
        public byte[] digest() {
            if (hash == null) {
                hash = messageDigest.digest();
            }
            return hash;
        }
    }

    private static Map<String, ResourceDigest> digests = new HashMap<>();

    /**
     * Create a resource digest for a resource path. If a cached digest already
     * exists it will be replaced.
     * @param path The path to get a resource digest for
     * @return The digest
     */
    public static ResourceDigest create(String path) throws CompileExceptionError {
        ResourceDigest digest = null;
        try {
            MessageDigest messageDigest = CryptographicOperations.getMessageDigest(HashAlgorithm.HASH_SHA1);
            digest = new ResourceDigest(path, messageDigest);
            digests.put(path, digest);
        }
        catch (NoSuchAlgorithmException e) {
            throw new CompileExceptionError(e);
        }
        return digest;
    }

    public static ResourceDigest create(IResource resource) throws CompileExceptionError {
        return create(resource.getAbsPath());
    }

    /**
     * Get a resource digest for a path created with 'create()'
     * @param path The path to get a digest for
     * @return The digest or null if none has been created
     */
    public static ResourceDigest get(String path) {
        return digests.get(path);
    }

    public static ResourceDigest get(IResource resource) {
        return get(resource.getAbsPath());
    }

    /**
     * Check if a resource digest exists for a path.
     * @param path The path to check for a digest
     * @return true if a digest exists for the path
     */
    public static boolean hasDigest(String path) {
        return digests.containsKey(path);
    }

    public static boolean hasDigest(IResource resource) {
        return hasDigest(resource.getAbsPath());
    }
}