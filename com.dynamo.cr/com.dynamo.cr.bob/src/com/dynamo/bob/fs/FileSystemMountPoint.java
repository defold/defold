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

import org.apache.commons.io.FilenameUtils;

import java.io.IOException;
import java.io.InputStream;
import java.util.Collection;

public class FileSystemMountPoint implements IMountPoint {

    private final IFileSystem rootFileSystem;
    private final IFileSystem fileSystem;

    public FileSystemMountPoint(IFileSystem rootFileSystem, IFileSystem fileSystem) {
        this.rootFileSystem = rootFileSystem;
        this.fileSystem = fileSystem;
    }

    @Override
    public IResource get(String path) {
        IResource resource = fileSystem.get(path);
        if (resource.exists()) {
            return new MountedFileSystemResource(rootFileSystem, resource);
        } else {
            return null;
        }
    }

    @Override
    public void mount() throws IOException {
    }

    @Override
    public void unmount() {
        fileSystem.close();
    }

    @Override
    public void walk(String path, IFileSystem.IWalker walker, Collection<String> results) {
        fileSystem.walk(path, walker, results);
    }

    public static class MountedFileSystemResource implements IResource {

        private final IFileSystem rootFileSystem;
        private final IResource resource;

        public MountedFileSystemResource(IFileSystem rootFileSystem, IResource resource) {
            this.rootFileSystem = rootFileSystem;
            this.resource = resource;
        }

        @Override
        public IResource changeExt(String ext) {
            String name = ResourceUtil.changeExt(getPath(), ext);
            return rootFileSystem.get(FilenameUtils.separatorsToUnix(FilenameUtils.concat(rootFileSystem.getBuildDirectory(), name)));
        }

        @Override
        public byte[] getContent() throws IOException {
            return resource.getContent();
        }

        @Override
        public void setContent(byte[] content) throws IOException {
            resource.setContent(content);
        }

        @Override
        public void appendContent(byte[] content) throws IOException {
            resource.appendContent(content);
        }

        @Override
        public byte[] sha1() throws IOException {
            return resource.sha1();
        }

        @Override
        public boolean exists() {
            return resource.exists();
        }

        @Override
        public boolean isFile() {
            return resource.isFile();
        }

        @Override
        public String getAbsPath() {
            return resource.getAbsPath();
        }

        @Override
        public String getPath() {
            return resource.getPath();
        }

        @Override
        public void remove() {
            resource.remove();
        }

        @Override
        public IResource getResource(String name) {
            return resource.getResource(name);
        }

        @Override
        public IResource output() {
            return rootFileSystem.get(FilenameUtils.separatorsToUnix(FilenameUtils.concat(rootFileSystem.getBuildDirectory(), resource.getPath())));
        }

        @Override
        public boolean isOutput() {
            return false;
        }

        @Override
        public void setContent(InputStream stream) throws IOException {
            resource.setContent(stream);
        }

        @Override
        public long getLastModified() {
            return resource.getLastModified();
        }

        @Override
        public IResource disableCache() {
            return resource.disableCache();
        }

        @Override
        public boolean isCacheable() {
            return resource.isCacheable();
        }

        @Override
        public IResource disableMinifyPath() { return resource.disableMinifyPath(); }

        @Override
        public boolean isMinifyPath() { return resource.isMinifyPath(); }

    }
}
