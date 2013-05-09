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
     * @note Must be a relative path
     * @param buildDirectory build directory
     */
    public void setBuildDirectory(String buildDirectory);

    /**
     * Get build directory
     * @see #setBuildDirectory(String)
     * @return build directory
     */
    public String getBuildDirectory();

    /**
     * Set project root directory. Similar to cwd when building in command-line
     * @param rootDirectory root directory
     */
    public void setRootDirectory(String rootDirectory);

    /**
     * Get root directory
     * @see #setRootDirectory(String)
     * @return root directory
     */
    public String getRootDirectory();

    /**
     * Load cache of file signatures (optionally)
     * @note This method can only be invoked after setBuildDirectory is invoked
     */
    public void loadCache();

    /**
     * Save cache of file signatures (optionally)
     */
    public void saveCache();
}
