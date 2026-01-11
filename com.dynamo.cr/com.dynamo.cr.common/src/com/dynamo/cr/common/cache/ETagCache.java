// Copyright 2020-2026 The Defold Foundation
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

package com.dynamo.cr.common.cache;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.math.BigInteger;

/**
 * LRU-cache for etags
 * @author chmu
 *
 */
public class ETagCache {
    LRUCache<String, Entry> etagCache;
    private int cacheHits;
    private int cacheMisses;

    private static class Entry {
        public Entry(String hash, long lastModified) {
            this.hash = hash;
            this.lastModified = lastModified;
        }
        String hash;
        long lastModified;
    }

    /**
     * Constructor
     * @param capacity lru-cache capacity
     */
    public ETagCache(int capacity) {
        etagCache = new LRUCache<String, Entry>(capacity);
    }

    private static String calculateSHA1(File file) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-1");
            BufferedInputStream is = new BufferedInputStream(
                    new FileInputStream(file));
            byte[] buffer = new byte[1024];
            int n = is.read(buffer);
            while (n != -1) {
                md.update(buffer, 0, n);
                n = is.read(buffer);
            }
            is.close();
            return new BigInteger(1, md.digest()).toString(16);

        } catch (IOException e) {
            return null;
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Get etag-hash for file
     * @param file file to get etag-hash for
     * @return etag-hash as a string. null is the file doesn't exists
     */
    public synchronized String getETag(File file) {
        if (!file.isFile()) {
            return null;
        }

        String path = file.getAbsolutePath();
        Entry entry = etagCache.get(path);
        long lastModified = file.lastModified();
        if (entry != null && entry.lastModified == lastModified) {
            ++cacheHits;
            return etagCache.get(path).hash;
        } else {
            String digest = calculateSHA1(file);
            if (digest != null) {
                ++cacheMisses;
                etagCache.put(path, new Entry(digest, lastModified));
                return digest;
            } else {
                return null;
            }
        }
    }

    public int getCacheHits() {
        return cacheHits;
    }

    public int getCacheMisses() {
        return cacheMisses;
    }

}
