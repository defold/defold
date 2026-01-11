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

package com.dynamo.bob.test.util;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.TreeMap;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.fs.AbstractFileSystem;
import com.dynamo.bob.fs.IMountPoint;
import com.dynamo.bob.fs.IResource;

public class MockFileSystem extends AbstractFileSystem<MockFileSystem, MockResource> {

    // To easier handle walking we want the resources to be sorted by their key.
    protected Map<String, MockResource> resources = new TreeMap<String, MockResource>();

    public MockResource addFile(String path, byte[] content, long lastModified) {
        path = FilenameUtils.normalize(path, true);
        // Paths are always root relative.
        if (path.startsWith("/"))
            path = path.substring(1);
        MockResource resource = new MockResource(this, path, content, lastModified);
        resources.put(path, resource);
        return resource;
    }

    public MockResource addFile(String path, byte[] content) {
        return addFile(path, content, System.currentTimeMillis());
    }

    public MockResource addDirectory(String path) {
        path = FilenameUtils.normalize(path, true);
        // Paths are always root relative.
        if (path.startsWith("/"))
            path = path.substring(1);
        MockResource resource = new MockResource(this, path);
        resources.put(path, resource);
        return resource;
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
            r = new MockResource(fileSystem, path, null, System.currentTimeMillis());
            resources.put(path, (MockResource) r);
        }
        return r;
    }

    @Override
    public void loadCache() {}

    @Override
    public void saveCache() {}

    // Sort a list of paths based on lexicographically
    // Needed to be able to mimic the directory traversal on disc
    class SortPath implements Comparator<String> {
        public int compare(String a, String b) {
            return a.compareTo(b);
        }
    }

    private List<String> getResourcePaths() {
        List<String> paths = new ArrayList<String>();
        Iterator<Map.Entry<String, MockResource>> it = resources.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<String, MockResource> entry = (Map.Entry<String, MockResource>)it.next();
            paths.add(entry.getKey());
        }
        Collections.sort(paths, new SortPath());
        return paths;
    }

    @Override
    public void walk(String path, IWalker walker, Collection<String> results) {

        List<String> paths = getResourcePaths();
        ListIterator<String> it = paths.listIterator(0);

        while (it.hasNext()) {
            String entryPath = (String) it.next();

            boolean isInPath = entryPath.startsWith(path);
            if (isInPath) {
                boolean isDirectory = !resources.get(entryPath).isFile();
                if (isDirectory) {
                    if (!walker.handleDirectory(entryPath, results)) {
                        // If we should skip this directory, continue loop until entryPath does not match anymore.
                        while (it.hasNext()) {
                            String subEntryPath = (String) it.next();
                            if (!subEntryPath.startsWith(entryPath)) {
                                it.previous();
                                break;
                            }
                        }
                    }
                } else {
                    walker.handleFile(entryPath, results);
                }
            }
        }
        for (IMountPoint mountPoint : this.mountPoints) {
            mountPoint.walk(path, walker, results);
        }
    }

}
