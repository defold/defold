package com.dynamo.cr.sceneed.core.test;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.google.protobuf.Message;

public class ChildLoader implements INodeLoader {

    public ChildLoader() {
        // TODO Auto-generated constructor stub
    }

    @Override
    public Node load(ILoaderContext context, String type, InputStream contents)
            throws IOException, CoreException {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public Node createNode(String type) throws IOException,
    CoreException {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public Message buildMessage(ILoaderContext context, Node node, IProgressMonitor monitor) throws IOException, CoreException {
        // TODO Auto-generated method stub
        return null;
    }

}
