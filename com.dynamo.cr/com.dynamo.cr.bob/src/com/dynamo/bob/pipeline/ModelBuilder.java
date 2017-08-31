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

import com.dynamo.model.proto.ModelProto.Model;
import com.dynamo.model.proto.ModelProto.ModelDesc;
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
        if((!modelDescBuilder.getAnimations().isEmpty()) && modelDescBuilder.getAnimations().endsWith(".dae")) {
            IResource animations = BuilderUtil.checkResource(this.project, input, "animation", modelDescBuilder.getAnimations());
            taskBuilder.addInput(animations);
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
        rigBuilder.setMeshSet(BuilderUtil.replaceExt(modelDescBuilder.getMesh(), ".dae", ".meshsetc"));
        if(!modelDescBuilder.getSkeleton().isEmpty()) {
            rigBuilder.setSkeleton(BuilderUtil.replaceExt(modelDescBuilder.getSkeleton(), ".dae", ".skeletonc"));
        }
        if(!modelDescBuilder.getAnimations().isEmpty()) {
            if(modelDescBuilder.getAnimations().endsWith(".dae")) {
                // if a collada file is animation input, use animations from that file and other related data (weights, boneindices..) from the mesh collada file
                // we define this a generated file as the animation set does not come from an animset resource, but is exported directly from this collada file
                // and because we alseo avoid possible resource name collision (ref: atlas <-> texture).
                rigBuilder.setAnimationSet(BuilderUtil.replaceExt(modelDescBuilder.getAnimations(), ".dae", "_generated_0.animationsetc"));
            } else if(modelDescBuilder.getAnimations().endsWith(".animationset")) {
                // if an animsetdesc file is animation input, use animations and skeleton from that file(s) and other related data (weights, boneindices..) from the mesh collada file
                rigBuilder.setAnimationSet(BuilderUtil.replaceExt(modelDescBuilder.getAnimations(), ".animationset", ".animationsetc"));
            } else {
                throw new CompileExceptionError(task.input(0), -1, "Unknown animation format: " + modelDescBuilder.getAnimations());
            }

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




