// Copyright 2020-2023 The Defold Foundation
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

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.List;
import java.util.Collections;
import java.util.Comparator;
import java.util.Scanner;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Task;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ShaderUtil.Common;
import com.dynamo.bob.pipeline.ShaderUtil.ES2ToES3Converter;
import com.dynamo.bob.pipeline.ShaderUtil.VariantTextureArrayFallback;
import com.dynamo.bob.pipeline.ShaderUtil.SPIRVReflector;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.google.protobuf.ByteString;

public abstract class ShaderProgramBuilder extends Builder<ShaderPreprocessor> {

    public static class ShaderBuildResult {
        public ShaderDesc.Shader.Builder shaderBuilder;
        public String[]                  buildWarnings;

        public ShaderBuildResult(ArrayList<String> fromWarnings) {
            this.buildWarnings = fromWarnings.toArray(new String[0]);
        }

        public ShaderBuildResult(ShaderDesc.Shader.Builder fromBuilder) {
            this.shaderBuilder = fromBuilder;
        }
    }

    private static class ShaderDescBuildResult {
        ShaderDesc shaderDesc;
        String[]   buildWarnings;
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

        taskBuilder.addOutput(input.changeExt(params.outExt()));
        taskBuilder.setData(shaderPreprocessor);
        Task<ShaderPreprocessor> tsk = taskBuilder.build();
        return tsk;
    }

    public ShaderDesc getCompiledShaderDesc(Task<ShaderPreprocessor> task, ES2ToES3Converter.ShaderType shaderType)
            throws IOException, CompileExceptionError {
        List<IResource> inputs                = task.getInputs();
        IResource in                          = inputs.get(0);
        ShaderPreprocessor shaderPreprocessor = task.getData();
        boolean isDebug                       = (this.project.hasOption("debug") || (this.project.option("variant", Bob.VARIANT_RELEASE) != Bob.VARIANT_RELEASE));
        boolean outputSpirv                   = this.project.getProjectProperties().getBooleanValue("shader", "output_spirv", false);
        String resourceOutputPath             = task.getOutputs().get(0).getPath();

        ShaderDescBuildResult shaderDescBuildResult = makeShaderDesc(shaderPreprocessor,
            shaderType, resourceOutputPath, this.project.getPlatformStrings()[0], isDebug, outputSpirv, false);

        handleShaderDescBuildResult(shaderDescBuildResult, resourceOutputPath);

        return shaderDescBuildResult.shaderDesc;
    }

    static private void handleShaderDescBuildResult(ShaderDescBuildResult result, String resourceOutputPath) throws CompileExceptionError {
        if (result.buildWarnings != null) {
            for(String warningStr : result.buildWarnings) {
                System.err.println(warningStr);
            }
            throw new CompileExceptionError("Errors when producing output " + resourceOutputPath);
        }
    }

    static public String compileGLSL(String shaderSource, ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language shaderLanguage, boolean isDebug)  throws IOException, CompileExceptionError {

        ByteArrayOutputStream os = new ByteArrayOutputStream();
        PrintWriter writer = new PrintWriter(os);

        // Write directives from shader.
        String line = null;
        String firstNonDirectiveLine = null;
        int directiveLineCount = 0;

        Pattern directiveLinePattern = Pattern.compile("^\\s*(#).*");
        Scanner scanner = new Scanner(shaderSource);

        // Iterate the source lines to find the first occurance of a 'valid' line
        // i.e a line that doesn't already have a preprocessor directive, or is empty
        // This is needed to get a valid line number when shader compilation has failed
        while (scanner.hasNextLine()) {
            line = scanner.nextLine();

            if (line.isEmpty() || directiveLinePattern.matcher(line).find()) {
                writer.println(line);
                ++directiveLineCount;
            } else {
                firstNonDirectiveLine = line;
                break;
            }
        }

        if (directiveLineCount != 0) {
            writer.println();
        }

        boolean gles =  shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 ||
                        shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300;

        boolean gles3Standard = shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM140 ||
                                shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300;

        // Write our directives.
        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100) {
            // Normally, the ES2ToES3Converter would do this
            writer.println("precision mediump float;");
        }

        if (!gles) {
            writer.println("#ifndef GL_ES");
            writer.println("#define lowp");
            writer.println("#define mediump");
            writer.println("#define highp");
            writer.println("#endif");
            writer.println();
        }

        // We want "correct" line numbers from the GLSL compiler.
        //
        // Some Android devices don't like setting #line to something below 1,
        // see JIRA issue: DEF-1786.
        // We still want to have correct line reporting on most devices so
        // only output the "#line N" directive in debug builds.
        if (isDebug) {
            writer.printf(Locale.ROOT, "#line %d", directiveLineCount);
            writer.println();
        }

        // Write the first non-directive line from above.
        if (firstNonDirectiveLine != null) {
            writer.println(firstNonDirectiveLine);
        }

        // Write the remaining lines from the shader.
        while (scanner.hasNextLine()) {
            line = scanner.nextLine();
            writer.println(line);
        }
        scanner.close();
        writer.flush();

        String source = source = os.toString().replace("\r", "");

        if (gles3Standard) {
            int version = shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300 ? 300 : 140;
            ES2ToES3Converter.Result es3Result = ES2ToES3Converter.transform(source, shaderType, gles ? "es" : "", version, false);
            source = es3Result.output;
        }

        return source;
    }

    public static ShaderBuildResult makeShaderBuilderFromGLSLSource(String source, ShaderDesc.Language shaderLanguage) throws IOException{
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(shaderLanguage);
        builder.setSource(ByteString.copyFrom(source, "UTF-8"));
        return new ShaderBuildResult(builder);
    }

    // Maybe remove this
    private static ShaderBuildResult buildGLSL(String source, ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language shaderLanguage, boolean isDebug)  throws IOException, CompileExceptionError {
        String glslSource = compileGLSL(source, shaderType, shaderLanguage, isDebug);
        return makeShaderBuilderFromGLSLSource(glslSource, shaderLanguage);
    }

    // Called from editor and bob
    public static Common.GLSLCompileResult buildGLSLVariantTextureArray(String source, ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language shaderLanguage, boolean isDebug, int maxPageCount) throws IOException, CompileExceptionError {
        Common.GLSLCompileResult variantCompileResult = VariantTextureArrayFallback.transform(source, maxPageCount);

        // If the variant transformation didn't do anything, we pass the original source but without array samplers
        if (variantCompileResult == null) {
            Common.GLSLCompileResult originalRes = new Common.GLSLCompileResult();
            originalRes.source = source;
            return originalRes;
        }

        variantCompileResult.source = compileGLSL(variantCompileResult.source, shaderType, shaderLanguage, isDebug);
        return variantCompileResult;
    }

    // Generate a texture array variant builder if necessary, calling functions need to guard for this function returning null
    private static ShaderBuildResult getGLSLVariantTextureArrayBuilder(String source, ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language shaderLanguage, boolean isDebug, int maxPageCount) throws IOException, CompileExceptionError {
        Common.GLSLCompileResult variantCompileResult = buildGLSLVariantTextureArray(source, shaderType, shaderLanguage, isDebug, maxPageCount);

        // make a builder if the array transformation has picked up any array samplers
        if (variantCompileResult.arraySamplers != null) {
            return makeShaderBuilderFromGLSLSource(variantCompileResult.source, shaderLanguage);
        }

        return null;
    }

    static private String getResultString(Result r) {
        if (r.ret != 0 ) {
            String[] tokenizedResult = new String(r.stdOutErr).split(":", 2);
            String message = tokenizedResult[0];
            if(tokenizedResult.length != 1) {
                message = tokenizedResult[1];
            }
            return message;
        }
        return null;
    }

    static private void checkResult(String result_string, IResource resource, String resourceOutput) throws CompileExceptionError {
        if (result_string != null ) {
            if(resource != null) {
                throw new CompileExceptionError(resource, 0, result_string);
            } else {
                throw new CompileExceptionError(resourceOutput + ":" + result_string, null);
            }
        }
    }

    static private class BindingEntry extends ArrayList<SPIRVReflector.Resource> {}
    static private class SetEntry extends HashMap<Integer,BindingEntry> {}
    static private class SortBindingsComparator implements Comparator<SPIRVReflector.Resource> {
        public int compare(SPIRVReflector.Resource a, SPIRVReflector.Resource b) {
            return a.binding - b.binding;
        }
    }

    static public class SPIRVCompileResult {
        public byte[] source;
        public ArrayList<String> compile_warnings = new ArrayList<String>();
        public ArrayList<SPIRVReflector.Resource> attributes = new ArrayList<SPIRVReflector.Resource>();
        public ArrayList<SPIRVReflector.Resource> resource_list = new ArrayList<SPIRVReflector.Resource>();
    };

    static public SPIRVCompileResult compileGLSLToSPIRV(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutput, String targetProfile, boolean isDebug, boolean soft_fail)  throws IOException, CompileExceptionError {
        SPIRVCompileResult res = new SPIRVCompileResult();

        // Start all bindings on 1, this means that we can catch unused uniforms
        int glslcBindingBase = 1;

        int version = 140;
        if(targetProfile.equals("es")) {
            version = 310;
        }

        // Convert to ES3 (or GL 140+)
        ES2ToES3Converter.Result es3Result = ES2ToES3Converter.transform(shaderSource, shaderType, targetProfile, version, true);

        // Update version for SPIR-V (GLES >= 310, Core >= 140)
        es3Result.shaderVersion = es3Result.shaderVersion.isEmpty() ? "0" : es3Result.shaderVersion;
        if(es3Result.shaderProfile.equals("es")) {
            es3Result.shaderVersion = Integer.parseInt(es3Result.shaderVersion) < 310 ? "310" : es3Result.shaderVersion;
        } else {
            es3Result.shaderVersion = Integer.parseInt(es3Result.shaderVersion) < 140 ? "140" : es3Result.shaderVersion;
        }

        // compile GLSL (ES3 or Desktop 140) to SPIR-V
        File file_in_glsl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".glsl");
        file_in_glsl.deleteOnExit();
        FileUtils.writeByteArrayToFile(file_in_glsl, es3Result.output.getBytes());

        File file_out_spv = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
        file_out_spv.deleteOnExit();

        String spirvShaderStage = (shaderType == ES2ToES3Converter.ShaderType.VERTEX_SHADER ? "vert" : "frag");
        String glslcBindingBaseStr = String.valueOf(glslcBindingBase);

        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "glslc"),
                "-w",
                "-fauto-bind-uniforms",
                "-fauto-map-locations",
                "-fubo-binding-base", glslcBindingBaseStr,
                "-ftexture-binding-base", glslcBindingBaseStr,
                "-std=" + es3Result.shaderVersion + es3Result.shaderProfile,
                "-fshader-stage=" + spirvShaderStage,
                "-o", file_out_spv.getAbsolutePath(),
                file_in_glsl.getAbsolutePath()
                );

        String result_string = getResultString(result);
        if (soft_fail && result_string != null) {
            res.compile_warnings.add("\nCompatability issue: " + result_string);
            return res;
        } else {
            checkResult(result_string, null, resourceOutput);
        }

        // Generate reflection data
        File file_out_refl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".json");
        file_out_refl.deleteOnExit();

        result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-cross"),
            file_out_spv.getAbsolutePath(),
            "--output",file_out_refl.getAbsolutePath(),
            "--reflect");

        result_string = getResultString(result);
        if (soft_fail && result_string != null) {
            res.compile_warnings.add("\nUnable to get reflection data: " + result_string);
            return res;
        } else {
            checkResult(result_string, null, resourceOutput);
        }

        String result_json             = FileUtils.readFileToString(file_out_refl, StandardCharsets.UTF_8);
        SPIRVReflector reflector       = new SPIRVReflector(result_json);
        ArrayList<String> shaderIssues = new ArrayList<String>();

        // Put all shader resources on a separate list that will be sorted by binding number later
        ArrayList<SPIRVReflector.Resource> resource_list = new ArrayList();

        // Generate a mapping of Uniform Set -> List of Bindings for that set
        // so that we can check for duplicate bindings.
        //
        // Set (0) |- Binding(0) |- U1
        //         |_ Binding(1) |- U2
        //                       |_ U3
        //
        // Set (1) |- Binding(0) |_ U4
        //                       |_ U5
        //
        HashMap<Integer,SetEntry> setBindingMap = new HashMap<Integer,SetEntry>();
        for (SPIRVReflector.UniformBlock ubo : reflector.getUniformBlocks()) {

            // We only support a 1-1 mapping between uniform blocks and uniforms.
            // I.e uniform blocks can't have more (or less) than one uniform in them.
            if (ubo.uniforms.size() > 1) {
                shaderIssues.add("More than one uniforms in uniform block '" + ubo.name + "'");
            } else if (ubo.uniforms.size() == 0) {
                shaderIssues.add("No uniforms found in uniform block '" + ubo.name + "'");
            } else {
                SetEntry setEntry = setBindingMap.get(ubo.set);
                if (setEntry == null) {
                    setEntry = new SetEntry();
                    setBindingMap.put(ubo.set, setEntry);
                }

                BindingEntry bindingEntry = setEntry.get(ubo.binding);
                if (bindingEntry == null) {
                    bindingEntry = new BindingEntry();
                    setEntry.put(ubo.binding, bindingEntry);
                }

                SPIRVReflector.Resource firstUniform = ubo.uniforms.get(0);
                SPIRVReflector.Resource uniform = new SPIRVReflector.Resource();
                uniform.name         = firstUniform.name;
                uniform.type         = firstUniform.type;
                uniform.elementCount = firstUniform.elementCount;
                uniform.binding      = ubo.binding;
                uniform.set          = ubo.set;
                bindingEntry.add(uniform);

                ShaderDesc.ShaderDataType type = Common.stringTypeToShaderType(uniform.type);

                int issue_count = shaderIssues.size();
                if (type == ShaderDesc.ShaderDataType.SHADER_TYPE_UNKNOWN) {
                    shaderIssues.add("Unsupported type for uniform '" + uniform.name + "'");
                }

                if (uniform.set > 1) {
                    shaderIssues.add("Unsupported set value for uniform '" + uniform.name + "', expected <= 1 but found " + uniform.set);
                }

                if (issue_count == shaderIssues.size()) {
                    resource_list.add(uniform);
                }
            }
        }

        for (SPIRVReflector.Resource tex : reflector.getTextures()) {
            SetEntry setEntry = setBindingMap.get(tex.set);
            if (setEntry == null) {
                setEntry = new SetEntry();
                setBindingMap.put(tex.set, setEntry);
            }

            BindingEntry bindingEntry = setEntry.get(tex.binding);
            if (bindingEntry == null) {
                bindingEntry = new BindingEntry();
                setEntry.put(tex.binding, bindingEntry);
            }

            ShaderDesc.ShaderDataType type = Common.stringTypeToShaderType(tex.type);

            if (!Common.isShaderTypeTexture(type)) {
                shaderIssues.add("Unsupported type '" + tex.type + "'for texture sampler '" + tex.name + "'");
            } else {
                resource_list.add(tex);
            }

            bindingEntry.add(tex);
        }

        for (Map.Entry<Integer, SetEntry> sets : setBindingMap.entrySet()) {
            Integer setIndex = sets.getKey();
            SetEntry setEntry = sets.getValue();
            for (Map.Entry<Integer, BindingEntry> bindings : setEntry.entrySet()) {
                Integer bindingIndex = bindings.getKey();
                BindingEntry bindingEntry = bindings.getValue();
                if (bindingIndex >= glslcBindingBase && bindingEntry.size() > 1) {
                    String duplicateList = "";
                    for (int i = 0; i < bindingEntry.size(); i++) {
                        duplicateList += bindingEntry.get(i).name;

                        if (i != bindingEntry.size() - 1) {
                            duplicateList += ", ";
                        }
                    }

                    shaderIssues.add("Uniforms '" + duplicateList + "' from set " + setIndex + " have the same binding " + bindingIndex);
                }
            }
        }

        // This is a soft-fail mechanism just to notify that the shaders won't work in runtime.
        // At some point we should probably throw a compilation error here so that the build fails.
        if (shaderIssues.size() > 0) {
            res.compile_warnings = shaderIssues;
            return res;
        }

        // Build vertex attributes (inputs)
        if (shaderType == ES2ToES3Converter.ShaderType.VERTEX_SHADER) {
            ArrayList<SPIRVReflector.Resource> attributes = reflector.getInputs();
            Collections.sort(attributes, new SortBindingsComparator());
            res.attributes = attributes;
        }

        // Sort shader resources by increasing binding number
        Collections.sort(resource_list, new SortBindingsComparator());

        res.resource_list = resource_list;
        res.source        = FileUtils.readFileToByteArray(file_out_spv);

        return res;
    }

    static private ShaderBuildResult buildSpirvFromGLSL(String source, ES2ToES3Converter.ShaderType shaderType, String resourceOutputPath, String targetProfile, boolean isDebug, boolean soft_fail)  throws IOException, CompileExceptionError {
        source = Common.stripComments(source);
        SPIRVCompileResult compile_res = compileGLSLToSPIRV(source, shaderType, resourceOutputPath, targetProfile, isDebug, soft_fail);

        if (compile_res.compile_warnings.size() > 0)
        {
            return new ShaderBuildResult(compile_res.compile_warnings);
        }

        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(ShaderDesc.Language.LANGUAGE_SPIRV);
        builder.setSource(ByteString.copyFrom(compile_res.source));

        // Note: No need to check for duplicates for vertex attributes,
        // the SPIR-V compiler will complain about it.
        for (SPIRVReflector.Resource input : compile_res.attributes) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
            resourceBindingBuilder.setName(input.name);
            resourceBindingBuilder.setType(Common.stringTypeToShaderType(input.type));
            resourceBindingBuilder.setSet(input.set);
            resourceBindingBuilder.setBinding(input.binding);
            builder.addAttributes(resourceBindingBuilder);
        }

        // Build uniforms
        for (SPIRVReflector.Resource res : compile_res.resource_list) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
            resourceBindingBuilder.setName(res.name);
            resourceBindingBuilder.setType(Common.stringTypeToShaderType(res.type));
            resourceBindingBuilder.setElementCount(res.elementCount);
            resourceBindingBuilder.setSet(res.set);
            resourceBindingBuilder.setBinding(res.binding);
            builder.addUniforms(resourceBindingBuilder);
        }

        return new ShaderBuildResult(builder);
    }

    private ShaderDesc.Language[] getShaderLanguagesList(Platform platformKey, boolean outputSpirvRequested) {
        ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<ShaderDesc.Language>();
        boolean doOutputSpirv = false;
        switch(platformKey) {
            case X86_64MacOS:
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM140);
                doOutputSpirv = outputSpirvRequested;
            break;

            case X86Win32:
            case X86_64Win32:
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM140);
                doOutputSpirv = outputSpirvRequested;
            break;

            case X86Linux:
            case X86_64Linux:
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM140);
                doOutputSpirv = outputSpirvRequested;
            break;

            case Arm64Ios:
            case X86_64Ios:
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                doOutputSpirv = outputSpirvRequested;
            break;

            case Armv7Android:
            case Arm64Android:
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
                doOutputSpirv = outputSpirvRequested;
            break;

            case JsWeb:
            case WasmWeb:
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
            break;

            case Arm64NX64:
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM140);
                doOutputSpirv = true;
            break;

            default:
                System.err.println("Unsupported platform for shader program builder: " + platformKey);
            break;
        }

        if (doOutputSpirv) {
            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
        }

        return shaderLanguages.toArray(new ShaderDesc.Language[0]);
    }

    // Generate a shader desc struct that consists of either the built shader desc, or a list of compile warnings/errors
    private static ArrayList<ShaderBuildResult> getBaseShaderBuildResults(String resourceOutputPath, String fullShaderSource,
            ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language[] shaderLanguages,
            String spirvTargetProfile, boolean isDebug, boolean softFail) throws IOException, CompileExceptionError {

        ArrayList<ShaderBuildResult> shaderBuildResults = new ArrayList<ShaderBuildResult>();

        for (ShaderDesc.Language shaderLanguage : shaderLanguages) {
            if (shaderLanguage == ShaderDesc.Language.LANGUAGE_SPIRV) {
                shaderBuildResults.add(buildSpirvFromGLSL(fullShaderSource, shaderType, resourceOutputPath, spirvTargetProfile, isDebug, softFail));
            } else {
                shaderBuildResults.add(buildGLSL(fullShaderSource, shaderType, shaderLanguage, isDebug));
            }
        }

        return shaderBuildResults;
    }

    private static ShaderDescBuildResult buildResultsToShaderDescBuildResults(ArrayList<ShaderBuildResult> shaderBuildResults) {

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

        shaderDescBuildResult.shaderDesc = shaderDescBuilder.build();

        return shaderDescBuildResult;
    }

    // Called from editor for producing a ShaderDesc with a list of finalized shaders,
    // fully transformed from source to context shaders based on languages
    static public ShaderDescBuildResult makeShaderDescWithVariants(String shaderSource, ES2ToES3Converter.ShaderType shaderType,
            ShaderDesc.Language[] shaderLanguages, int maxPageCount) throws IOException, CompileExceptionError {

        ArrayList<ShaderBuildResult> shaderBuildResults = getBaseShaderBuildResults("", shaderSource, shaderType, shaderLanguages, "", false, true);

        for (ShaderDesc.Language shaderLanguage : shaderLanguages) {
            if (VariantTextureArrayFallback.tryBuildVariant(shaderLanguage)) {
                shaderBuildResults.add(getGLSLVariantTextureArrayBuilder(shaderSource, shaderType, shaderLanguage, true, maxPageCount));
            }
        }

        return buildResultsToShaderDescBuildResults(shaderBuildResults);
    }

    // Called from bob
    public ShaderDescBuildResult makeShaderDesc(ShaderPreprocessor shaderPreprocessor, ES2ToES3Converter.ShaderType shaderType, String resourceOutputPath,
            String platform, boolean isDebug, boolean outputSpirv, boolean softFail) throws IOException, CompileExceptionError {
        Platform platformKey = Platform.get(platform);
        if(platformKey == null) {
            throw new CompileExceptionError("Unknown platform for shader program '" + resourceOutputPath + "'': " + platform);
        }

        String finalShaderSource  = shaderPreprocessor.getCompiledSource();
        String spirvTargetProfile = platformKey == Platform.X86_64Ios ? "es" : "";

        return buildResultsToShaderDescBuildResults(getBaseShaderBuildResults(resourceOutputPath, finalShaderSource, shaderType,
            getShaderLanguagesList(platformKey, outputSpirv),
            spirvTargetProfile, isDebug, softFail));
    }

    // Called from command line to invoke shader pipeline directly (mostly for tests)
    public void BuildShader(String[] args, ES2ToES3Converter.ShaderType shaderType) throws IOException, CompileExceptionError {
        try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(args[0]));
            BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(args[1]))) {

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

           byte[] inBytes = new byte[is.available()];
           is.read(inBytes);
           ByteArrayInputStream bais = new ByteArrayInputStream(inBytes);

           String source = new String(inBytes, StandardCharsets.UTF_8);
           Project project = new Project(new DefaultFileSystem());
           ShaderPreprocessor shaderPreprocessor = new ShaderPreprocessor(project, args[0], source);

           ShaderDescBuildResult shaderDescBuildResult = makeShaderDesc(shaderPreprocessor,
                shaderType, args[1], cmd.getOptionValue("platform", ""),
                cmd.getOptionValue("variant", "").equals("debug") ? true : false, true, false);

           handleShaderDescBuildResult(shaderDescBuildResult, args[1]);

           shaderDescBuildResult.shaderDesc.writeTo(os);
           os.close();
       }
    }

}
