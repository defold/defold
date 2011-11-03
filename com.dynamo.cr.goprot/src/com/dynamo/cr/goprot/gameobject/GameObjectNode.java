package com.dynamo.cr.goprot.gameobject;

import com.dynamo.cr.goprot.core.Node;

public class GameObjectNode extends Node {

    public void addComponent(ComponentNode component) {
        addChild(component);
    }

    public void removeComponent(ComponentNode component) {
        removeChild(component);
    }

}
