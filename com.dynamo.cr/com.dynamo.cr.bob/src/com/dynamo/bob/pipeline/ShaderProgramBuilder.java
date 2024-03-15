// Copyright 2020-2024 The Defold Foundation
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
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;

import java.nio.charset.StandardCharsets;

import java.util.ArrayList;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.Platform;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ShaderUtil.ES2ToES3Converter;
import com.dynamo.bob.pipeline.ShaderUtil.VariantTextureArrayFallback;
import com.dynamo.bob.pipeline.ShaderUtil.Common;

import com.dynamo.graphics.proto.Graphics.ShaderDesc;

public abstract class ShaderProgramBuilder extends Builder<ShaderPreprocessor> {

    static public class ShaderBuildResult {
        public ShaderDesc.Shader.Builder shaderBuilder;
        public String[]                  buildWarnings;

        public ShaderBuildResult(String fromWarning) {
            this.buildWarnings = new String[] { fromWarning };
        }

        public ShaderBuildResult(ArrayList<String> fromWarnings) {
            this.buildWarnings = fromWarnings.toArray(new String[0]);
        }

        public ShaderBuildResult(ShaderDesc.Shader.Builder fromBuilder) {
            this.shaderBuilder = fromBuilder;
        }
    }

    static public class ShaderDescBuildResult {
        public ShaderDesc shaderDesc;
        public String[]   buildWarnings;
    }

    @Override
    public Task<ShaderPreprocessor> create(IResource input) throws IOException, CompileExceptionError {

        Task.TaskBuilder<ShaderPreprocessor> taskBuilder = Task.<ShaderPreprocessor>newBuilder(this)
            .setName(params.name())
            .addInput(input);

        // Parse source for includes and add the include nodes as inputs/dependancies to the shader
        String source     = new String(input.getContent(), StandardCharsets.UTF_8);
        String projectDir = this.project.getRootDirectory();
        ShaderPreprocessor shaderPreprocessor = new ShaderPreprocessor(this.project, input.getPath(), source);
        String[] includes = shaderPreprocessor.getIncludes();

        for (String path : includes) {
            taskBuilder.addInput(this.project.getResource(path));
        }

        // Include the spir-v flag into the cache key so we can invalidate the output results accordingly
        String spirvCacheKey = "output_spirv=" + getOutputSpirvFlag();
        taskBuilder.addOutput(input.changeExt(params.outExt()));
        taskBuilder.setData(shaderPreprocessor);
        taskBuilder.addExtraCacheKey(spirvCacheKey);

        Task<ShaderPreprocessor> tsk = taskBuilder.build();
        return tsk;
    }

    private boolean getOutputSpirvFlag() {
        boolean fromProjectOptions    = this.project.option("output-spirv", "false").equals("true");
        boolean fromProjectProperties = this.project.getProjectProperties().getBooleanValue("shader", "output_spirv", false);
        return fromProjectOptions || fromProjectProperties;
    }

    static private ShaderDescBuildResult buildResultsToShaderDescBuildResults(ArrayList<ShaderBuildResult> shaderBuildResults, ES2ToES3Converter.ShaderType shaderType) {

        ShaderDescBuildResult shaderDescBuildResult = new ShaderDescBuildResult();
        ShaderDesc.Builder shaderDescBuilder = ShaderDesc.newBuilder();

        for (ShaderBuildResult shaderBuildResult : shaderBuildResults) {
            if (shaderBuildResult != null) {
                if (shaderBuildResult.buildWarnings != null) {
                    shaderDescBuildResult.buildWarnings = shaderBuildResult.buildWarnings;
                    return shaderDescBuildResult;
                }

                shaderDescBuilder.addShaders(shaderBuildResult.shaderBuilder);
            }
        }

        if (shaderType == ES2ToES3Converter.ShaderType.COMPUTE_SHADER) {
            shaderDescBuilder.setShaderClass(ShaderDesc.ShaderClass.SHADER_CLASS_COMPUTE);
        } else {
            shaderDescBuilder.setShaderClass(ShaderDesc.ShaderClass.SHADER_CLASS_GRAPHICS);
        }

        shaderDescBuildResult.shaderDesc = shaderDescBuilder.build();

        return shaderDescBuildResult;
    }

    // Generate a texture array variant builder, but only if necessary
    static private ShaderBuildResult getGLSLVariantTextureArrayBuilder(String source, ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language shaderLanguage, boolean isDebug, int maxPageCount) throws IOException, CompileExceptionError {
        Common.GLSLCompileResult variantCompileResult = buildGLSLVariantTextureArray(source, shaderType, shaderLanguage, isDebug, maxPageCount);

        // make a builder if the array transformation has picked up any array samplers
        if (variantCompileResult.arraySamplers.length > 0) {
            ShaderBuildResult buildResult = ShaderCompilerHelpers.makeShaderBuilderFromGLSLSource(variantCompileResult.source, shaderLanguage);
            assert(buildResult != null);
            buildResult.shaderBuilder.setVariantTextureArray(true);
            return buildResult;
        }

        return null;
    }

    // Called from editor for producing a ShaderDesc with a list of finalized shaders,
    // fully transformed from source to context shaders based on a list of languages
    static public ShaderDescBuildResult makeShaderDescWithVariants(String resourceOutputPath, String shaderSource, ES2ToES3Converter.ShaderType shaderType,
            ShaderDesc.Language[] shaderLanguages, int maxPageCount) throws IOException, CompileExceptionError {

        ArrayList<ShaderBuildResult> shaderBuildResults = ShaderCompilers.getBaseShaderBuildResults(resourceOutputPath, shaderSource, shaderType, shaderLanguages, "", false, true);

        for (ShaderDesc.Language shaderLanguage : shaderLanguages) {
            if (VariantTextureArrayFallback.isRequired(shaderLanguage)) {
                shaderBuildResults.add(getGLSLVariantTextureArrayBuilder(shaderSource, shaderType, shaderLanguage, true, maxPageCount));
            }
        }

        return buildResultsToShaderDescBuildResults(shaderBuildResults, shaderType);
    }

    // Called from bob
    public ShaderDescBuildResult makeShaderDesc(String resourceOutputPath, ShaderPreprocessor shaderPreprocessor, ES2ToES3Converter.ShaderType shaderType,
            String platform, boolean isDebug, boolean outputSpirv, boolean softFail) throws IOException, CompileExceptionError {
        Platform platformKey = Platform.get(platform);
        if(platformKey == null) {
            throw new CompileExceptionError("Unknown platform for shader program '" + resourceOutputPath + "'': " + platform);
        }

        String finalShaderSource                          = shaderPreprocessor.getCompiledSource();
        IShaderCompiler shaderCompiler                    = project.getShaderCompiler(platformKey);
        ArrayList<ShaderBuildResult> shaderCompilerResult = shaderCompiler.compile(finalShaderSource, shaderType, resourceOutputPath, resourceOutputPath, isDebug, outputSpirv, false);
        return buildResultsToShaderDescBuildResults(shaderCompilerResult, shaderType);
    }

    static private void handleShaderDescBuildResult(ShaderDescBuildResult result, String resourceOutputPath) throws CompileExceptionError {
        if (result.buildWarnings != null) {
            for(String warningStr : result.buildWarnings) {
                System.err.println(warningStr);
            }
            throw new CompileExceptionError("Errors when producing output " + resourceOutputPath);
        }
    }

    // Called from editor and bob
    static public Common.GLSLCompileResult buildGLSLVariantTextureArray(String source, ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language shaderLanguage, boolean isDebug, int maxPageCount) throws IOException, CompileExceptionError {
        Common.GLSLCompileResult variantCompileResult = VariantTextureArrayFallback.transform(source, maxPageCount);

        // If the variant transformation didn't do anything, we pass the original source but without array samplers
        if (variantCompileResult == null) {
            Common.GLSLCompileResult originalRes = new Common.GLSLCompileResult();
            originalRes.source = ShaderCompilerHelpers.compileGLSL(source, shaderType, shaderLanguage, isDebug);
            return originalRes;
        }

        variantCompileResult.source = ShaderCompilerHelpers.compileGLSL(variantCompileResult.source, shaderType, shaderLanguage, isDebug);

        return variantCompileResult;
    }

    public ShaderDesc getCompiledShaderDesc(Task<ShaderPreprocessor> task, ES2ToES3Converter.ShaderType shaderType)
            throws IOException, CompileExceptionError {
        IResource in                          = task.input(0);
        ShaderPreprocessor shaderPreprocessor = task.getData();
        boolean isDebug                       = (this.project.hasOption("debug") || (this.project.option("variant", Bob.VARIANT_RELEASE) != Bob.VARIANT_RELEASE));
        boolean outputSpirv                   = getOutputSpirvFlag();
        String resourceOutputPath             = task.getOutputs().get(0).getPath();

        ShaderDescBuildResult shaderDescBuildResult = makeShaderDesc(resourceOutputPath, shaderPreprocessor,
            shaderType, this.project.getPlatformStrings()[0], isDebug, outputSpirv, false);

        handleShaderDescBuildResult(shaderDescBuildResult, resourceOutputPath);

        return shaderDescBuildResult.shaderDesc;
    }

    public CommandLine GetShaderCommandLineOptions(String[] args) {
        Options options = new Options();
        options.addOption("p", "platform", true, "Platform");
        options.addOption(null, "variant", true, "Specify debug or release");
        CommandLineParser parser = new PosixParser();
        CommandLine cmd = null;
        try {
            cmd = parser.parse(options, args);
        } catch (ParseException e) {
            System.err.println(e.getMessage());
            System.exit(5);
        }
        return cmd;
    }

    public void BuildShader(String[] args, ES2ToES3Converter.ShaderType shaderType, CommandLine cmd, IShaderCompiler shaderCompiler) throws IOException, CompileExceptionError {
        if (shaderCompiler == null) {
            System.err.println(String.format("Unable to build shader %s - no shader compiler found.", args[0]));
            return;
        }

        try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(args[0]));
            BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(args[1]))) {

            byte[] inBytes = new byte[is.available()];
            is.read(inBytes);

            String source = new String(inBytes, StandardCharsets.UTF_8);
            ShaderPreprocessor shaderPreprocessor = new ShaderPreprocessor(this.project, args[0], source);

            boolean outputSpirv = true;
            boolean softFail = false;

            ShaderDescBuildResult shaderDescBuildResult = makeShaderDesc(args[1], shaderPreprocessor,
                shaderType, cmd.getOptionValue("platform", ""),
                cmd.getOptionValue("variant", "").equals("debug") ? true : false,
                outputSpirv, softFail);

            handleShaderDescBuildResult(shaderDescBuildResult, args[1]);

            shaderDescBuildResult.shaderDesc.writeTo(os);
            os.close();
        }
    }
}
