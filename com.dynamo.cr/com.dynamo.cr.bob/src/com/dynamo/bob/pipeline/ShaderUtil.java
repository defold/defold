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

import java.io.ByteArrayOutputStream;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;
import java.util.Scanner;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

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
        // Used in editor
        public static final int     MAX_ARRAY_SAMPLERS                   = 8;

        public static final Pattern regexUniformKeywordPattern           = Pattern.compile("((?<keyword>uniform)\\s+|(?<layout>layout\\s*\\(.*\\n*.*\\)\\s*)\\s+|(?<precision>lowp|mediump|highp)\\s+)*(?<type>\\S+)\\s+(?<identifier>\\S+)\\s*(?<any>.*)\\s*;");
        public static final Pattern regexUniformBlockBeginKeywordPattern = Pattern.compile("((?<keyword>uniform)\\s+|(?<layout>layout\\s*\\(.*\\n*.*\\)\\s*)\\s+)*(?<type>\\S+)(?<any>.*)");
        public static String        includeDirectiveReplaceBaseStr       = "[^\\S\r\n]?\\s*\\#include\\s+(?:<%s>|\"%s\")";
        public static String        includeDirectiveBaseStr              = "^\\s*\\#include\\s+(?:<(?<pathbrackets>[^\"<>|\b]+)>|\"(?<pathquotes>[^\"<>|\b]+)\")\\s*(?://.*)?$";
        public static final Pattern includeDirectivePattern              = Pattern.compile(includeDirectiveBaseStr);
        public static final Pattern arrayArraySamplerPattern             = Pattern.compile("^\\s*uniform(?<qualifier>.*)sampler2DArray\\s+(?<uniform>\\w+);$");
        public static final Pattern regexVersionStringPattern            = Pattern.compile("^\\h*#\\h*version\\h+(?<version>\\d+)(\\h+(?<profile>\\S+))?\\h*\\n");

        public static class GLSLShaderInfo
        {
            public int    version;
            public String profile;
        };

        public static class GLSLCompileResult
        {
            public String   source;
            public String[] arraySamplers = new String[0];

            public GLSLCompileResult() {}

            public GLSLCompileResult(String source) {
                this.source = source;
            }
        }

        public static GLSLShaderInfo getShaderInfo(String source)
        {
            GLSLShaderInfo info = null;
            Matcher versionMatcher = regexVersionStringPattern.matcher(source.substring(0, Math.min(source.length(), 128)));
            if (versionMatcher.find()) {
                info                 = new GLSLShaderInfo();
                String shaderVersion = versionMatcher.group("version");
                String shaderProfile = versionMatcher.group("profile");
                info.version         = Integer.parseInt(shaderVersion);
                info.profile         = shaderProfile == null ? "" : shaderProfile;
            }
            return info;
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

        public static String compileGLSL(String shaderSource, ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage, boolean isDebug, boolean useLatestFeatures) throws CompileExceptionError {

            ByteArrayOutputStream os = new ByteArrayOutputStream();
            PrintWriter writer = new PrintWriter(os);

            // Write directives from shader.
            String line;
            String firstNonDirectiveLine = null;
            int directiveLineCount = 0;

            Pattern directiveLinePattern = Pattern.compile("^\\s*(#|//).*");
            Scanner scanner = new Scanner(shaderSource);

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

            int version;
            boolean gles3Standard;
            boolean gles;

            if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM430) {
                version       = 430;
                gles          = false;
                gles3Standard = true;
            } else {
                gles = shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 ||
                        shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300;

                gles3Standard = shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM140 ||
                        shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300 ||
                        shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM330;

                if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300) {
                    version = 300;
                } else if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM330) {
                    version = 330;
                } else {
                    version = 140;
                }

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

            String source = os.toString().replace("\r", "");

            if (gles3Standard) {
                ES2ToES3Converter.Result es3Result = ES2ToES3Converter.transform(source, shaderType, gles ? "es" : "", version, useLatestFeatures);
                source = es3Result.output;
            }
            return source;
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
                new ShaderDataTypeConversionEntry("int",            ShaderDesc.ShaderDataType.SHADER_TYPE_INT),
                new ShaderDataTypeConversionEntry("uint",           ShaderDesc.ShaderDataType.SHADER_TYPE_UINT),
                new ShaderDataTypeConversionEntry("float",          ShaderDesc.ShaderDataType.SHADER_TYPE_FLOAT),
                new ShaderDataTypeConversionEntry("vec2",           ShaderDesc.ShaderDataType.SHADER_TYPE_VEC2),
                new ShaderDataTypeConversionEntry("vec3",           ShaderDesc.ShaderDataType.SHADER_TYPE_VEC3),
                new ShaderDataTypeConversionEntry("vec4",           ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4),
                new ShaderDataTypeConversionEntry("mat2",           ShaderDesc.ShaderDataType.SHADER_TYPE_MAT2),
                new ShaderDataTypeConversionEntry("mat3",           ShaderDesc.ShaderDataType.SHADER_TYPE_MAT3),
                new ShaderDataTypeConversionEntry("mat4",           ShaderDesc.ShaderDataType.SHADER_TYPE_MAT4),
                new ShaderDataTypeConversionEntry("sampler2D",      ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D),
                new ShaderDataTypeConversionEntry("sampler3D",      ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER3D),
                new ShaderDataTypeConversionEntry("samplerCube",    ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER_CUBE),
                new ShaderDataTypeConversionEntry("sampler2DArray", ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D_ARRAY),
                new ShaderDataTypeConversionEntry("ubo",            ShaderDesc.ShaderDataType.SHADER_TYPE_UNIFORM_BUFFER),
                new ShaderDataTypeConversionEntry("uvec2",          ShaderDesc.ShaderDataType.SHADER_TYPE_UVEC2),
                new ShaderDataTypeConversionEntry("uvec3",          ShaderDesc.ShaderDataType.SHADER_TYPE_UVEC3),
                new ShaderDataTypeConversionEntry("uvec4",          ShaderDesc.ShaderDataType.SHADER_TYPE_UVEC4),
                new ShaderDataTypeConversionEntry("texture2D",      ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D),
                new ShaderDataTypeConversionEntry("texture2DArray", ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D_ARRAY),
                new ShaderDataTypeConversionEntry("textureCube",    ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE_CUBE),
                new ShaderDataTypeConversionEntry("utexture2D",     ShaderDesc.ShaderDataType.SHADER_TYPE_UTEXTURE2D),
                new ShaderDataTypeConversionEntry("uimage2D",       ShaderDesc.ShaderDataType.SHADER_TYPE_UIMAGE2D),
                new ShaderDataTypeConversionEntry("image2D",        ShaderDesc.ShaderDataType.SHADER_TYPE_IMAGE2D),
                new ShaderDataTypeConversionEntry("sampler",        ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER)
            ));

        public static ShaderDesc.ShaderDataType stringTypeToShaderType(String typeAsString) {
            for (ShaderDataTypeConversionEntry e : shaderDataTypeConversionLut) {
                if (e.asStr.equals(typeAsString)) {
                    return e.asDataType;
                }
            }
            return ShaderDesc.ShaderDataType.SHADER_TYPE_UNKNOWN;
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
            return shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM120 ||
                   shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100;
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

            for (String samplerName : arraySamplers) {
                String texture2DReplaceTextureRegex = String.format("texture2D(\\W?)+\\((\\W?)+%s(\\W?)+,", samplerName);
                String texture2DReplaceTextureReplacement = String.format("texture2DArray_%s(", samplerName);
                shaderBodyStr = shaderBodyStr.replaceAll(texture2DReplaceTextureRegex,  texture2DReplaceTextureReplacement);
            }

            result.source        = shaderBodyStr + "\n";
            result.arraySamplers = arraySamplers.toArray(new String[0]);

            return result;
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

        private static final String[] opaqueUniformTypesPrefix    = { "sampler", "image", "atomic_uint" };
        private static final Pattern regexPrecisionKeywordPattern = Pattern.compile("(?<keyword>precision)\\s+(?<precision>lowp|mediump|highp)\\s+(?<type>float|int)\\s*;");
        private static final Pattern regexFragDataArrayPattern    = Pattern.compile("gl_FragData\\[(?<index>\\d+)\\]");
        private static final Pattern regexCombinedSamplerPattern  = Pattern.compile("^sampler(?<type>Cube|2DArray|2D|3D)$");

        private static final String[][] vsKeywordReps = {{"varying", "out"}, {"attribute", "in"}, {"texture2D", "texture"}, {"texture2DArray", "texture"}, {"textureCube", "texture"}};
        private static final String[][] fsKeywordReps = {{"varying", "in"}, {"texture2D", "texture"}, {"texture2DArray", "texture"}, {"textureCube", "texture"}};

        private static final String dmEngineGeneratedRep = "_DMENGINE_GENERATED_";

        private static final String glUBRep                     = dmEngineGeneratedRep + "UB_";
        private static final String glUBRepVs                   = glUBRep + "VS_";
        private static final String glUBRepFs                   = glUBRep + "FS_";
        private static final String glVertexIDKeyword           = "gl_VertexID";
        private static final String glFragColorKeyword          = "gl_FragColor";
        private static final String glFragDataKeyword           = "gl_FragData";
        private static final String glVertexIDRep               = "gl_VertexIndex";
        private static final String glFragColorRep              = dmEngineGeneratedRep + glFragColorKeyword;
        private static final String glFragColorAttrRep          = "\n%sout vec4 " + glFragColorRep + "%s;\n";
        private static final String glFragColorAttrLayoutPrefix = "layout(location = %d) ";
        private static final String floatPrecisionAttrRep       = "precision mediump float;\n";

        public static Result transform(String input, ShaderDesc.ShaderType shaderType, String targetProfile, int targetVersion, boolean useLatestFeatures) throws CompileExceptionError {
            Result result = new Result();

            if(input.isEmpty()) {
                return result;
            }

            // Preprocess the source so we can potentially reduce the workload a bit
            input = Common.stripComments(input);

            // Shader sets are explicitly separated between fragment and vertex shaders as 1 and 0,
            // for compute shaders we always use 0. This makes sure that we stay true to that.
            int layoutSet = shaderType == ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT ? 1 : 0;

            // Index to output used for post patching tasks
            int floatPrecisionIndex = -1;

            Common.GLSLShaderInfo shaderInfo = Common.getShaderInfo(input);

            // If no version (and/or profile) was set in shader, append the target to the shader source.
            if (shaderInfo == null) {
                String versionPrefix = String.format("#version %d", targetVersion);
                if (!targetProfile.isEmpty()) {
                    versionPrefix += String.format(" %s", targetProfile);
                }
                versionPrefix += "\n";
                input = versionPrefix + input;
            } else {
                targetVersion = shaderInfo.version;
                targetProfile = shaderInfo.profile;
            }

            result.shaderVersion = Integer.toString(targetVersion);
            result.shaderProfile = targetProfile;

            // Patch qualifiers (reserved keywords so word boundary replacement is safe)
            String[][] keywordReps = (shaderType == ShaderDesc.ShaderType.SHADER_TYPE_VERTEX) ? vsKeywordReps : fsKeywordReps;
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
            boolean output_glVertexID = input.contains(glVertexIDKeyword);
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

            if (useLatestFeatures && output_glVertexID)
            {
                input = input.replaceAll("\\b" + glVertexIDKeyword + "\\b", glVertexIDRep);
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

            ArrayList<String[]> lineReplacements = new ArrayList<String[]>();

            // Preallocate array of resulting slices. This makes patching in specific positions less complex
            ArrayList<String> output = new ArrayList<String>(input.length());

            String ubBase = shaderType == ShaderDesc.ShaderType.SHADER_TYPE_VERTEX ? glUBRepVs : glUBRepFs;

            // Multi-instance patching
            int ubIndex = 0;
            for(String line : inputLines) {

                if(line.contains("uniform") && useLatestFeatures)
                {
                    Matcher uniformMatcher = Common.regexUniformKeywordPattern.matcher(line);

                    if (uniformMatcher.find()) {
                        String keyword = uniformMatcher.group("keyword");
                        if(keyword != null) {
                            String layout     = uniformMatcher.group("layout");
                            String precision  = uniformMatcher.group("precision");
                            String type       = uniformMatcher.group("type");
                            String identifier = uniformMatcher.group("identifier");
                            String any        = uniformMatcher.group("any");

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

                            if (isOpaque) {
                                Matcher combinedSamplerMatcher = regexCombinedSamplerPattern.matcher(type);
                                if(combinedSamplerMatcher.find()) { // Use the separated sampler/texture
                                    String lines = "";
                                    String samplerType = combinedSamplerMatcher.group("type");
                                    lines += layout + " " + line.replaceAll("\\b" + type + "\\b", "texture" + samplerType) + System.lineSeparator();
                                    lines += layout + " " + line.replaceAll("\\b" + type + "\\b", "sampler").replaceAll("\\b" + identifier + "\\b", identifier + "_separated") + System.lineSeparator();
                                    line = lines;
                                    String[] texture_replacement = {String.format("texture(\\W?)+\\((\\W?)+%s(\\W?)+,", identifier), String.format("texture(%s(%s, %s_separated),", type, identifier, identifier)};
                                    lineReplacements.add(texture_replacement);
                                    String[] textureLod_replacement = {String.format("textureLod(\\W?)+\\((\\W?)+%s(\\W?)+,", identifier), String.format("textureLod(%s(%s, %s_separated),", type, identifier, identifier)};
                                    lineReplacements.add(textureLod_replacement);
                                } else {
                                    line = layout + " " + line;
                                }
                            } else {
                                line = "\n" + layout + " " + keyword + " " + ubBase + ubIndex++ + " { " +
                                (precision == null ? "" : (precision + " ")) + type + " " + identifier + " " + (any == null ? "" : (any + " ")) + "; };";
                            }
                        }
                    } else {
                        Matcher uniforBlockBeginMatcher = Common.regexUniformBlockBeginKeywordPattern.matcher(line);
                        if (uniforBlockBeginMatcher.find()) {
                            String keyword = uniforBlockBeginMatcher.group("keyword");
                            if(keyword != null) {
                                String layout = uniforBlockBeginMatcher.group("layout");
                                String type   = uniforBlockBeginMatcher.group("type");
                                String any    = uniforBlockBeginMatcher.group("any");

                                boolean blockScopeBegin = line.contains("{");

                                if (layout == null) {
                                    layout = "layout(set=" + layoutSet + ")";
                                }

                                line = layout + " " + keyword + " " + type + (blockScopeBegin ? "{" : "");
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
                for( String[] replace : lineReplacements) {
                    line = line.replaceAll(replace[0], replace[1]);
                }
                output.add(line + System.lineSeparator());
            }

            // Post patching
            if (shaderType == ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT) {
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
