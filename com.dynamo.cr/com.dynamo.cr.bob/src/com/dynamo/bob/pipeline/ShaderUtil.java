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

import java.util.List;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.Scanner;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.io.IOException;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.node.ArrayNode;
import org.codehaus.jackson.map.ObjectMapper;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;

import org.antlr.v4.runtime.Token;
import org.antlr.v4.runtime.CharStream;
import org.antlr.v4.runtime.CharStreams;
import org.antlr.v4.runtime.CommonTokenStream;
import org.antlr.v4.runtime.TokenStreamRewriter;
import com.dynamo.bob.pipeline.antlr.glsl.GLSLLexer;

public class ShaderUtil {
    public static class Common {
        public static final int     MAX_ARRAY_SAMPLERS             = 8;
        public static final String  glSampler2DArrayRegex          = "(.+)sampler2DArray\\s+(\\w+);";
        public static final Pattern regexUniformKeywordPattern     = Pattern.compile("((?<keyword>uniform)\\s+|(?<layout>layout\\s*\\(.*\\n*.*\\)\\s*)\\s+|(?<precision>lowp|mediump|highp)\\s+)*(?<type>\\S+)\\s+(?<identifier>\\S+)\\s*(?<any>.*)\\s*;");
        public static String        includeDirectiveReplaceBaseStr = "[^\\S\r\n]?\\s*\\#include\\s+(?:<%s>|\"%s\")";
        public static String        includeDirectiveBaseStr        = "^\\s*\\#include\\s+(?:<(?<pathbrackets>[^\"<>|\b]+)>|\"(?<pathquotes>[^\"<>|\b]+)\")\\s*(?://.*)?$";
        public static final Pattern includeDirectivePattern        = Pattern.compile(includeDirectiveBaseStr);
        public static final Pattern arrayArraySamplerPattern       = Pattern.compile("^\\s*uniform(?<qualifier>.*)sampler2DArray\\s+(?<uniform>\\w+);$");

        public static class GLSLCompileResult
        {
            public String   source;
            public String[] arraySamplers = new String[0];
        }

        public static String stripComments(String source)
        {  
            CharStream stream = CharStreams.fromString(source);
            GLSLLexer lexer = new GLSLLexer(stream);
            CommonTokenStream tokens = new CommonTokenStream(lexer, GLSLLexer.COMMENTS);
            // Get all tokens from lexer until EOF 
            tokens.fill();
            TokenStreamRewriter rewriter = new TokenStreamRewriter(tokens);
            // Iterate over the tokens and remove comments
            for (Token token : tokens.getTokens()) {
                if (token.getChannel() == GLSLLexer.COMMENTS) {
                    if (token.getType() == GLSLLexer.BLOCK_COMMENT) {
                        // Insert a new line instead of each line of the multiline comment
                        int linesInComment = token.getText().split("\r\n|\r|\n").length - 1;
                        rewriter.replace(token, System.lineSeparator().repeat(linesInComment));
                    }
                    else {
                        rewriter.delete(token);
                    }
                }
            }
            return rewriter.getText();
        }

        public static boolean isShaderTypeTexture(ShaderDesc.ShaderDataType data_type)
        {
            return data_type == ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER_CUBE    ||
                   data_type == ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D       ||
                   data_type == ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D_ARRAY ||
                   data_type == ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER3D;
        }

        private static class ShaderDataTypeConversionEntry {
            public String                    asStr;
            public ShaderDesc.ShaderDataType asDataType;
            public ShaderDataTypeConversionEntry(String str, ShaderDesc.ShaderDataType dataType) {
                this.asStr      = str;
                this.asDataType = dataType;
            }
        }

        private static final ArrayList<ShaderDataTypeConversionEntry> shaderDataTypeConversionLut = new ArrayList<>(Arrays.asList(
                new ShaderDataTypeConversionEntry("int", ShaderDesc.ShaderDataType.SHADER_TYPE_INT),
                new ShaderDataTypeConversionEntry("uint", ShaderDesc.ShaderDataType.SHADER_TYPE_UINT),
                new ShaderDataTypeConversionEntry("float", ShaderDesc.ShaderDataType.SHADER_TYPE_FLOAT),
                new ShaderDataTypeConversionEntry("vec2", ShaderDesc.ShaderDataType.SHADER_TYPE_VEC2),
                new ShaderDataTypeConversionEntry("vec3", ShaderDesc.ShaderDataType.SHADER_TYPE_VEC3),
                new ShaderDataTypeConversionEntry("vec4", ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4),
                new ShaderDataTypeConversionEntry("mat2", ShaderDesc.ShaderDataType.SHADER_TYPE_MAT2),
                new ShaderDataTypeConversionEntry("mat3", ShaderDesc.ShaderDataType.SHADER_TYPE_MAT3),
                new ShaderDataTypeConversionEntry("mat4", ShaderDesc.ShaderDataType.SHADER_TYPE_MAT4),
                new ShaderDataTypeConversionEntry("sampler2D", ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D),
                new ShaderDataTypeConversionEntry("sampler3D", ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER3D),
                new ShaderDataTypeConversionEntry("samplerCube", ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER_CUBE),
                new ShaderDataTypeConversionEntry("sampler2DArray", ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D_ARRAY)
            ));

        public static ShaderDesc.ShaderDataType stringTypeToShaderType(String typeAsString) {
            for (ShaderDataTypeConversionEntry e : shaderDataTypeConversionLut) {
                if (e.asStr.equals(typeAsString)) {
                    return e.asDataType;
                }
            }
            return ShaderDesc.ShaderDataType.SHADER_TYPE_UNKNOWN;
        }

        public static String shaderTypeToString(ShaderDesc.ShaderDataType dataType) {
            for (ShaderDataTypeConversionEntry e : shaderDataTypeConversionLut) {
                if (e.asDataType == dataType) {
                    return e.asStr;
                }
            }
            return null;
        }
    }

    public static class VariantTextureArrayFallback {
        private static void generateTextureArrayFn(ArrayList<String> buffer, String samplerName, int maxPages) {
            buffer.add(String.format("vec4 texture2DArray_%s(vec3 dm_texture_array_args) {", samplerName));
            buffer.add("    int page_index = int(dm_texture_array_args.z);");
            String lineFmt = "    %s if (page_index == %d) return texture2D(%s_%d, dm_texture_array_args.st);";

            for (int i = 0; i < maxPages; i++) {
                if (i == 0) {
                    buffer.add(String.format(lineFmt, "", i, samplerName, i));
                } else {
                    buffer.add(String.format(lineFmt, "else", i, samplerName, i));
                }
            }

            buffer.add("   return vec4(0.0);");
            buffer.add("}");
        }

        public static boolean isRequired(ShaderDesc.Language shaderLanguage) {
            return shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM120 || shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100;
        }

        public static String samplerNameToSliceSamplerName(String samplerName, int slice) {
            return String.format("%s_%d", samplerName, slice);
        }

        public static Common.GLSLCompileResult transform(String shaderSource, int maxPageCount) {
            // For the texture array fallback variant, we need to convert texture2DArray functions
            // into separate texture samplers. Samplers in arrays (uniform sampler2D my_samplers[4]; does not work on all platforms unfortunately)
            Common.GLSLCompileResult result = new Common.GLSLCompileResult();

            ArrayList<String> arraySamplers = new ArrayList<String>();
            ArrayList<String> shaderBody = new ArrayList<>();

            String arrayReplaceTextureRegex  = "texture2DArray\\s*\\((([^,]+)),\\s*";
            String uniformArrayFormat        = "uniform %s sampler2D %s_%d;";

            Scanner scanner = new Scanner(shaderSource);
            while (scanner.hasNextLine()) {
                String line = scanner.nextLine();
                Matcher samplerMatcher = Common.arrayArraySamplerPattern.matcher(line);

                if(samplerMatcher.find()) {
                    String uniformName = samplerMatcher.group("uniform");
                    String qualifier   = samplerMatcher.group("qualifier");

                    for (int i=0; i < maxPageCount; i++) {
                        String pageUniform = String.format(uniformArrayFormat, qualifier, uniformName, i);
                        shaderBody.add(pageUniform);
                    }

                    shaderBody.add("");
                    generateTextureArrayFn(shaderBody, uniformName, maxPageCount);
                    arraySamplers.add(uniformName);

                } else {
                    shaderBody.add(line);
                }
            }

            if (arraySamplers.size() == 0) {
                return null;
            }

            String shaderBodyStr = String.join("\n", shaderBody);
            shaderBodyStr        = shaderBodyStr.replaceAll(arrayReplaceTextureRegex, "texture2DArray_$1(");
            result.source        = shaderBodyStr + "\n";
            result.arraySamplers = arraySamplers.toArray(new String[0]);

            return result;
        }
    }

    public static class SPIRVReflector {
        private static JsonNode root;

        public SPIRVReflector(String json) throws IOException
        {
            this.root = (new ObjectMapper()).readTree(json);
        }

        public static class Resource
        {
            public String name;
            public String type;
            public int    elementCount;
            public int    binding;
            public int    set;
        }

        public static class UniformBlock extends Resource
        {
            public ArrayList<Resource> uniforms;
        }

        public static ArrayList<UniformBlock> getUniformBlocks()
        {
            ArrayList<UniformBlock> uniformBlocks = new ArrayList<UniformBlock>();

            JsonNode uboNode   = root.get("ubos");
            JsonNode typesNode = root.get("types");

            if (uboNode == null || typesNode == null) {
                return uniformBlocks;
            }

            Iterator<JsonNode> uniformBlockNodeIt = uboNode.getElements();
            while (uniformBlockNodeIt.hasNext()) {
                JsonNode uniformBlockNode = uniformBlockNodeIt.next();

                UniformBlock ubo = new UniformBlock();
                ubo.name         = uniformBlockNode.get("name").asText();
                ubo.set          = uniformBlockNode.get("set").asInt();
                ubo.binding      = uniformBlockNode.get("binding").asInt();
                ubo.uniforms     = new ArrayList<Resource>();

                JsonNode typeNode    = typesNode.get(uniformBlockNode.get("type").asText());
                JsonNode membersNode = typeNode.get("members");

                for (Iterator<JsonNode> membersNodeIt = membersNode.getElements(); membersNodeIt.hasNext();) {
                    JsonNode uniformNode = membersNodeIt.next();
                    Resource res         = new Resource();
                    res.name             = uniformNode.get("name").asText();
                    res.type             = uniformNode.get("type").asText();
                    res.elementCount     = 1;
                    res.binding          = 0;
                    res.set              = 0;

                    JsonNode arrayNode = uniformNode.get("array");
                    if (arrayNode != null && arrayNode.isArray())
                    {
                        ArrayNode array = (ArrayNode) arrayNode;
                        res.elementCount = arrayNode.get(0).asInt();
                    }

                    ubo.uniforms.add(res);
                }

                uniformBlocks.add(ubo);
            }

            return uniformBlocks;
        }

        public static ArrayList<Resource> getTextures() {
            ArrayList<Resource> textures = new ArrayList<Resource>();

            JsonNode texturesNode = root.get("textures");

            if (texturesNode == null) {
                return textures;
            }

            for (Iterator<JsonNode> iter = texturesNode.getElements(); iter.hasNext();) {
                JsonNode textureNode = iter.next();
                Resource res     = new Resource();
                res.name         = textureNode.get("name").asText();
                res.type         = textureNode.get("type").asText();
                res.binding      = textureNode.get("binding").asInt();
                res.set          = textureNode.get("set").asInt();
                res.elementCount = 1;
                textures.add(res);
            }

            return textures;
        }

        public static ArrayList<Resource> getInputs() {
            ArrayList<Resource> inputs = new ArrayList<Resource>();

            JsonNode inputsNode = root.get("inputs");

            if (inputsNode == null) {
                return inputs;
            }

            for (Iterator<JsonNode> iter = inputsNode.getElements(); iter.hasNext();) {
                JsonNode inputNode = iter.next();
                Resource res = new Resource();
                res.name     = inputNode.get("name").asText();
                res.type     = inputNode.get("type").asText();
                res.binding  = inputNode.get("location").asInt();
                inputs.add(res);
            }

            return inputs;
        }

        public static ArrayList<Resource> getOutputs() {
            ArrayList<Resource> outputs = new ArrayList<Resource>();

            JsonNode outputsNode = root.get("outputs");

            if (outputsNode == null) {
                return outputs;
            }

            for (Iterator<JsonNode> iter = outputsNode.getElements(); iter.hasNext();) {
                JsonNode outputNode = iter.next();
                Resource res = new Resource();
                res.name     = outputNode.get("name").asText();
                res.type     = outputNode.get("type").asText();
                res.binding  = outputNode.get("location").asInt();
                outputs.add(res);
            }

            return outputs;
        }
    }

    public static class ES2ToES3Converter {

        /*
         * ES2ToES3Converter is converting shaders using old shader language syntax into GLES version 310 compliant shaders or GL version 140 compliant shaders depending on target (GLES or GL)
         *
         * The following rules apply
         *
         * * Shader version and profile (#version xxx xx) is overridden if declared in the shader (default is minimum per target, see above)
         *
         * Vertex Shaders:
         * * "attribute" keyword is changed to "in"
         * * "varying" keyword is changed to "out"
         * * Uniform declarations are wrapped to global space uniform buffers
         *   - Unless the type is opaque (sampler, image or atomic_uint)
         *
         * Fragment Shaders:
         * * "varying" keyword is changed to "out"
         * * Uniform declarations are wrapped to global space uniform buffers
         *   - Unless the type is opaque (sampler, image or atomic_uint)
         * * Precision mediump float is added to ES shaders if not existing
         * * If exists, gl_FragColor or gl_FragData are converted to a generated out attribute.
         *   - If they exist (otherwise, one can assume this is already a compliant shader)
         *   - On ES targets only (GL targets accepts old style)
         *   - Placed after the first precision statement (if ES)
         *
         * Note: This covers known cases, but if the shader has reserved variable names of (newer) keywords as "in", "out" or "texture" they will have to be modified by the writer.
         * This would have to be done in any case upgrading shaders from GLES2 and is nothing we can patch as changing those members names would instead mean the run-time access (get/set uniform)
         * will fail which is even worse.
         *
         */

        public static class Result {
            public String shaderVersion = "";
            public String shaderProfile = "";
            public String output = "";
        }

        public static enum ShaderType {
            VERTEX_SHADER, FRAGMENT_SHADER
        };

        private static final String[] opaqueUniformTypesPrefix    = { "sampler", "image", "atomic_uint" };
        private static final Pattern regexVersionStringPattern    = Pattern.compile("^\\h*#\\h*version\\h+(?<version>\\d+)(\\h+(?<profile>\\S+))?\\h*\\n");
        private static final Pattern regexPrecisionKeywordPattern = Pattern.compile("(?<keyword>precision)\\s+(?<precision>lowp|mediump|highp)\\s+(?<type>float|int)\\s*;");
        private static final Pattern regexFragDataArrayPattern    = Pattern.compile("gl_FragData\\[(?<index>\\d+)\\]");

        private static final String[][] vsKeywordReps = {{"varying", "out"}, {"attribute", "in"}, {"texture2D", "texture"}, {"texture2DArray", "texture"}, {"textureCube", "texture"}};
        private static final String[][] fsKeywordReps = {{"varying", "in"}, {"texture2D", "texture"}, {"texture2DArray", "texture"}, {"textureCube", "texture"}};

        private static final String dmEngineGeneratedRep = "_DMENGINE_GENERATED_";

        private static final String glUBRep                     = dmEngineGeneratedRep + "UB_";
        private static final String glFragColorKeyword          = "gl_FragColor";
        private static final String glFragDataKeyword           = "gl_FragData";
        private static final String glFragColorRep              = dmEngineGeneratedRep + glFragColorKeyword;
        private static final String glFragColorAttrRep          = "\n%sout vec4 " + glFragColorRep + "%s;\n";
        private static final String glFragColorAttrLayoutPrefix = "layout(location = %d) ";
        private static final String floatPrecisionAttrRep       = "precision mediump float;\n";

        public static Result transform(String input, ShaderType shaderType, String targetProfile, int targetVersion, boolean useLatestFeatures) throws CompileExceptionError {
            Result result = new Result();

            if(input.isEmpty()) {
                return result;
            }

            // Preprocess the source so we can potentially reduce the workload a bit
            input = Common.stripComments(input);

            int layoutSet = shaderType == ShaderType.VERTEX_SHADER ? 0 : 1;

            // Index to output used for post patching tasks
            int floatPrecisionIndex = -1;

            // Try get version and profile. Override targetProfile if version is set in shader
            Matcher versionMatcher = regexVersionStringPattern.matcher(input.substring(0, Math.min(input.length(), 128)));
            if (versionMatcher.find()) {
                result.shaderVersion = versionMatcher.group("version");
                result.shaderProfile = versionMatcher.group("profile");
                result.shaderProfile = result.shaderProfile == null ? "" : result.shaderProfile;
                // override targetProfile if version is set in shader
                targetProfile = result.shaderProfile;
            } else {
                String versionPrefix = String.format("#version %d", targetVersion);
                if (!targetProfile.isEmpty())
                    versionPrefix += String.format(" %s", targetProfile);
                versionPrefix += "\n";
                input = versionPrefix + input;
            }

            if (!result.shaderVersion.isEmpty()) {
                targetVersion = Integer.parseInt(result.shaderVersion);
            }

            // Patch qualifiers (reserved keywords so word boundary replacement is safe)
            String[][] keywordReps = (shaderType == ShaderType.VERTEX_SHADER) ? vsKeywordReps : fsKeywordReps;
            for ( String[] keywordRep : keywordReps) {
                input = input.replaceAll("\\b" + keywordRep[0] + "\\b", keywordRep[1]);
            }

            // Get data output array size
            Matcher fragDataArrayMatcher = regexFragDataArrayPattern.matcher(input);
            int maxColorOutputs = 1;
            while (fragDataArrayMatcher.find()) {
                String fragDataArrayIndex = fragDataArrayMatcher.group("index");
                maxColorOutputs = Math.max(maxColorOutputs, Integer.parseInt(fragDataArrayIndex) + 1);
            }

            // Replace fragment output variables
            boolean output_glFragColor = input.contains(glFragColorKeyword);
            boolean output_glFragData = input.contains(glFragDataKeyword);

            if (output_glFragColor)
            {
                input = input.replaceAll("\\b" + glFragColorKeyword + "\\b", glFragColorRep + "_0");
            }

            if (output_glFragData)
            {
                input = input.replaceAll("\\b" + glFragDataKeyword + "\\[(\\d+)\\]", glFragColorRep + "_$1");
            }

            String[] inputLines = input.split("\\r?\\n");

            // Find the first non directive line
            Pattern directiveLinePattern = Pattern.compile("^\\s*(#|//).*");
            int patchLineIndex = 0;
            for(String line : inputLines) {
                line = line.trim();
                if (line.startsWith("#line")) { // The next line is where the used code starts
                    break;
                }
                if (line.isEmpty() || directiveLinePattern.matcher(line).find()) {
                    patchLineIndex++;
                    continue;
                }
                break;
            }

            // Preallocate array of resulting slices. This makes patching in specific positions less complex
            ArrayList<String> output = new ArrayList<String>(input.length());

            // Multi-instance patching
            int ubIndex = 0;
            for(String line : inputLines) {

                if(line.contains("uniform") && !line.contains("{") && useLatestFeatures)
                {
                    // Transform non-opaque uniforms into uniform blocks (UB's). Do not process existing UB's
                    Matcher uniformMatcher = Common.regexUniformKeywordPattern.matcher(line);
                    if(uniformMatcher.find()) {
                        String keyword = uniformMatcher.group("keyword");
                        if(keyword != null) {
                            String layout = uniformMatcher.group("layout");
                            String precision = uniformMatcher.group("precision");
                            String type = uniformMatcher.group("type");
                            String identifier = uniformMatcher.group("identifier");
                            String any = uniformMatcher.group("any");

                            boolean isOpaque = false;
                            for( String opaqueTypePrefix : opaqueUniformTypesPrefix) {
                                if(type.startsWith(opaqueTypePrefix)) {
                                    isOpaque = true;
                                    break;
                                }
                            }

                            if (layout == null) {
                                layout = "layout(set=" + layoutSet + ")";
                            }

                            if (isOpaque){
                                line = layout + " " + line;
                            } else {
                                line = "\n" + layout + " " + keyword + " " + glUBRep + ubIndex++ + " { " +
                                (precision == null ? "" : (precision + " ")) + type + " " + identifier + " " + (any == null ? "" : (any + " ")) + "; };";
                            }
                        }
                    }
                }
                else if (line.contains("precision")) {
                    // Check if precision keyword present and store index if so, for post patch tasks
                    Matcher precisionMatcher = regexPrecisionKeywordPattern.matcher(line);
                    if(precisionMatcher.find()) {
                        if(precisionMatcher.group("type").equals("float")) {
                            floatPrecisionIndex = output.size();
                        }
                    }
                }
                output.add(line + System.lineSeparator());
            }

            // Post patching
            if (shaderType == ShaderType.FRAGMENT_SHADER) {
                // if we have patched glFragColor
                if(output_glFragColor || output_glFragData) {
                    // insert precision if not found, as it is mandatory for out attributes
                    if(floatPrecisionIndex < 0 && targetProfile.equals("es")) {
                        output.add(patchLineIndex++, floatPrecisionAttrRep);
                    }

                    // insert fragcolor out attribute for all output targets
                    for (int i = 0; i < maxColorOutputs; i++) {
                        String colorOutputLayoutPrefix = "";
                        // For ES targets we need to add a layout specification for the outputs
                        if (targetProfile.equals("es") && maxColorOutputs > 1)
                        {
                            colorOutputLayoutPrefix = String.format(glFragColorAttrLayoutPrefix, i);
                        }
                        output.add(patchLineIndex++, String.format(glFragColorAttrRep, colorOutputLayoutPrefix, "_" + i));
                    }
                }
            }

            result.output = String.join("", output);
            return result;
        }
    }

}
