package com.dynamo.bob;

import java.net.URL;
import java.util.Set;

/**
 * Resource scanner interface
 *
 */
public interface IResourceScanner {
    /**
     * Retrieve a URL to the resource at the specified path.
     * @param path path of the resource
     * @return URL pointing to the resource
     */
    URL getResource(String path);

    /**
     * Scan after resources matching the filter
     * @param filter filter to match resources
     * @return {@link Set} of resources
     */
    public Set<String> scan(String filter);
}
