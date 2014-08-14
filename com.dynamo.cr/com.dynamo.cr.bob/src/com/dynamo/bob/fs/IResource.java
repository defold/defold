package com.dynamo.bob.fs;

import java.io.IOException;
import java.io.InputStream;


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
     * @return content. <code>null</code> if the resource doesn't exists
     * @throws IOException
     */
    byte[] getContent() throws IOException;

    /**
     * Set content for resource. #
     * @note only valid operation for output-resources, see {@link IResource#output()}
     * @param content content to set
     * @throws IOException
     */
    void setContent(byte[] content) throws IOException;

    /**
     * Get sha1 checksum for resource
     * @return sha1 checksum
     * @throws IOException
     */
    byte[] sha1() throws IOException;

    /**
     * Check if the resource exists
     * @return true if the resource exists
     */
    boolean exists();

    /**
     * Get abs-path for resource
     * @return path
     */
    String getAbsPath();

    /**
     * Get root-relative path for resource
     * @return path
     */
    String getPath();

    /**
     * Remove resource
     */
    void remove();

    /**
     * Get resource in the same folder as "this" resource
     * @param name name of the resource
     * @return {@link IResource}
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

    /**
     * Set content from input stream
     * @param stream input stream
     * @throws IOException
     */
    void setContent(InputStream stream) throws IOException;
}
