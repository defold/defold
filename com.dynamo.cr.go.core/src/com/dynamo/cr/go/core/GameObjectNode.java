package com.dynamo.cr.go.core;

import java.util.Comparator;

import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.sceneed.core.Node;

public class GameObjectNode extends Node {

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

    @Override
    protected Class<? extends NLS> getMessages() {
        return Messages.class;
    }

}