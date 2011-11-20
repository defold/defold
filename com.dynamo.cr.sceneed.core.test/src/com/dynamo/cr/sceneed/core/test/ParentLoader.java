package com.dynamo.cr.sceneed.core.test;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.google.protobuf.Message;

public class ParentLoader implements INodeLoader {

    public ParentLoader() {
    }

    @Override
    public Node load(ILoaderContext context, String type, InputStream contents)
            throws IOException, CoreException {
        if (type.equals("parent")) {
            return new ParentNode();
        }
        return null;
    }

    @Override
    public Node createNode(String type) throws IOException, CoreException {
        if (type.equals("parent")) {
            return new ParentNode();
        }
        return null;
    }

    @Override
    public Message buildMessage(ILoaderContext context, Node node, IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
