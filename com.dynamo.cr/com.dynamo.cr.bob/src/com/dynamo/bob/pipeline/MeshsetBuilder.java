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

import java.util.Objects;

// https://lwjglgamedev.gitbooks.io/3d-game-development-with-lwjgl/content/chapter27/chapter27.html
// https://javadoc.lwjgl.org/index.html?org/lwjgl/assimp/Assimp.html
import java.util.*; //need this??
import org.lwjgl.PointerBuffer;
import org.lwjgl.assimp.*;
import static org.lwjgl.assimp.Assimp.*;

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


@BuilderParams(name="Meshset", inExts={".dae",".fbx"}, outExt=".meshsetc")
public class MeshsetBuilder extends Builder<Void>  {

    private static final String USE_JAGATOO=System.getenv("USE_JAGATOO");

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);

System.out.printf("MESHSET: %s\n", input.getAbsPath());

        taskBuilder.addOutput(input.changeExt(params.outExt()));
        taskBuilder.addOutput(input.changeExt(".skeletonc"));
        taskBuilder.addOutput(input.changeExt("_generated_0.animationsetc"));
        return taskBuilder.build();
    }

    private void buildJagatooCollada(Task<Void> task) throws CompileExceptionError, IOException {

        System.out.printf("jagatoo: Collada file: %s", task.input(0).getAbsPath());

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

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {

        if (USE_JAGATOO != null)
        {
            buildJagatooCollada(task);
            return;
        }

        String suffix = BuilderUtil.getSuffix(task.input(0).getPath());
        AIScene aiScene = ModelUtil.loadScene(task.input(0).getContent(), suffix);
        if (aiScene == null) {
            throw new CompileExceptionError(task.input(0), -1, "Error loading model");
        }

        // MeshSet
        {
            MeshSet.Builder meshSetBuilder = MeshSet.newBuilder();
            ModelUtil.loadMeshes(aiScene, meshSetBuilder);

            ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
            meshSetBuilder.build().writeTo(out);
            out.close();
            task.output(0).setContent(out.toByteArray());
        }

        // Skeleton
        {
            Skeleton.Builder skeletonBuilder = Skeleton.newBuilder();
            try {
                ModelUtil.loadSkeleton(aiScene, skeletonBuilder);
            } catch (LoaderException e) {
                throw new CompileExceptionError(task.input(0), -1, "Failed to compile skeleton: " + e.getLocalizedMessage(), e);
            }

            ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
            skeletonBuilder.build().writeTo(out);
            out.close();
            task.output(1).setContent(out.toByteArray());
        }

        // Animationset
        {
            ArrayList<ModelUtil.Bone> skeleton = ModelUtil.loadSkeleton(aiScene);
            AnimationSet.Builder animationSetBuilder = AnimationSet.newBuilder();
            ModelUtil.loadAnimations(aiScene, skeleton, animationSetBuilder, FilenameUtils.getBaseName(task.input(0).getPath()), new ArrayList<String>());

            ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
            animationSetBuilder.build().writeTo(out);
            out.close();
            task.output(2).setContent(out.toByteArray());
        }

        aiReleaseImport(aiScene);
    }
}




