package com.dynamo.cr.sceneed.core;

public interface INodeTypeRegistry {

    INodeType[] getNodeTypes();

    INodeType getNodeType(String extension);
    INodeType getNodeType(Class<? extends Node> nodeClass);

}
