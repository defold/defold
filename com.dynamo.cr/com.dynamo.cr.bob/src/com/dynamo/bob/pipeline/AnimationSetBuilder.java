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

import java.io.File;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;
import java.util.LinkedHashSet;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.AnimationSetDesc;
import com.dynamo.rig.proto.Rig.AnimationInstanceDesc;
import com.dynamo.rig.proto.Rig.RigAnimation;
import com.google.protobuf.TextFormat;

@BuilderParams(name="AnimationSet", inExts=".animationset", outExt=".animationsetc", isCacheble = true)
public class AnimationSetBuilder extends Builder  {

    private static final String GENERATED_ANIMATION_SET_EXT = "_generated_0.animationsetc";

    public static void collectAnimations(Task.TaskBuilder taskBuilder, Project project, IResource owner, AnimationSetDesc.Builder animSetDescBuilder) throws IOException, CompileExceptionError  {
        for(AnimationInstanceDesc instance : animSetDescBuilder.getAnimationsList()) {
            IResource animFile = BuilderUtil.checkResource(project, owner, "animationset", instance.getAnimation());

            if(instance.getAnimation().endsWith(".animationset")) {
                taskBuilder.addInput(animFile);
                ByteArrayInputStream animFileIS = new ByteArrayInputStream(animFile.getContent());
                InputStreamReader subAnimSetDescBuilderISR = new InputStreamReader(animFileIS);
                AnimationSetDesc.Builder subAnimSetDescBuilder = AnimationSetDesc.newBuilder();
                TextFormat.merge(subAnimSetDescBuilderISR, subAnimSetDescBuilder);
                collectAnimations(taskBuilder, project, owner, subAnimSetDescBuilder);
            } else {
                String suffix = BuilderUtil.getSuffix(animFile.getPath());
                if (suffix.equals("gltf") || suffix.equals("glb")) {
                    Task meshsetTask = project.createTask(animFile, MeshsetBuilder.class);
                    IResource compiledAnimations = meshsetTask.output(2);
                    if (compiledAnimations == null) {
                        throw new CompileExceptionError(animFile, 0, "Meshset task has no compiled animation output");
                    }
                    taskBuilder.addInput(compiledAnimations);
                } else {
                    // Preserve the existing build-time unsupported-format diagnostic.
                    taskBuilder.addInput(animFile);
                }
            }
        }
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
            .setName(params.name())
            .addInput(input)
            .addOutput(input.changeExt(params.outExt()));

        if( input.getAbsPath().endsWith(".animationset") ) {
            ByteArrayInputStream animFileIS = new ByteArrayInputStream(input.getContent());
            InputStreamReader animSetDescBuilderISR = new InputStreamReader(animFileIS);
            AnimationSetDesc.Builder animSetDescBuilder = AnimationSetDesc.newBuilder();
            TextFormat.merge(animSetDescBuilderISR, animSetDescBuilder);

            AnimationSetBuilder.collectAnimations(taskBuilder, this.project, input, animSetDescBuilder);
        }

        return taskBuilder.build();
    }

    private void validateAndAddFile(Task task, String path, ArrayList<String> animFiles) throws CompileExceptionError {
        if(animFiles.contains(path)) {
            throw new CompileExceptionError(task.input(0), -1, "Animation file referenced more than once: " + path);
        }
        animFiles.add(path);
    }

    private static RigAnimation getLongestAnimation(AnimationSet animationSet) {
        RigAnimation longestAnimation = null;
        for (RigAnimation animation : animationSet.getAnimationsList()) {
            if (longestAnimation == null || animation.getDuration() > longestAnimation.getDuration()) {
                longestAnimation = animation;
            }
        }
        return longestAnimation;
    }

    private static void addCompiledAnimations(AnimationSet.Builder animationSetBuilder, AnimationSet compiledAnimations,
                                              boolean isAnimationSet, String animId) {
        if (!isAnimationSet) {
            animationSetBuilder.addAllAnimations(compiledAnimations.getAnimationsList());
            return;
        }

        RigAnimation longestAnimation = getLongestAnimation(compiledAnimations);
        if (longestAnimation != null) {
            animationSetBuilder.addAnimations(longestAnimation.toBuilder()
                    .setId(MurmurHash.hash64(animId))
                    .build());
        }
    }

    private void buildAnimations(Task task, boolean isAnimationSet, AnimationSetDesc.Builder animSetDescBuilder, AnimationSet.Builder animationSetBuilder,
                                            String parentId, ArrayList<String> animFiles) throws CompileExceptionError, IOException {
        ArrayList<String> idList = new ArrayList<>(animSetDescBuilder.getAnimationsCount());

        for(AnimationInstanceDesc instance : animSetDescBuilder.getAnimationsList()) {
            if(instance.getAnimation().endsWith(".animationset")) {
                IResource animFile = BuilderUtil.checkResource(this.project, task.input(0), "animationset", instance.getAnimation());
                validateAndAddFile(task, animFile.getAbsPath(), animFiles);
                ByteArrayInputStream animFileIS = new ByteArrayInputStream(animFile.getContent());
                InputStreamReader subAnimSetDescBuilderISR = new InputStreamReader(animFileIS);
                AnimationSetDesc.Builder subAnimSetDescBuilder = AnimationSetDesc.newBuilder();
                TextFormat.merge(subAnimSetDescBuilderISR, subAnimSetDescBuilder);
                buildAnimations(task, true, subAnimSetDescBuilder, animationSetBuilder, FilenameUtils.getBaseName(animFile.getPath()), animFiles);
                continue;
            }
            IResource animFile = BuilderUtil.checkResource(this.project, task.input(0), "animation", instance.getAnimation());
            validateAndAddFile(task, animFile.getAbsPath(), animFiles);

            // Previously we only allowed a model file to contain a single animation,
            // but now we grab all animations inside it.
            // However, for .animationset files we still expect each model file to contain one animation

            String animId = (parentId.isEmpty() ? "" : parentId + "/" ) + FilenameUtils.getBaseName(animFile.getPath());
            if(idList.contains(animId)) {
                throw new CompileExceptionError(task.input(0), -1, "Animation id already exists: " + animId);
            }
            idList.add(animId);

            String suffix = BuilderUtil.getSuffix(animFile.getPath());
            if (!suffix.equals("gltf") && !suffix.equals("glb")) {
                throw new CompileExceptionError(animFile, -1, "Unsupported animation format '." + suffix + "'");
            }

            try {
                IResource compiledAnimationsResource = animFile.changeExt(GENERATED_ANIMATION_SET_EXT);
                AnimationSet compiledAnimations = AnimationSet.parseFrom(compiledAnimationsResource.getContent());
                addCompiledAnimations(animationSetBuilder, compiledAnimations, isAnimationSet, animId);
            } catch (IOException e) {
                throw new CompileExceptionError(animFile, -1, e.getMessage(), e);
            }
        }
    }

// For the Editor

    static public AnimationSetDesc getAnimationSetDesc(InputStream animationSetStream) throws IOException {
        InputStreamReader animSetDescISR = new InputStreamReader(animationSetStream);
        AnimationSetDesc.Builder animSetDescBuilder = AnimationSetDesc.newBuilder();
        TextFormat.merge(animSetDescISR, animSetDescBuilder);
        return animSetDescBuilder.build();
    }

    private static ArrayList<String> makeUnique(List<String> items) {
        LinkedHashSet<String> unique = new LinkedHashSet<>(items);
        return new ArrayList<>(unique);
    }

    static public ArrayList<String> getAnimationsPaths(AnimationSetDesc desc) {
        ArrayList<String> animations = new ArrayList<String>();
        for(AnimationInstanceDesc instance : desc.getAnimationsList()) {
            animations.add(instance.getAnimation());
        }
        return makeUnique(animations);
    }

    static void loadModelAnimations(boolean isAnimationSet, AnimationSet.Builder animationSetBuilder,
                                    InputStream is, ModelImporterJni.DataResolver dataResolver, String animId, String parentId,
                                    String path, ArrayList<String> animationIds) throws IOException {

        Modelimporter.Scene scene = ModelUtil.loadScene(is, path, new Modelimporter.Options(), dataResolver);

        ArrayList<String> localAnimationIds = new ArrayList<String>();
        AnimationSet.Builder animBuilder = AnimationSet.newBuilder();

        // Currently, by design choice (for animation sets), each file must only contain one animation.
        // Our current approach is to choose the longest animation (to eliminate target poses etc)
        boolean topLevel = parentId.isEmpty();
        ModelUtil.loadAnimations(scene, animBuilder, isAnimationSet ? animId : "", localAnimationIds);

        animationSetBuilder.addAllAnimations(animBuilder.getAnimationsList());

        ModelUtil.unloadScene(scene);
    }

    public static class ResourceDataResolver implements ModelImporterJni.DataResolver
    {
        Project project;

        public ResourceDataResolver(Project project) {
            this.project = project;
        }

        public byte[] getData(String path, String uri) {
            File file = new File(path);
            File bufferFile = new File(file.getParentFile(), uri);
            IResource resource = project.getResource(bufferFile.getPath());
            if (resource == null)
            {
                System.out.printf("Failed to find data for %s\n", bufferFile.getPath());
                return null;
            }
            try {
                return resource.getContent();
            } catch (IOException e) {
                return null; // Actual errors are reported by ModelUtil.loadScene
            }
        }
    };

    // For the editor
    static public void buildAnimations(boolean isAnimationSet, List<String> paths, List<InputStream> streams, ModelImporterJni.DataResolver dataResolver, List<String> parentIds,
                             AnimationSet.Builder animationSetBuilder, ArrayList<String> animationIds) throws IOException, CompileExceptionError {


        int count = paths.size();
        for (int i = 0; i < count; ++i) {
            String path = paths.get(i);
            InputStream stream = streams.get(i);
            String parentId = parentIds.get(i);

            String baseName = FilenameUtils.getBaseName(path);

            // Previously we only allowed a model file to contain a single animation,
            // but now we grab all animations inside it.
            // However, for .animationset files we still expect each model file to contain one animation

            String animId = (parentId.isEmpty() ? "" : parentId + "/" ) + baseName;
            if(animationIds.contains(animId)) {
                throw new CompileExceptionError(String.format("Animation set contains duplicate entries for animation id '%s'", animId));
            }
            animationIds.add(animId);

            String suffix = BuilderUtil.getSuffix(path);
            if (!suffix.equals("gltf") && !suffix.equals("glb")) {
                throw new CompileExceptionError(String.format("Unsupported animation format '.%s' in animation set: %s", suffix, path));
            }

            try {
                loadModelAnimations(isAnimationSet, animationSetBuilder, stream, dataResolver, animId, parentId, path, animationIds);
            } catch (IOException e) {
                throw new CompileExceptionError(String.format("File %s:%d: Failed to load animation: %s", path, -1, e.getMessage()), e);
            }
        }
    }
// END EDITOR SPECIFIC FUNCTIONS

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {

        ByteArrayInputStream animSetDescIS = new ByteArrayInputStream(task.input(0).getContent());
        InputStreamReader animSetDescISR = new InputStreamReader(animSetDescIS);
        AnimationSetDesc.Builder animSetDescBuilder = AnimationSetDesc.newBuilder();
        TextFormat.merge(animSetDescISR, animSetDescBuilder);

        // evaluate hierarchy
        AnimationSet.Builder animationSetBuilder = AnimationSet.newBuilder();

        ArrayList<String> animFiles = new ArrayList<String>();
        animFiles.add(task.input(0).getAbsPath());

        String suffix = BuilderUtil.getSuffix(task.input(0).getPath());
        buildAnimations(task, suffix.equals("animationset"), animSetDescBuilder, animationSetBuilder, "", animFiles);

        // write merged animationset
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        animationSetBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}
