package com.dynamo.cr.guied.core;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.guied.util.Clipping;
import com.dynamo.gui.proto.Gui.NodeDesc.ClippingMode;

@SuppressWarnings("serial")
public class ClippingNode extends GuiNode {

    @Property(displayName = "Clipping")
    private ClippingMode clippingMode = ClippingMode.CLIPPING_MODE_NONE;

    @Property(displayName = "Visible clipper")
    private boolean clippingVisible = true;

    @Property(displayName = "Inverted clipper")
    private boolean clippingInverted = false;

    @Override()
    public void setClippingMode(ClippingMode mode) {
        clippingMode = mode;
    }

    @Override()
    public ClippingMode getClippingMode() {
        return clippingMode;
    }

    @Override()
    public void setClippingVisible(boolean show) {
        clippingVisible = show;
    }

    @Override()
    public boolean getClippingVisible() {
        return clippingVisible;
    }

    public boolean isClippingVisibleEditable() {
        return clippingMode != ClippingMode.CLIPPING_MODE_NONE;
    }

    @Override()
    public void setClippingInverted(boolean inverted) {
        clippingInverted = inverted;
    }

    @Override()
    public boolean getClippingInverted() {
        return clippingInverted;
    }

    public boolean isClippingInvertedEditable() {
        return clippingMode == ClippingMode.CLIPPING_MODE_STENCIL;
    }

    protected IStatus validateClippingMode() {
        switch(Clipping.validate(this)) {
        case OK:
            break;

        case INCOMPATIBLE_NODE_TYPE:
            return new Status(IStatus.ERROR, "com.dynamo", "Clipping mode not available for this node type.");

        case STENCIL_HIERARCHY_EXCEEDED:
            return new Status(IStatus.ERROR, "com.dynamo", "Stencil hierarchy depth exceeded.");

        default:
            assert(false);
        }
        return Status.OK_STATUS;
    }

}

