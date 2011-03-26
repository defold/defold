package com.dynamo.cr.editor.core;

/**
 * Registry of resource types
 * @author chmu
 *
 */
public interface IResourceTypeRegistry {

    /**
     * Get all resource types.
     * @return array of all resource types
     */
    IResourceType[] getResourceTypes();

    /**
     * Get resource type from file extension.
     * @param extension file extension
     * @return resource type. null if no mapping exists
     */
    IResourceType getResourceTypeFromExtension(String extension);

    /**
     * Get resource type from id
     * @param id id
     * @return resource type. null if no mapping exists
     */
    IResourceType getResourceTypeFromId(String id);

}
