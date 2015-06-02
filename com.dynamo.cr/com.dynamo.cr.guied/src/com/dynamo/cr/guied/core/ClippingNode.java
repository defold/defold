package com.dynamo.cr.guied.core;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.guied.util.Clipping;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.Property;
import com.dynamo.gui.proto.Gui.NodeDesc.ClippingMode;
import com.google.protobuf.Descriptors.EnumValueDescriptor;

@SuppressWarnings("serial")
public class ClippingNode extends GuiNode {

    public static class ClippingState {
        public int stencilRefVal = 0;
        public int stencilMask = 0;
        public int stencilWriteMask = 0;
        public boolean stencilClearBuffer = false;
        public boolean[] colorMask = new boolean[] {true, true, true, true};
        public boolean overflow = false;
    }

    @Property(displayName = "Clipping")
    private ClippingMode clippingMode = ClippingMode.CLIPPING_MODE_NONE;

    @Property(displayName = "Visible clipper")
    private boolean clippingVisible = true;

    @Property(displayName = "Inverted clipper")
    private boolean clippingInverted = false;

    private transient ClippingState clippingState;
    private transient ClippingState childClippingState;
    private transient int clippingKey;

    @Override()
    public void setClippingMode(ClippingMode mode) {
        clippingMode = mode;
        updateClippingState();
        GuiNodeStateBuilder.setField(this, "ClippingMode", mode);
    }

    @Override()
    public ClippingMode getClippingMode() {
        return clippingMode;
    }

    public void resetClippingMode() {
        this.clippingMode = ClippingMode.valueOf((EnumValueDescriptor)GuiNodeStateBuilder.resetField(this, "ClippingMode"));
        updateClippingState();
    }

    public boolean isClippingModeOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "ClippingMode", this.clippingMode);
    }

    @Override()
    public void setClippingVisible(boolean show) {
        clippingVisible = show;
        GuiNodeStateBuilder.setField(this, "ClippingVisible", show);
    }

    @Override()
    public boolean getClippingVisible() {
        return clippingVisible;
    }

    public void resetClippingVisible() {
        this.clippingVisible = (Boolean)GuiNodeStateBuilder.resetField(this, "ClippingVisible");
    }

    public boolean isClippingVisibleOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "ClippingVisible", this.clippingVisible);
    }

    public boolean isClippingVisibleEditable() {
        return clippingMode != ClippingMode.CLIPPING_MODE_NONE;
    }

    @Override()
    public void setClippingInverted(boolean inverted) {
        clippingInverted = inverted;
        updateClippingState();
        GuiNodeStateBuilder.setField(this, "ClippingInverted", inverted);
    }

    @Override()
    public boolean getClippingInverted() {
        return clippingInverted;
    }

    public void resetClippingInverted() {
        this.clippingInverted = (Boolean)GuiNodeStateBuilder.resetField(this, "ClippingInverted");
        updateClippingState();
    }

    public boolean isClippingInvertedOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "ClippingInverted", this.clippingInverted);
    }

    public boolean isClippingInvertedEditable() {
        return clippingMode == ClippingMode.CLIPPING_MODE_STENCIL;
    }

    protected IStatus validateClippingMode() {
        if (isStencil() == (this.clippingState == null)) {
            Clipping.updateClippingStates(getScene());
        }
        if (this.clippingState != null && this.clippingState.overflow) {
            return new Status(IStatus.ERROR, "com.dynamo", "Stencil hierarchy depth exceeded.");
        }
        return Status.OK_STATUS;
    }

    public boolean isClipping() {
        return this.clippingMode != ClippingMode.CLIPPING_MODE_NONE;
    }

    public boolean isStencil() {
        return this.clippingMode == ClippingMode.CLIPPING_MODE_STENCIL;
    }

    public ClippingState getClippingState() {
        return this.clippingState;
    }

    public void setClippingState(ClippingState clippingState) {
        this.clippingState = clippingState;
    }

    public ClippingState getChildClippingState() {
        return this.childClippingState;
    }

    public void setChildClippingState(ClippingState childClippingState) {
        this.childClippingState = childClippingState;
    }

    public int getClippingKey() {
        return this.clippingKey;
    }

    public void setClippingKey(int key) {
        this.clippingKey = key;
    }

    @Override
    public void parentSet() {
        updateClippingState();
    }

    private void updateClippingState() {
        if (getModel() != null) {
            boolean error = this.clippingState != null && this.clippingState.overflow;
            Clipping.updateClippingStates(getScene());
            if (error != (this.clippingState != null && this.clippingState.overflow)) {
                updateStatus();
            }
        }
    }
}

