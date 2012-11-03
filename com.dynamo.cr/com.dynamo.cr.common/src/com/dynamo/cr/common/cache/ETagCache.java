package com.dynamo.cr.common.cache;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import org.glassfish.grizzly.http.util.HexUtils;

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
            return HexUtils.convert(md.digest());

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
