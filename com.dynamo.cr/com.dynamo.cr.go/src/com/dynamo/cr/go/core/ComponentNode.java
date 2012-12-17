package com.dynamo.cr.go.core;

import org.eclipse.core.runtime.IStatus;

import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;

@SuppressWarnings("serial")
public class ComponentNode extends Node implements Identifiable {

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    @Unique(scope = ComponentNode.class)
    private String id;

    public ComponentNode() {
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

}
