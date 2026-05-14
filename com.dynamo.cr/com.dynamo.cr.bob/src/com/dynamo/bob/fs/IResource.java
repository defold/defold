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
     * Append content for resource. #
     * @note only valid operation for output-resources, see {@link IResource#output()}
     * @param content content to append to resource
     * @throws IOException
     */
    void appendContent(byte[] content) throws IOException;

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
     * Check if the resource is a file
     * @return true if the resource is a file
     */
    boolean isFile();

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
     * Check if the resource is an output-resource, see {@link IResource#output()}
     * @return true if an output-resource
     */
    boolean isOutput();

    /**
     * Set content from input stream
     * @param stream input stream
     * @throws IOException
     */
    void setContent(InputStream stream) throws IOException;

    /**
     * Get the time when the resource was modified
     * @return long representing Unix time when the resource was modified
     */
    long getLastModified();

    /**
     * Disable caching of this resource
     * @return This instance (for function chaining)
     */
    IResource disableCache();

    /**
     * Check if this resource can be cached
     * @return True if resource can be cached. Defaults to true
     */
    boolean isCacheable();

    /**
     * Disable minification of an output resource path
     * @return This instance (for function chaining)
     */
    IResource disableMinifyPath();

    /**
     * Check if this resource path should be minified
     * @return True if resource output path should be minified
     */
    boolean isMinifyPath();
}
