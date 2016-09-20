package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import javax.xml.stream.XMLStreamException;

import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.MeshEntry;
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
        taskBuilder.addOutput(input.changeExt(".animationsetc"));
        return taskBuilder.build();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        ByteArrayOutputStream out;

        // MeshSet
        ByteArrayInputStream mesh_is = new ByteArrayInputStream(task.input(0).getContent());
        Mesh.Builder meshBuilder = Mesh.newBuilder();
        AnimationSet.Builder animSetBuilder = AnimationSet.newBuilder();
        Skeleton.Builder skeletonBuilder = Skeleton.newBuilder();
        try {
            ColladaUtil.load(mesh_is, meshBuilder, animSetBuilder, skeletonBuilder);
        } catch (XMLStreamException e) {
            throw new CompileExceptionError(task.input(0), e.getLocation().getLineNumber(), "Failed to compile mesh: " + e.getLocalizedMessage(), e);
        } catch (LoaderException e) {
            throw new CompileExceptionError(task.input(0), -1, "Failed to compile mesh: " + e.getLocalizedMessage(), e);
        }
        Mesh mesh = meshBuilder.build();
        MeshSet.Builder meshSetBuilder = MeshSet.newBuilder();
        MeshEntry.Builder meshEntryBuilder = MeshEntry.newBuilder();
        meshEntryBuilder.addMeshes(mesh);
        meshEntryBuilder.setId(0);
        meshSetBuilder.addMeshEntries(meshEntryBuilder);
        out = new ByteArrayOutputStream(64 * 1024);
        meshSetBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());

        // Skeleton
        out = new ByteArrayOutputStream(64 * 1024);
        skeletonBuilder.build().writeTo(out);
        out.close();
        task.output(1).setContent(out.toByteArray());

        // AnimationSet
        out = new ByteArrayOutputStream(64 * 1024);
        animSetBuilder.build().writeTo(out);
        out.close();
        task.output(2).setContent(out.toByteArray());
    }
}




