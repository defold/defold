package com.dynamo.bob.test.util;

import java.io.File;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.TreeMap;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.fs.AbstractFileSystem;
import com.dynamo.bob.fs.IMountPoint;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.IFileSystem.IWalker;

public class MockFileSystem extends AbstractFileSystem<MockFileSystem, MockResource> {

    // To easier handle walking we want the resources to be sorted by their key.
    protected Map<String, MockResource> resources = new TreeMap<String, MockResource>();

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

    @Override
    public void walk(String path, IWalker walker, Collection<String> results) {
        Iterator<Map.Entry<String, MockResource>> it = resources.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<String, MockResource> entry = (Map.Entry<String, MockResource>)it.next();
            String entryPath = entry.getKey();

            boolean isInPath = entryPath.startsWith(path);
            if (isInPath) {

                boolean isDirectory = entryPath.lastIndexOf('/') == entryPath.length() - 1;
                if (isDirectory) {
                    if (!walker.handleDirectory(entryPath, results)) {
                        // If we should skip this directory, continue loop until entryPath does not match anymore.
                        while (it.hasNext()) {
                            Map.Entry<String, MockResource> subEntry = (Map.Entry<String, MockResource>)it.next();
                            String subEntryPath = subEntry.getKey();
                            if (!subEntryPath.startsWith(entryPath)) {
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
