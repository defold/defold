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

import com.dynamo.bob.fs.IFileSystem.IWalker;

/**
 * A mount point to be included in a file system.
 * @author Ragnar Svensson
 *
 */
public interface IMountPoint {
    /**
     * Retrieve a resource from the mount point given its path.
     * @param path Path of the requested resource
     * @return the requested resource or null if it does not exist
     */
    IResource get(String path);

    /**
     * Mount the mount point.
     * @throws IOException
     */
    void mount() throws IOException;

    /**
     * Unmount the mount point.
     */
    void unmount();

    /**
     * Walk recursively through the content of this mount point.
     * @param path path within the mount point to start from
     * @param walker walker to use when walking, which will store the results
     * @param results collection of encountered and stored paths
     */
    void walk(String path, IWalker walker, Collection<String> results);
}
