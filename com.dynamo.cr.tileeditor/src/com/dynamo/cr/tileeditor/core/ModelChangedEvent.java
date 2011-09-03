package com.dynamo.cr.tileeditor.core;

public class ModelChangedEvent {
    public Object source;
    public String key;
    public Object previousValue;
    public Object value;

    public ModelChangedEvent(Object source, String key, Object previousValue, Object value) {
        this.source = source;
        this.key = key;
        this.previousValue = previousValue;
        this.value = value;
    }
}
