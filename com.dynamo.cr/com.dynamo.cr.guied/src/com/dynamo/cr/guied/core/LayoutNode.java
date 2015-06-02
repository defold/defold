package com.dynamo.cr.guied.core;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.guied.util.Layouts;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;

@SuppressWarnings("serial")
public class LayoutNode extends Node implements Identifiable {

    @Property(displayName="display profile")
    @NotEmpty(severity = IStatus.ERROR)
    @Unique(scope = LayoutNode.class, base = LayoutsNode.class)
    private String id;

    public LayoutNode() {
        this.id = "";
    }

    public LayoutNode(String id) {
        this.id = id;
    }

    protected IStatus validateId() {
        if(this.id != GuiNodeStateBuilder.getDefaultStateId()) {
            Layouts.Layout layout = Layouts.getLayout(this.id);
            if (layout == null) {
                return new Status(IStatus.ERROR, "com.dynamo", "'" + id + "' is not a valid display profile.");
            }
        }
        return Status.OK_STATUS;
    }

    public boolean isIdEditable() {
        return getStatus() != Status.OK_STATUS;
    }

    @Override
    public String getId() {
        return this.id;
    }

    @Override
    public void setId(String id) {
        this.id = id;
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.LAYOUT_IMAGE_ID);
    }

    @Override
    public int getLabelTextStyle() {
        GuiSceneNode scene = (GuiSceneNode) this.getModel().getRoot();
        int style = SWT.NORMAL;
        if(this.id.compareTo(scene.getCurrentLayout().getId())==0) {
            style |= SWT.BOLD;
        }
        if(this.id.compareTo(scene.getDefaultLayout().getId())==0) {
            style |= SWT.ITALIC;
        }
        return style;
    }

}
