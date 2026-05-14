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

package com.dynamo.bob.fs;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;


public class DefaultFileSystem extends AbstractFileSystem<DefaultFileSystem, DefaultResource> {

    static class CacheEntry implements Serializable {
        private static final long serialVersionUID = 1L;
        long mTime;
        byte[] sha1;
    }

    private Map<String, CacheEntry> cache = new ConcurrentHashMap<String, CacheEntry>();

    @Override
    public IResource get(String path) {
        // Paths are always root relative.
        while (path.startsWith("/") || path.startsWith("\\"))
            path = path.substring(1);
        IResource resource = getFromMountPoints(path);
        if (resource != null) {
            return resource;
        }
        return new DefaultResource(this, path);
    }

    private byte[] calcSha1(DefaultResource resource) throws IOException {
        byte[] content = resource.getContent();
        if (content == null) {
            throw new IllegalArgumentException(String.format("Resource '%s' is not created", resource.getPath()));
        }
        MessageDigest sha1;
        try {
            sha1 = MessageDigest.getInstance("SHA1");
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
        sha1.update(content);
        return sha1.digest();
    }

    byte[] sha1(DefaultResource resource) throws IOException {
        String absPath = resource.getAbsPath();
        File file = new File(absPath);
        CacheEntry e = cache.get(resource.getPath());
        if (e != null && file.lastModified() == e.mTime) {
            return e.sha1;
        } else {
            e = new CacheEntry();
            e.mTime = file.lastModified();
            e.sha1 = calcSha1(resource);
            cache.put(resource.getPath(), e);
            return e.sha1;
        }
    }

    @SuppressWarnings("unchecked")
    @Override
    public void loadCache() {
        cache = new ConcurrentHashMap<String, CacheEntry>();
        String fileName = FilenameUtils.concat(FilenameUtils.concat(this.rootDirectory, this.buildDirectory), "digest_cache");
        ObjectInputStream is = null;
        try {
            is = new ObjectInputStream(new BufferedInputStream(new FileInputStream(fileName)));
            cache = (Map<String, CacheEntry>) is.readObject();
            is.close();
        } catch (IOException e) {
        } catch (ClassNotFoundException e) {
        } finally {
            IOUtils.closeQuietly(is);
        }
    }

    @Override
    public void saveCache() {
        String fileName = FilenameUtils.concat(FilenameUtils.concat(this.rootDirectory, this.buildDirectory), "digest_cache");
        ObjectOutputStream os = null;
        try {
            os = new ObjectOutputStream(new BufferedOutputStream(new FileOutputStream(fileName)));
            os.writeObject(cache);
        } catch (IOException e) {
        } finally {
            IOUtils.closeQuietly(os);
        }
    }

}
