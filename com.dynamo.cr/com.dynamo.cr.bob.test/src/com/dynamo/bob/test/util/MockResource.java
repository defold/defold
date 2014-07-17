package com.dynamo.bob.test.util;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

import com.dynamo.bob.fs.AbstractResource;

public class MockResource extends AbstractResource<MockFileSystem> {

    private byte[] content;

    public MockResource(MockFileSystem fileSystem, String path, byte[] content) {
        super(fileSystem, path);
        this.content = content;
    }

    @Override
    public byte[] getContent() {
        return content;
    }

    @Override
    public void setContent(byte[] content) {
        if (!isOutput()) {
            throw new IllegalArgumentException(String.format("Resource '%s' is not an output resource", this.toString()));
        }
        this.content = Arrays.copyOf(content, content.length);
    }

    // Only for testing
    public void forceSetContent(byte[] content) {
        this.content = Arrays.copyOf(content, content.length);
    }

    @Override
    public boolean exists() {
        return content != null;
    }

    @Override
    public void remove() {
        content = null;
    }

    @Override
    public void setContent(InputStream stream) throws IOException {
        throw new RuntimeException("Not implemented");
    }
}