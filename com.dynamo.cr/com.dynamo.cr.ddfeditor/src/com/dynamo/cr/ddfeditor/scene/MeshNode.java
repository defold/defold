package com.dynamo.cr.ddfeditor.scene;

import java.nio.FloatBuffer;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.bob.pipeline.Mesh;
import com.dynamo.cr.ddfeditor.Activator;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class MeshNode extends Node {

    private Mesh mesh;
    private Exception exception;

    public MeshNode(Mesh mesh) {
        this.mesh = mesh;
    }

    public MeshNode() {
    }

    public FloatBuffer getPositions() {
        return mesh.getPositions();
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
