package com.dynamo.cr.sceneed.go;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Node;

public class ComponentNode extends Node {

    @Property
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
        if (this.id != null ? !this.id.equals(id) : id != null) {
            this.id = id;
            notifyChange();
            if (getParent() != null) {
                ((GameObjectNode)getParent()).sortComponents();
            }
        }
    }

    public IStatus validateId() {
        if (this.id == null || this.id.equals("")) {
            return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.ComponentNode_id_NOT_SPECIFIED);
        } else if (getParent() != null) {
            for (Node sibling : getParent().getChildren()) {
                if (sibling != this) {
                    if (this.id.equals(((ComponentNode)sibling).getId())) {
                        return new Status(IStatus.ERROR, Activator.PLUGIN_ID, NLS.bind(Messages.ComponentNode_id_DUPLICATED, this.id));
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
    protected Class<? extends NLS> getMessages() {
        return Messages.class;
    }

    @Override
    public Image getImage() {
        if (hasChildren()) {
            return this.getChildren().get(0).getImage();
        } else {
            return Activator.getDefault().getImageRegistry().get(Activator.IMG_COMPONENT);
        }
    }
}
