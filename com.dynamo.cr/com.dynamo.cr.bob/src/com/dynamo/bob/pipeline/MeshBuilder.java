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

import com.dynamo.mesh.proto.MeshProto.MeshDesc;
import com.google.protobuf.TextFormat;


@BuilderParams(name="Mesh", inExts=".mesh", outExt=".meshc")
public class MeshBuilder extends Builder<Void> {


    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        MeshDesc.Builder meshDescBuilder = MeshDesc.newBuilder();
        ProtoUtil.merge(input, meshDescBuilder);

        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);
        taskBuilder.addOutput(input.changeExt(params.outExt()));

        IResource res = BuilderUtil.checkResource(this.project, input, "vertices", meshDescBuilder.getVertices());
        taskBuilder.addInput(res);
        res = BuilderUtil.checkResource(this.project, input, "material", meshDescBuilder.getMaterial());
        taskBuilder.addInput(res);

        return taskBuilder.build();
    }


    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        ByteArrayInputStream mesh_is = new ByteArrayInputStream(task.input(0).getContent());
        InputStreamReader mesh_isr = new InputStreamReader(mesh_is);
        MeshDesc.Builder meshDescBuilder = MeshDesc.newBuilder();
        TextFormat.merge(mesh_isr, meshDescBuilder);

        IResource resource = task.input(0);
        BuilderUtil.checkResource(this.project, resource, "vertices", meshDescBuilder.getVertices());
        meshDescBuilder.setVertices(BuilderUtil.replaceExt(meshDescBuilder.getVertices(), ".buffer", ".bufferc"));
        BuilderUtil.checkResource(this.project, resource, "material", meshDescBuilder.getMaterial());
        meshDescBuilder.setMaterial(BuilderUtil.replaceExt(meshDescBuilder.getMaterial(), ".material", ".materialc"));

        List<String> newTextureList = new ArrayList<String>();
        for (String t : meshDescBuilder.getTexturesList()) {
            BuilderUtil.checkResource(this.project, resource, "texture", t);
            newTextureList.add(ProtoBuilders.replaceTextureName(t));
        }
        meshDescBuilder.clearTextures();
        meshDescBuilder.addAllTextures(newTextureList);

        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        meshDescBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}




