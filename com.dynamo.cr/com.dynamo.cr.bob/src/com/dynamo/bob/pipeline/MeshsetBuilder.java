// Copyright 2020-2022 The Defold Foundation
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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;

import javax.xml.stream.XMLStreamException;

import org.apache.commons.io.FilenameUtils;

import java.util.Objects;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.nio.channels.FileChannel;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Skeleton;


@BuilderParams(name="Meshset", inExts={".dae",".gltf",".glb"}, outExt=".meshsetc")
public class MeshsetBuilder extends Builder<Void>  {

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

    public void buildCollada(Task<Void> task) throws CompileExceptionError, IOException {
        // Previously ColladaModelBuilder.java
        ByteArrayInputStream collada_is = new ByteArrayInputStream(task.input(0).getContent());

        // MeshSet
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        MeshSet.Builder meshSetBuilder = MeshSet.newBuilder();

        boolean split_meshes = this.project.getProjectProperties().getIntValue("model", "split_large_meshes", 0) != 0;
        try {
            ColladaUtil.loadMesh(collada_is, meshSetBuilder, true, split_meshes);
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

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {

        String suffix = BuilderUtil.getSuffix(task.input(0).getPath());

        if (suffix.equals("dae")) {
            buildCollada(task); // Until our model importer supports collada
            return;
        }

        ModelImporter.Options options = new ModelImporter.Options();
        ModelImporter.Scene scene = ModelUtil.loadScene(task.input(0).getContent(), task.input(0).getPath(), options);
        if (scene == null) {
            throw new CompileExceptionError(task.input(0), -1, "Error loading model");
        }

        // MeshSet
        {
            MeshSet.Builder meshSetBuilder = MeshSet.newBuilder();

            int split_meshes = this.project.getProjectProperties().getIntValue("model", "split_large_meshes", 0);
            if (split_meshes != 0) {
                ModelUtil.splitMeshes(scene);
            }

            ModelUtil.loadModels(scene, meshSetBuilder);

            ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
            meshSetBuilder.build().writeTo(out);
            out.close();
            task.output(0).setContent(out.toByteArray());
        }

        // Skeleton
        {
            Skeleton.Builder skeletonBuilder = Skeleton.newBuilder();
            if (ModelUtil.getNumSkins(scene) > 0)
            {
                if (!ModelUtil.loadSkeleton(scene, skeletonBuilder))
                {
                    throw new CompileExceptionError(task.input(0), -1, "Failed to load skeleton");
                }
            }

            ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
            skeletonBuilder.build().writeTo(out);
            out.close();
            task.output(1).setContent(out.toByteArray());
        }

        // Animationset
        {
            ArrayList<ModelImporter.Bone> skeleton = ModelUtil.loadSkeleton(scene);

            AnimationSet.Builder animationSetBuilder = AnimationSet.newBuilder();
            if (skeleton.size() > 0) {
                ModelUtil.loadAnimations(scene, skeleton, animationSetBuilder, FilenameUtils.getBaseName(task.input(0).getPath()), new ArrayList<String>());
            }

            ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
            animationSetBuilder.build().writeTo(out);
            out.close();
            task.output(2).setContent(out.toByteArray());
        }

        ModelUtil.unloadScene(scene);
    }
}




