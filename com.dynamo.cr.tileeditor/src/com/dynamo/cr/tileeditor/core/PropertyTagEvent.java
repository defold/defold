package com.dynamo.cr.tileeditor.core;

import java.util.Set;

public class PropertyTagEvent {
    private final Object source;
    private final String propertyName;
    private final Set<Tag> tags;

    public PropertyTagEvent(Object source, String propertyName, Set<Tag> tags) {
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

    public Set<Tag> getTags() {
        return this.tags;
    }
}
