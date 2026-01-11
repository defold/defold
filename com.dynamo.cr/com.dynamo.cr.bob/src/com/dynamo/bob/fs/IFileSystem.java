// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.fs;

import java.io.IOException;
import java.util.Collection;

/**
 * Abstract file-system
 * @author Christian Murray
 *
 */
public interface IFileSystem {

    /**
     * Used to traverse the file system and any attached mount points.
     */
    public interface IWalker {
        /**
         * Inspect the directory described by the path.
         * @param path relative path of the directory
         * @param results result of traversed paths, write to the collection if the supplied path should be included
         * @return true if the directory should be entered, false to disregard
         */
        boolean handleDirectory(String path, Collection<String> results);
        /**
         * Inspect the file described by the path.
         * @param path relative path of the file
         * @param results result of traversed paths, write to the collection if the supplied path should be included
         */
        void handleFile(String path, Collection<String> results);
    }

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

    /**
     * Add a mount point to the file system, e.g. a zip archive or Java class loader.
     * @param mountPoint mount point to add
     * @throws IOException
     */
    public void addMountPoint(IMountPoint mountPoint) throws IOException;

    /**
     * Remove all mount points from the file system.
     */
    public void clearMountPoints();

    /**
     * Close the file system, all clients are responsible for doing this once the file system is no longer in use.
     */
    public void close();

    /**
     * Walk through all files recursively under a given path.
     * @param path relative path of the files to traverse
     * @param walker walker to perform and possibly store the result
     * @param results collection to write the results to
     */
    public void walk(String path, IWalker walker, Collection<String> results);
}
