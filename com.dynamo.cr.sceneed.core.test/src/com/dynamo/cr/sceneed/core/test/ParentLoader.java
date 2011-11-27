package com.dynamo.cr.sceneed.core.test;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.google.protobuf.Message;

public class ParentLoader implements INodeLoader<ParentNode> {

    @Override
    public ParentNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        return new ParentNode();
    }

    @Override
    public Message buildMessage(ILoaderContext context, ParentNode node, IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
