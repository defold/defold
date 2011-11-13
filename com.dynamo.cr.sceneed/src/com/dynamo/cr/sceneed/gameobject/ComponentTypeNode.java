package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodePresenter;

public abstract class ComponentTypeNode extends Node {

    public abstract String getTypeId();
    public abstract String getTypeName();

    public ComponentTypeNode(NodePresenter presenter) {
        super(presenter);
    }

    @Override
    public final String toString() {
        return getTypeName();
    }
}
