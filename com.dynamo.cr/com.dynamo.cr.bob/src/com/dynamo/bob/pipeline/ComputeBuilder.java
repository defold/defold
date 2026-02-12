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

import com.google.protobuf.TextFormat;

import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.ProtoParams;
import com.dynamo.render.proto.Compute.ComputeDesc;
import com.dynamo.render.proto.Material.MaterialDesc;

// For tests
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.Reader;
import java.io.OutputStream;

@ProtoParams(srcClass = ComputeDesc.class, messageClass = ComputeDesc.class)
@BuilderParams(name = "Compute", inExts = {".compute"}, outExt = ".computec")
public class ComputeBuilder extends ProtoBuilder<ComputeDesc.Builder> {

    private static void buildSamplers(ComputeDesc.Builder computeBuilder) throws CompileExceptionError {
        for (int i=0; i < computeBuilder.getSamplersCount(); i++) {
            MaterialDesc.Sampler sampler = computeBuilder.getSamplers(i);
            computeBuilder.setSamplers(i, GraphicsUtil.buildSampler(sampler));
        }
    }

    private IResource getShaderProgram(ComputeDesc.Builder fromBuilder) {
        String shaderPath = ResourceUtil.replaceExt(fromBuilder.getComputeProgram(), ".cp", ShaderProgramBuilderBundle.EXT);
        return this.project.getResource(shaderPath);
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        ComputeDesc.Builder computeBuilder = getSrcBuilder(input);

        // The material should depend on the finally built shader resource file
        // that is a combination of one or more shader modules
        IResource shaderResourceOut = getShaderProgram(computeBuilder);

        ShaderProgramBuilderBundle.ModuleBundle modules = ShaderProgramBuilderBundle.createBundle();
        modules.addModule(computeBuilder.getComputeProgram());
        shaderResourceOut.setContent(modules.toByteArray());

        Task.TaskBuilder computeTaskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        createSubTask(shaderResourceOut, computeTaskBuilder);

        return computeTaskBuilder.build();
    }


    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        IResource res = task.firstInput();
        ComputeDesc.Builder computeBuilder = getSrcBuilder(res);

        BuilderUtil.checkResource(this.project, res, "compute program", computeBuilder.getComputeProgram());
        IResource shaderResourceOut = getShaderProgram(computeBuilder);

        computeBuilder.setComputeProgram(ResourceUtil.changeExt(shaderResourceOut.getPath(), ".spc"));

        buildSamplers(computeBuilder);

        ComputeDesc ComputeDesc = computeBuilder.build();
        task.output(0).setContent(ComputeDesc.toByteArray());
    }

    // Running standalone:
    // java -classpath $DYNAMO_HOME/share/java/bob-light.jar com.dynamo.bob.pipeline.computeBuilder <path-in.compute> <path-in.compute_shader> <path-out.computec>
    public static void main(String[] args) throws IOException, CompileExceptionError {

        System.setProperty("java.awt.headless", "true");

        String pathIn = args[0];
        String nameSpc = args[1];
        String pathOut = args[2];

        try (Reader reader = new BufferedReader(new FileReader(pathIn)); OutputStream output = new BufferedOutputStream(new FileOutputStream(pathOut))) {
            ComputeDesc.Builder computeBuilder = ComputeDesc.newBuilder();
            TextFormat.merge(reader, computeBuilder);

            computeBuilder.setComputeProgram(nameSpc);

            buildSamplers(computeBuilder);

            ComputeDesc ComputeDesc = computeBuilder.build();
            ComputeDesc.writeTo(output);
        }
    }
}
