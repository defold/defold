package com.dynamo.cr.tileeditor.core;

import org.eclipse.core.resources.IContainer;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public interface ITileWorld extends IPropertyObjectWorld {
    public IContainer getContentRoot();
}
