// Copyright 2020 The Defold Foundation
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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;

import javax.xml.stream.XMLStreamException;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Skeleton;


@BuilderParams(name="ColladaModel", inExts=".dae", outExt=".meshsetc")
public class ColladaModelBuilder extends Builder<Void>  {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);
        taskBuilder.addOutput(input.changeExt(params.outExt()));
        taskBuilder.addOutput(input.changeExt(".skeletonc"));
        taskBuilder.addOutput(input.changeExt("_generated_0.animationsetc"));
        return taskBuilder.build();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        ByteArrayInputStream collada_is = new ByteArrayInputStream(task.input(0).getContent());

        // MeshSet
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        MeshSet.Builder meshSetBuilder = MeshSet.newBuilder();
        try {
            ColladaUtil.loadMesh(collada_is, meshSetBuilder, true);
        } catch (XMLStreamException e) {
            throw new CompileExceptionError(task.input(0), e.getLocation().getLineNumber(), "Failed to compile mesh: " + e.getLocalizedMessage(), e);
        } catch (LoaderException e) {
            throw new CompileExceptionError(task.input(0), -1, "Failed to compile mesh: " + e.getLocalizedMessage(), e);
        }
        meshSetBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());

        // Skeleton
        out = new ByteArrayOutputStream(64 * 1024);
        collada_is.reset();
        Skeleton.Builder skeletonBuilder = Skeleton.newBuilder();
        try {
            ColladaUtil.loadSkeleton(collada_is, skeletonBuilder, new ArrayList<String>());
        } catch (XMLStreamException e) {
            throw new CompileExceptionError(task.input(0), e.getLocation().getLineNumber(), "Failed to compile skeleton: " + e.getLocalizedMessage(), e);
        } catch (LoaderException e) {
            throw new CompileExceptionError(task.input(0), -1, "Failed to compile skeleton: " + e.getLocalizedMessage(), e);
        }
        skeletonBuilder.build().writeTo(out);
        out.close();
        task.output(1).setContent(out.toByteArray());

        // Animationset
        out = new ByteArrayOutputStream(64 * 1024);
        collada_is.reset();
        AnimationSet.Builder animationSetBuilder = AnimationSet.newBuilder();
        try {
            ColladaUtil.loadAnimations(collada_is, animationSetBuilder, FilenameUtils.getBaseName(task.input(0).getPath()), new ArrayList<String>());
        } catch (XMLStreamException e) {
            throw new CompileExceptionError(task.input(0), e.getLocation().getLineNumber(), "Failed to compile animation: " + e.getLocalizedMessage(), e);
        } catch (LoaderException e) {
            throw new CompileExceptionError(task.input(0), -1, "Failed to compile animation: " + e.getLocalizedMessage(), e);
        }
        animationSetBuilder.build().writeTo(out);
        out.close();
        task.output(2).setContent(out.toByteArray());

    }
}




