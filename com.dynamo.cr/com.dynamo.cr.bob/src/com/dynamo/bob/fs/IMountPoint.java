package com.dynamo.bob.fs;

import java.io.IOException;

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
}
