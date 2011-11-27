package com.dynamo.cr.sceneed.core.test;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.google.protobuf.Message;

public class ChildLoader implements INodeLoader<ChildNode> {

    @Override
    public ChildNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        return new ChildNode();
    }

    @Override
    public Message buildMessage(ILoaderContext context, ChildNode node, IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
