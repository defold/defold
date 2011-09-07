package com.dynamo.cr.tileeditor.core;

import java.beans.PropertyChangeListener;

public interface TaggedPropertyListener extends PropertyChangeListener {
    public void propertyTag(PropertyTagEvent evt);
}
