package com.dynamo.bob;

import java.util.Set;

/**
 * Class  scanner interface
 * @author chmu
 *
 */
public interface IClassScanner {
    /**
     * Scan after classes in package
     * @param pkg package to list classes
     * @return {@link Set} of classes
     */
    public Set<String> scan(String pkg);
}
