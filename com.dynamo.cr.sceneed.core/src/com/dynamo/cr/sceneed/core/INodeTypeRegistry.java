package com.dynamo.cr.sceneed.core;

public interface INodeTypeRegistry {

    INodeType[] getNodeTypes();

    INodeType getNodeTypeFromExtension(String extension);
    INodeType getNodeTypeFromID(String id);
    INodeType getNodeType(Class<? extends Node> nodeClass);

}
