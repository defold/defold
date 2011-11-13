package com.dynamo.cr.sceneed;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodePresenter;
import com.google.protobuf.Message;

public class DefaultNodePresenter extends NodePresenter {

    @Override
    public Node doLoad(String type, InputStream contents) throws IOException, CoreException {
        throw new UnsupportedOperationException();
    }

    @Override
    public Node createNode(String type) throws IOException, CoreException {
        throw new UnsupportedOperationException();
    }

    @Override
    public Message buildMessage(Node node, IProgressMonitor monitor) throws IOException, CoreException {
        throw new UnsupportedOperationException();
    }

}
