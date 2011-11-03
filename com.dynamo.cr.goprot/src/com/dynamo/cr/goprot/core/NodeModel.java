package com.dynamo.cr.goprot.core;

import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.util.ArrayList;
import java.util.List;

public class NodeModel {
    private Node root;
    private List<PropertyChangeListener> listeners;

    public NodeModel() {
        this.listeners = new ArrayList<PropertyChangeListener>();
    }

    public Node getRoot() {
        return this.root;
    }

    public void setRoot(Node root) {
        if (this.root != root) {
            this.root = root;
            root.setModel(this);
        }
    }

    public void addListener(PropertyChangeListener listener) {
        if (!this.listeners.contains(listener)) {
            this.listeners.add(listener);
        }
    }

    public void removeListener(PropertyChangeListener listener) {
        this.listeners.remove(listener);
    }

    public void firePropertyChangeEvent(PropertyChangeEvent propertyChangeEvent) {
        for (PropertyChangeListener listener : this.listeners) {
            listener.propertyChange(propertyChangeEvent);
        }
    }
}
