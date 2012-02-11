package com.dynamo.bob;

/**
 * Abstract file-system
 * @author Christian Murray
 *
 */
public interface IFileSystem {

    /**
     * Get resource from path. The resource is created if it doesn't exists.
     * @param path resource-path
     * @return resource
     */
    public IResource get(String path);

    /**
     * Set build directory. The directory where the output-files are put.
     * The source directory structure is typically mirrored to this directory.
     * @param buildDirectory build directory
     */
    public void setBuildDirectory(String buildDirectory);
}
