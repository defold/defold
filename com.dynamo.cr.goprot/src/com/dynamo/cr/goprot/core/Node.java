package com.dynamo.cr.goprot.core;

import java.beans.PropertyChangeEvent;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class Node {

    private NodeModel model;
    private boolean selected;
    private List<Node> children;

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

    protected void addChild(Node child) {
        if (!this.children.contains(child)) {
            children.add(child);
            notifyChange();
        }
    }

    protected void removeChild(Node child) {
        if (this.children.contains(child)) {
            children.remove(child);
            notifyChange();
        }
    }

    protected void notifyChange() {
        if (this.model != null) {
            this.model.firePropertyChangeEvent(new PropertyChangeEvent(this, null, null, null));
        }
    }
}
