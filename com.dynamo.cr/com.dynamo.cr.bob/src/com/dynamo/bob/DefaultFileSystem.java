package com.dynamo.bob;

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

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;


public class DefaultFileSystem extends AbstractFileSystem<DefaultFileSystem, DefaultResource> {

    static class CacheEntry implements Serializable {
        private static final long serialVersionUID = 1L;
        long mTime;
        byte[] sha1;
    }

    private Map<String, CacheEntry> cache = new HashMap<String, DefaultFileSystem.CacheEntry>();

    @Override
    public IResource get(String path) {
        // Paths are always root relative.
        if (path.startsWith("/"))
            path = path.substring(1);

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
        cache = new HashMap<String, DefaultFileSystem.CacheEntry>();
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
