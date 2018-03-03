package com.dynamo.cr.ddfeditor.scene;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.cr.ddfeditor.Activator;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class MeshNode extends Node {

    private FloatBuffer positions;
    private FloatBuffer texCoords0;
    private Exception exception;

    private static FloatBuffer newFloatBuffer(int n) {
        ByteBuffer bb = ByteBuffer.allocateDirect(n * 4);
        return bb.order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer();
    }

    public MeshNode(Mesh mesh) {
        this.positions = newFloatBuffer(mesh.getIndicesCount()*3);
        this.texCoords0 = newFloatBuffer(mesh.getIndicesCount()*2);
        for(int i = 0; i < mesh.getIndicesCount(); i++){
            int ind = mesh.getIndices(i)*4;
            this.positions.put(mesh.getPositions(ind+0));
            this.positions.put(mesh.getPositions(ind+1));
            this.positions.put(mesh.getPositions(ind+2));
            ind = mesh.getTexcoord0Indices(i)*2;
            this.texCoords0.put(mesh.getTexcoord0(ind+0));
            this.texCoords0.put(mesh.getTexcoord0(ind+1));

        }
        positions.rewind();
        texCoords0.rewind();
    }

    public MeshNode() {
    }

    public FloatBuffer getPositions() {
        return positions;
    }

    public FloatBuffer getTexCoords0() {
        return texCoords0;
    }

    public void setLoadError(Exception e) {
        exception = e;
    }

    @Override
    protected IStatus validateNode() {
        if (exception == null) {
            return Status.OK_STATUS;
        } else {
            return new Status(IStatus.ERROR, Activator.PLUGIN_ID, exception.getMessage());
        }
    }

}
