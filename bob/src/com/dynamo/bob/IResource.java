package com.dynamo.bob;


/**
 * Resource abstraction
 * @author Christian Murray
 *
 */
public interface IResource {


    /**
     * Create output-resource with new extension. The returned
     * resource is an output-resource.
     * @param ext new extension including dot
     * @return resource
     */
    IResource changeExt(String ext);

    /**
     * Get content for resource.
     * @return content. <code>null</code> is the resource doesn't exists
     */
    byte[] getContent();

    /**
     * Set content for resource. #
     * @note only valid operation for output-resources, see {@link IResource#output()}
     * @param content content to set
     */
    void setContent(byte[] content);

    /**
     * Get sha1 checksum for resource
     * @return sha1 checksum
     */
    byte[] sha1();

    /**
     * Check if the resource exists
     * @return true if the resource exists
     */
    boolean exists();

    /**
     * Get path for resource
     * @return path
     */
    String getPath();

    /**
     * Remove resource
     */
    void remove();

    /**
     * Get resource in the same folder as "this" resource
     * @param name
     * @return
     */
    IResource getResource(String name);

    /**
     * Create a output (build) resource of this
     * @return output resource
     */
    IResource output();

    /**
     * Check is the resource is an output-resource, see {@link IResource#output()}
     * @return true if an output-resource
     */
    boolean isOutput();
}
