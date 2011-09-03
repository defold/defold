package com.dynamo.cr.tileeditor.core;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class Model {
    HashMap<Object, HashMap<String, Object>> objectProperties;
    List<IModelChangedListener> listeners;

    public Model() {
        objectProperties = new HashMap<Object, HashMap<String, Object>>();
        listeners = new ArrayList<IModelChangedListener>();
    }

    public void addModelChangedListener(IModelChangedListener listener) {
        listeners.add(listener);
    }

    public void removeModelChangedListener(IModelChangedListener listener) {
        listeners.remove(listener);
    }

    protected Object getProperty(Object source, String key) {
        HashMap<String, Object> properties = objectProperties.get(source);
        if (properties != null) {
            return properties.get(key);
        } else {
            return null;
        }
    }

    public void setProperty(Object source, String key, Object value) {
        HashMap<String, Object> properties = objectProperties.get(source);
        if (properties == null) {
            properties = new HashMap<String, Object>();
            objectProperties.put(source, properties);
        }
        Object previousValue = properties.get(key);
        if (value == null || !value.equals(previousValue)) {
            properties.put(key, value);
            fireModelChangedEvent(new ModelChangedEvent(source, key, previousValue, value));
        }
    }

    private void fireModelChangedEvent(ModelChangedEvent e) {
        for (IModelChangedListener listener : listeners) {
            listener.onModelChanged(e);
        }
    }
}
