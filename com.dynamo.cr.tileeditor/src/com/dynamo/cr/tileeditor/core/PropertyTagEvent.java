package com.dynamo.cr.tileeditor.core;

import java.util.List;

public class PropertyTagEvent {
    private final Object source;
    private final String propertyName;
    private final List<Tag> tags;

    public PropertyTagEvent(Object source, String propertyName, List<Tag> tags) {
        this.source = source;
        this.propertyName = propertyName;
        this.tags = tags;
    }

    public Object getSource() {
        return this.source;
    }

    public String getPropertyName() {
        return this.propertyName;
    }

    public List<Tag> getTags() {
        return this.tags;
    }
}
