package com.dynamo.cr.ddfeditor.scene;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.cr.ddfeditor.Activator;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class MeshNode extends Node {

    private IntBuffer indices;
    private FloatBuffer positions;
    private Exception exception;

    private static FloatBuffer newFloatBuffer(int n) {
        ByteBuffer bb = ByteBuffer.allocateDirect(n * 4);
        return bb.order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer();
    }

    private static IntBuffer newIntBuffer(int n) {
        ByteBuffer bb = ByteBuffer.allocateDirect(n * 4);
        return bb.order(ByteOrder.LITTLE_ENDIAN).asIntBuffer();
    }

    public MeshNode(Mesh mesh) {
        this.positions = newFloatBuffer(mesh.getPositionsCount());
        for(int i = 0; i < mesh.getPositionsCount(); i++){
            this.positions.put(mesh.getPositions(i));
        }
        positions.rewind();

        this.indices = newIntBuffer(mesh.getIndicesCount());
        for(int i = 0; i < mesh.getIndicesCount(); i++){
            this.indices.put(mesh.getIndices(i));
        }
        indices.rewind();
    }

    public MeshNode() {
    }

    public FloatBuffer getPositions() {
        return positions;
    }

    public IntBuffer getIndices() {
        return indices;
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
