package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;

public interface ILoaderContext {
    Node loadNode(String path) throws IOException, CoreException;
    Node loadNode(String extension, InputStream contents) throws IOException, CoreException;
    Node loadNodeFromTemplate(Class<? extends Node> nodeClass) throws IOException, CoreException;
    Node loadNodeFromTemplate(String extension) throws IOException, CoreException;
    INodeTypeRegistry getNodeTypeRegistry();
    void removeFromCache(String path);
}