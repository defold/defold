package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.google.protobuf.Message;

public class ModelLoader implements INodeLoader<ModelNode> {

    @Override
    public ModelNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        return new ModelNode();
    }

    @Override
    public Message buildMessage(ILoaderContext context, ModelNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
