package com.dynamo.cr.sceneed.core;

import org.eclipse.core.resources.IContainer;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public interface ISceneWorld extends IPropertyObjectWorld {

    IContainer getContentRoot();

}
