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
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);
        taskBuilder.addOutput(input.changeExt(params.outExt()));
        taskBuilder.addOutput(input.changeExt(".rigscenec"));
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
        IResource resource = task.input(0);

        BuilderUtil.checkResource(this.project, resource, "model", modelDescBuilder.getMesh());
        rigBuilder.setMeshSet(BuilderUtil.replaceExt(modelDescBuilder.getMesh(), ".dae", ".meshsetc"));

        if(!modelDescBuilder.getSkeleton().isEmpty()) {
            BuilderUtil.checkResource(this.project, resource, "skeleton", modelDescBuilder.getSkeleton());
            rigBuilder.setSkeleton(BuilderUtil.replaceExt(modelDescBuilder.getSkeleton(), ".dae", ".skeletonc"));
        }

        if(!modelDescBuilder.getAnimations().isEmpty()) {
            BuilderUtil.checkResource(this.project, resource, "animations", modelDescBuilder.getAnimations());
            rigBuilder.setAnimationSet(BuilderUtil.replaceExt(modelDescBuilder.getAnimations(), ".dae", ".animationsetc"));
        }

        rigBuilder.setTextureSet(""); // this is set in the model
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        rigBuilder.build().writeTo(out);
        out.close();
        task.output(1).setContent(out.toByteArray());

        // Model
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




