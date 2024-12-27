package com.dynamo.bob.pipeline;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;
import com.dynamo.graphics.proto.Graphics;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Objects;

import static com.dynamo.bob.pipeline.ShaderProgramBuilder.buildResultsToShaderDescBuildResults;

public class ShaderProgramBuilderEditor {
    static public ShaderUtil.Common.GLSLCompileResult buildGLSLVariantTextureArray(String source, Graphics.ShaderDesc.ShaderType shaderType, Graphics.ShaderDesc.Language shaderLanguage, boolean isDebug, int maxPageCount) throws IOException, CompileExceptionError {
        if (!ShaderUtil.VariantTextureArrayFallback.isRequired(shaderLanguage)) {
            return new ShaderUtil.Common.GLSLCompileResult(source);
        }

        // Make sure we have the correct output language
        ShaderCompilePipeline pipeline;
        ShaderCompilePipeline.Options options = new ShaderCompilePipeline.Options();
        try {
            pipeline = ShaderProgramBuilder.newShaderPipelineFromShaderSource(shaderType, "variant-texture-array", source, options);
        } catch (Exception e) {
            // We don't have a graceful way to handle shader errors in the editor except for building/bundling
            return new ShaderUtil.Common.GLSLCompileResult(source);
        }

        byte[] result = pipeline.crossCompile(shaderType, shaderLanguage);
        String compiledSource = new String(result);

        ShaderUtil.Common.GLSLCompileResult variantCompileResult = ShaderUtil.VariantTextureArrayFallback.transform(compiledSource, maxPageCount);

        // If the variant transformation didn't do anything, we pass the original source but without array samplers
        if (variantCompileResult != null) {
            variantCompileResult.reflector = pipeline.getReflectionData();
            return variantCompileResult;
        } else {
            return new ShaderUtil.Common.GLSLCompileResult(compiledSource, pipeline.getReflectionData());
        }
    }

    static private boolean isCompatibleLanguage(Graphics.ShaderDesc.ShaderType shaderType, Graphics.ShaderDesc.Language shaderLanguage) {
        if (shaderType == Graphics.ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
            return switch (shaderLanguage) {
                case LANGUAGE_SPIRV, LANGUAGE_GLSL_SM430, LANGUAGE_PSSL, LANGUAGE_WGSL -> true;
                default -> false;
            };
        } else {
            return true;
        }
    }

    // Called from editor for producing a ShaderDesc with a list of finalized shaders,
    // fully transformed from source to context shaders based on a list of languages
    static public ShaderProgramBuilder.ShaderDescBuildResult makeShaderDescWithVariants(String resourceOutputPath, String shaderSource, Graphics.ShaderDesc.ShaderType shaderType,
                                                                                        Graphics.ShaderDesc.Language[] shaderLanguages, int maxPageCount) throws IOException, CompileExceptionError {

        ShaderCompilePipeline pipeline;
        ShaderCompilePipeline.Options options = new ShaderCompilePipeline.Options();
        try {
            pipeline = ShaderProgramBuilder.newShaderPipelineFromShaderSource(shaderType, resourceOutputPath, shaderSource, options);
        } catch (Exception e) {
            ShaderProgramBuilder.ShaderDescBuildResult res = new ShaderProgramBuilder.ShaderDescBuildResult();
            res.buildWarnings = new String[] { e.getMessage()};
            return res;
        }

        ArrayList<ShaderProgramBuilder.ShaderBuildResult> shaderBuildResults = new ArrayList<>();

        for (Graphics.ShaderDesc.Language shaderLanguage : shaderLanguages) {

            // Some languages might not be compatible with the shader type, e.g webgl and compute shaders
            if (!isCompatibleLanguage(shaderType, shaderLanguage)) {
                continue;
            }

            byte[] source = pipeline.crossCompile(shaderType, shaderLanguage);
            boolean variantTextureArray = false;

            if (ShaderUtil.VariantTextureArrayFallback.isRequired(shaderLanguage)) {
                ShaderUtil.Common.GLSLCompileResult variantCompileResult = ShaderUtil.VariantTextureArrayFallback.transform(new String(source), maxPageCount);
                if (variantCompileResult != null && variantCompileResult.arraySamplers.length > 0) {
                    source = variantCompileResult.source.getBytes();
                    variantTextureArray = true;
                }
            }

            Graphics.ShaderDesc.Shader.Builder builder = ShaderProgramBuilder.makeShaderBuilder(shaderLanguage, source);

            // Note: We are not doing builder.setVariantTextureArray(variantTextureArray); because calling that function
            //       will mark the field as being set, regardless of the value. We only want to mark the field if we need to.
            if (variantTextureArray) {
                builder.setVariantTextureArray(true);
            }
            shaderBuildResults.add(new ShaderProgramBuilder.ShaderBuildResult(builder));
        }

        ShaderProgramBuilder.ShaderCompileResult compileResult = new ShaderProgramBuilder.ShaderCompileResult();
        compileResult.reflector = pipeline.getReflectionData();
        compileResult.shaderBuildResults = shaderBuildResults;

        return buildResultsToShaderDescBuildResults(compileResult, shaderType);
    }
}
