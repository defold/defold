package com.dynamo.bob.test.util;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.fs.AbstractFileSystem;
import com.dynamo.bob.fs.IResource;

public class MockFileSystem extends AbstractFileSystem<MockFileSystem, MockResource> {

    public void addFile(String path, byte[] content) {
        path = FilenameUtils.normalize(path, true);
        // Paths are always root relative.
        if (path.startsWith("/"))
            path = path.substring(1);
        resources.put(path, new MockResource(this, path, content));
    }

    @Override
    public IResource get(String path) {
        path = FilenameUtils.normalize(path, true);
        // Paths are always root relative.
        if (path.startsWith("/"))
            path = path.substring(1);
        IResource r = getFromMountPoints(path);
        if (r != null) {
            return r;
        }
        r = resources.get(path);
        if (r == null) {
            r = new MockResource(fileSystem, path, null);
            resources.put(path, (MockResource) r);
        }
        return r;
    }

    @Override
    public void loadCache() {}

    @Override
    public void saveCache() {}

}
