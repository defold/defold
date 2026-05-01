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

import java.io.File;
import java.io.IOException;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.Vector;

import org.apache.commons.io.FilenameUtils;

public abstract class AbstractFileSystem<F extends IFileSystem, R extends IResource> implements IFileSystem {
    protected F fileSystem;
    protected String rootDirectory;
    protected String buildDirectory;
    protected Map<String, R> resources = new HashMap<String, R>();
    protected Vector<IMountPoint> mountPoints;

    @SuppressWarnings("unchecked")
    public AbstractFileSystem() {
        fileSystem = (F) this;
        mountPoints = new Vector<IMountPoint>();
    }

    @Override
    public void setRootDirectory(String rootDirectory) {
        this.rootDirectory = FilenameUtils.normalizeNoEndSeparator(rootDirectory, true);
    }

    @Override
    public String getRootDirectory() {
        return rootDirectory;
    }

    @Override
    public void setBuildDirectory(String buildDirectory) {
        buildDirectory = FilenameUtils.normalizeNoEndSeparator(buildDirectory, true);
        if (buildDirectory.startsWith("/")) {
            throw new IllegalArgumentException("Build directory must be relative to root directory and not absolute.");
        }
        this.buildDirectory = buildDirectory;
    }

    public String getBuildDirectory() {
        return buildDirectory;
    }

    @Override
    public void addMountPoint(IMountPoint mountPoint) throws IOException {
        mountPoint.mount();
        this.mountPoints.add(mountPoint);
    }

    @Override
    public void clearMountPoints() {
        this.mountPoints.clear();
    }

    @Override
    public void close() {
        for (IMountPoint mountPoint : this.mountPoints) {
            mountPoint.unmount();
        }
    }

    protected IResource getFromMountPoints(String path) {
        for (IMountPoint mountPoint : this.mountPoints) {
            IResource resource = mountPoint.get(path);
            if (resource != null) {
                return resource;
            }
        }
        return null;
    }

    private void walk(IWalker walker, String path, Collection<String> results) {
        String absolutePath = FilenameUtils.normalizeNoEndSeparator(FilenameUtils.concat(this.rootDirectory, path));
        File file = new File(absolutePath);
        if (!file.exists()) {
            return;
        }
        if (file.isDirectory()) {
            if (walker.handleDirectory(path, results)) {
                String[] children = file.list();
                for (String child : children) {
                    walk(walker, FilenameUtils.concat(path, child), results);
                }
            }
        } else {
            walker.handleFile(path, results);
        }
    }

    @Override
    public void walk(String path, IWalker walker, Collection<String> results) {
        walk(walker, path, results);
        for (IMountPoint mountPoint : this.mountPoints) {
            mountPoint.walk(path, walker, results);
        }
    }
}
