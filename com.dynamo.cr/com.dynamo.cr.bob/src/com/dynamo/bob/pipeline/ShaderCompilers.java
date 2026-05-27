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
import java.util.Map;
import java.util.Set;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;

public class ShaderCompilers {
    public static final String SHADER_ADAPTERS_OPTION = "shader-adapters";

    private enum GraphicsAdapter {
        OPENGL("opengl"),
        OPENGLES("opengles"),
        VULKAN("vulkan"),
        METAL("metal"),
        WEBGPU("webgpu"),
        DX12("dx12");

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

    private static boolean isMacOS(Platform platform) {
        return platform == Platform.Arm64MacOS || platform == Platform.X86_64MacOS;
    }

    private static boolean isAndroid(Platform platform) {
        return platform == Platform.Armv7Android || platform == Platform.Arm64Android;
    }

    private static boolean isWeb(Platform platform) {
        return platform == Platform.WasmWeb || platform == Platform.WasmPthreadWeb;
    }

    private static boolean isIOS(Platform platform) {
        return platform == Platform.Arm64Ios || platform == Platform.X86_64Ios;
    }

    private static boolean isSwitch(Platform platform) {
        return platform == Platform.Arm64NX64;
    }

    private static boolean isXbox(Platform platform) {
        return platform == Platform.X86_64XBone;
    }

    private static boolean usesGlesShaderLanguages(Platform platform) {
        return isAndroid(platform) || isWeb(platform) || isIOS(platform) || platform == Platform.Arm64Linux;
    }

    private static boolean isDesktopOpenGLPlatform(Platform platform) {
        return platform == Platform.X86Win32 ||
               platform == Platform.X86_64Win32 ||
               platform == Platform.X86_64Linux ||
               isMacOS(platform);
    }

    private static LinkedHashSet<GraphicsAdapter> getDefaultShaderAdapters(Platform platform) {
        LinkedHashSet<GraphicsAdapter> adapters = new LinkedHashSet<>();
        if (isMacOS(platform)) {
            adapters.add(GraphicsAdapter.VULKAN);
        } else if (isAndroid(platform)) {
            adapters.add(GraphicsAdapter.VULKAN);
            adapters.add(GraphicsAdapter.OPENGLES);
        } else if (isSwitch(platform)) {
            adapters.add(GraphicsAdapter.VULKAN);
        } else if (isXbox(platform)) {
            adapters.add(GraphicsAdapter.DX12);
        } else if (usesGlesShaderLanguages(platform)) {
            adapters.add(GraphicsAdapter.OPENGLES);
        } else if (isDesktopOpenGLPlatform(platform)) {
            adapters.add(GraphicsAdapter.OPENGL);
        }
        return adapters;
    }

    @SuppressWarnings("unchecked")
    private static List<String> getManifestContextList(Map<String, Object> platformSettings, String key) {
        Map<String, Object> context = (Map<String, Object>) platformSettings.getOrDefault("context", null);
        if (context == null) {
            return List.of();
        }
        Object value = context.getOrDefault(key, null);
        if (!(value instanceof List<?>)) {
            return List.of();
        }
        return (List<String>) value;
    }

    private static void addAdaptersFromManifestItems(Platform platform, Set<GraphicsAdapter> adapters, List<String> items) {
        for (String item : items) {
            GraphicsAdapter adapter = getAdapterFromManifestItem(platform, item);
            if (adapter != null) {
                adapters.add(adapter);
            }
        }
    }

    private static void removeAdaptersFromManifestItems(Platform platform, Set<GraphicsAdapter> adapters, List<String> items) {
        for (String item : items) {
            GraphicsAdapter adapter = getAdapterFromManifestItem(platform, item);
            if (adapter != null) {
                adapters.remove(adapter);
            }
        }
    }

    private static GraphicsAdapter getAdapterFromManifestItem(Platform platform, String item) {
        if (item == null) {
            return null;
        }

        switch (item) {
            case "GraphicsAdapterVulkan":
                return GraphicsAdapter.VULKAN;
            case "GraphicsAdapterOpenGLES":
                return GraphicsAdapter.OPENGLES;
            case "GraphicsAdapterOpenGL":
                return usesGlesShaderLanguages(platform) ? GraphicsAdapter.OPENGLES : GraphicsAdapter.OPENGL;
            case "GraphicsAdapterMetal":
                return GraphicsAdapter.METAL;
            case "GraphicsAdapterWebGPU":
                return GraphicsAdapter.WEBGPU;
            case "GraphicsAdapterDX12":
                return GraphicsAdapter.DX12;
        }

        String normalized = item;
        if (normalized.startsWith("lib")) {
            normalized = normalized.substring(3);
        }
        if (normalized.endsWith(".lib")) {
            normalized = normalized.substring(0, normalized.length() - 4);
        }

        switch (normalized) {
            case "graphics_vulkan":
            case "platform_vulkan":
            case "dmglfw_vulkan":
            case "MoltenVK":
            case "vulkan":
            case "vulkan-1":
                return GraphicsAdapter.VULKAN;
            case "graphics_opengles":
            case "graphics_gles":
                return GraphicsAdapter.OPENGLES;
            case "graphics":
            case "platform":
            case "dmglfw":
            case "glfw3":
                return usesGlesShaderLanguages(platform) ? GraphicsAdapter.OPENGLES : GraphicsAdapter.OPENGL;
            case "graphics_webgpu":
                return GraphicsAdapter.WEBGPU;
            case "graphics_dx12":
            case "dx12":
                return GraphicsAdapter.DX12;
        }

        return null;
    }

    private static void collectManifestShaderAdapters(Platform platform, Set<GraphicsAdapter> adaptersToAdd, Set<GraphicsAdapter> adaptersToRemove, Map<String, Object> platformSettings) {
        addAdaptersFromManifestItems(platform, adaptersToAdd, getManifestContextList(platformSettings, "symbols"));
        addAdaptersFromManifestItems(platform, adaptersToAdd, getManifestContextList(platformSettings, "libs"));
        addAdaptersFromManifestItems(platform, adaptersToAdd, getManifestContextList(platformSettings, "engineLibs"));

        addAdaptersFromManifestItems(platform, adaptersToRemove, getManifestContextList(platformSettings, "excludeSymbols"));
        addAdaptersFromManifestItems(platform, adaptersToRemove, getManifestContextList(platformSettings, "excludeLibs"));
        addAdaptersFromManifestItems(platform, adaptersToRemove, getManifestContextList(platformSettings, "excludeDynamicLibs"));
    }

    public static String getShaderAdaptersOption(Platform platform, List<Map<String, Object>> platformsSettings) {
        LinkedHashSet<GraphicsAdapter> adapters = getDefaultShaderAdapters(platform);
        LinkedHashSet<GraphicsAdapter> adaptersToAdd = new LinkedHashSet<>();
        LinkedHashSet<GraphicsAdapter> adaptersToRemove = new LinkedHashSet<>();
        for (Map<String, Object> platformSettings : platformsSettings) {
            collectManifestShaderAdapters(platform, adaptersToAdd, adaptersToRemove, platformSettings);
        }
        adapters.addAll(adaptersToAdd);
        adapters.removeAll(adaptersToRemove);

        ArrayList<String> adapterNames = new ArrayList<>();
        for (GraphicsAdapter adapter : adapters) {
            adapterNames.add(adapter.optionName);
        }
        return String.join(",", adapterNames);
    }

    private static Set<GraphicsAdapter> getShaderAdaptersFromOptions(Platform platform, IShaderCompiler.CompileOptions compileOptions) {
        LinkedHashSet<GraphicsAdapter> adapters = new LinkedHashSet<>();
        if (compileOptions.shaderAdapters == null || compileOptions.shaderAdapters.isEmpty()) {
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

        private static void copyOptionalPipelineOption(ShaderCompilePipeline.Options dst, ShaderCompilePipeline.Options src, String fieldName) {
            try {
                dst.getClass().getField(fieldName).set(dst, src.getClass().getField(fieldName).get(src));
            } catch (ReflectiveOperationException e) {
                // Console branches may add fields to pipeline options, e.g. external shader compiler settings.
            }
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
                        if (!isWeb(platform)) {
                            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
                        }
                    }
                    case METAL -> {
                        if (isMacOS(platform) || isIOS(platform)) {
                            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_MSL_22);
                        }
                    }
                    case WEBGPU -> shaderLanguages.add(ShaderDesc.Language.LANGUAGE_WGSL);
                    case DX12 -> {
                        if (platform == Platform.X86_64Win32 || isXbox(platform)) {
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

        public ShaderProgramBuilder.ShaderCompileResult compile(ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderModules, String resourceOutputPath, CompileOptions compileOptions) throws IOException, CompileExceptionError {

            // We need this for e.g. Win32 when creating the root signature bindings, to get a deterministic order.
            shaderModules.sort(Comparator.comparingInt(m -> m.type.getNumber()));

            boolean isComputeType = shaderModules.get(0).type == ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE;

            ShaderCompilePipeline.Options opts = new ShaderCompilePipeline.Options();
            if (this.baseOptions != null) {
                copyOptionalPipelineOption(opts, this.baseOptions, "externalToolPath");
                copyOptionalPipelineOption(opts, this.baseOptions, "externalToolArgs");
            }
            opts.splitTextureSamplers = compileOptions.forceSplitSamplers;
            opts.glslEsDefaultFloatPrecision = compileOptions.glslEsDefaultFloatPrecision;
            opts.glslEsDefaultIntPrecision = compileOptions.glslEsDefaultIntPrecision;

            for (ShaderDesc.Language shaderLanguage : compileOptions.forceIncludeShaderLanguages) {
                boolean isHLSL = shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_51 || shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_50;

                opts.splitTextureSamplers |= isHLSL || shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL;
            }

            ShaderCompilePipeline pipeline = ShaderProgramBuilder.newShaderPipeline(resourceOutputPath, shaderModules, opts);
            ArrayList<ShaderProgramBuilder.ShaderBuildResult> shaderBuildResults = new ArrayList<>();

            validateModules(shaderModules);

            Set<ShaderDesc.Language> shaderLanguages = getPlatformShaderLanguages(isComputeType, compileOptions);
            assert shaderLanguages != null;

            // Used for tests, merge in potentially unsupported languages here.
            shaderLanguages.addAll(compileOptions.forceIncludeShaderLanguages);

            HashMap<ShaderDesc.ShaderType, Boolean> shaderTypeKeys = new HashMap<>();
            Shaderc.HLSLRootSignature hlslRootSignature = null;

            for (ShaderDesc.Language shaderLanguage : shaderLanguages) {

                boolean arrayTextureFallbackRequired = ShaderUtil.VariantTextureArrayFallback.isRequired(shaderLanguage);

                boolean create_hlsl_root_signature = shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_51;
                List<Shaderc.ShaderCompileResult> compiled_shaders = new ArrayList<>();

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

                    ShaderDesc.Shader.Builder builder = ShaderProgramBuilder.makeShaderBuilder(crossCompileResult, shaderLanguage, shaderModule.type);
                    shaderBuildResults.add(new ShaderProgramBuilder.ShaderBuildResult(builder));

                    if (variantTextureArray) {
                        builder.setVariantTextureArray(true);
                    }

                    compiled_shaders.add(crossCompileResult);
                }

                if (create_hlsl_root_signature) {
                    hlslRootSignature = pipeline.createRootSignature(shaderLanguage, compiled_shaders);
                }
            }

            ShaderProgramBuilder.ShaderCompileResult compileResult = new ShaderProgramBuilder.ShaderCompileResult();
            compileResult.shaderBuildResults = shaderBuildResults;

            for(ShaderDesc.ShaderType type : shaderTypeKeys.keySet()) {
                compileResult.reflectors.add(pipeline.getReflectionData(type));
            }

            compileResult.hlslRootSignature = hlslRootSignature != null ? hlslRootSignature.hLSLRootSignature : null;

            ShaderCompilePipeline.destroyShaderPipeline(pipeline);

            return compileResult;
        }
    }

    public static IShaderCompiler GetCommonShaderCompiler(Platform platform) {
        if (platform == Platform.X86_64PS4 ||
            platform == Platform.X86_64PS5 ||
            platform == Platform.X86_64XBone) {
            return null;
        }
        return new CommonShaderCompiler(platform);
    }

    public static IShaderCompiler GetCommonShaderCompiler(Platform platform, ShaderCompilePipeline.Options baseOptions) {
        return new CommonShaderCompiler(platform, baseOptions);
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
