package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.google.protobuf.Message;

public class GuiLoader implements INodeLoader<GuiNode> {

    @Override
    public GuiNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        return new GuiNode();
    }

    @Override
    public Message buildMessage(ILoaderContext context, GuiNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
