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
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;

import com.dynamo.graphics.proto.Graphics.ShaderDesc;

@BuilderParams(name = "ComputeProgram", inExts = ".cp", outExt = ".cpc")
public class ComputeProgramBuilder extends ShaderProgramBuilder {
    private static final ShaderDesc.ShaderType SHADER_TYPE = ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE;

    @Override
    public void build(Task task) throws IOException, CompileExceptionError {
        task.output(0).setContent(getCompiledShaderDesc(task, SHADER_TYPE).toByteArray());
    }

    // Running standalone:
    // java -classpath $DYNAMO_HOME/share/java/bob-light.jar com.dynamo.bob.pipeline.ComputeProgramBuilder <path-in.cp> <path-out.cpc> <platform>
    public static void main(String[] args) throws IOException, CompileExceptionError {
    	System.setProperty("java.awt.headless", "true");

        ComputeProgramBuilder builder = new ComputeProgramBuilder();

        Project project = new Project(new DefaultFileSystem());
        project.scanJavaClasses();

        builder.setProject(project);
        builder.BuildShader(args, SHADER_TYPE);
    }
}