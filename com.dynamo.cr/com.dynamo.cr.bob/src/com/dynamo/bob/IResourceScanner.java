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

package com.dynamo.bob;

import java.io.IOException;
import java.io.InputStream;
import java.util.Set;

/**
 * Resource scanner interface
 *
 */
public interface IResourceScanner {
    /**
     * Retrieve an input stream from the resource at the specified path.
     * The caller is responsible for closing the stream.
     * @param path path of the resource
     * @return InputStream of the resource
     */
    InputStream openInputStream(String path) throws IOException;

    /**
     * Whether the resource exists on disk or in an archive or not.
     * @param path
     * @return true if it exists
     */
    boolean exists(String path);

    /**
     * Whether the resource represents a file on disk or not.
     * @param path
     * @return true if it is a file
     */
    boolean isFile(String path);

    /**
     * Scan after resources matching the filter
     * @param filter filter to match resources
     * @return {@link Set} of resources
     */
    public Set<String> scan(String filter);
}
