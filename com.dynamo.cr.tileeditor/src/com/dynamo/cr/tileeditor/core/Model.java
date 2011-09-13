package com.dynamo.cr.tileeditor.core;

import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.util.ArrayList;
import java.util.List;

public class Model {

    private final List<PropertyChangeListener> listeners;

    public Model() {
        this.listeners = new ArrayList<PropertyChangeListener>();
    }

    public void addTaggedPropertyListener(PropertyChangeListener listener) {
        this.listeners.add(listener);
    }

    public void removeTaggedPropertyListener(PropertyChangeListener listener) {
        this.listeners.remove(listener);
    }

    protected void firePropertyChangeEvent(PropertyChangeEvent e) {
        for (PropertyChangeListener listener : this.listeners) {
            listener.propertyChange(e);
        }
    }

}
