package com.dynamo.bob.test.util;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.AbstractFileSystem;
import com.dynamo.bob.IResource;

public class MockFileSystem extends AbstractFileSystem<MockFileSystem, MockResource> {

    public void addFile(String name, byte[] content) {
        name = FilenameUtils.normalize(name, true);
        resources.put(name, new MockResource(this, name, content));
    }

    public void addFile(MockResource resource) {
        this.resources.put(resource.getAbsPath(), resource);
    }

    @Override
    public IResource get(String name) {
        name = FilenameUtils.normalize(name, true);
        IResource r = resources.get(name);
        if (r == null) {
            r = new MockResource(fileSystem, name, null);
            resources.put(name, (MockResource) r);
        }
        return r;
    }
}