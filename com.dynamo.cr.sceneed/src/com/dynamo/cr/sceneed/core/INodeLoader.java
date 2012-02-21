package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.google.protobuf.Message;

public interface INodeLoader<T extends Node, U extends Message> {
    T load(ILoaderContext context, InputStream contents) throws IOException, CoreException;
    T load(ILoaderContext context, U message) throws IOException, CoreException;
    U buildMessage(ILoaderContext context, T node, IProgressMonitor monitor) throws IOException, CoreException;
}