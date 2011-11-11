package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.sceneed.core.Node;

public class GameObjectNode extends Node {

    public void addComponent(ComponentNode component) {
        addChild(component);
    }

    public void removeComponent(ComponentNode component) {
        removeChild(component);
    }

    @Override
    public String toString() {
        return "Game Object";
    }

}
