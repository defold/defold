package com.dynamo.cr.scene.resource;

import java.util.ArrayList;

public class Resource {

    private String path;
    private ArrayList<IResourceListener> listeners;

    Resource(String path) {
        this.path = path;
        this.listeners = new ArrayList<IResourceListener>();
    }

    public String getPath() {
        return path;
    }

    public void addListener(IResourceListener listener) {
        this.listeners.add(listener);
    }

    public void removeListener(IResourceListener listener) {
        this.listeners.remove(listener);
    }

    public void fireChanged() {
        for (IResourceListener l : this.listeners) {
            l.resourceChanged(this);
        }
    }

}
