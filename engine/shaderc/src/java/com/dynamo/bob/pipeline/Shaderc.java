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
        SHADER_LANGUAGE_HLSL(2);
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
        SHADER_STAGE_COMPUTE(3);
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

    public static class ShaderCompilerOptions {
        public int version = 0;
        public String entryPoint;
        public ShaderStage stage = ShaderStage.SHADER_STAGE_VERTEX;
        public byte removeUnusedVariables = 0;
        public byte no420PackExtension = 0;
        public byte glslEmitUboAsPlainUniforms = 0;
        public byte glslEs = 0;
    };
    public static class ResourceType {
        public BaseType baseType = BaseType.BASE_TYPE_UNKNOWN;
        public int typeIndex = 0;
        public boolean useTypeIndex = false;
    };
    public static class ResourceMember {
        public String name;
        public long nameHash = 0;
        public ResourceType type;
        public int vectorSize = 0;
        public int columnCount = 0;
        public int offset = 0;
    };
    public static class ResourceTypeInfo {
        public String name;
        public long nameHash = 0;
        public ResourceMember members;
        public int memberCount = 0;
    };
    public static class ShaderResource {
        public String name;
        public long nameHash = 0;
        public String instanceName;
        public long instanceNameHash = 0;
        public ResourceType type;
        public int id = 0;
        public byte location = 0;
        public byte binding = 0;
        public byte set = 0;
    };
    public static class ShaderReflection {
        public ShaderResource[] inputs;
        public ShaderResource[] outputs;
        public ShaderResource[] uniformBuffers;
        public ShaderResource[] textures;
        public ResourceTypeInfo[] types;
    };
}

