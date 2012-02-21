package com.dynamo.cr.sceneed.core;

import java.util.List;

import com.dynamo.cr.editor.core.IResourceType;
import com.google.protobuf.Message;

public interface INodeType {

    String getExtension();

    INodeLoader<? extends Node, ? extends Message> getLoader();

    ISceneView.INodePresenter<?> getPresenter();

    INodeRenderer<Node> getRenderer();

    IResourceType getResourceType();

    Class<?> getNodeClass();
    List<INodeType> getReferenceNodeTypes();

    String getDisplayGroup();

}
