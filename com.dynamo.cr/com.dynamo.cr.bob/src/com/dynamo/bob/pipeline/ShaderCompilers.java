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

package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;
import com.dynamo.graphics.proto.Graphics.PlatformProfile.OS;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;

public class ShaderCompilers {
    public static final String SHADER_ADAPTERS_OPTION = "shader-adapters";
    public static final String SHADER_ADAPTER_OPENGL = "opengl";
    public static final String SHADER_ADAPTER_OPENGLES = "opengles";
    public static final String SHADER_ADAPTER_VULKAN = "vulkan";
    public static final String SHADER_ADAPTER_METAL = "metal";
    public static final String SHADER_ADAPTER_WEBGPU = "webgpu";
    public static final String SHADER_ADAPTER_DX12 = "dx12";

    private enum GraphicsAdapter {
        OPENGL(SHADER_ADAPTER_OPENGL),
        OPENGLES(SHADER_ADAPTER_OPENGLES),
        VULKAN(SHADER_ADAPTER_VULKAN),
        METAL(SHADER_ADAPTER_METAL),
        WEBGPU(SHADER_ADAPTER_WEBGPU),
        DX12(SHADER_ADAPTER_DX12);

        private final String optionName;

        GraphicsAdapter(String optionName) {
            this.optionName = optionName;
        }

        static GraphicsAdapter fromOptionName(String optionName) {
            for (GraphicsAdapter adapter : values()) {
                if (adapter.optionName.equals(optionName)) {
                    return adapter;
                }
            }
            return null;
        }
    }

    private static boolean usesGlesShaderLanguages(Platform platform) {
        return platform.matchesOS(OS.OS_ID_ANDROID) ||
               platform.matchesOS(OS.OS_ID_WEB) ||
               platform.matchesOS(OS.OS_ID_IOS) ||
               (platform.isLinux() && platform.getArch().equals("arm64"));
    }

    private static boolean isDesktopOpenGLPlatform(Platform platform) {
        return platform.isWindows() ||
               platform.isMacOS() ||
               (platform.isLinux() && !usesGlesShaderLanguages(platform));
    }

    private static LinkedHashSet<GraphicsAdapter> getDefaultShaderAdapters(Platform platform) {
        LinkedHashSet<GraphicsAdapter> adapters = new LinkedHashSet<>();
        if (platform.isMacOS()) {
            adapters.add(GraphicsAdapter.VULKAN);
        } else if (platform.matchesOS(OS.OS_ID_ANDROID)) {
            adapters.add(GraphicsAdapter.VULKAN);
            adapters.add(GraphicsAdapter.OPENGLES);
        } else if (platform.matchesOS(OS.OS_ID_SWITCH)) {
            adapters.add(GraphicsAdapter.VULKAN);
        } else if (platform.matchesOS(OS.OS_ID_XBOX)) {
            adapters.add(GraphicsAdapter.DX12);
        } else if (usesGlesShaderLanguages(platform)) {
            adapters.add(GraphicsAdapter.OPENGLES);
        } else if (isDesktopOpenGLPlatform(platform)) {
            adapters.add(GraphicsAdapter.OPENGL);
        }
        return adapters;
    }

    private static Set<GraphicsAdapter> getShaderAdaptersFromOptions(Platform platform, IShaderCompiler.CompileOptions compileOptions) {
        LinkedHashSet<GraphicsAdapter> adapters = new LinkedHashSet<>();
        if (compileOptions.shaderAdapters == null) {
            return getDefaultShaderAdapters(platform);
        }
        for (String adapterName : compileOptions.shaderAdapters.split(",")) {
            GraphicsAdapter adapter = GraphicsAdapter.fromOptionName(adapterName);
            if (adapter != null) {
                adapters.add(adapter);
            }
        }
        return adapters;
    }

    private static int readU32LE(byte[] data, int offset) {
        return (data[offset] & 0xff) |
               ((data[offset + 1] & 0xff) << 8) |
               ((data[offset + 2] & 0xff) << 16) |
               ((data[offset + 3] & 0xff) << 24);
    }

    private static boolean chunkFourCCEquals(byte[] data, int offset, String fourCC) {
        return data[offset] == (byte) fourCC.charAt(0) &&
               data[offset + 1] == (byte) fourCC.charAt(1) &&
               data[offset + 2] == (byte) fourCC.charAt(2) &&
               data[offset + 3] == (byte) fourCC.charAt(3);
    }

    private static boolean containsAscii(byte[] data, int begin, int end, String needle) {
        if (begin < 0 || end > data.length || end - begin < needle.length()) {
            return false;
        }
        for (int i = begin; i <= end - needle.length(); ++i) {
            boolean matches = true;
            for (int j = 0; j < needle.length(); ++j) {
                if (data[i + j] != (byte) needle.charAt(j)) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                return true;
            }
        }
        return false;
    }

    private static boolean hLSLShaderHasSVPositionInput(byte[] shaderData) {
        // D3DCompiler stores input signatures in DXBC ISGN/ISG1 chunks. We only
        // need to know whether the pixel shader consumes gl_FragCoord, which is
        // represented as an SV_Position input semantic in that chunk.
        if (shaderData == null || shaderData.length < 32 ||
            !chunkFourCCEquals(shaderData, 0, "DXBC")) {
            return false;
        }

        int chunkCount = readU32LE(shaderData, 28);
        long chunkOffsetsEnd = 32L + (long) chunkCount * 4L;
        if (chunkCount < 0 || chunkOffsetsEnd > shaderData.length) {
            return false;
        }

        for (int i = 0; i < chunkCount; ++i) {
            int chunkOffset = readU32LE(shaderData, 32 + i * 4);
            if (chunkOffset < 0 || chunkOffset + 8 > shaderData.length) {
                continue;
            }
            if (!chunkFourCCEquals(shaderData, chunkOffset, "ISGN") &&
                !chunkFourCCEquals(shaderData, chunkOffset, "ISG1")) {
                continue;
            }

            int chunkSize = readU32LE(shaderData, chunkOffset + 4);
            int chunkBegin = chunkOffset + 8;
            long chunkEnd = (long) chunkBegin + (long) chunkSize;
            if (chunkSize < 0 || chunkEnd > shaderData.length) {
                continue;
            }
            if (containsAscii(shaderData, chunkBegin, (int) chunkEnd, "SV_Position")) {
                return true;
            }
        }

        return false;
    }
    public static class CommonShaderCompiler implements IShaderCompiler {
        private final Platform platform;
        private final ShaderCompilePipeline.Options baseOptions;

        public CommonShaderCompiler(Platform platform) {
            this(platform, null);
        }

        public CommonShaderCompiler(Platform platform, ShaderCompilePipeline.Options baseOptions) {
            this.platform = platform;
            this.baseOptions = baseOptions;
        }

        private void addGlslLanguages(Set<ShaderDesc.Language> shaderLanguages, boolean isComputeType) {
            if (isComputeType) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM430);
            } else {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM330);
            }
        }

        private void addGlesLanguages(Set<ShaderDesc.Language> shaderLanguages, boolean isComputeType, CompileOptions compileOptions) {
            if (isComputeType) {
                return;
            }
            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
            if (!compileOptions.excludeGlesSm100) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
            }
        }

        private Set<ShaderDesc.Language> getPlatformShaderLanguages(boolean isComputeType, CompileOptions compileOptions) {
            Set<ShaderDesc.Language> shaderLanguages = new LinkedHashSet<>();
            for (GraphicsAdapter adapter : getShaderAdaptersFromOptions(platform, compileOptions)) {
                switch (adapter) {
                    case OPENGL -> {
                        if (usesGlesShaderLanguages(platform)) {
                            addGlesLanguages(shaderLanguages, isComputeType, compileOptions);
                        } else {
                            addGlslLanguages(shaderLanguages, isComputeType);
                        }
                    }
                    case OPENGLES -> addGlesLanguages(shaderLanguages, isComputeType, compileOptions);
                    case VULKAN -> {
                        if (!platform.matchesOS(OS.OS_ID_WEB)) {
                            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
                        }
                    }
                    case METAL -> {
                        if (platform.isMacOS() || platform.matchesOS(OS.OS_ID_IOS)) {
                            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_MSL_22);
                        }
                    }
                    case WEBGPU -> shaderLanguages.add(ShaderDesc.Language.LANGUAGE_WGSL);
                    case DX12 -> {
                        if (platform.isWindows() || platform.matchesOS(OS.OS_ID_XBOX)) {
                            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_HLSL_51);
                        }
                    }
                }
            }

            return shaderLanguages;
        }

        private static void validateModules(ArrayList<ShaderCompilePipeline.ShaderModuleDesc> descs) throws CompileExceptionError{
            if (descs.isEmpty())
                throw new CompileExceptionError("No shader modules");

            int vsCount=0, fsCount=0, computeCount=0;
            for (ShaderCompilePipeline.ShaderModuleDesc desc : descs) {
                switch(desc.type) {
                    case SHADER_TYPE_COMPUTE -> computeCount++;
                    case SHADER_TYPE_VERTEX -> vsCount++;
                    case SHADER_TYPE_FRAGMENT -> fsCount++;
                }
            }

            if (computeCount > 0 && (vsCount > 0 || fsCount > 0))
                throw new CompileExceptionError("Can't match compute with graphics modules");
        }

        private static boolean shaderLanguageRequiresSplitTextureSamplers(ShaderDesc.Language shaderLanguage) {
            return shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL ||
                   shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_51 ||
                   shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_50;
        }

        public ShaderProgramBuilder.ShaderCompileResult compile(ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderModules, String resourceOutputPath, CompileOptions compileOptions) throws IOException, CompileExceptionError {

            // We need this for e.g. Win32 when creating the root signature bindings, to get a deterministic order.
            shaderModules.sort(Comparator.comparingInt(m -> m.type.getNumber()));
            validateModules(shaderModules);

            boolean isComputeType = shaderModules.get(0).type == ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE;
            Set<ShaderDesc.Language> shaderLanguages = getPlatformShaderLanguages(isComputeType, compileOptions);
            assert shaderLanguages != null;

            // Used for tests, merge in potentially unsupported languages here.
            shaderLanguages.addAll(compileOptions.forceIncludeShaderLanguages);

            boolean hasHlslTarget = shaderLanguages.contains(ShaderDesc.Language.LANGUAGE_HLSL_50) ||
                                    shaderLanguages.contains(ShaderDesc.Language.LANGUAGE_HLSL_51);
            boolean hasWgslTarget = shaderLanguages.contains(ShaderDesc.Language.LANGUAGE_WGSL);

            ShaderCompilePipeline.Options opts = new ShaderCompilePipeline.Options();
            if (this.baseOptions != null) {
                opts.externalToolPath = this.baseOptions.externalToolPath;
                opts.externalToolArgs = this.baseOptions.externalToolArgs;
            }
            // Keep combined samplers for HLSL by default. SPIRV-Cross can emit valid HLSL from combined samplers,
            // while pre-splitting GLSL uniforms can break shaders that pass sampler2D into helper functions.
            opts.splitTextureSamplers = compileOptions.forceSplitSamplers || hasWgslTarget;
            opts.remapVertexFragmentIOForHLSL = hasHlslTarget;
            opts.targetPlatform = this.platform;
            opts.glslEsDefaultFloatPrecision = compileOptions.glslEsDefaultFloatPrecision;
            opts.glslEsDefaultIntPrecision = compileOptions.glslEsDefaultIntPrecision;

            for (ShaderDesc.Language shaderLanguage : shaderLanguages) {
                opts.splitTextureSamplers |= shaderLanguageRequiresSplitTextureSamplers(shaderLanguage) &&
                                             shaderLanguage != ShaderDesc.Language.LANGUAGE_HLSL_51 &&
                                             shaderLanguage != ShaderDesc.Language.LANGUAGE_HLSL_50;
            }

            ShaderCompilePipeline pipeline = null;
            try {
                pipeline = ShaderProgramBuilder.newShaderPipeline(resourceOutputPath, shaderModules, opts);
                ArrayList<ShaderProgramBuilder.ShaderBuildResult> shaderBuildResults = new ArrayList<>();

                HashMap<ShaderDesc.ShaderType, Boolean> shaderTypeKeys = new HashMap<>();
                Shaderc.HLSLRootSignature hlslRootSignature = null;

            for (ShaderDesc.Language shaderLanguage : shaderLanguages) {

                boolean arrayTextureFallbackRequired = ShaderUtil.VariantTextureArrayFallback.isRequired(shaderLanguage);

                boolean create_hlsl_root_signature = shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_51;
                List<Shaderc.ShaderCompileResult> compiled_shaders = new ArrayList<>();
                List<ShaderDesc.ShaderType> compiled_shader_types = new ArrayList<>();
                List<Boolean> compiled_shader_variant_flags = new ArrayList<>();

                for (ShaderCompilePipeline.ShaderModuleDesc shaderModule : shaderModules) {

                    boolean variantTextureArray = false;
                    Shaderc.ShaderCompileResult crossCompileResult = pipeline.crossCompile(shaderModule.type, shaderLanguage);

                    if (!shaderTypeKeys.containsKey(shaderModule.type)) {
                        shaderTypeKeys.put(shaderModule.type, true);
                    }

                    if (arrayTextureFallbackRequired) {
                        ShaderUtil.Common.GLSLCompileResult variantCompileResult = ShaderUtil.VariantTextureArrayFallback.transform(new String(crossCompileResult.data), compileOptions.maxPageCount);
                        if (variantCompileResult != null && variantCompileResult.arraySamplers.length > 0) {
                            crossCompileResult.data = variantCompileResult.source.getBytes();
                            variantTextureArray = true;
                        }
                    }

                    compiled_shaders.add(crossCompileResult);
                    compiled_shader_types.add(shaderModule.type);
                    compiled_shader_variant_flags.add(variantTextureArray);
                }

                boolean hLSLMoveSVPositionToFront = false;
                if (create_hlsl_root_signature && !isComputeType && compiled_shaders.size() > 1) {
                    int fragmentShaderIndex = compiled_shader_types.indexOf(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT);
                    if (fragmentShaderIndex >= 0 && hLSLShaderHasSVPositionInput(compiled_shaders.get(fragmentShaderIndex).data)) {
                        hLSLMoveSVPositionToFront = true;
                        for (int i = 0; i < compiled_shaders.size(); ++i) {
                            Shaderc.ShaderCompileResult recompiledShader = pipeline.crossCompileWithHLSLSVPositionFirst(compiled_shader_types.get(i), shaderLanguage);
                            compiled_shaders.set(i, recompiledShader);
                        }
                    }
                }

                if (create_hlsl_root_signature) {
                    hlslRootSignature = pipeline.createRootSignature(shaderLanguage, compiled_shaders);
                    if (hlslRootSignature == null) {
                        throw new CompileExceptionError("Failed to create HLSL root signature: native merge returned null");
                    }
                    if (hlslRootSignature.lastError != null && !hlslRootSignature.lastError.isEmpty()) {
                        throw new CompileExceptionError("Failed to create HLSL root signature: " + hlslRootSignature.lastError);
                    }

                    // Xbox precompiled validation compares runtime root signature against each emplaced shader signature.
                    // Recompile all stages with the merged signature so both blobs carry the same emplaced signature.
                    boolean recompileWithMergedRootSignature = platform != Platform.X86Win32 && platform != Platform.X86_64Win32;
                    if (recompileWithMergedRootSignature && compiled_shaders.size() > 1 && hlslRootSignature.hLSLRootSignature != null && hlslRootSignature.hLSLRootSignature.length > 0) {
                        String mergedRootSignatureText = ShadercJni.HLSLRootSignatureToString(hlslRootSignature.hLSLRootSignature);
                        if (mergedRootSignatureText == null || mergedRootSignatureText.isEmpty()) {
                            throw new CompileExceptionError("Failed to convert merged HLSL root signature to text for shared-root-signature recompile");
                        }
                        if (platform == Platform.X86Win32 || platform == Platform.X86_64Win32) {
                            mergedRootSignatureText = Win32ShaderCompiler.ensureInputAssemblerRootFlag(mergedRootSignatureText);
                        }

                        for (int i = 0; i < shaderModules.size(); ++i) {
                            ShaderCompilePipeline.ShaderModuleDesc shaderModule = shaderModules.get(i);
                            Shaderc.ShaderCompileResult recompiledShader = pipeline.crossCompileWithRootSignature(shaderModule.type, shaderLanguage, mergedRootSignatureText, hLSLMoveSVPositionToFront);
                            recompiledShader.hLSLRootSignature = hlslRootSignature.hLSLRootSignature;
                            compiled_shaders.set(i, recompiledShader);
                        }

                        hlslRootSignature = pipeline.createRootSignature(shaderLanguage, compiled_shaders);
                        if (hlslRootSignature == null) {
                            throw new CompileExceptionError("Failed to create HLSL root signature after shared-root-signature recompile");
                        }
                        if (hlslRootSignature.lastError != null && !hlslRootSignature.lastError.isEmpty()) {
                            throw new CompileExceptionError("Failed to create HLSL root signature after shared-root-signature recompile: " + hlslRootSignature.lastError);
                        }
                    }
                }

                // Build shader descs after root signature merge so any patched HLSL blobs are preserved.
                for (int i = 0; i < compiled_shaders.size(); ++i) {
                    Shaderc.ShaderCompileResult compiledShader = compiled_shaders.get(i);
                    ShaderDesc.ShaderType compiledShaderType = compiled_shader_types.get(i);
                    boolean variantTextureArray = compiled_shader_variant_flags.get(i);

                    ShaderDesc.Shader.Builder builder = ShaderProgramBuilder.makeShaderBuilder(compiledShader, shaderLanguage, compiledShaderType);
                    shaderBuildResults.add(new ShaderProgramBuilder.ShaderBuildResult(builder));

                    if (variantTextureArray) {
                        builder.setVariantTextureArray(true);
                    }
                }
            }

            ShaderProgramBuilder.ShaderCompileResult compileResult = new ShaderProgramBuilder.ShaderCompileResult();
            compileResult.shaderBuildResults = shaderBuildResults;

            for(ShaderDesc.ShaderType type : shaderTypeKeys.keySet()) {
                compileResult.reflectors.add(pipeline.getReflectionData(type));
            }

            compileResult.hlslRootSignature = hlslRootSignature != null ? hlslRootSignature.hLSLRootSignature : null;

                return compileResult;
            } finally {
                if (pipeline != null) {
                    ShaderCompilePipeline.destroyShaderPipeline(pipeline);
                }
            }
        }
    }

    public static ArrayList<ShaderDesc.Language> GetSupportedOpenGLVersionsForPlatform(Platform platform) {
        ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<>();
        if (platform == Platform.Arm64MacOS || platform == Platform.X86_64MacOS) {
            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM330);
        } else if (platform == Platform.Arm64Ios || platform == Platform.X86_64Ios) {
            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
        } else if (platform == Platform.X86Win32 || platform == Platform.X86_64Win32 || platform == Platform.X86_64Linux) {
            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM330);
            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM430); // Compute
        } else if (platform == Platform.Arm64Linux || platform == Platform.Armv7Android || platform == Platform.Arm64Android ||
                platform == Platform.WasmWeb || platform == Platform.WasmPthreadWeb) {
            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
        }
        return shaderLanguages;
    }
}
