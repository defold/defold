package com.dynamo.bob.fs;

import java.io.IOException;
import java.util.Collection;

import com.dynamo.bob.fs.IFileSystem.IWalker;

/**
 * A mount point to be included in a file system.
 * @author Ragnar Svensson
 *
 */
public interface IMountPoint {
    /**
     * Retrieve a resource from the mount point given its path.
     * @param path Path of the requested resource
     * @return the requested resource or null if it does not exist
     */
    IResource get(String path);

    /**
     * Mount the mount point.
     * @throws IOException
     */
    void mount() throws IOException;

    /**
     * Unmount the mount point.
     */
    void unmount();

    /**
     * Walk recursively through the content of this mount point.
     * @param path path within the mount point to start from
     * @param walker walker to use when walking, which will store the results
     * @param results collection of encountered and stored paths
     */
    void walk(String path, IWalker walker, Collection<String> results);
}
