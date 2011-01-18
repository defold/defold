package com.dynamo.cr.editor.core;

public interface IResourceTypeRegistry {

    IResourceType[] getResourceTypes();

    IResourceType getResourceTypeFromExtension(String type);

}
