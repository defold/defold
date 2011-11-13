package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Resource;

public class RefComponentNode extends ComponentNode {

    @Property(isResource=true)
    @Resource
    private String component;

    public RefComponentNode(ComponentTypeNode type) {
        super(type);
    }

    public String getComponent() {
        return this.component;
    }

    public void setComponent(String component) {
        if (this.component != null ? !this.component.equals(component) : component != null) {
            this.component = component;
            notifyChange();
        }
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getId(), this.component);
    }

}
