package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.sceneed.core.Node;

public abstract class ComponentTypeNode extends Node {

    public abstract String getTypeId();
    public abstract String getTypeName();

    public ComponentTypeNode() {
        super();
    }

    @Override
    public final String toString() {
        return getTypeName();
    }
}
