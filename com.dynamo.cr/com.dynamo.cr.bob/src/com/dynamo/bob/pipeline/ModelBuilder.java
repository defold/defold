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
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

import com.dynamo.gamesys.proto.ModelProto.Model;
import com.dynamo.gamesys.proto.ModelProto.ModelDesc;
import com.dynamo.rig.proto.Rig.RigScene;
import com.google.protobuf.TextFormat;


@BuilderParams(name="Model", inExts=".model", outExt=".modelc")
public class ModelBuilder extends Builder<Void> {


    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        ModelDesc.Builder modelDescBuilder = ModelDesc.newBuilder();
        ProtoUtil.merge(input, modelDescBuilder);

        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);
        taskBuilder.addOutput(input.changeExt(params.outExt()));
        taskBuilder.addOutput(input.changeExt(".rigscenec"));

        IResource mesh = BuilderUtil.checkResource(this.project, input, "mesh", modelDescBuilder.getMesh());
        taskBuilder.addInput(mesh);
        if(!modelDescBuilder.getSkeleton().isEmpty()) {
            IResource skeleton = BuilderUtil.checkResource(this.project, input, "skeleton", modelDescBuilder.getSkeleton());
            taskBuilder.addInput(skeleton);
        }
        // Check if it's an individual file or not
        if((!modelDescBuilder.getAnimations().isEmpty()) && !modelDescBuilder.getAnimations().endsWith(".animationset")) {
            IResource animations = BuilderUtil.checkResource(this.project, input, "animation", modelDescBuilder.getAnimations());
            taskBuilder.addInput(animations);
        }
        for (String t : modelDescBuilder.getTexturesList()) {
            IResource res = BuilderUtil.checkResource(this.project, input, "texture", t);
            Task<?> embedTask = this.project.createTask(res);
            if (embedTask == null) {
                throw new CompileExceptionError(input,
                                                0,
                                                String.format("Failed to create build task for component '%s'", res.getPath()));
            }
        }
        return taskBuilder.build();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        ByteArrayInputStream model_is = new ByteArrayInputStream(task.input(0).getContent());
        InputStreamReader model_isr = new InputStreamReader(model_is);
        ModelDesc.Builder modelDescBuilder = ModelDesc.newBuilder();
        TextFormat.merge(model_isr, modelDescBuilder);

        // Rigscene
        RigScene.Builder rigBuilder = RigScene.newBuilder();
        rigBuilder.setMeshSet(BuilderUtil.replaceExt(modelDescBuilder.getMesh(), ".meshsetc"));
        if(!modelDescBuilder.getSkeleton().isEmpty()) {
            rigBuilder.setSkeleton(BuilderUtil.replaceExt(modelDescBuilder.getSkeleton(), ".skeletonc"));
        }

        if (modelDescBuilder.getAnimations().equals("")) {
            // No animations
        }
        else if(modelDescBuilder.getAnimations().endsWith(".animationset")) {
            // if an animsetdesc file is animation input, use animations and skeleton from that file(s) and other related data (weights, boneindices..) from the mesh collada file
            rigBuilder.setAnimationSet(BuilderUtil.replaceExt(modelDescBuilder.getAnimations(), ".animationsetc"));
        }
        else if(!modelDescBuilder.getAnimations().isEmpty()) {
            // if a collada file is animation input, use animations from that file and other related data (weights, boneindices..) from the mesh collada file
            // we define this a generated file as the animation set does not come from an .animationset resource, but is exported directly from this collada file
            // and because we also avoid possible resource name collision (ref: atlas <-> texture).
            rigBuilder.setAnimationSet(BuilderUtil.replaceExt(modelDescBuilder.getAnimations(), "_generated_0.animationsetc"));
        } else {
            throw new CompileExceptionError(task.input(0), -1, "No animation set in model!");
        }

        rigBuilder.setTextureSet(""); // this is set in the model
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        rigBuilder.build().writeTo(out);
        out.close();
        task.output(1).setContent(out.toByteArray());

        // Model
        IResource resource = task.input(0);
        Model.Builder model = Model.newBuilder();
        model.setRigScene(task.output(1).getPath().replace(this.project.getBuildDirectory(), ""));

        BuilderUtil.checkResource(this.project, resource, "material", modelDescBuilder.getMaterial());
        model.setMaterial(BuilderUtil.replaceExt(modelDescBuilder.getMaterial(), ".material", ".materialc"));

        List<String> newTextureList = new ArrayList<String>();
        for (String t : modelDescBuilder.getTexturesList()) {
            BuilderUtil.checkResource(this.project, resource, "texture", t);
            newTextureList.add(ProtoBuilders.replaceTextureName(t));
        }
        model.addAllTextures(newTextureList);
        model.setDefaultAnimation(modelDescBuilder.getDefaultAnimation());

        out = new ByteArrayOutputStream(64 * 1024);
        model.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}




