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
    private Exception exception;

    private static FloatBuffer newFloatBuffer(int n) {
        ByteBuffer bb = ByteBuffer.allocateDirect(n * 4);
        return bb.order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer();
    }

    public MeshNode(Mesh mesh) {
        this.positions = newFloatBuffer(mesh.getPositionsCount());
        for(int i = 0; i < mesh.getPositionsCount(); i++){
            this.positions.put(mesh.getPositions(i));
        }
        positions.rewind();
    }

    public MeshNode() {
    }

    public FloatBuffer getPositions() {
        return positions;
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
