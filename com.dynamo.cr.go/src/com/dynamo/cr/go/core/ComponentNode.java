package com.dynamo.cr.go.core;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.go.Constants;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Node;

public class ComponentNode extends Node {

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    private String id;

    public ComponentNode() {
    }

    public ComponentNode(ComponentTypeNode type) {
        addChild(type);
    }

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        this.id = id;
        if (getParent() != null) {
            ((GameObjectNode)getParent()).sortComponents();
        }
    }

    public IStatus validateId() {
        if (getParent() != null) {
            for (Node sibling : getParent().getChildren()) {
                if (sibling != this) {
                    if (this.id.equals(((ComponentNode)sibling).getId())) {
                        return new Status(IStatus.ERROR, Constants.PLUGIN_ID, NLS.bind(Messages.ComponentNode_id_DUPLICATED, this.id));
                    }
                }
            }
        }
        return Status.OK_STATUS;
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public Image getIcon() {
        if (hasChildren()) {
            return this.getChildren().get(0).getIcon();
        }
        return super.getIcon();
    }

}
