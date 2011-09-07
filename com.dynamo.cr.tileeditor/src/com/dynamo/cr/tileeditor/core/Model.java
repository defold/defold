package com.dynamo.cr.tileeditor.core;

import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.util.ArrayList;
import java.util.List;

public class Model {

    private final List<TaggedPropertyListener> listeners;

    public Model() {
        this.listeners = new ArrayList<TaggedPropertyListener>();
    }

    public void addTaggedPropertyListener(TaggedPropertyListener listener) {
        this.listeners.add(listener);
    }

    public void removeTaggedPropertyListener(TaggedPropertyListener listener) {
        this.listeners.remove(listener);
    }

    protected void firePropertyChangeEvent(PropertyChangeEvent e) {
        for (PropertyChangeListener listener : this.listeners) {
            listener.propertyChange(e);
        }
    }

    protected void firePropertyTagEvent(PropertyTagEvent e) {
        for (TaggedPropertyListener listener : this.listeners) {
            listener.propertyTag(e);
        }
    }

}
