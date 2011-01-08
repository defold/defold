package com.dynamo.cr.protobind;

import java.util.List;

/**
 * Base class for wrapped google protocol buffer types
 * @author chmu
 *
 */
public abstract class Node {

    /**
     * Get all paths to fields. See {@link #getPathTo(String)} for further explaination of paths.
     * @return all paths to fields
     */
    public abstract IPath[] getAllPaths();

    /**
     * Build a google protocol buffer message from current state
     * @return a google protocol buffer message for messages and {@link List} for repeated fields
     */
    public abstract Object build();
}
