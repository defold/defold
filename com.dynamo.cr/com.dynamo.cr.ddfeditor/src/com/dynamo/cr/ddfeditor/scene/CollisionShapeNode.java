package com.dynamo.cr.ddfeditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.ddfeditor.Activator;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public abstract class CollisionShapeNode extends Node {

    private IStatus boundsErrorStatus = null;

    public CollisionShapeNode() {
        super();
        setFlags(Flags.TRANSFORMABLE);
        setFlags(Flags.SUPPORTS_SCALE);
    }

    public CollisionShapeNode(Vector4d position, Quat4d rotation) {
        super(position, rotation);
        setFlags(Flags.TRANSFORMABLE);
        setFlags(Flags.SUPPORTS_SCALE);
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

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.COLLISION_SHAPE_IMAGE_ID);
    }

}
