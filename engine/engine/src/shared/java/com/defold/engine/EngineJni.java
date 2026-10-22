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

package com.defold.engine;

import java.util.Arrays;

/** Loads the engine modules exposed through dmengine_shared. */
public final class EngineJni {
    static {
        // An unpacked editor library can be supplied by absolute path.
        String libraryPath = System.getProperty("dmengine.library");
        if (libraryPath != null)
            System.load(libraryPath);
        else
            System.loadLibrary("dmengine_shared");
    }

    private EngineJni() {}

    /** Ensures the library is loaded and its module bindings are registered. */
    public static void Initialize() {}

    /** Outputs a deterministic slice-9 fixture for command-line and integration tests. */
    public static void main(String[] args) {
        Geometry.Slice9Params params = new Geometry.Slice9Params();
        params.width = 32.5f;
        params.height = 24.25f;
        params.slice9 = new float[] {2, 3, 4, 5};
        params.pivotX = 0.75f;
        params.pivotY = 0.25f;
        float[] corners = {0, 0, 0, 1, 1, 1, 1, 0};
        System.out.println("{\"positions\":" + Arrays.toString(GeometryJni.GenerateSlice9Positions(params))
            + ",\"uvs\":" + Arrays.toString(GeometryJni.GenerateSlice9TexCoords(params, 64, 64, corners))
            + ",\"indices\":" + Arrays.toString(GeometryJni.GetSlice9Indices()) + "}");
    }
}
