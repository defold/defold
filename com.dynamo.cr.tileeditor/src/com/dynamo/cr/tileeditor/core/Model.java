package com.dynamo.cr.tileeditor.core;

import java.util.ArrayList;
import java.util.List;

public class Model {

    private final List<IModelChangedListener> listeners;

    public Model() {
        this.listeners = new ArrayList<IModelChangedListener>();
    }

    public void addModelChangedListener(IModelChangedListener listener) {
        this.listeners.add(listener);
    }

    public void removeModelChangedListener(IModelChangedListener listener) {
        this.listeners.remove(listener);
    }

    protected void fireModelChangedEvent(ModelChangedEvent e) {
        for (IModelChangedListener listener : this.listeners) {
            listener.onModelChanged(e);
        }
    }

}
