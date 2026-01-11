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

package com.defold.extension.pipeline;

/**
 * Interface for Lua source code preprocessing prior to compilation.
 * We will scan for public non-abstract classes that implement this interface inside the com.defold.extension.pipeline
 * package and instantiate them to transform Lua source code. Implementors must provide a no-argument constructor.
 */
public interface ILuaPreprocessor {
    /**
     * Apply Lua source code preprocessing prior to compilation.
     * @param source The unprocessed Lua source code.
     * @param path The path to the resource containing the unprocessed source inside the project.
     * @param buildVariant The build variant. Typically, "debug", "release", or "headless".
     * @return The processed Lua source code.
     * @throws Exception In case of an error.
     */
    String preprocess(String source, String path, String buildVariant) throws Exception;
}
