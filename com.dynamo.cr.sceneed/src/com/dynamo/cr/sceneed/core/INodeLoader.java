package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.google.protobuf.Message;

public interface INodeLoader {
    Node load(ILoaderContext context, Class<? extends Node> type, InputStream contents) throws IOException, CoreException;
    Message buildMessage(ILoaderContext context, Node node, IProgressMonitor monitor) throws IOException, CoreException;
}