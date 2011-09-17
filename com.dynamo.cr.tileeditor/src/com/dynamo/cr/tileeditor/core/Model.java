package com.dynamo.cr.tileeditor.core;

import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.tileeditor.Activator;

public class Model {
    private final List<PropertyChangeListener> listeners = new ArrayList<PropertyChangeListener>();

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

    protected Status createStatus(String message, Object[] binding) {
        return new Status(IStatus.ERROR, Activator.PLUGIN_ID, 0, NLS.bind(message, binding), null);
    }

    protected Status createStatus(String message, Object binding) {
        return new Status(IStatus.ERROR, Activator.PLUGIN_ID, 0, NLS.bind(message, binding), null);
    }

    protected Status createStatus(String message) {
        return new Status(IStatus.ERROR, Activator.PLUGIN_ID, 0, message, null);
    }
}
