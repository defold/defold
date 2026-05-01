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

package com.dynamo.bob.pipeline.shader;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.Shaderc;
import com.dynamo.bob.pipeline.ShadercJni;
import com.dynamo.graphics.proto.Graphics;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;

import java.io.IOException;
import java.lang.reflect.Array;
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

    private static Shaderc.ResourceTypeInfo getResourceTypeInfo(ArrayList<Shaderc.ResourceTypeInfo> types, String typeName) throws CompileExceptionError {
        for (Shaderc.ResourceTypeInfo type : types) {
            if (type.name.equals(typeName)) {
                return type;
            }
        }
        return null;
    }

    public static boolean AreResourceTypesEqual(SPIRVReflector reflectionA, SPIRVReflector reflectionB, String typeName) throws CompileExceptionError {
        Shaderc.ResourceTypeInfo typeA = getResourceTypeInfo(reflectionA.getTypes(), typeName);
        Shaderc.ResourceTypeInfo typeB = getResourceTypeInfo(reflectionB.getTypes(), typeName);

        if (typeA == null || typeB == null)
            return false;
        if (!typeA.name.equals(typeB.name))
            return false;
        if (typeA.members.length != typeB.members.length)
            return false;
        for (int i=0; i < typeA.members.length; i++) {
            if (!typeA.members[i].name.equals(typeB.members[i].name))
                return false;
            // The actual type index might differ, so we can't compare those.
            if (typeA.members[i].type.useTypeIndex != typeB.members[i].type.useTypeIndex)
                return false;
            if (typeA.members[i].type.useTypeIndex)  {
                // Follow type information for sub types recursively
                Shaderc.ResourceTypeInfo subTypeA = reflectionA.getTypes().get(typeA.members[i].type.typeIndex);
                if (!AreResourceTypesEqual(reflectionA, reflectionB, subTypeA.name))
                    return false;
            } else if (typeA.members[i].type.baseType != typeB.members[i].type.baseType)
                return false;
        }
        return true;
    }

    public static boolean AreResourceTypesEqual(SPIRVReflector reflectionA, SPIRVReflector reflectionB, Shaderc.ShaderResource resourceA, Shaderc.ShaderResource resourceB) throws CompileExceptionError {
        if (resourceA.type.useTypeIndex && resourceB.type.useTypeIndex) {
            return AreResourceTypesEqual(reflectionA, reflectionB, resourceA.name);
        }
        return resourceA.type.baseType == resourceB.type.baseType &&
                resourceA.type.dimensionType == resourceB.type.dimensionType &&
                resourceA.type.imageStorageType == resourceB.type.imageStorageType &&
                resourceA.type.imageAccessQualifier == resourceB.type.imageAccessQualifier &&
                resourceA.type.imageBaseType == resourceB.type.imageBaseType &&
                resourceA.type.typeIndex == resourceB.type.typeIndex &&
                resourceA.type.vectorSize == resourceB.type.vectorSize &&
                resourceA.type.columnCount == resourceB.type.columnCount &&
                resourceA.type.arraySize == resourceB.type.arraySize &&
                resourceA.type.useTypeIndex == resourceB.type.useTypeIndex &&
                resourceA.type.imageIsArrayed == resourceB.type.imageIsArrayed &&
                resourceA.type.imageIsStorage == resourceB.type.imageIsStorage;
    }

    public static boolean CanMergeResources(SPIRVReflector A, SPIRVReflector B, Shaderc.ShaderResource resA, Shaderc.ShaderResource resB) throws CompileExceptionError {
        boolean bindingsMismatch = resA.binding != resB.binding;
        boolean setMisMatch = resA.set != resB.set;
        boolean typesMatch = AreResourceTypesEqual(A, B, resA, resB);
        return resA.name.equals(resB.name) && (bindingsMismatch || setMisMatch) && typesMatch;
    }

    private static Shaderc.ShaderResource[] RemoveResourceByNameHashInternal(Shaderc.ShaderResource[] resourcesIn, long nameHash) {
        if (resourcesIn == null) {
            return null;
        }
        ArrayList<Shaderc.ShaderResource> resourcesOut = new ArrayList<>();
        for (Shaderc.ShaderResource shaderResource : resourcesIn) {
            if (shaderResource.nameHash != nameHash) {
                resourcesOut.add(shaderResource);
            }
        }
        return resourcesOut.toArray(new Shaderc.ShaderResource[0]);
    }

    public void removeResourceByNameHash(long nameHash) {
        this.reflection.uniformBuffers = RemoveResourceByNameHashInternal(this.reflection.uniformBuffers, nameHash);
        this.reflection.textures = RemoveResourceByNameHashInternal(this.reflection.textures, nameHash);
        this.reflection.storageBuffers = RemoveResourceByNameHashInternal(this.reflection.storageBuffers, nameHash);
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

    public void addUBO(Shaderc.ShaderResource res) {

        if (reflection.uniformBuffers == null) {
            reflection.uniformBuffers = new Shaderc.ShaderResource[] { res };
        } else {
            ArrayList<Shaderc.ShaderResource> ubos = new ArrayList<>(Arrays.asList(reflection.uniformBuffers));
            ubos.add(res);
            reflection.uniformBuffers = ubos.toArray(new Shaderc.ShaderResource[0]);
        }
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
