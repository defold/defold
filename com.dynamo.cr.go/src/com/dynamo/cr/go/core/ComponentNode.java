package com.dynamo.cr.go.core;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;

public class ComponentNode extends Node {

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    @Unique
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
