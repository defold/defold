package com.dynamo.cr.go.core;


@SuppressWarnings("serial")
public abstract class ComponentTypeNode extends ComponentNode {

    public boolean isIdVisible() {
        return getParent() != null;
    }

    @Override
    public String toString() {
        if (getParent() == null && getModel() != null) {
            return getModel().getTypeName(getClass());
        } else {
            return super.toString();
        }
    }
}
