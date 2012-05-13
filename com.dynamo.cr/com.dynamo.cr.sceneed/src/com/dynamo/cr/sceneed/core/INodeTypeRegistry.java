package com.dynamo.cr.sceneed.core;

public interface INodeTypeRegistry {

    INodeType[] getNodeTypes();

    INodeType getNodeTypeFromExtension(String extension);
    INodeType getNodeTypeFromID(String id);
    INodeType getNodeTypeClass(Class<? extends Node> nodeClass);

}
