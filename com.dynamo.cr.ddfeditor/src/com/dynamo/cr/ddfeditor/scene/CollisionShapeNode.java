package com.dynamo.cr.ddfeditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.Node;

public abstract class CollisionShapeNode extends Node {

    private IStatus boundsErrorStatus = null;

    public CollisionShapeNode(Vector4d position, Quat4d rotation) {
        super(position, rotation);
    }

    protected void createBoundsStatusError() {
        boundsErrorStatus = new Status(IStatus.ERROR, Activator.PLUGIN_ID, NLS.bind(Messages.CollisionShape_bounds_ERROR, null));
    }

    protected void clearBoundsStatusError() {
        boundsErrorStatus = null;
    }

    @Override
    protected IStatus validateNode() {
        if (boundsErrorStatus != null)
            return boundsErrorStatus;
        else
            return Status.OK_STATUS;
    }

}
