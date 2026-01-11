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

package com.defold.util;

import java.io.Writer;

/**
 * Interface used to identify equivalent objects among instances of the same
 * type. Used during the build process to merge equivalent build outputs into a
 * single shared resource.
 */
public interface IDigestable {
    /**
     * Writes some unique representation of the object to the provided Writer.
     * Anything written to the Writer will be hashed, along with the class name
     * of the object, and used to identify equivalent objects among instances
     * of the same type. The written data is expected to be consistent between
     * runs of the application.
     * @param writer The java.io.Writer to write to.
     * @throws Exception In case of an error.
     */
    void digest(Writer writer) throws Exception;
}
