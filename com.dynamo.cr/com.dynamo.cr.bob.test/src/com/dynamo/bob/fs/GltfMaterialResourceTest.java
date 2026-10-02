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

import com.dynamo.bob.pipeline.Modelimporter;
import com.dynamo.render.proto.Material.MaterialDesc;
import org.junit.Test;
import static org.junit.Assert.*;

public class GltfMaterialResourceTest {
    // Verifies imported skinning selects a compatible shader without forcing instancing.
    @Test
    public void selectsSkinnedVertexProgram() {
        Modelimporter.Material source = new Modelimporter.Material();
        assertEquals("/builtins/materials/gltf.vp", GltfMaterialResource.createMaterialDesc(source).getVertexProgram());
        source.isSkinned = 1;
        MaterialDesc material = GltfMaterialResource.createMaterialDesc(source);
        assertEquals("/builtins/materials/gltf_skinned.vp", material.getVertexProgram());
        assertEquals(4, material.getVertexConstantsCount());
        assertEquals("/builtins/materials/gltf.fp", material.getFragmentProgram());
    }

    // Verifies every alpha mode keeps the model tag for existing render-script compatibility.
    @Test
    public void preservesModelTagForAllAlphaModes() {
        Modelimporter.Material source = new Modelimporter.Material();
        for (Modelimporter.AlphaMode mode : new Modelimporter.AlphaMode[] {
                Modelimporter.AlphaMode.ALPHA_MODE_OPAQUE,
                Modelimporter.AlphaMode.ALPHA_MODE_MASK,
                Modelimporter.AlphaMode.ALPHA_MODE_BLEND}) {
            source.alphaMode = mode;
            MaterialDesc material = GltfMaterialResource.createMaterialDesc(source);
            assertEquals(1, material.getTagsCount());
            assertEquals("model", material.getTags(0));
        }
    }
}
