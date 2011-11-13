package com.dynamo.cr.sceneed.gameobject;

import java.util.Comparator;

import com.dynamo.cr.sceneed.core.Node;

public class GameObjectNode extends Node {

    public GameObjectNode() {
        super();
    }

    public void addComponent(ComponentNode component) {
        addChild(component);
        sortComponents();
    }

    public void removeComponent(ComponentNode component) {
        removeChild(component);
    }

    public void sortComponents() {
        sortChildren(new Comparator<Node>() {
            @Override
            public int compare(Node o1, Node o2) {
                String id1 = ((ComponentNode)o1).getId();
                String id2 = ((ComponentNode)o2).getId();
                return id1.compareTo(id2);
            }
        });
    }

    @Override
    public String toString() {
        return "Game Object";
    }

}