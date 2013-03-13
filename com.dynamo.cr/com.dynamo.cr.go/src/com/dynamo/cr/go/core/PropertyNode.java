package com.dynamo.cr.go.core;

import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public abstract class PropertyNode<T extends Node> extends Node {
    private T node;

    public PropertyNode(T node) {
        super();
        this.node = node;
    }

    public T getNode() {
        return node;
    }

    public void setNode(T node) {
        this.node = node;
    }

    @Override
    public String toString() {
        return node.toString();
    }

    @Override
    public Image getIcon() {
        return node.getIcon();
    }

    @Override
    protected void childAdded(Node child) {
        sortChildren();
    }

    protected abstract void sortChildren();
}
