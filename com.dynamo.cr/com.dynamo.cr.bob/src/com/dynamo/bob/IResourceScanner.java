package com.dynamo.bob;

import java.util.Set;

/**
 * Resource scanner interface
 * @author chmu
 *
 */
public interface IResourceScanner {
    /**
     * Scan after resources matching the filter
     * @param filter filter to match resources
     * @return {@link Set} of resources
     */
    public Set<String> scan(String filter);
}
