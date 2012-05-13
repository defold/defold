package com.dynamo.cr.sceneed.core.test;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.google.protobuf.Message;

public class DummyNodeLoader implements INodeLoader<DummyNode> {

    @Override
    public DummyNode load(ILoaderContext context, InputStream contents) throws IOException, CoreException {
        return new DummyNode();
    }

    @Override
    public Message buildMessage(ILoaderContext context, DummyNode node, IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
