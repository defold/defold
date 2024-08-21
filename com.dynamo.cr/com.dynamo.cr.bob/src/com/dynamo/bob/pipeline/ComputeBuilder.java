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

import com.google.protobuf.TextFormat;

import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
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

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        IResource res = task.firstInput();
        ComputeDesc.Builder computeBuilder = getMessageBuilder(res);

        BuilderUtil.checkResource(this.project, res, "compute program", computeBuilder.getComputeProgram());
        computeBuilder.setComputeProgram(BuilderUtil.replaceExt(computeBuilder.getComputeProgram(), ".cp", ".cpc"));

        buildSamplers(computeBuilder);

        ComputeDesc ComputeDesc = computeBuilder.build();
        task.output(0).setContent(ComputeDesc.toByteArray());
    }

    // Running standalone:
    // java -classpath $DYNAMO_HOME/share/java/bob-light.jar com.dynamo.bob.pipeline.computeBuilder <path-in.compute> <path-out.computec>
    public static void main(String[] args) throws IOException, CompileExceptionError {

        System.setProperty("java.awt.headless", "true");

        Reader reader       = new BufferedReader(new FileReader(args[0]));
        OutputStream output = new BufferedOutputStream(new FileOutputStream(args[1]));

        try {
            ComputeDesc.Builder computeBuilder = ComputeDesc.newBuilder();
            TextFormat.merge(reader, computeBuilder);

            computeBuilder.setComputeProgram(BuilderUtil.replaceExt(computeBuilder.getComputeProgram(), ".cp", ".cpc"));

            buildSamplers(computeBuilder);

            ComputeDesc ComputeDesc = computeBuilder.build();
            ComputeDesc.writeTo(output);
        } finally {
            reader.close();
            output.close();
        }
    }
}
