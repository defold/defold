package com.dynamo.cr.sceneed.core.test;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.ISceneView.Context;
import com.dynamo.cr.sceneed.core.ISceneView.NodePresenter;
import com.google.protobuf.Message;

public class ChildPresenter implements NodePresenter {

    public ChildPresenter() {
        // TODO Auto-generated constructor stub
    }

    @Override
    public Node onLoad(Context context, String type, InputStream contents)
            throws IOException, CoreException {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public Node onCreateNode(Context context, String type) throws IOException,
    CoreException {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public Message onBuildMessage(Context context, Node node,
            IProgressMonitor monitor) throws IOException, CoreException {
        // TODO Auto-generated method stub
        return null;
    }

}
