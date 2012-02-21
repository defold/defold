package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.editor.core.ILogger;
import com.google.protobuf.Message;

public interface ILoaderContext extends ILogger {
    Node loadNode(String path) throws IOException, CoreException;
    Node loadNode(String extension, InputStream contents) throws IOException, CoreException;
    <T extends Node, U extends Message> T loadNode(Class<T> nodeClass, U message) throws IOException, CoreException;
    Node loadNodeFromTemplate(Class<? extends Node> nodeClass) throws IOException, CoreException;
    Node loadNodeFromTemplate(String extension) throws IOException, CoreException;
    Message buildNodeMessage(Node node, IProgressMonitor monitor) throws IOException, CoreException;
    INodeTypeRegistry getNodeTypeRegistry();
}