package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.google.protobuf.Message;

public class LightLoader implements INodeLoader<LightNode> {

    @Override
    public LightNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        return new LightNode();
    }

    @Override
    public Message buildMessage(ILoaderContext context, LightNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
