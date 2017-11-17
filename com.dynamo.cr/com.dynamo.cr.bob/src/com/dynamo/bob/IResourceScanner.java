package com.dynamo.bob;

import java.io.IOException;
import java.io.InputStream;
import java.util.Set;

/**
 * Resource scanner interface
 *
 */
public interface IResourceScanner {
    /**
     * Retrieve an input stream from the resource at the specified path.
     * The caller is responsible for closing the stream.
     * @param path path of the resource
     * @return InputStream of the resource
     */
    InputStream openInputStream(String path) throws IOException;

    /**
     * Whether the resource exists on disk or in an archive or not.
     * @param path
     * @return true if it exists
     */
    boolean exists(String path);

    /**
     * Whether the resource represents a file on disk or not.
     * @param path
     * @return true if it is a file
     */
    boolean isFile(String path);

    /**
     * Scan after resources matching the filter
     * @param filter filter to match resources
     * @return {@link Set} of resources
     */
    public Set<String> scan(String filter);
}
