package com.dynamo.cr.goprot.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.eclipse.swt.graphics.Image;

public class Node {

    private NodeModel model;
    private boolean selected;
    private List<Node> children;
    private Node parent;

    public Node() {
        this.children = new ArrayList<Node>();
    }

    public boolean isSelected() {
        return selected;
    }

    public void setSelected(boolean selected) {
        if (this.selected != selected) {
            notifyChange();
            this.selected = selected;
        }
    }

    public NodeModel getModel() {
        return this.model;
    }

    public void setModel(NodeModel model) {
        this.model = model;
    }

    public List<Node> getChildren() {
        return Collections.unmodifiableList(this.children);
    }

    public boolean hasChildren() {
        return !this.children.isEmpty();
    }

    protected void addChild(Node child) {
        if (!this.children.contains(child)) {
            children.add(child);
            child.parent = this;
            notifyChange();
        }
    }

    protected void removeChild(Node child) {
        if (this.children.contains(child)) {
            children.remove(child);
            child.parent = null;
            notifyChange();
        }
    }

    protected void notifyChange() {
        if (this.model != null) {
            this.model.notifyChange(this);
        }
    }

    public Node getParent() {
        return this.parent;
    }

    public Image getImage() {
        return null;
    }
}
