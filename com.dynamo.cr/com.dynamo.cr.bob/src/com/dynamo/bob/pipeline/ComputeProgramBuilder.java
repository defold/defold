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

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.pipeline.ShaderUtil.ES2ToES3Converter;
import com.dynamo.bob.pipeline.IShaderCompiler;
import com.dynamo.bob.pipeline.ShaderPreprocessor;

import org.apache.commons.cli.CommandLine;

@BuilderParams(name = "ComputeProgram", inExts = ".cp", outExt = ".cpc")
public class ComputeProgramBuilder extends ShaderProgramBuilder {

    private static final ES2ToES3Converter.ShaderType SHADER_TYPE = ES2ToES3Converter.ShaderType.COMPUTE_SHADER;
    private boolean soft_fail = true;

    @Override
    public void build(Task<ShaderPreprocessor> task) throws IOException, CompileExceptionError {
        task.output(0).setContent(getCompiledShaderDesc(task, SHADER_TYPE).toByteArray());
    }

    public static void main(String[] args) throws IOException, CompileExceptionError {
    	System.setProperty("java.awt.headless", "true");
        ComputeProgramBuilder builder = new ComputeProgramBuilder();
        CommandLine cmd = builder.GetShaderCommandLineOptions(args);
        String platformName = cmd.getOptionValue("platform", "");
        Platform platform = Platform.get(platformName);
        if (platform == null) {
            throw new CompileExceptionError(String.format("Invalid platform '%s'\n", platformName));
        }

        Project project = new Project(new DefaultFileSystem());
        project.scanJavaClasses();
        IShaderCompiler compiler = project.getShaderCompiler(platform);

        builder.setProject(project);
        builder.soft_fail = false;
        builder.BuildShader(args, SHADER_TYPE, cmd, compiler);
    }
}