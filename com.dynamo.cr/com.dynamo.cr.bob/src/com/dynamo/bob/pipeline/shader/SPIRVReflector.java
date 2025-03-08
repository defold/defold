// Copyright 2020-2025 The Defold Foundation
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

package com.dynamo.bob.pipeline.shader;

import com.dynamo.bob.pipeline.Shaderc;
import com.dynamo.bob.pipeline.ShadercJni;
import com.dynamo.graphics.proto.Graphics;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;

import java.io.IOException;
import java.util.*;

public class SPIRVReflector {

    private final Shaderc.ShaderReflection reflection;
    private final Graphics.ShaderDesc.ShaderType shaderStage;

    public SPIRVReflector() {
        reflection = new Shaderc.ShaderReflection();
        shaderStage = null;
    }

    public SPIRVReflector(long ctx, Graphics.ShaderDesc.ShaderType shaderStage) {
        this.reflection = ShadercJni.GetReflection(ctx);
        this.shaderStage = shaderStage;
    }

    private static class SortBindingsComparator implements Comparator<Shaderc.ShaderResource> {
        public int compare(Shaderc.ShaderResource a, Shaderc.ShaderResource b) {
            return a.binding - b.binding;
        }
    }

    private static class SortLocationsComparator implements Comparator<Shaderc.ShaderResource> {
        public int compare(Shaderc.ShaderResource a, Shaderc.ShaderResource b) {
            return a.location - b.location;
        }
    }

    public Graphics.ShaderDesc.ShaderType getShaderStage() {
        return this.shaderStage;
    }

    public ArrayList<Shaderc.ResourceTypeInfo> getTypes() {
        if (this.reflection.types == null)
            return new ArrayList<>();
        return new ArrayList<>(Arrays.asList(this.reflection.types));
    }

    public ArrayList<Shaderc.ShaderResource> getUBOs() {
        if (reflection.uniformBuffers == null)
            return new ArrayList<>();
        ArrayList<Shaderc.ShaderResource> ubos = new ArrayList<>(Arrays.asList(reflection.uniformBuffers));
        ubos.sort(new SortBindingsComparator());
        return ubos;
    }

    public ArrayList<Shaderc.ShaderResource> getSsbos() {
        if (reflection.storageBuffers == null)
            return new ArrayList<>();
        ArrayList<Shaderc.ShaderResource> ssbos = new ArrayList<>(Arrays.asList(reflection.storageBuffers));
        ssbos.sort(new SortBindingsComparator());
        return ssbos;
    }

    public ArrayList<Shaderc.ShaderResource> getTextures() {

        if (reflection.textures == null)
            return new ArrayList<>();

        ArrayList<Shaderc.ShaderResource> textures = new ArrayList<>(Arrays.asList(this.reflection.textures));
        textures.sort(new SortBindingsComparator());
        return textures;
    }

    public ArrayList<Shaderc.ShaderResource> getInputs() {
        if (reflection.inputs == null)
            return new ArrayList<>();
        ArrayList<Shaderc.ShaderResource> inputs = new ArrayList<>(Arrays.asList(this.reflection.inputs));
        inputs.sort(new SortLocationsComparator());
        return inputs;
    }

    public ArrayList<Shaderc.ShaderResource> getOutputs() {
        if (reflection.outputs == null)
            return new ArrayList<>();
        ArrayList<Shaderc.ShaderResource> outputs = new ArrayList<>(Arrays.asList(this.reflection.outputs));
        outputs.sort(new SortLocationsComparator());
        return outputs;
    }
}
