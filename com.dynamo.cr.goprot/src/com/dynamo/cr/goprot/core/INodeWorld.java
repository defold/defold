package com.dynamo.cr.goprot.core;

import org.eclipse.core.resources.IContainer;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public interface INodeWorld extends IPropertyObjectWorld {

    IContainer getContentRoot();

}
