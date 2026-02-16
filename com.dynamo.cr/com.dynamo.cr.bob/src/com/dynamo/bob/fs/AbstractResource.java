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

import static org.apache.commons.io.FilenameUtils.concat;

import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import org.apache.commons.io.FilenameUtils;


public abstract class AbstractResource<F extends IFileSystem> implements IResource {
    protected F fileSystem;
    protected String path; // Path relative to the current dir (or absolute). Starts with e.g. "build/default" build folder.
    private boolean cacheable = true;
    private boolean isOutput = false;
    private boolean minifyEnabled = true;

    public AbstractResource(F fileSystem, String path) {
        this.fileSystem = fileSystem;
        this.path = path;

        this.isOutput = fileSystem != null &&
                     (path.startsWith(fileSystem.getBuildDirectory() + "/")
                     || path.startsWith(fileSystem.getBuildDirectory() + "\\"));
    }

    @Override
    public boolean isOutput() {
        return this.isOutput;
    }

    @Override
    public IResource changeExt(String ext) {
        // Used to create an output node
        String newPath = ResourceUtil.changeExt(path, ext);

        if (minifyEnabled)
        {
            String buildDirectory = fileSystem.getBuildDirectory();
            if (path.startsWith(buildDirectory))
                newPath = newPath.substring(buildDirectory.length()+1);

            newPath = ResourceUtil.minifyPath(newPath);

            if (path.startsWith(buildDirectory))
                newPath = buildDirectory + "/" + newPath;
        }

        IResource newResource = fileSystem.get(newPath);
        return newResource.output();
    }

    @Override
    public byte[] sha1() throws IOException {
        byte[] content = getContent();
        if (content == null) {
            throw new IllegalArgumentException(String.format("Resource '%s' is not created", path));
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

    @Override
    public String getAbsPath() {
        return concat(fileSystem.getRootDirectory(), getPath());
    }

    @Override
    public String getPath() {
        return path;
    }

    @Override
    public IResource getResource(String name) {
        String basePath = FilenameUtils.getPath(this.path);
        String fullPath = FilenameUtils.normalize(FilenameUtils.concat(basePath, name), true);
        return this.fileSystem.get(fullPath);
    }

    @Override
    public IResource output() {
        if (isOutput()) {
            return this;
        } else {
            String p = path;
            if (p.startsWith("/"))
                p = p.substring(1);
            String buildPath = FilenameUtils.separatorsToUnix(FilenameUtils.concat(this.fileSystem.getBuildDirectory(), p));
            return fileSystem.get(buildPath);
        }
    }

    @Override
    public int hashCode() {
        return path.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof IResource) {
            IResource r = (IResource) obj;
            return this.path.equals(r.getPath());
        } else {
            return super.equals(obj);
        }
    }

    @Override
    public String toString() {
        return path;
    }

    @Override
    public IResource disableCache() {
        cacheable = false;
        return this;
    }

    @Override
    public boolean isCacheable() {
        return isOutput() && cacheable;
    }

    @Override
    public IResource disableMinifyPath() {
        minifyEnabled = false;
        return this;
    }

    @Override
    public boolean isMinifyPath() { return minifyEnabled; }
}
