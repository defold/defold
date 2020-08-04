// Copyright 2020 The Defold Foundation
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

package com.dynamo.bob.test.util;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

import com.dynamo.bob.fs.AbstractResource;

public class MockResource extends AbstractResource<MockFileSystem> {

    private byte[] content;
    private long modifiedTime;
    private boolean isFile = true;

    public MockResource(MockFileSystem fileSystem, String path, byte[] content, long modifiedTime) {
        super(fileSystem, path);
        this.content = content;
        this.modifiedTime = modifiedTime;
    }

    public MockResource(MockFileSystem fileSystem, String directory) {
        super(fileSystem, directory);
        this.content = "".getBytes();
        this.modifiedTime = 0;
        this.isFile = false;
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

    @Override
    public long getLastModified() {
        return modifiedTime;
    }

    @Override
    public boolean isFile() {
        return this.isFile;
    }
}
