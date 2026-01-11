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

// generated, do not edit

package com.dynamo.bob.pipeline;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.lang.reflect.Method;

public class Shaderc {
    public enum ShaderLanguage {
        SHADER_LANGUAGE_NONE(0),
        SHADER_LANGUAGE_GLSL(1),
        SHADER_LANGUAGE_HLSL(2),
        SHADER_LANGUAGE_MSL(3),
        SHADER_LANGUAGE_SPIRV(4);
        private final int value;
        private ShaderLanguage(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public ShaderLanguage fromValue(int value) throws IllegalArgumentException {
            for (ShaderLanguage e : ShaderLanguage.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to ShaderLanguage: %d", value) );
        }
    };

    public enum ShaderStage {
        SHADER_STAGE_VERTEX(1),
        SHADER_STAGE_FRAGMENT(2),
        SHADER_STAGE_COMPUTE(4);
        private final int value;
        private ShaderStage(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public ShaderStage fromValue(int value) throws IllegalArgumentException {
            for (ShaderStage e : ShaderStage.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to ShaderStage: %d", value) );
        }
    };

    public enum BaseType {
        BASE_TYPE_UNKNOWN(0),
        BASE_TYPE_VOID(1),
        BASE_TYPE_BOOLEAN(2),
        BASE_TYPE_INT8(3),
        BASE_TYPE_UINT8(4),
        BASE_TYPE_INT16(5),
        BASE_TYPE_UINT16(6),
        BASE_TYPE_INT32(7),
        BASE_TYPE_UINT32(8),
        BASE_TYPE_INT64(9),
        BASE_TYPE_UINT64(10),
        BASE_TYPE_ATOMIC_COUNTER(11),
        BASE_TYPE_FP16(12),
        BASE_TYPE_FP32(13),
        BASE_TYPE_FP64(14),
        BASE_TYPE_STRUCT(15),
        BASE_TYPE_IMAGE(16),
        BASE_TYPE_SAMPLED_IMAGE(17),
        BASE_TYPE_SAMPLER(18),
        BASE_TYPE_ACCELERATION_STRUCTURE(19);
        private final int value;
        private BaseType(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public BaseType fromValue(int value) throws IllegalArgumentException {
            for (BaseType e : BaseType.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to BaseType: %d", value) );
        }
    };

    public enum DimensionType {
        DIMENSION_TYPE_1D(0),
        DIMENSION_TYPE_2D(1),
        DIMENSION_TYPE_3D(2),
        DIMENSION_TYPE_CUBE(3),
        DIMENSION_TYPE_RECT(4),
        DIMENSION_TYPE_BUFFER(5),
        DIMENSION_TYPE_SUBPASS_DATA(6);
        private final int value;
        private DimensionType(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public DimensionType fromValue(int value) throws IllegalArgumentException {
            for (DimensionType e : DimensionType.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to DimensionType: %d", value) );
        }
    };

    public enum ImageStorageType {
        IMAGE_STORAGE_TYPE_UNKNOWN(0),
        IMAGE_STORAGE_TYPE_RGBA32F(1),
        IMAGE_STORAGE_TYPE_RGBA16F(2),
        IMAGE_STORAGE_TYPE_R32F(3),
        IMAGE_STORAGE_TYPE_RGBA8(4),
        IMAGE_STORAGE_TYPE_RGBA8_SNORM(5),
        IMAGE_STORAGE_TYPE_RG32F(6),
        IMAGE_STORAGE_TYPE_RG16F(7),
        IMAGE_STORAGE_TYPE_R11F_G11F_B10F(8),
        IMAGE_STORAGE_TYPE_R16F(9),
        IMAGE_STORAGE_TYPE_RGBA16(10),
        IMAGE_STORAGE_TYPE_RGB10A2(11),
        IMAGE_STORAGE_TYPE_RG16(12),
        IMAGE_STORAGE_TYPE_RG8(13),
        IMAGE_STORAGE_TYPE_R16(14),
        IMAGE_STORAGE_TYPE_R8(15),
        IMAGE_STORAGE_TYPE_RGBA16_SNORM(16),
        IMAGE_STORAGE_TYPE_RG16_SNORM(17),
        IMAGE_STORAGE_TYPE_RG8_SNORM(18),
        IMAGE_STORAGE_TYPE_R16_SNORM(19),
        IMAGE_STORAGE_TYPE_R8_SNORM(20),
        IMAGE_STORAGE_TYPE_RGBA32I(21),
        IMAGE_STORAGE_TYPE_RGBA16I(22),
        IMAGE_STORAGE_TYPE_RGBA8I(23),
        IMAGE_STORAGE_TYPE_R32I(24),
        IMAGE_STORAGE_TYPE_RG32I(25),
        IMAGE_STORAGE_TYPE_RG16I(26),
        IMAGE_STORAGE_TYPE_RG8I(27),
        IMAGE_STORAGE_TYPE_R16I(28),
        IMAGE_STORAGE_TYPE_R8I(29),
        IMAGE_STORAGE_TYPE_RGBA32UI(30),
        IMAGE_STORAGE_TYPE_RGBA16UI(31),
        IMAGE_STORAGE_TYPE_RGBA8UI(32),
        IMAGE_STORAGE_TYPE_R32UI(33),
        IMAGE_STORAGE_TYPE_RGb10a2UI(34),
        IMAGE_STORAGE_TYPE_RG32UI(35),
        IMAGE_STORAGE_TYPE_RG16UI(36),
        IMAGE_STORAGE_TYPE_RG8UI(37),
        IMAGE_STORAGE_TYPE_R16UI(38),
        IMAGE_STORAGE_TYPE_R8UI(39),
        IMAGE_STORAGE_TYPE_R64UI(40),
        IMAGE_STORAGE_TYPE_R64I(41);
        private final int value;
        private ImageStorageType(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public ImageStorageType fromValue(int value) throws IllegalArgumentException {
            for (ImageStorageType e : ImageStorageType.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to ImageStorageType: %d", value) );
        }
    };

    public enum ImageAccessQualifier {
        IMAGE_ACCESS_QUALIFIER_READ_ONLY(0),
        IMAGE_ACCESS_QUALIFIER_WRITE_ONLY(1),
        IMAGE_ACCESS_QUALIFIER_READ_WRITE(2);
        private final int value;
        private ImageAccessQualifier(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public ImageAccessQualifier fromValue(int value) throws IllegalArgumentException {
            for (ImageAccessQualifier e : ImageAccessQualifier.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to ImageAccessQualifier: %d", value) );
        }
    };

    public static class ShaderCompilerOptions {
        public int version = 0;
        public String entryPoint;
        public byte removeUnusedVariables = 0;
        public byte no420PackExtension = 0;
        public byte glslEmitUboAsPlainUniforms = 0;
        public byte glslEs = 0;
    };
    public static class ResourceType {
        public BaseType baseType = BaseType.BASE_TYPE_UNKNOWN;
        public DimensionType dimensionType = DimensionType.DIMENSION_TYPE_1D;
        public ImageStorageType imageStorageType = ImageStorageType.IMAGE_STORAGE_TYPE_UNKNOWN;
        public ImageAccessQualifier imageAccessQualifier = ImageAccessQualifier.IMAGE_ACCESS_QUALIFIER_READ_ONLY;
        public BaseType imageBaseType = BaseType.BASE_TYPE_UNKNOWN;
        public int typeIndex = 0;
        public int vectorSize = 0;
        public int columnCount = 0;
        public int arraySize = 0;
        public boolean useTypeIndex = false;
        public boolean imageIsArrayed = false;
        public boolean imageIsStorage = false;
    };
    public static class ResourceMember {
        public String name;
        public long nameHash = 0;
        public ResourceType type;
        public int offset = 0;
    };
    public static class ResourceTypeInfo {
        public String name;
        public long nameHash = 0;
        public ResourceMember[] members;
    };
    public static class ShaderResource {
        public String name;
        public long nameHash = 0;
        public String instanceName;
        public long instanceNameHash = 0;
        public ResourceType type;
        public int id = 0;
        public int blockSize = 0;
        public byte location = 0;
        public byte binding = 0;
        public byte set = 0;
        public byte stageFlags = 0;
    };
    public static class ShaderReflection {
        public ShaderResource[] inputs;
        public ShaderResource[] outputs;
        public ShaderResource[] uniformBuffers;
        public ShaderResource[] storageBuffers;
        public ShaderResource[] textures;
        public ResourceTypeInfo[] types;
    };
    public static class HLSLResourceMapping {
        public String name;
        public long nameHash = 0;
        public byte shaderResourceSet = 0;
        public byte shaderResourceBinding = 0;
    };
    public static class ShaderCompileResult {
        public byte[] data;
        public String lastError;
        public HLSLResourceMapping[] hLSLResourceMappings;
        public byte hLSLNumWorkGroupsId = 0;
    };
}

