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

import com.dynamo.model.proto.Model.ModelDesc;
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
        ByteArrayOutputStream out;

        ByteArrayInputStream model_is = new ByteArrayInputStream(task.input(0).getContent());
        InputStreamReader model_isr = new InputStreamReader(model_is);
        ModelDesc.Builder modelBuilder = ModelDesc.newBuilder();
        TextFormat.merge(model_isr, modelBuilder);

        // Model
        IResource resource = task.input(0);
        BuilderUtil.checkResource(this.project, resource, "model", modelBuilder.getMesh());
        modelBuilder.setMesh(BuilderUtil.replaceExt(modelBuilder.getMesh(), ".dae", ".meshsetc"));

        if(modelBuilder.getSkeleton().isEmpty()) {
            modelBuilder.setSkeleton(BuilderUtil.replaceExt(modelBuilder.getMesh(), ".meshsetc", ".skeletonc"));
        } else {
            BuilderUtil.checkResource(this.project, resource, "skeleton", modelBuilder.getSkeleton());
            modelBuilder.setSkeleton(BuilderUtil.replaceExt(modelBuilder.getSkeleton(), ".dae", ".skeletonc"));
        }

        if(modelBuilder.getAnimations().isEmpty()) {
            modelBuilder.setAnimations(BuilderUtil.replaceExt(modelBuilder.getMesh(), ".meshsetc", ".animationsetc"));
        } else {
            BuilderUtil.checkResource(this.project, resource, "animations", modelBuilder.getAnimations());
            modelBuilder.setAnimations(BuilderUtil.replaceExt(modelBuilder.getAnimations(), ".dae", ".animationsetc"));
        }

        BuilderUtil.checkResource(this.project, resource, "material", modelBuilder.getMaterial());
        modelBuilder.setMaterial(BuilderUtil.replaceExt(modelBuilder.getMaterial(), ".material", ".materialc"));

        List<String> newTextureList = new ArrayList<String>();
        for (String t : modelBuilder.getTexturesList()) {
            BuilderUtil.checkResource(this.project, resource, "texture", t);
            newTextureList.add(ProtoBuilders.replaceTextureName(t));
        }
        modelBuilder.clearTextures();
        modelBuilder.addAllTextures(newTextureList);

        out = new ByteArrayOutputStream(64 * 1024);
        ModelDesc modelDesc = modelBuilder.build();
        modelDesc.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());

        // Rigscene
        com.dynamo.rig.proto.Rig.RigScene.Builder rigSceneBuilder = com.dynamo.rig.proto.Rig.RigScene.newBuilder();
        out = new ByteArrayOutputStream(64 * 1024);
        rigSceneBuilder.setMeshSet(modelDesc.getMesh());
        rigSceneBuilder.setSkeleton(modelDesc.getSkeleton());
        rigSceneBuilder.setAnimationSet(modelDesc.getAnimations());
        rigSceneBuilder.setTextureSet(""); // this is set in the model
        rigSceneBuilder.build().writeTo(out);
        out.close();
        task.output(1).setContent(out.toByteArray());
    }
}




