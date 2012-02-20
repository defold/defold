package com.dynamo.cr.sceneed.core;

import java.util.List;

import com.dynamo.cr.editor.core.IResourceType;

public interface INodeType {

    String getExtension();

    INodeLoader getLoader();

    ISceneView.INodePresenter<?> getPresenter();

    INodeRenderer<Node> getRenderer();

    IResourceType getResourceType();

    Class<?> getNodeClass();
    List<Class<?>> getChildClasses();

    String getDisplayGroup();

}
