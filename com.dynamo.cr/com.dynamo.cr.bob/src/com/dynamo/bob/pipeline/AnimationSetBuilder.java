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
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;
import java.util.LinkedHashSet;
import java.util.Map;

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
import com.google.protobuf.TextFormat;

import javax.xml.stream.XMLStreamException;

@BuilderParams(name="AnimationSet", inExts=".animationset", outExt=".animationsetc")
public class AnimationSetBuilder extends Builder<Void>  {

    public static void collectAnimations(Task.TaskBuilder<Void> taskBuilder, Project project, IResource owner, AnimationSetDesc.Builder animSetDescBuilder) throws IOException, CompileExceptionError  {
        for(AnimationInstanceDesc instance : animSetDescBuilder.getAnimationsList()) {
            IResource animFile = BuilderUtil.checkResource(project, owner, "animationset", instance.getAnimation());
            taskBuilder.addInput(animFile);

            if(instance.getAnimation().endsWith(".animationset")) {
                ByteArrayInputStream animFileIS = new ByteArrayInputStream(animFile.getContent());
                InputStreamReader subAnimSetDescBuilderISR = new InputStreamReader(animFileIS);
                AnimationSetDesc.Builder subAnimSetDescBuilder = AnimationSetDesc.newBuilder();
                TextFormat.merge(subAnimSetDescBuilderISR, subAnimSetDescBuilder);
                collectAnimations(taskBuilder, project, owner, subAnimSetDescBuilder);
            }
        }

        if(!animSetDescBuilder.getSkeleton().isEmpty()) {
            IResource skeleton = BuilderUtil.checkResource(project, owner, "skeleton", animSetDescBuilder.getSkeleton());
            taskBuilder.addInput(skeleton);
        }
    }


    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);
        taskBuilder.addOutput(input.changeExt(params.outExt()));

        if( input.getAbsPath().endsWith(".animationset") ) {
            ByteArrayInputStream animFileIS = new ByteArrayInputStream(input.getContent());
            InputStreamReader animSetDescBuilderISR = new InputStreamReader(animFileIS);
            AnimationSetDesc.Builder animSetDescBuilder = AnimationSetDesc.newBuilder();
            TextFormat.merge(animSetDescBuilderISR, animSetDescBuilder);

            AnimationSetBuilder.collectAnimations(taskBuilder, this.project, input, animSetDescBuilder);
        }

        return taskBuilder.build();
    }

    private void validateAndAddFile(Task<Void> task, String path, ArrayList<String> animFiles) throws CompileExceptionError {
        if(animFiles.contains(path)) {
            throw new CompileExceptionError(task.input(0), -1, "Animation file referenced more than once: " + path);
        }
        animFiles.add(path);
    }

    private void buildAnimations(Task<Void> task, AnimationSetDesc.Builder animSetDescBuilder, AnimationSet.Builder animationSetBuilder,
                                            String parentId, ArrayList<ModelImporter.Bone> bones, ArrayList<String> animFiles) throws CompileExceptionError, IOException {
        ArrayList<String> idList = new ArrayList<>(animSetDescBuilder.getAnimationsCount());

        for(AnimationInstanceDesc instance : animSetDescBuilder.getAnimationsList()) {
            if(instance.getAnimation().endsWith(".animationset")) {
                IResource animFile = BuilderUtil.checkResource(this.project, task.input(0), "animationset", instance.getAnimation());
                validateAndAddFile(task, animFile.getAbsPath(), animFiles);
                ByteArrayInputStream animFileIS = new ByteArrayInputStream(animFile.getContent());
                InputStreamReader subAnimSetDescBuilderISR = new InputStreamReader(animFileIS);
                AnimationSetDesc.Builder subAnimSetDescBuilder = AnimationSetDesc.newBuilder();
                TextFormat.merge(subAnimSetDescBuilderISR, subAnimSetDescBuilder);
                buildAnimations(task, subAnimSetDescBuilder, animationSetBuilder, FilenameUtils.getBaseName(animFile.getPath()), bones, animFiles);
                continue;
            }
            IResource animFile = BuilderUtil.checkResource(this.project, task.input(0), "animation", instance.getAnimation());
            validateAndAddFile(task, animFile.getAbsPath(), animFiles);

            String animId = (parentId.isEmpty() ? "" : parentId + "/" ) + FilenameUtils.getBaseName(animFile.getPath());
            if(idList.contains(animId)) {
                throw new CompileExceptionError(task.input(0), -1, "Animation id already exists: " + animId);
            }
            idList.add(animId);

            ByteArrayInputStream animFileIS = new ByteArrayInputStream(animFile.getContent());
            AnimationSet.Builder animBuilder = AnimationSet.newBuilder();
            ArrayList<String> animationIds = new ArrayList<String>();

            String suffix = BuilderUtil.getSuffix(animFile.getPath());
            boolean isCollada = suffix.equals("dae");

            try {
                if (isCollada)
                    loadColladaAnimations(animBuilder, animFileIS, animId, parentId);
                else
                    loadModelAnimations(bones, animBuilder, animFileIS, animId, parentId, animFile.getPath(), animationIds);

            } catch (XMLStreamException e) {
                throw new CompileExceptionError(animFile, e.getLocation().getLineNumber(), "Failed to load animation: " + e.getLocalizedMessage(), e);
            } catch (LoaderException e) {
                throw new CompileExceptionError(animFile, -1, "Failed to load animation: " + e.getLocalizedMessage(), e);
            }

            animationSetBuilder.addAllAnimations(animBuilder.getAnimationsList());
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

    static void loadColladaAnimations(AnimationSet.Builder animationSetBuilder, InputStream is, String animId, String parentId)
    throws IOException, XMLStreamException, LoaderException {
        ArrayList<String> localAnimationIds = new ArrayList<String>();
        AnimationSet.Builder animBuilder = AnimationSet.newBuilder();
        ColladaUtil.loadAnimations(is, animBuilder, animId, localAnimationIds);

        animationSetBuilder.addAllAnimations(animBuilder.getAnimationsList());
    }

    static void loadModelAnimations(ArrayList<ModelImporter.Bone> bones, AnimationSet.Builder animationSetBuilder, InputStream is, String animId, String parentId,
                                                            String path, ArrayList<String> animationIds) throws IOException {

        ModelImporter.Scene scene = ModelUtil.loadScene(is, path, new ModelImporter.Options());

        ArrayList<String> localAnimationIds = new ArrayList<String>();
        AnimationSet.Builder animBuilder = AnimationSet.newBuilder();

        // Currently, by design choice, each file must only contain one animation.
        // Our current approach is to choose the longest animation (to eliminate target poses etc)
        ModelUtil.loadAnimations(scene, bones, animBuilder, animId, localAnimationIds);

        animationSetBuilder.addAllAnimations(animBuilder.getAnimationsList());

        ModelUtil.unloadScene(scene);
    }

    static public void buildAnimations(List<String> paths, ArrayList<ModelImporter.Bone> bones, List<InputStream> streams, List<String> parentIds,
                             AnimationSet.Builder animationSetBuilder, ArrayList<String> animationIds) throws IOException, CompileExceptionError {

        int count = paths.size();
        for (int i = 0; i < count; ++i) {
            String path = paths.get(i);
            InputStream stream = streams.get(i);
            String parentId = parentIds.get(i);

            String baseName = FilenameUtils.getBaseName(path);

            String animId = (parentId.isEmpty() ? "" : parentId + "/" ) + baseName;
            if(animationIds.contains(animId)) {
                throw new CompileExceptionError(String.format("Animation set contains duplicate entries for animation id '%s'", animId));
            }
            animationIds.add(animId);

            boolean isCollada = false;
            String suffix = BuilderUtil.getSuffix(path);
            if (suffix.equals("dae")) {
                isCollada = true;
            }

            try {
                if (isCollada)
                    loadColladaAnimations(animationSetBuilder, stream, animId, parentId);
                else
                    loadModelAnimations(bones, animationSetBuilder, stream, animId, parentId, path, animationIds);

            } catch (XMLStreamException e) {
                throw new CompileExceptionError(String.format("File %s:%d: Failed to load animation: %s", path, e.getLocation().getLineNumber(), e.getLocalizedMessage()), e);
            } catch (LoaderException e) {
                throw new CompileExceptionError(String.format("File %s:%d: Failed to load animation: %s", path, -1, e.getLocalizedMessage()), e);
            }

        }
        ModelUtil.setBoneList(animationSetBuilder, bones);
    }
// END EDITOR SPECIFIC FUNCTIONS

    public ArrayList<ModelImporter.Bone> buildSkeleton(IResource skeletonFile) throws IOException {
        String suffix = BuilderUtil.getSuffix(skeletonFile.getPath());
        if (suffix.equals("dae")) {
            return ColladaUtil.loadSkeleton(skeletonFile.getContent()); // Until our model importer supports collada
        }

        ModelImporter.Scene skeletonScene = ModelUtil.loadScene(skeletonFile.getContent(), skeletonFile.getPath(), new ModelImporter.Options());
        ArrayList<ModelImporter.Bone> bones = ModelUtil.loadSkeleton(skeletonScene);

        return bones;
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {

        ByteArrayInputStream animSetDescIS = new ByteArrayInputStream(task.input(0).getContent());
        InputStreamReader animSetDescISR = new InputStreamReader(animSetDescIS);
        AnimationSetDesc.Builder animSetDescBuilder = AnimationSetDesc.newBuilder();
        TextFormat.merge(animSetDescISR, animSetDescBuilder);

        IResource skeletonFile = BuilderUtil.checkResource(this.project, task.input(0), "skeleton", animSetDescBuilder.getSkeleton());

        ArrayList<ModelImporter.Bone> bones = buildSkeleton(skeletonFile);

        if (bones.size() == 0) {
            throw new CompileExceptionError(skeletonFile, -1, "No skeleton found in file!");
        }

        // evaluate hierarchy
        AnimationSet.Builder animationSetBuilder = AnimationSet.newBuilder();

        ArrayList<String> animFiles = new ArrayList<String>();
        animFiles = new ArrayList<String>();
        animFiles.add(task.input(0).getAbsPath());

        buildAnimations(task, animSetDescBuilder, animationSetBuilder, "", bones, animFiles);

        ModelUtil.setBoneList(animationSetBuilder, bones);

        // write merged animationset
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        animationSetBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}

