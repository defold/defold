package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.FloatBuffer;

import javax.xml.stream.XMLStreamException;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.mesh.proto.Mesh.MeshComponent;
import com.dynamo.mesh.proto.Mesh.MeshDesc;
import com.dynamo.mesh.proto.Mesh.MeshDesc.Primitive;
import com.google.protobuf.Message;

@BuilderParams(name = "Mesh", inExts = ".dae", outExt = ".meshc")
public class MeshBuilder extends Builder<Void>  {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task<Void> task = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .build();
        return task;
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {


        ByteArrayInputStream is = new ByteArrayInputStream(task.input(0).getContent());

        Mesh mesh;
        try {
            mesh = ColladaUtil.loadMesh(is);
        } catch (XMLStreamException e) {
            throw new CompileExceptionError(task.input(0), e.getLocation().getLineNumber(), "Failed to compile mesh", e);
        } catch (LoaderException e) {
            throw new CompileExceptionError(task.input(0), -1, "Failed to compile mesh", e);
        }

        FloatBuffer pos = mesh.getPositions();
        FloatBuffer nrms = mesh.getNormals();
        FloatBuffer texcoord0 = mesh.getTexcoord0();
        int n = pos.capacity() / 3;
        MeshDesc.Builder b = MeshDesc.newBuilder();
        MeshComponent.Builder c = MeshComponent.newBuilder();
        for (int i = 0; i < n * 3; ++i) {
            c.addPositions(pos.get(i));
            c.addNormals(nrms.get(i));
        }
        for (int i = 0; i < n * 2; ++i) {
            c.addTexcoord0(texcoord0.get(i));
        }

        c.setPrimitiveCount(n);
        c.setName("unnamed");
        b.setPrimitiveType(Primitive.TRIANGLES);

        b.addComponents(c);

        Message msg = b.build();
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        msg.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}
